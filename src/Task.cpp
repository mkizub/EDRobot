//
// Created by mkizub on 23.05.2025.
//

#include "pch.h"

#include "Task.h"
#include "Keyboard.h"
#include "Template.h"
#include "UI.h"
#include <synchapi.h>

void Task::preciseSleep(double seconds)const {
    using namespace std;
    using namespace std::chrono;

    static double estimate = 5e-3;
    static double mean = 5e-3;
    static double m2 = 0;
    static int64_t count = 1;

    while (seconds > estimate) {
        auto start = high_resolution_clock::now();
        this_thread::sleep_for(milliseconds(1));
        auto end = high_resolution_clock::now();

        double observed = (end - start).count() / 1e9;
        seconds -= observed;

        ++count;
        double delta = observed - mean;
        mean += delta / count;
        m2   += delta * (observed - mean);
        double stddev = sqrt(m2 / (count - 1));
        estimate = mean + stddev;
        check_completed();
    }

    // spin lock
    auto start = high_resolution_clock::now();
    while ((high_resolution_clock::now() - start).count() / 1e9 < seconds)
        check_completed();
}

void Task::sleep(int milliseconds) const {
    check_completed();
    if (milliseconds <= 0)
        return;
    if (milliseconds >= 75) {
        Sleep(milliseconds);
        check_completed();
        return;
    }
//    try {
//        timeBeginPeriod(1);
//        preciseSleep(milliseconds * 0.001);
//        timeEndPeriod(1);
//    } catch (...) {
//        timeEndPeriod(1);
//        throw;
//    }

    LARGE_INTEGER frequency, start, end;
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&start);

    double seconds = milliseconds * 0.001;
    while (true) {
        QueryPerformanceCounter(&end);
        double elapsed_seconds = static_cast<double>(end.QuadPart - start.QuadPart) / frequency.QuadPart;
        if (elapsed_seconds >= seconds)
            break;
        check_completed();
    }
}

bool Task::sendKey(const std::string& name, int delay_ms, int pause_ms) const {
    try {
        if (!keyboard::sendKeyDown(name))
            return false;
        sleep(delay_ms > 0 ? delay_ms : master.getDefaultKeyHoldTime());
        bool ok = keyboard::sendKeyUp(name);
        sleep(pause_ms > 0 ? pause_ms : master.getDefaultKeyAfterTime());
        return ok;
    } catch (...) {
        keyboard::sendKeyUp(name);
        throw;
    }
}

bool Task::sendMouseMove(const cv::Point& point, int pause_ms, bool absolute) const {
    bool virtualDesktop = false;
    int x = point.x;
    int y = point.y;
    if (absolute) {
        virtualDesktop = (GetSystemMetrics(SM_CMONITORS) > 1);
        cv::Point screen = master.lastEDState().cEnv.cvtReferenceToDesktop(point);
        screen = master.lastEDState().cEnv.cvtReferenceToDesktop(point);
        x = screen.x;
        y = screen.y;
    }
    //LOG(INFO) << "sendMouseMove recalculated from reference " << point << " to screen " << screen;
    if (!keyboard::sendMouseMoveTo(x, y, absolute, virtualDesktop))
        return false;
    sleep(pause_ms > 0 ? pause_ms : master.getDefaultKeyAfterTime());
    return true;
}

bool Task::sendMouseClick(const cv::Point& point, int delay_ms, int pause_ms) const {
    cv::Point screen = master.lastEDState().cEnv.cvtReferenceToDesktop(point);
    bool virtualDesktop = (GetSystemMetrics(SM_CMONITORS) > 1);
    //LOG(INFO) << "sendMouseClick recalculated from reference " << point << " to screen " << screen;
    if (!keyboard::sendMouseMoveTo(screen.x, screen.y, true, virtualDesktop))
        return false;
    try {
        sleep(delay_ms > 0 ? delay_ms : master.getDefaultKeyHoldTime());
        if (!keyboard::sendMouseDown(keyboard::MOUSE_L_BUTTON))
            return false;
        sleep(delay_ms > 0 ? delay_ms : master.getDefaultKeyHoldTime());
        bool ok = keyboard::sendMouseUp(keyboard::MOUSE_L_BUTTON);
        sleep(pause_ms > 0 ? pause_ms : master.getDefaultKeyAfterTime());
        return ok;
    } catch (...) {
        keyboard::sendMouseUp(keyboard::MOUSE_L_BUTTON);
        throw;
    }
    return true;
}

static int get_int(const json5pp::value& val, const json5pp::value& args, int dflt = -1) {
    if (val.is_null() && dflt >= 0)
        return dflt;
    if (val.is_integer())
        return val.as_integer();
    if (val.is_string()) {
        const json5pp::value& resolved = args.at(val.as_string());
        if (resolved.is_integer())
            return resolved.as_integer();
    }
    LOG(ERROR) << "integer value expected, but got: " << val << " with args: " << args;
    return 0;
}

bool Task::decodePosition(const json5pp::value& pos, cv::Point& point, const json5pp::value& args) const {
    if (pos.is_string()) {
        cv::Rect rect = master.resolveWidgetReferenceRect(master.lastEDState().path() + ":" + pos.as_string());
        if (rect.empty()) {
            LOG(ERROR) << "Widget '" << pos << "' not found in current state";
            return false;
        }
        point = (rect.tl() + rect.br()) * 0.5;
        return true;
    }
    else if (pos.is_array()) {
        int x = get_int(pos.at(0), args);
        int y = get_int(pos.at(1), args);
        point = {x, y};
        if (x < 0 || y < 0) {
            LOG(ERROR) << "Bad position " << point;
            return false;
        }
        return true;
    }
    LOG(ERROR) << "Expected button name or [x,y]";
    return false;
}

bool Task::executeAction(const std::string& actionName, const json5pp::value& args = json5pp::value()) {
    const json5pp::value& action = taskActions.at(actionName);
    if (!action.is_object()) {
        LOG(ERROR) << "Action '" << actionName << "' not found";
        return false;
    }
    if (!action.at("from").is_string() || !action.at("dest").is_string()) {
        LOG(ERROR) << "Action '" << actionName << "' has no 'from' or 'dest' states declarations";
        return false;
    }
    this->actionName = actionName;
    this->fromState = action.at("from").as_string();
    this->destState = action.at("dest").as_string();
    const json5pp::value& execute = action.at("exec");

    return executeStep(execute, args);
}

bool Task::executeWait(const json5pp::value& step, const json5pp::value& args) {
    LOG(DEBUG) << "action step wait: " << step;
    const json5pp::value& state = step.at("wait");
    const json5pp::value &focus = step.at("focus");
    const json5pp::value &disabled = step.at("disabled");
    auto start = std::chrono::system_clock::now();
    auto now = start;
    int during = 1000;
    int period = 250;
    if (step.at("during").is_integer())
        during = std::max(100, step.at("during").as_integer());
    if (step.at("period").is_integer())
        period = std::max(100, step.at("period").as_integer());
    auto until = now + std::chrono::duration<int, std::milli>(during);
    LOG(INFO) << "Step 'wait' #0 duration " << during << " left " << std::chrono::duration_cast<std::chrono::milliseconds>(until - now).count();
    bool ok;
    for (int counter=1; now < until; counter++) {
        master.detectEDState(DetectLevel::Buttons);
        ok = master.isEDStateMatch(state.as_string());
        if (ok) {
            bool ok_focus = true;
            if (focus.is_string()) {
                auto& cbs = master.lastEDState().cEnv.classifiedButtonStates;
                auto it = cbs.find(focus.as_string());
                ok_focus = (it != cbs.end() && it->second == WState::Focused);
            }
            bool ok_disabled = true;
            if (disabled.is_string()) {
                auto& cbs = master.lastEDState().cEnv.classifiedButtonStates;
                auto it = cbs.find(disabled.as_string());
                ok_disabled = (it != cbs.end() && it->second == WState::Disabled);
            }
            ok = ok_focus && ok_disabled;
            if (ok)
                break;
        }
        std::this_thread::sleep_for(std::chrono::duration<int, std::milli>(period));
        now = std::chrono::system_clock::now();
        LOG_IF(!ok,INFO) << "Step 'wait' #"<<counter<<" duration " << during << " left " << std::chrono::duration_cast<std::chrono::milliseconds>(until - now).count();
    }
    LOG_IF(!ok,ERROR) << "Step " << step << " failed - wait time expired, current state is " << master.lastEDState();
    LOG_IF(ok,INFO) << "Step " << step << " successful, waited " << std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count();
    return ok;
}

bool Task::executeStep(const json5pp::value& step, const json5pp::value& args) {
    if (step.is_array()) {
        for (auto& s : step.as_array()) {
            bool ok = executeStep(s, args);
            if (!ok)
                return false;
        }
        return true;
    }
    if (step.is_object()) {
        if (step.as_object().contains("loop")) {
            LOG(DEBUG) << "action step loop: " << step;
            const json5pp::value& loop = step.at("loop");
            const json5pp::value& action = step.at("action");
            int count = get_int(loop, args);
            if (count < 0) {
                LOG(ERROR) << "bad loop counter value: " << step << " with args: " << args;
                LOG(ERROR) << "Step " << step << " failed";
                return false;
            }
            for (int i=0; i < count; i++) {
                bool ok = executeStep(action, args);
                LOG_IF(!ok,ERROR) << "Step " << step << " failed";
                if (!ok)
                    return false;
            }
            return true;
        }
        if (step.as_object().contains("wait")) {
            return executeWait(step, args);
        }
        if (step.as_object().contains("check")) {
            LOG(DEBUG) << "action step check: " << step;
            const json5pp::value& state = step.at("check");
            master.detectEDState(DetectLevel::Buttons);
            bool ok = master.isEDStateMatch(state.as_string());
            if (ok) {
                const json5pp::value &focus = step.at("focus");
                if (focus) {
                    const widget::Widget* fw = master.lastEDState().focused;
                    std::string fn = fw ? fw->name : "";
                    ok = focus.is_string() && fn == focus.as_string();
                    LOG_IF(!ok,ERROR) << "Step failed, current focus at '" << fn << "', but '" << focus << "' required";
                }
            }
            LOG_IF(!ok,ERROR) << "Step " << step << " failed, current state is " << master.lastEDState();
            return ok;
        }
        if (step.as_object().contains("key")) {
            LOG(DEBUG) << "action step key: " << step;
            const json5pp::value& key = step.at("key");
            bool ok;
            if (step.at("hold").is_object() || step.at("hold").is_array()) {
                keyboard::sendKeyDown(key.as_string());
                ok = executeStep(step.at("hold"), args);
                keyboard::sendKeyUp(key.as_string());
                int after = get_int(step.at("after"), args, master.getDefaultKeyAfterTime());
                sleep(after);
            } else {
                int hold = get_int(step.at("hold"), args, master.getDefaultKeyHoldTime());
                int after = get_int(step.at("after"), args, master.getDefaultKeyAfterTime());
                ok = sendKey(key.as_string(), hold, after);
            }
            LOG_IF(!ok,ERROR) << "Step " << step << " failed";
            return ok;
        }
        if (step.as_object().contains("goto")) {
            LOG(DEBUG) << "action goto: " << step;
            const json5pp::value& widget = step.at("goto");
            cv::Point pos;
            if (!decodePosition(widget, pos, args)) {
                LOG(ERROR) << "Step " << step << " failed";
                return false;
            }
            int after = get_int(step.at("after"), args, master.getDefaultKeyAfterTime());
            bool ok = sendMouseMove(pos, after);
            LOG_IF(!ok,ERROR) << "Step " << step << " failed";
            return ok;
        }
        if (step.as_object().contains("click")) {
            LOG(DEBUG) << "action click: " << step;
            const json5pp::value& widget = step.at("click");
            cv::Point pos;
            if (!decodePosition(widget, pos, args)) {
                LOG(ERROR) << "Step " << step << " failed";
                return false;
            }
            int hold = get_int(step.at("hold"), args, master.getDefaultKeyHoldTime());
            int after = get_int(step.at("after"), args, master.getDefaultKeyAfterTime());
            bool ok = sendMouseClick(pos, hold, after);
            LOG_IF(!ok,ERROR) << "Step " << step << " failed";
            return ok;
        }
        if (step.as_object().contains("sleep")) {
            LOG(DEBUG) << "action step sleep: " << step;
            int duration = get_int(step.at("sleep"), args);
            sleep(duration);
            return true;
        }
        // fall through
    }
    LOG(ERROR) << "Unknown action step: " << step;
    return false;
}

void Task::hardcodedStep(const char* step, DetectLevel level) {
    json5pp::value parsed, args;
    try {
        std::stringstream in(step);
        in >> json5pp::rule::json5() >> parsed;
    } catch (...) {
        LOG(ERROR) << "Failed to parse json " << step;
        throw completed_error("hardcoded step failed");
    }
    if (!executeStep(parsed, args)) {
        LOG(ERROR) << "Failed to execute " << step;
        throw completed_error("hardcoded step failed");
    }
    master.detectEDState(level);
}

void Task::notifyProgress(const std::string& msg) const {
    LOG(INFO) << msg;
    UI::showToast(taskName, msg);
}
void Task::notifyError(const std::string& msg) const {
    LOG(ERROR) << msg;
    UI::showToast(taskName, msg);
}

TaskCalibrate::TaskCalibrate()
    : mDetector(HistogramTemplate::CompareMode::Luv, cv::Rect(), cv::Vec3b())
{
    taskName = "Calibration";
}

void TaskCalibrate::recordButtonLuv(const char* button, WState bs) {
    cv::Rect rect = master.resolveWidgetReferenceRect(master.lastEDState().path()+":"+button);
    if (rect.empty()) {
        LOG(ERROR) << "Cannot get rect of button '" << button << "'";
        return;
    }
    mDetector.mRect = rect;
    ClassifyEnv cEnv = master.lastEDState().cEnv; // copy
    mDetector.match(cEnv);
    cv::Vec3b luv = mDetector.mLastColor;
    mButtonLuv[int(bs)].push_back(luv);
    const char* names[] = {"Normal   ", "Focused  ", "Active   ", "Disabled "};
    LOG(INFO) << names[int(bs)] << " button: luv=" << mButtonLuv[int(bs)].back()
              << " bgr=" << luv2rgb(luv) << " rgb=0x"<< std::format("{:06x}", decodeRGB(luv2rgb(luv)));
}

void TaskCalibrate::recordLstRowLuv(const char* list, cv::Point mouse, WState bs) {
    cv::Rect rect = master.resolveWidgetReferenceRect(master.lastEDState().path()+":"+list);
    if (rect.empty()) {
        LOG(ERROR) << "Cannot get rect of list '" << list << "'";
        return;
    }
    ClassifyEnv cEnv = master.lastEDState().cEnv; // copy
    auto& rows = cEnv.classifiedListRows[list];
    if (rows.empty()) {
        LOG(ERROR) << "Cannot get rows of list '" << list << "'";
        return;
    }
    std::vector<cv::Vec3b> colors;
    std::vector<double> lums;
    for (auto& cr : rows) {
        cv::Rect refRect = cEnv.cvtCapturedToReference(cr.detectedRect);
        mDetector.mRect = refRect;
        mDetector.match(cEnv);
        cv::Vec3b luv = mDetector.mLastColor;
        if (refRect.contains(mouse)) {
            mLstRowLuv[int(WState::Focused)].push_back(luv);
        } else {
            colors.push_back(luv);
            lums.push_back(colors.back()[0]);
        }
    }
    cv::Point minLoc;
    cv::Point maxLoc;
    cv::minMaxLoc(lums, nullptr, nullptr, &minLoc, &maxLoc);
    cv::Vec3d minColor(colors[minLoc.x]);
    cv::Vec3d maxColor(colors[maxLoc.x]);
    cv::Vec3d delta = maxColor - minColor;

    if (cv::norm(delta) < 6) {
        mLstRowLuv[int(bs)].insert(mLstRowLuv[int(bs)].end(), colors.begin(), colors.end());
    } else {
        for (auto& c : colors) {
            double distNorm = cv::norm(minColor - cv::Vec3d(c));
            double distActv = cv::norm(maxColor - cv::Vec3d(c));
            if (distNorm < distActv)
                mLstRowLuv[int(WState::Normal)].push_back(c);
            else
                mLstRowLuv[int(WState::Active)].push_back(c);
        }
    }
}

bool TaskCalibrate::calculateAverage(bool incomplete) {
    bool buttonSuccess = true;
    for(auto bs : enum_values<WState>()) {
        auto& luvState = mButtonLuv[int(bs)];
        int len = (int)luvState.size();
        if (!len) {
            LOG(INFO) << "No samples for " << enum_name(bs) << " button color";
            if (!incomplete)
                return false;
            continue;
        }
        cv::Mat colorsMatrix(len, 1, CV_8UC3);
        for (int j=0; j < len; j++)
            colorsMatrix.at<cv::Vec3b>(j) = luvState[j];
        cv::Scalar meanS;
        cv::Scalar stddevS;
        cv::meanStdDev(colorsMatrix, meanS, stddevS);
        cv::Vec3b mean(meanS[0], meanS[1], meanS[2]);
        cv::Vec3d stddev(stddevS[0], stddevS[1], stddevS[2]);
        LOG(INFO) << "Luv button color for " << enum_name(bs) << " mean " << mean << " stddev " << stddev << " over " << len << " samples";
        mButtonLuvAverage[int(bs)] = mean;
        if (stddevS[0] > 3 || stddevS[1] > 3 || stddevS[2] > 3) {
            buttonSuccess = false;
            LOG(ERROR) << "Luv color for " << enum_name(bs) << ", has too high deviation " << stddev;
        }
    }
    bool lstRowSuccess = true;
    for(auto bs : enum_values<WState>()) {
        auto& luvState = mLstRowLuv[int(bs)];
        int len = (int)luvState.size();
        if (!len) {
            LOG(INFO) << "No samples for " << enum_name(bs) << " list row color";
            continue;
        }
        cv::Mat colorsMatrix(len, 1, CV_8UC3);
        for (int j=0; j < len; j++)
            colorsMatrix.at<cv::Vec3b>(j) = luvState[j];
        cv::Scalar meanS;
        cv::Scalar stddevS;
        cv::meanStdDev(colorsMatrix, meanS, stddevS);
        cv::Vec3b mean(meanS[0], meanS[1], meanS[2]);
        cv::Vec3d stddev(stddevS[0], stddevS[1], stddevS[2]);
        LOG(INFO) << "Luv list row color for " << enum_name(bs) << " mean " << mean << " stddev " << stddev << " over " << len << " samples";
        mLstRowLuvAverage[int(bs)] = mean;
        if (stddevS[0] > 3 || stddevS[1] > 3 || stddevS[2] > 3) {
            lstRowSuccess = false;
            LOG(ERROR) << "Luv color for " << enum_name(bs) << ", has too high deviation " << stddev;
        }
    }
    std::array<cv::Vec3b,4> lstRowLuvAverage;
    for (int i=0; i < 4; i++) {
        if (mLstRowLuvAverage[i] == cv::Vec3b::zeros()) {
            if (i == int(WState::Normal))
                lstRowLuvAverage[i] = mButtonLuvAverage[i] * 1.3;
            if (i == int(WState::Focused))
                lstRowLuvAverage[i] = mButtonLuvAverage[i];
            if (i == int(WState::Active))
                lstRowLuvAverage[i] = mButtonLuvAverage[i] * 1.3;
        }
        else
            lstRowLuvAverage[i] = mLstRowLuvAverage[i];
    }
    master.setCalibrationResult(mButtonLuvAverage, lstRowLuvAverage);
    return buttonSuccess;
}

bool TaskCalibrate::getRowsByState(const ClassifyEnv::ResultListRow** rows) {
    for (int i=0; i < 4; i++)
        rows[i] = nullptr;
    auto it_rows = master.lastEDState().cEnv.classifiedListRows.find("lst-goods");
    if (it_rows == master.lastEDState().cEnv.classifiedListRows.end()) {
        notifyError(_("Rows in commodity list are not detected, calibration fails"));
        return false;
    }
    auto &classified_rows = it_rows->second;
    if (classified_rows.empty()) {
        notifyError(_("Rows in commodity list are not detected, calibration fails"));
        return false;
    }
    const ClassifyEnv::ResultListRow *row_to_test = nullptr;
    for (auto &row: classified_rows) {
        if (rows[int(row.bs)] == nullptr)
            rows[int(row.bs)] = &row;
    }
    return true;
}

bool TaskCalibrate::run() {
    if (done) {
        LOG(WARNING) << "done at start";
        return false;
    }
    try {
        master.detectEDState(DetectLevel::Buttons);
        if (!master.isEDStateMatch("scr-market:*")) {
            notifyError(_("Not at market, calibration fails"));
            return false;
        }

        notifyProgress(_("Calibration started"));

        //
        // Detect normal, focused, activated colors using buttons
        //

        if (master.isEDStateMatch("scr-market:mod-sell")) {
            hardcodedStep("{click:'btn-to-sell', after: 500}", DetectLevel::Buttons);
        }
        else if (master.isEDStateMatch("scr-market:mod-buy")) {
            hardcodedStep("{click:'btn-to-buy', after: 500}", DetectLevel::Buttons);
        }

        hardcodedStep("{goto:'btn-exit', after: 500}", DetectLevel::Buttons);
        LOG(INFO) << "State " << master.lastEDState() << " expected focused 'btn-exit'";

        recordButtonLuv("btn-help", WState::Normal);
        recordButtonLuv("btn-exit", WState::Focused);
        recordButtonLuv("btn-filter", WState::Normal);
        if (master.isEDStateMatch("scr-market:mod-sell")) {
            recordButtonLuv("btn-to-sell", WState::Active);
            recordButtonLuv("btn-to-buy", WState::Normal);
        }
        else if (master.isEDStateMatch("scr-market:mod-buy")) {
            recordButtonLuv("btn-to-sell", WState::Normal);
            recordButtonLuv("btn-to-buy", WState::Active);
        }

        hardcodedStep("{goto:'btn-help', after: 500}", DetectLevel::Buttons);
        LOG(INFO) << "State " << master.lastEDState() << " expected focused 'btn-help'";

        recordButtonLuv("btn-help", WState::Focused);
        recordButtonLuv("btn-exit", WState::Normal);
        recordButtonLuv("btn-filter", WState::Normal);
        if (master.isEDStateMatch("scr-market:mod-sell")) {
            recordButtonLuv("btn-to-sell", WState::Active);
            recordButtonLuv("btn-to-buy", WState::Normal);
        }
        else if (master.isEDStateMatch("scr-market:mod-buy")) {
            recordButtonLuv("btn-to-sell", WState::Normal);
            recordButtonLuv("btn-to-buy", WState::Active);
        }

        hardcodedStep("{goto:'btn-filter', after: 500}", DetectLevel::Buttons);
        LOG(INFO) << "State " << master.lastEDState() << " expected focused 'btn-filter'";

        recordButtonLuv("btn-help", WState::Normal);
        recordButtonLuv("btn-exit", WState::Normal);
        recordButtonLuv("btn-filter", WState::Focused);
        if (master.isEDStateMatch("scr-market:mod-sell")) {
            recordButtonLuv("btn-to-sell", WState::Active);
            recordButtonLuv("btn-to-buy", WState::Normal);
        }
        else if (master.isEDStateMatch("scr-market:mod-buy")) {
            recordButtonLuv("btn-to-sell", WState::Normal);
            recordButtonLuv("btn-to-buy", WState::Active);
        }

        hardcodedStep("{goto:'btn-to-buy', after: 500}", DetectLevel::Buttons);
        LOG(INFO) << "State " << master.lastEDState() << " expected focused 'btn-to-buy'";

        recordButtonLuv("btn-help", WState::Normal);
        recordButtonLuv("btn-exit", WState::Normal);
        recordButtonLuv("btn-filter", WState::Normal);
        recordButtonLuv("btn-to-buy", WState::Focused);

        hardcodedStep("{goto:'btn-to-sell', after: 500}", DetectLevel::Buttons);
        LOG(INFO) << "State " << master.lastEDState() << " expected focused 'btn-to-sell'";

        recordButtonLuv("btn-help", WState::Normal);
        recordButtonLuv("btn-exit", WState::Normal);
        recordButtonLuv("btn-filter", WState::Normal);
        recordButtonLuv("btn-to-sell", WState::Focused);

        calculateAverage(true);

        //
        // Goto sell market
        //

        hardcodedStep("{click:'btn-to-sell', after: 1000}", DetectLevel::ListRows);
        const UIState& uiState = master.lastEDState();
        LOG(INFO) << "State " << uiState << " expected state 'scr-market:mod-sell'";
        if (!master.isEDStateMatch("scr-market:mod-sell")) {
            notifyError(_("Not at market sell, calibration fails"));
            return false;
        }

        //
        // Detect normal list rows in sell market
        //
        {
            auto it = master.lastEDState().cEnv.classifiedListRows.find("lst-goods");
            std::vector<ClassifyEnv::ResultListRow> rows;
            if (it != master.lastEDState().cEnv.classifiedListRows.end())
                rows = it->second;
            for (auto &cr: rows) {
                cv::Point mouse = master.lastEDState().cEnv.cvtCapturedToReference(
                        (cr.detectedRect.tl() + cr.detectedRect.br()) / 2);
                sendMouseMove(mouse, 300);
                master.detectEDState(DetectLevel::ListRows);
                recordLstRowLuv("lst-goods", mouse, WState::Normal);
                if (mLstRowLuv[int(WState::Normal)].size() > 35)
                    break;
            }
        }

        calculateAverage(true);
        master.detectEDState(DetectLevel::ListRows);

        const ClassifyEnv::ResultListRow* list_rows[4];
        if (!getRowsByState(list_rows))
            return false;
        const ClassifyEnv::ResultListRow* row_to_test = list_rows[int(WState::Normal)];
        if (!row_to_test) {
            notifyError(_("Cannot find commodity to test sell dialog, calibration fails"));
            return false;
        }

        //
        // found commodity to test, check we detected list row correctly
        //

        {
            cv::Rect row_rect = uiState.cEnv.cvtCapturedToReference(row_to_test->detectedRect);
            cv::Point row_point = (row_rect.tl() + row_rect.br()) / 2;
            std::ostringstream goto_str;
            goto_str << "{goto:" << row_point << ", after:500}";
            hardcodedStep(goto_str.str().c_str(), DetectLevel::ListRows);
            LOG(INFO) << "State " << master.lastEDState();
        }
        master.detectEDState(DetectLevel::ListRows);
        if (!getRowsByState(list_rows))
            return false;
        row_to_test = list_rows[int(WState::Focused)];
        if (!row_to_test) {
            notifyError(_("Cannot find commodity to test sell dialog, calibration fails"));
            return false;
        }

        hardcodedStep("[{key:'space', after:2000},"
                      "{check:'scr-market:mod-sell:dlg-trade:*'},"
                      "{goto:'btn-more', after:500}]", DetectLevel::Buttons);
        LOG(INFO) << "State " << master.lastEDState();

        recordButtonLuv("btn-exit", WState::Normal);
        recordButtonLuv("btn-more", WState::Focused);
        recordButtonLuv("btn-commit", WState::Disabled);

        hardcodedStep("[{key:'left'},"
                      "{key:'space', after:1000},"
                      "{check:'scr-market:mod-sell'},"
                      "{click:'btn-to-buy', after:1000},"
                      "{check:'scr-market:mod-buy'},"
                      "{goto:'btn-help', after:500}]", DetectLevel::ListRows);
        LOG(INFO) << "State " << master.lastEDState() << " expected focused 'btn-help'";

        recordButtonLuv("btn-exit", WState::Normal);
        recordButtonLuv("btn-help", WState::Focused);
        recordButtonLuv("btn-to-buy", WState::Active);

        //
        // Detect activated list rows in sell market
        //
        {
            auto it = master.lastEDState().cEnv.classifiedListRows.find("lst-goods");
            std::vector<ClassifyEnv::ResultListRow> rows;
            if (it != master.lastEDState().cEnv.classifiedListRows.end())
                rows = it->second;
            for (auto &cr: rows) {
                cv::Point mouse = master.lastEDState().cEnv.cvtCapturedToReference(
                        (cr.detectedRect.tl() + cr.detectedRect.br()) / 2);
                sendMouseMove(mouse, 300);
                master.detectEDState(DetectLevel::ListRows);
                recordLstRowLuv("lst-goods", mouse, WState::Active);
                if (mLstRowLuv[int(WState::Active)].size() > 30)
                    break;
            }
        }

        calculateAverage(true);
        master.detectEDState(DetectLevel::ListRows);

        if (!getRowsByState(list_rows))
            return false;
        row_to_test = list_rows[int(WState::Active)];
        if (!row_to_test) {
            notifyError(_("Cannot find commodity to test buy dialog, calibration fails"));
            return false;
        }

        {
            cv::Rect row_rect = uiState.cEnv.cvtCapturedToReference(row_to_test->detectedRect);
            cv::Point row_point = (row_rect.tl() + row_rect.br()) / 2;
            std::ostringstream goto_str;
            goto_str << "{goto:" << row_point << ", after:500}";
            hardcodedStep(goto_str.str().c_str(), DetectLevel::ListRows);
            LOG(INFO) << "State " << master.lastEDState();
        }

        hardcodedStep("[{key:'space', after:2000},"
                      "{check:'scr-market:mod-buy:dlg-trade:*'},"
                      "{goto:'btn-more', after:500}]", DetectLevel::Buttons);
        LOG(INFO) << "State " << master.lastEDState();

        recordButtonLuv("btn-exit", WState::Normal);
        recordButtonLuv("btn-more", WState::Focused);
        recordButtonLuv("btn-commit", WState::Disabled);

        hardcodedStep("[{key:'left'},"
                      "{key:'space', after:500},"
                      "{check:'scr-market:mod-buy'},"
                      "{click:'btn-to-sell', after:500},"
                      "{check:'scr-market:mod-sell'},"
                      "{goto:'btn-exit', after:500}]", DetectLevel::Buttons);

        LOG(INFO) << "State " << master.lastEDState();

        if (calculateAverage(false)) {
            notifyProgress(_("Calibration completed successfully!"));
            master.getConfiguration()->saveCalibration();
        } else {
            notifyError(_("Failed to calibrate button state detector"));
        }
    } catch (completed_error& e) {
        notifyError(_("Failed to calibrate button state detector"));
        return false;
    }
    done = true;
    return true;
}

bool TaskSell::run() {
    if (done) {
        LOG(WARNING) << "done at start";
        return false;
    }
    taskActions = master.getTaskActions("TaskSell");
    try {
        for (int sellAllItems=0; sellAllItems < 20; sellAllItems++) {
            int sellItems = 0;
            Commodity* currCommodity = mCommodity;
            if (!currCommodity) {
                // sell all we can
                auto cargo = master.getConfiguration()->getCurrentCargo();
                if (cargo) {
                    for (auto commodity: cargo->inventory) {
                        sellItems = master.canSell(commodity);
                        if (sellItems > 0) {
                            currCommodity = commodity;
                            break;
                        }
                    }
                }
            } else {
                sellItems = master.canSell(currCommodity);
            }
            sellItems = std::min(mTotal, sellItems);
            if (!sellItems) {
                notifyProgress(_("Sold everything we can"));
                done = true;
                return true;
            }

            notifyProgress(std_format(_("Start selling {} by {} item(s)"), sellItems, mItems));
            auto actionArgs = json5pp::object({{"$items", mItems}});
            while (sellItems > 0) {
                master.detectEDState(DetectLevel::ListOcrFocusedRow);
                if (master.isEDStateMatch("scr-market:mod-buy")) {
                    // go to sell mode
                    hardcodedStep("{click:'btn-to-sell', after: 1000}", DetectLevel::None);
                    continue;
                }
                if (master.isEDStateMatch("scr-market:mod-sell")) {
                    auto it = master.lastEDState().cEnv.classifiedListRows.find("lst-goods");
                    if (it == master.lastEDState().cEnv.classifiedListRows.end()) {
                        notifyError(_("Cannot detect state of 'lst-goods', aborting"));
                        done = true;
                        return false;
                    }
                    const ClassifyEnv::ResultListRow* focusedRow = nullptr;
                    for (auto &r: it->second) {
                        if (r.bs == WState::Focused) {
                            focusedRow = &r;
                            LOG(INFO) << "Focused row text: " << focusedRow->text;
                        }
                        if (r.text == currCommodity->name) {
                            LOG(INFO) << "Row with text '" << r.text << "' found";
                            if (r.bs == WState::Focused) {
                                LOG(INFO) << "Pressing 'space'";
                                sendKey("space", 0, 500);
                                continue;
                            } else {
                                LOG(INFO) << "Not focused, using mouse click";
                                cv::Rect rect = master.lastEDState().cEnv.cvtCapturedToReference(r.detectedRect);
                                sendMouseClick((rect.tl() + rect.br()) / 2, 0, 500);
                            }
                            continue;
                        }
                    }
                    if (!focusedRow) {
                        LOG(INFO) << "No focused row found, moving mouse to the list area";
                        cv::Rect rect = master.resolveWidgetReferenceRect("lst-goods");
                        int x = rect.x+rect.width/2;
                        int y = rect.y - 20;
                        sendMouseClick({x,y}, 0, 500);
                        for (int i=0; i < 10; i++)
                            sendMouseMove({0, 10}, 25, false);
                        continue;
                    }
                    Commodity* focusedCommodity = master.getConfiguration()->getCommodityByName(focusedRow->text, true);
                    if (!focusedCommodity) {
                        notifyError(_("Cannot detect commodities in 'lst-goods', aborting"));
                        done = true;
                        return false;
                    }

                    int focusedIdx = -1;
                    int needIdx = -1;
                    std::vector<Commodity *> sellTable = master.getConfiguration()->getMarketInSellOrder();
                    for (int idx = 0; idx < sellTable.size(); idx++) {
                        auto &c = sellTable[idx];
                        if (c == focusedCommodity)
                            focusedIdx = idx;
                        if (c == currCommodity)
                            needIdx = idx;
                    }
                    if (needIdx >= 0 && focusedIdx >= 0) {
                        LOG(INFO) << "Distance is "<<(needIdx - focusedIdx)<<" lines from focused '" << sellTable[focusedIdx]->name << " to " << currCommodity->name;
                        if (needIdx < focusedIdx) {
                            for (int cnt=0; cnt < focusedIdx-needIdx; cnt++)
                                sendKey("up");
                        } else {
                            for (int cnt=0; cnt < needIdx-focusedIdx; cnt++)
                                sendKey("down");
                        }
                        continue;
                    }
                    notifyError(_("Cannot detect commodities in 'lst-goods', aborting"));
                    done = true;
                    return false;
                } else if (master.isEDStateMatch("scr-market:mod-sell:dlg-trade:*")) {
                    LOG(INFO) << "At market sell dialog, execute action 'sell-some'";
                    bool ok = executeAction("sell-some", actionArgs);
                    if (!ok) {
                        LOG(WARNING) << "Step 'sell-some' not successful, recovering";
                        for (int i = 0; i < 3; i++) {
                            master.detectEDState(DetectLevel::Buttons);
                            if (master.isEDStateMatch("scr-market:mod-sell:dlg-trade:*")) {
                                LOG(WARNING) << "Step 'sell-some' not successful, retrying";
                                executeAction("restart");
                            } else if (master.isEDStateMatch("scr-market:mod-sell")) {
                                LOG(INFO) << "Recovered to state 'scr-market:mod-sell'";
                                ok = true;
                                break;
                            }
                        }
                        if (!ok) {
                            notifyError(_("Step 'sell-some' not successful, cannot recover"));
                            break;
                        }
                    }
                    if (ok) {
                        mTotal -= mItems;
                        sellItems -= mItems;
                    }
                    continue;
                } else {
                    notifyError(
                            std_format(_("Unknown state '{}', aborting trade task"), master.lastEDState().to_string()));
                    done = true;
                    return false;
                }
            }
        }
    } catch (completed_error& e) {
        return false;
    }
    done = true;
    return true;
}
