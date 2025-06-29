//
// Created by mkizub on 23.05.2025.
//

#include "../pch.h"

#include "Task.h"
#include "AIManager.h"
#include "../Keyboard.h"
#include "../ui/UIManager.h"
#include "../FuzzyMatch.h"
#include <synchapi.h>

namespace ai {

Task::Task(Task* parent, AIManager& mgr, const TaskTemplate& templ)
    : parent(parent)
    , mgr(mgr)
    , templ(templ)
    , taskName(templ.name)
    , maxMisses(templ.maxMisses)
{
}

Result Task::run_sub_task(upTask& pTask) {
    Task* task  = pTask.get();
    if (!task)
        return Result::Failure;

    Result t_res;
    try {
        t_res = task->run();
    } catch (const nonlocal_return& ex) {
        t_res = ex.result;
    }
    task->result = t_res;
    return t_res;
}

void Task::check_interrupted() const {
    if (mgr.isInterrupted)
        throw interrupted_error();
}

void Task::sleep(int milliseconds) const {
    check_interrupted();
    if (milliseconds <= 0)
        return;
    if (milliseconds >= 75) {
        auto now = std::chrono::system_clock::now();
        auto until = now + std::chrono::milliseconds(milliseconds);
        while (now < until) {
            auto left = std::chrono::duration_cast<std::chrono::milliseconds>(until - now);
            if (left.count() < 5)
                break;
            auto duration = std::min(std::chrono::milliseconds(500), left);
            std::this_thread::sleep_for(duration);
            now = std::chrono::system_clock::now();
        }
        check_interrupted();
        return;
    }

    LARGE_INTEGER frequency, start, end;
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&start);

    double seconds = milliseconds * 0.001;
    while (true) {
        QueryPerformanceCounter(&end);
        double elapsed_seconds = double(end.QuadPart - start.QuadPart) / double(frequency.QuadPart);
        if (elapsed_seconds >= seconds)
            break;
        check_interrupted();
    }
}

bool Task::sendKey(const std::string& name, int delay_ms, int pause_ms) const {
    try {
        const KeyBindings& keyBindings = mgr.cfg.getGameKeyBindings(name);
        const GameKey* gk = nullptr;
        if (keyBindings.primary.device != GameKey::Void)
            gk = &keyBindings.primary;
        else if (keyBindings.secondary.device != GameKey::Void)
            gk = &keyBindings.secondary;
        if (gk) {
            if (!keyboard::sendKeyDown(*gk))
                return false;
        } else {
            if (!keyboard::sendKeyDown(name))
                return false;
        }
        sleep(delay_ms > 0 ? delay_ms : mgr.cfg.getDefaultKeyHoldTime());
        bool ok = false;
        if (gk) {
            ok = keyboard::sendKeyUp(*gk);
        } else {
            ok = keyboard::sendKeyUp(name);
        }
        sleep(pause_ms > 0 ? pause_ms : mgr.cfg.getDefaultKeyAfterTime());
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
        cv::Point screen = mgr.rEnv.cvtReferenceToDesktop(point);
        screen = mgr.rEnv.cvtReferenceToDesktop(point);
        x = screen.x;
        y = screen.y;
    }
    //LOG(INFO) << "sendMouseMove recalculated from reference " << point << " to screen " << screen;
    if (!keyboard::sendMouseMoveTo(x, y, absolute, virtualDesktop))
        return false;
    sleep(pause_ms > 0 ? pause_ms : mgr.cfg.getDefaultKeyAfterTime());
    return true;
}

bool Task::sendMouseClick(const cv::Point& point, int delay_ms, int pause_ms) const {
    cv::Point screen = mgr.rEnv.cvtReferenceToDesktop(point);
    bool virtualDesktop = (GetSystemMetrics(SM_CMONITORS) > 1);
    //LOG(INFO) << "sendMouseClick recalculated from reference " << point << " to screen " << screen;
    if (!keyboard::sendMouseMoveTo(screen.x, screen.y, true, virtualDesktop))
        return false;
    try {
        sleep(delay_ms > 0 ? delay_ms : mgr.cfg.getDefaultKeyHoldTime());
        if (!keyboard::sendMouseDown(keyboard::MOUSE_L_BUTTON))
            return false;
        sleep(delay_ms > 0 ? delay_ms : mgr.cfg.getDefaultKeyHoldTime());
        bool ok = keyboard::sendMouseUp(keyboard::MOUSE_L_BUTTON);
        sleep(pause_ms > 0 ? pause_ms : mgr.cfg.getDefaultKeyAfterTime());
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
        cv::Rect rect = mgr.master.resolveWidgetReferenceRect(pos.as_string());
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
    this->fromState = action.at("from").as_string();
    this->destState = action.at("dest").as_string();
    const json5pp::value& execute = action.at("exec");

    return executeStep(execute, args);
}

bool Task::executeWait(const json5pp::value& step, const json5pp::value& args) {
    LOG(DEBUG) << "action step wait: " << step;
    const json5pp::value& state = step.at("wait");
    const json5pp::value& focus = step.at("focus");
    const json5pp::value& disabled = step.at("disabled");
    auto start = std::chrono::system_clock::now();
    auto now = start;
    int during = 3000;
    int period = 250;
    if (step.at("during").is_integer())
        during = std::max(100, step.at("during").as_integer());
    if (step.at("period").is_integer())
        period = std::max(100, step.at("period").as_integer());
    auto until = now + std::chrono::duration<int, std::milli>(during);
    LOG(INFO) << "Step 'wait' #0 duration " << during << " left " << std::chrono::duration_cast<std::chrono::milliseconds>(until - now).count();
    bool ok;
    for (int counter=1; now < until; counter++) {
        ok = mgr.detectEDState(DetectLevel::Buttons);
        if (ok && mgr.uiState.match(state.as_string())) {
            bool ok_focus = true;
            if (focus.is_string()) {
                ok_focus = false;
                for (auto& cr : mgr.rEnv.classified) {
                    if (cr.cdt == ClsDetType::Widget && cr.u.widg.ws == WState::Focused && cr.u.widg.widget->name == focus.as_string()) {
                        ok_focus = true;
                        break;
                    }
                }
            }
            bool ok_disabled = true;
            if (disabled.is_string()) {
                ok_disabled = false;
                for (auto& cr : mgr.rEnv.classified) {
                    if (cr.cdt == ClsDetType::Widget && cr.u.widg.ws == WState::Disabled && cr.u.widg.widget->name == disabled.as_string()) {
                        ok_disabled = true;
                        break;
                    }
                }
            }
            ok = ok_focus && ok_disabled;
            if (ok)
                break;
        }
        std::this_thread::sleep_for(std::chrono::duration<int, std::milli>(period));
        now = std::chrono::system_clock::now();
        LOG_IF(!ok,INFO) << "Step 'wait' #"<<counter<<" duration " << during << " left " << std::chrono::duration_cast<std::chrono::milliseconds>(until - now).count();
    }
    LOG_IF(!ok,ERROR) << "Step " << step << " failed - wait time expired, current state is " << mgr.uiState;
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
            mgr.detectEDState(DetectLevel::Buttons);
            bool ok = mgr.uiState.match(state.as_string());
            if (ok) {
                const json5pp::value &focus = step.at("focus");
                if (focus) {
                    const widget::Widget* fw = mgr.uiState.focused;
                    std::string fn = fw ? fw->name : "";
                    ok = focus.is_string() && fn == focus.as_string();
                    LOG_IF(!ok,ERROR) << "Step failed, current focus at '" << fn << "', but '" << focus << "' required";
                }
            }
            LOG_IF(!ok,ERROR) << "Step " << step << " failed, current state is " << mgr.uiState;
            return ok;
        }
        if (step.as_object().contains("key")) {
            LOG(DEBUG) << "action step key: " << step;
            const json5pp::value& key = step.at("key");
            bool ok;
            if (step.at("hold").is_object() || step.at("hold").is_array()) {
                const KeyBindings& keyBindings = mgr.cfg.getGameKeyBindings(key.as_string());
                const GameKey* gk = nullptr;
                if (keyBindings.primary.device != GameKey::Void)
                    gk = &keyBindings.primary;
                else if (keyBindings.secondary.device != GameKey::Void)
                    gk = &keyBindings.secondary;
                if (gk)
                    keyboard::sendKeyDown(*gk);
                else
                    keyboard::sendKeyDown(key.as_string());
                ok = executeStep(step.at("hold"), args);
                if (gk)
                    keyboard::sendKeyUp(*gk);
                else
                    keyboard::sendKeyUp(key.as_string());
                int after = get_int(step.at("after"), args, mgr.cfg.getDefaultKeyAfterTime());
                sleep(after);
            } else {
                int hold = get_int(step.at("hold"), args, mgr.cfg.getDefaultKeyHoldTime());
                int after = get_int(step.at("after"), args, mgr.cfg.getDefaultKeyAfterTime());
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
            int after = get_int(step.at("after"), args, mgr.cfg.getDefaultKeyAfterTime());
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
            int hold = get_int(step.at("hold"), args, mgr.cfg.getDefaultKeyHoldTime());
            int after = get_int(step.at("after"), args, mgr.cfg.getDefaultKeyAfterTime());
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

void Task::hardcodedStep(const std::string& step, DetectLevel level) {
    json5pp::value parsed, args;
    try {
        std::stringstream in(step);
        in >> json5pp::rule::json5() >> parsed;
    } catch (...) {
        LOG(ERROR) << "Failed to parse json " << step;
        task_return(Result::Failure, "hardcoded step parse failed");
    }
    if (!executeStep(parsed, args)) {
        LOG(ERROR) << "Failed to execute " << step;
        task_return(Result::Trouble, "hardcoded step failed");
    }
    mgr.detectEDState(level);
}

void Task::notifyProgress(const char* msg) const {
    LOG(INFO) << msg;
    UIManager::showToast(taskName, msg);
}
void Task::notifyProgress(const std::string& msg) const {
    LOG(INFO) << msg;
    UIManager::showToast(taskName, msg);
}
void Task::notifyError(const char* msg, Result res) const {
    LOG(ERROR) << msg;
    UIManager::showToast(taskName, msg);
    throw nonlocal_return(res, this, msg);
}
void Task::notifyError(const std::string& msg, Result res) const {
    LOG(ERROR) << msg;
    UIManager::showToast(taskName, msg);
    throw nonlocal_return(res, this, msg);
}

void Task::task_return(Result res) const {
    throw nonlocal_return(res, this);
}

void Task::task_return(Result res, const char* msg) const {
    throw nonlocal_return(res, this, msg);
}

TaskCalibrate::TaskCalibrate(Task* parent, AIManager& mgr, const TaskTemplate& templ)
    : Task(parent, mgr, templ)
    , mDetector(HistogramTemplate::CompareMode::Hsv, cv::Rect(), cv::Vec3b())
{
    assert(templ.name == ED_TASK_CALIBRATE);
    taskName = "Calibration";
}

void TaskCalibrate::recordButton(const char* button, WState bs) {
    cv::Rect rect = mgr.master.resolveWidgetReferenceRect(button);
    if (rect.empty()) {
        LOG(ERROR) << "Cannot get rect of button '" << button << "'";
        return;
    }
    mDetector.mRect = rect;
    mDetector.match(const_cast<ClassifyEnv&>(mgr.master.cEnv()));
    cv::Vec3b bgr = mDetector.mLastColorBGR;
    mButtonBGR[int(bs)].push_back(bgr);
    const char* names[] = {"Normal   ", "Focused  ", "Active   ", "Disabled "};
    LOG(INFO) << names[int(bs)] << " button: bgr=" << mButtonBGR[int(bs)].back()
              << " rgb=0x"<< std::format("{:06x}", decodeBGR(bgr));
}

void TaskCalibrate::recordLstRow(const char* list, cv::Point mouse, WState bs) {
    cv::Rect rect = mgr.master.resolveWidgetReferenceRect(list);
    if (rect.empty()) {
        LOG(ERROR) << "Cannot get rect of list '" << list << "'";
        return;
    }
    std::vector<cv::Vec3b> colors;
    std::vector<double> lums;
    for (auto& cr : mgr.rEnv.classified) {
        if (cr.cdt != ClsDetType::ListRow || cr.u.lrow.list->name != list)
            continue;
        cv::Rect refRect = cr.detectedRect;
        mDetector.mRect = refRect;
        mDetector.match(const_cast<ClassifyEnv&>(mgr.master.cEnv()));
        cv::Vec3b bgr = mDetector.mLastColorBGR;
        if (refRect.contains(mouse)) {
            mLstRowBGR[int(WState::Focused)].push_back(bgr);
        } else {
            colors.push_back(bgr);
            lums.push_back(sBgr2Hsv(colors.back())[2]);
        }
    }
    cv::Point minLoc;
    cv::Point maxLoc;
    cv::minMaxLoc(lums, nullptr, nullptr, &minLoc, &maxLoc);
    cv::Vec3d darkColor(colors[minLoc.x]);
    cv::Vec3d lightColor(colors[maxLoc.x]);
    double lumDelta = lums[maxLoc.x] - lums[minLoc.x];

    if (lumDelta < 6) {
        mLstRowBGR[int(bs)].insert(mLstRowBGR[int(bs)].end(), colors.begin(), colors.end());
    } else {
        for (auto& c : colors) {
            double distNorm = distanceBGR(darkColor, c);
            double distActv = distanceBGR(lightColor, c);
            if (distNorm < distActv)
                mLstRowBGR[int(WState::Normal)].push_back(c);
            else
                mLstRowBGR[int(WState::Active)].push_back(c);
        }
    }
}

bool TaskCalibrate::calculateAverage(bool incomplete) {
    bool buttonSuccess = true;
    for(auto ws : enum_values<WState>()) {
        if (ws == WState::Unknown)
            continue;
        auto& bgrState = mButtonBGR[int(ws)];
        int len = (int)bgrState.size();
        if (!len) {
            LOG(INFO) << "No samples for " << enum_name(ws) << " button color";
            if (!incomplete)
                return false;
            continue;
        }
        cv::Mat colorsMatrix(len, 1, CV_8UC3);
        for (int j=0; j < len; j++)
            colorsMatrix.at<cv::Vec3b>(j) = bgrState[j];
        cv::Scalar meanS;
        cv::Scalar stddevS;
        cv::meanStdDev(colorsMatrix, meanS, stddevS);
        cv::Vec3b mean(meanS[0], meanS[1], meanS[2]);
        cv::Vec3d stddev(stddevS[0], stddevS[1], stddevS[2]);
        LOG(INFO) << "BGR button color for " << enum_name(ws) << " mean " << mean << " stddev " << stddev << " over " << len << " samples";
        mButtonBGRAverage[int(ws)] = mean;
        if (stddevS[0] > 3 || stddevS[1] > 3 || stddevS[2] > 3) {
            buttonSuccess = false;
            LOG(ERROR) << "Luv color for " << enum_name(ws) << ", has too high deviation " << stddev;
        }
    }
    bool lstRowSuccess = true;
    for(auto ws : enum_values<WState>()) {
        if (ws == WState::Unknown)
            continue;
        auto& bgrState = mLstRowBGR[int(ws)];
        int len = (int)bgrState.size();
        if (!len) {
            LOG(INFO) << "No samples for " << enum_name(ws) << " list row color";
            continue;
        }
        cv::Mat colorsMatrix(len, 1, CV_8UC3);
        for (int j=0; j < len; j++)
            colorsMatrix.at<cv::Vec3b>(j) = bgrState[j];
        cv::Scalar meanS;
        cv::Scalar stddevS;
        cv::meanStdDev(colorsMatrix, meanS, stddevS);
        cv::Vec3b mean(meanS[0], meanS[1], meanS[2]);
        cv::Vec3d stddev(stddevS[0], stddevS[1], stddevS[2]);
        LOG(INFO) << "Luv list row color for " << enum_name(ws) << " mean " << mean << " stddev " << stddev << " over " << len << " samples";
        mLstRowBGRAverage[int(ws)] = mean;
        if (stddevS[0] > 3 || stddevS[1] > 3 || stddevS[2] > 3) {
            lstRowSuccess = false;
            LOG(ERROR) << "Luv color for " << enum_name(ws) << ", has too high deviation " << stddev;
        }
    }
    mgr.master.setCalibrationResult(mButtonBGRAverage, mLstRowBGRAverage);
    return buttonSuccess;
}

void TaskCalibrate::getRowsByState(const ClassifiedRect** rows) {
    for (int i=0; i < 4; i++)
        rows[i] = nullptr;
    for (auto &row: mgr.rEnv.classified) {
        if (row.cdt != ClsDetType::ListRow || row.u.lrow.list->name != "lst-goods")
            continue;
        WState ws = row.u.lrow.ws;
        if (ws == WState::Unknown)
            continue;
        if (rows[int(ws)] == nullptr)
            rows[int(ws)] = &row;
    }
}

Result TaskCalibrate::run() {
    if (result == Result::Started) {
        for (int i=0; i < 4; i++) {
            mButtonBGR[i].clear();
            mLstRowBGR[i].clear();
            mButtonBGRAverage[i] = cv::Vec3b::zeros();
            mLstRowBGRAverage[i] = cv::Vec3b::zeros();
        }
        result = Result::Created;
    }
    if (result != Result::Created) {
        return Result::Failure;
    }
    result = Result::Started;

    mgr.detectEDState(DetectLevel::Buttons);
    if (!mgr.uiState.match("scr-market:*"))
        notifyError(_("Not at market, calibration fails"), Result::Failure);

    notifyProgress(_("Calibration started"));

    //
    // Detect normal, focused, activated colors using buttons
    //

    if (mgr.uiState.match("scr-market:mod-sell")) {
        hardcodedStep("{click:'btn-to-sell', after: 500}", DetectLevel::Buttons);
    }
    else if (mgr.uiState.match("scr-market:mod-buy")) {
        hardcodedStep("{click:'btn-to-buy', after: 500}", DetectLevel::Buttons);
    }

    hardcodedStep("{goto:'btn-exit', after: 500}", DetectLevel::Buttons);
    LOG(INFO) << "State " << mgr.uiState << " expected focused 'btn-exit'";

    recordButton("btn-help", WState::Normal);
    recordButton("btn-exit", WState::Focused);
    recordButton("btn-filter", WState::Normal);
    if (mgr.uiState.match("scr-market:mod-sell")) {
        recordButton("btn-to-sell", WState::Active);
        recordButton("btn-to-buy", WState::Normal);
    }
    else if (mgr.uiState.match("scr-market:mod-buy")) {
        recordButton("btn-to-sell", WState::Normal);
        recordButton("btn-to-buy", WState::Active);
    }

    hardcodedStep("{goto:'btn-help', after: 500}", DetectLevel::Screen);
    LOG(INFO) << "State " << mgr.uiState << " expected focused 'btn-help'";

    recordButton("btn-help", WState::Focused);
    recordButton("btn-exit", WState::Normal);
    recordButton("btn-filter", WState::Normal);
    if (mgr.uiState.match("scr-market:mod-sell")) {
        recordButton("btn-to-sell", WState::Active);
        recordButton("btn-to-buy", WState::Normal);
    }
    else if (mgr.uiState.match("scr-market:mod-buy")) {
        recordButton("btn-to-sell", WState::Normal);
        recordButton("btn-to-buy", WState::Active);
    }

    hardcodedStep("{goto:'btn-filter', after: 500}", DetectLevel::Buttons);
    LOG(INFO) << "State " << mgr.uiState << " expected focused 'btn-filter'";

    recordButton("btn-help", WState::Normal);
    recordButton("btn-exit", WState::Normal);
    recordButton("btn-filter", WState::Focused);
    if (mgr.uiState.match("scr-market:mod-sell")) {
        recordButton("btn-to-sell", WState::Active);
        recordButton("btn-to-buy", WState::Normal);
    }
    else if (mgr.uiState.match("scr-market:mod-buy")) {
        recordButton("btn-to-sell", WState::Normal);
        recordButton("btn-to-buy", WState::Active);
    }

    hardcodedStep("{goto:'btn-to-buy', after: 500}", DetectLevel::Buttons);
    LOG(INFO) << "State " << mgr.uiState << " expected focused 'btn-to-buy'";

    recordButton("btn-help", WState::Normal);
    recordButton("btn-exit", WState::Normal);
    recordButton("btn-filter", WState::Normal);
    recordButton("btn-to-buy", WState::Focused);

    hardcodedStep("{goto:'btn-to-sell', after: 500}", DetectLevel::Buttons);
    LOG(INFO) << "State " << mgr.uiState << " expected focused 'btn-to-sell'";

    recordButton("btn-help", WState::Normal);
    recordButton("btn-exit", WState::Normal);
    recordButton("btn-filter", WState::Normal);
    recordButton("btn-to-sell", WState::Focused);

    calculateAverage(true);

    //
    // Goto sell market
    //

    hardcodedStep("{click:'btn-to-sell', after: 1000}", DetectLevel::ListRows);
    LOG(INFO) << "State " << mgr.uiState << " expected state 'scr-market:mod-sell'";
    if (!mgr.uiState.match("scr-market:mod-sell"))
        notifyError(_("Not at market sell, calibration fails"), Result::Failure);

    //
    // Detect normal list rows in sell market
    //
    {
        for (auto &cr: mgr.rEnv.classified) {
            if (cr.cdt != ClsDetType::ListRow || cr.u.lrow.list->name != "lst-goods")
                continue;
            cv::Point mouse = (cr.detectedRect.tl() + cr.detectedRect.br()) / 2;
            sendMouseMove(mouse, 300);
            mgr.detectEDState(DetectLevel::ListRows);
            recordLstRow("lst-goods", mouse, WState::Normal);
            if (mLstRowBGR[int(WState::Normal)].size() > 35)
                break;
        }
    }

    calculateAverage(true);
    mgr.detectEDState(DetectLevel::ListRows);

    const ClassifiedRect* list_rows[4];
    getRowsByState(list_rows);
    const ClassifiedRect* row_to_test = list_rows[int(WState::Normal)];
    if (!row_to_test)
        notifyError(_("Cannot find commodity to test sell dialog, calibration fails"), Result::Failure);

    //
    // found commodity to test, check we detected list row correctly
    //

    {
        auto& row_rect = row_to_test->detectedRect;
        cv::Point row_point = (row_rect.tl() + row_rect.br()) / 2;
        std::ostringstream goto_str;
        goto_str << "{goto:" << row_point << ", after:500}";
        hardcodedStep(goto_str.str().c_str(), DetectLevel::ListRows);
        LOG(INFO) << "State " << mgr.uiState;
    }
    mgr.detectEDState(DetectLevel::ListRows);
    getRowsByState(list_rows);
    row_to_test = list_rows[int(WState::Focused)];
    if (!row_to_test)
        notifyError(_("Cannot find commodity to test sell dialog, calibration fails"), Result::Failure);

    hardcodedStep("[{key:'UI_Select', after:2000},"
                  "{check:'scr-market:mod-sell:dlg-trade:*'},"
                  "{goto:'btn-more', after:500}]", DetectLevel::Buttons);
    LOG(INFO) << "State " << mgr.uiState;

    recordButton("btn-cancel", WState::Normal);
    recordButton("btn-more", WState::Focused);
    recordButton("btn-commit", WState::Disabled);

    hardcodedStep("[{key:'UI_Left'},"
                  "{key:'UI_Select', after:1000},"
                  "{check:'scr-market:mod-sell'},"
                  "{click:'btn-to-buy', after:1000},"
                  "{check:'scr-market:mod-buy'},"
                  "{goto:'btn-help', after:500}]", DetectLevel::ListRows);
    LOG(INFO) << "State " << mgr.uiState << " expected focused 'btn-help'";

    recordButton("btn-exit", WState::Normal);
    recordButton("btn-help", WState::Focused);
    recordButton("btn-to-buy", WState::Active);

    //
    // Detect activated list rows in sell market
    //
    {
        for (auto &cr: mgr.rEnv.classified) {
            if (cr.cdt != ClsDetType::ListRow || cr.u.lrow.list->name != "lst-goods")
                continue;
            cv::Point mouse = (cr.detectedRect.tl() + cr.detectedRect.br()) / 2;
            sendMouseMove(mouse, 300);
            mgr.detectEDState(DetectLevel::ListRows);
            recordLstRow("lst-goods", mouse, WState::Active);
            if (mLstRowBGR[int(WState::Active)].size() > 35)
                break;
        }
    }

    calculateAverage(true);
    mgr.detectEDState(DetectLevel::ListRows);

    getRowsByState(list_rows);
    row_to_test = list_rows[int(WState::Active)];
    if (!row_to_test)
        notifyError(_("Cannot find commodity to test buy dialog, calibration fails"), Result::Failure);

    {
        auto& row_rect = row_to_test->detectedRect;
        cv::Point row_point = (row_rect.tl() + row_rect.br()) / 2;
        std::ostringstream goto_str;
        goto_str << "{goto:" << row_point << ", after:500}";
        hardcodedStep(goto_str.str().c_str(), DetectLevel::ListRows);
        LOG(INFO) << "State " << mgr.uiState;
    }

    hardcodedStep("[{key:'UI_Select', after:2000},"
                  "{check:'scr-market:mod-buy:dlg-trade:*'},"
                  "{goto:'btn-more', after:500}]", DetectLevel::Buttons);
    LOG(INFO) << "State " << mgr.uiState;

    recordButton("btn-cancel", WState::Normal);
    recordButton("btn-more", WState::Focused);
    recordButton("btn-commit", WState::Disabled);

    hardcodedStep("[{key:'UI_Left'},"
                  "{key:'UI_Select', after:500},"
                  "{check:'scr-market:mod-buy'},"
                  "{click:'btn-to-sell', after:500},"
                  "{check:'scr-market:mod-sell'},"
                  "{goto:'btn-exit', after:500}]", DetectLevel::Buttons);

    LOG(INFO) << "State " << mgr.uiState;

    if (calculateAverage(false)) {
        notifyProgress(_("Calibration completed successfully!"));
        mgr.cfg.saveCalibration();
    } else {
        notifyError(_("Failed to calibrate button state detector"), Result::Failure);
    }
    return Result::Success;
}

TaskSellAll::TaskSellAll(Task* parent, AIManager& mgr, const TaskTemplate& templ_)
        : Task(parent, mgr, templ_)
        , mChunk(1000)
{
    assert (templ.name == ED_TASK_MARKET_SELL_ALL);
    for (auto& p : templ.params) {
        if (p.name == "chunk")
            mChunk = std::get<int64_t>(p.value);
    }
}

void TaskSellAll::plan() {
    spShipCargo shipCargo = mgr.cfg.getCurrentCargo();
    if (!shipCargo)
        task_return(Result::Trouble, "Ship cargo not loaded");
    for (Commodity* commodity: shipCargo->inventory) {
        auto it_arch = std::find_if(sell_archive.begin(), sell_archive.end(), [commodity](const upTask& t) {
            auto ts = dynamic_cast<TaskSell*>(t.get());
            return (ts && ts->mCommodity == commodity);
        });
        if (it_arch != sell_archive.end())
            continue;
        auto it_old = std::find_if(sub_tasks.begin(), sub_tasks.end(), [commodity](const upTask& t) {
            auto ts = dynamic_cast<TaskSell*>(t.get());
            return (ts && ts->mCommodity == commodity);
        });
        TaskSell* old = it_old == sub_tasks.end() ? nullptr : dynamic_cast<TaskSell*>(it_old->get());
        int toSell = mgr.master.canSell(commodity);
        if (toSell > 0) {
            if (old) {
                old->mTotal = toSell;
                old->mItems = mChunk;
            } else {
                TaskTemplate impl = mgr.getTaskTemplate(ED_TASK_MARKET_SELL);
                impl.set("commodity", commodity->nameId);
                impl.set("amount", toSell);
                impl.set("chunk", mChunk);
                sub_tasks.push_back(std::make_unique<TaskSell>(this, mgr, impl));
            }
        }
        else if (old) {
            sub_tasks.erase(it_old);
        }
    }
    if (result == Result::Created)
        result = Result::Started;
}

Result TaskSellAll::run() {
    switch (result) {
    case Result::Created:
    case Result::Started:
    case Result::Trouble:
        plan();
        break;
    case Result::Failure:
    case Result::Partly:
    case Result::Success:
        LOG(ERROR) << "Bad state on task run(): " << enum_name<Result>(result);
        return result;
    }

    while (!sub_tasks.empty()) {
        upTask& pTask = sub_tasks.front();
        Result res = run_sub_task(pTask);
        switch (res) {
        case Result::Created:
        case Result::Started:
            LOG(ERROR) << "Bad state after task run(): " << enum_name<Result>(res);
            plan();
            continue;
        case Result::Trouble:
            if (pTask->missCount < pTask->maxMisses) {
                plan();
                pTask->result = Result::Started;
                continue;
            }
            pTask->result = Result::Failure;
            // fall through
        case Result::Failure:
        case Result::Partly:
        case Result::Success:
            if ( dynamic_cast<TaskSell*>(pTask.get()) )
                sell_archive.emplace_back(std::move(pTask));
            sub_tasks.pop_front();
            break;
        }
    }
    notifyProgress(_("Sold everything we can"));

    int total = 0;
    int sold = 0;
    bool have_success = false;
    std::for_each(sell_archive.begin(), sell_archive.end(), [&](const upTask& t){
        auto st = dynamic_cast<TaskSell*>(t.get());
        if (st) {
            total += st->mTotal;
            sold += st->mSold;
        }
    });
    if (sold >= total)
        result = Result::Success;
    else if (sold == 0)
        result = Result::Failure;
    else
        result = Result::Partly;
    return result;
}

TaskSell::TaskSell(Task* parent, AIManager& mgr, const TaskTemplate& templ_)
    : Task(parent, mgr, templ_)
    , mCommodity(nullptr)
    , mTotal(1000)
    , mItems(1000)
{
    assert (templ.name == ED_TASK_MARKET_SELL);
    for (auto& p : templ.params) {
        if (p.name == "commodity")
            mCommodity = mgr.cfg.getCommodityById(std::get<std::string>(p.value));
        if (p.name == "amount")
            mTotal = std::get<int64_t>(p.value);
        if (p.name == "chunk")
            mItems = std::get<int64_t>(p.value);
    }
}

Result TaskSell::run() {
    switch (result) {
    case Result::Created:
    case Result::Started:
    case Result::Trouble:
        result = Result::Started;
        break;
    case Result::Failure:
    case Result::Partly:
    case Result::Success:
        return result;
    }
    taskActions = mgr.master.getTaskActions("TaskSell");
    FuzzyMatch matcher;

    if (!mCommodity)
        return Result::Failure;

    int sellItems = mgr.master.canSell(mCommodity);
    sellItems = std::min(mTotal, sellItems);
    if (sellItems <= 0) {
        notifyProgress(_("Sold everything we can"));
        return mSold >= mTotal ? Result::Success : Result::Partly;
    }

    notifyProgress(std_format(_("Start selling {} by {} item(s)"), sellItems, mItems));
    auto actionArgs = json5pp::object({{"$items", mItems}});
    while (sellItems > 0) {
        mgr.detectEDState(DetectLevel::ListOcrFocusedRow);
        if (mgr.uiState.match("scr-services")) {
            // go to sell mode
            hardcodedStep("{click:'til-market', after: 2000}", DetectLevel::None);
            continue;
        }
        if (mgr.uiState.match("scr-market:mod-buy")) {
            // go to sell mode
            hardcodedStep("{click:'btn-to-sell', after: 1000}", DetectLevel::None);
            continue;
        }
        if (mgr.uiState.match("scr-market:mod-sell")) {
            if (!mgr.master.approximateListOfCommodities("lst-goods", mgr.cfg.getMarketInSellOrder()))
                notifyError(_("Cannot detect commodities in 'lst-goods', aborting"), Result::Trouble);
            mgr.rEnv.classified = mgr.master.cEnv().classified; // TODO: evil hack
            const ClassifiedRect* focusedRow = nullptr;
            const Commodity* focusedCommodity = nullptr;
            for (auto &cr: mgr.rEnv.classified) {
                if (cr.cdt != ClsDetType::ListRow || cr.u.lrow.list->name != "lst-goods")
                    continue;
                const Commodity* rowCommodity = cr.u.lrow.commodity;
                if (!rowCommodity)
                    rowCommodity = mgr.cfg.getCommodityByName(cr.text, true);
                if (cr.u.lrow.ws == WState::Focused) {
                    focusedRow = &cr;
                    LOG(INFO) << "Focused row text: " << focusedRow->text;
                    focusedCommodity = rowCommodity;
                    if (focusedCommodity)
                        LOG(INFO) << "Focused commodity: " << focusedCommodity->name;
                }
                if (rowCommodity == mCommodity) {
                    LOG(INFO) << "Row with required commodity found";
                    if (cr.u.lrow.ws == WState::Focused) {
                        LOG(INFO) << "Pressing 'space'";
                        sendKey("space", 0, 500);
                        continue;
                    } else {
                        LOG(INFO) << "Not focused, using mouse click";
                        cv::Rect rect = cr.detectedRect;
                        sendMouseClick((rect.tl() + rect.br()) / 2, 0, 500);
                    }
                    continue;
                }
            }
            if (!focusedRow) {
                LOG(INFO) << "No focused row found, moving mouse to the list area";
                cv::Rect rect = mgr.master.resolveWidgetReferenceRect("lst-goods");
                int x = rect.x+rect.width/2;
                int y = rect.y - 20;
                sendMouseClick({x,y}, 0, 500);
                for (int i=0; i < 10; i++)
                    sendMouseMove({0, 10}, 25, false);
                continue;
            }
            if (!focusedCommodity)
                notifyError(_("Cannot detect commodities in 'lst-goods', aborting"), Result::Trouble);

            int focusedIdx = -1;
            int needIdx = -1;
            std::vector<Commodity *> sellTable = mgr.cfg.getMarketInSellOrder();
            for (int idx = 0; idx < sellTable.size(); idx++) {
                auto &c = sellTable[idx];
                if (c == focusedCommodity)
                    focusedIdx = idx;
                if (c == mCommodity)
                    needIdx = idx;
            }
            if (needIdx >= 0 && focusedIdx >= 0) {
                LOG(INFO) << "Distance is "<<(needIdx - focusedIdx)<<" lines from focused '" << sellTable[focusedIdx]->name << " to " << mCommodity->name;
                if (needIdx < focusedIdx) {
                    for (int cnt=0; cnt < focusedIdx-needIdx; cnt++)
                        sendKey("up");
                } else {
                    for (int cnt=0; cnt < needIdx-focusedIdx; cnt++)
                        sendKey("down");
                }
                continue;
            }
            notifyError(_("Cannot detect commodities in 'lst-goods', aborting"), Result::Trouble);
        } else if (mgr.uiState.match("scr-market:mod-sell:dlg-trade:*")) {
            LOG(INFO) << "At market sell dialog, checking commodity '" << mCommodity->name << "'";
            auto lblCommodity = mgr.master.getLabelCommodity("lbl-commodity");
            if (lblCommodity != mCommodity) {
                executeAction("restart");
                notifyError(_("Wrong sell dialog commodity, aborting"), Result::Trouble);
                continue;
            }
            LOG(INFO) << "At market sell dialog, execute action 'sell-some'";
            bool ok = executeAction("sell-some", actionArgs);
            if (!ok) {
                LOG(WARNING) << "Step 'sell-some' not successful, recovering";
                executeAction("restart");
                notifyError(_("Step 'sell-some' not successful"), Result::Trouble);
            }
            mTotal -= mItems;
            sellItems -= mItems;
            missCount = 0;
            continue;
        } else {
            notifyError(std_format(_("Unknown state '{}', aborting trade task"), mgr.uiState.to_string()), Result::Trouble);
        }
    }
    return Result::Success;
}

TaskDebugFindAllCommodities::TaskDebugFindAllCommodities(Task* parent, AIManager& mgr, const TaskTemplate& templ)
    : Task(parent, mgr, templ)
    , shuffle(false)
    , dump_index(4)
    , start_index(0)
{
    assert (templ.name == ED_TASK_DEBUG_FILE_ALL_COMMODITIES);
}

Result TaskDebugFindAllCommodities::run() {
    mgr.detectEDState(DetectLevel::Screen);
    if (!mgr.uiState.match("scr-market:*"))
        notifyError("Not in market?", Result::Failure);
    std::string marketMode = mgr.uiState.path().substr(11);
    std::vector<Commodity*> table;
    if (marketMode == "mod-sell")
        table = mgr.cfg.getMarketInSellOrder();
    else if (marketMode == "mod-buy")
        table = mgr.cfg.getMarketInBuyOrder();
    else
        notifyError("Unknown market mode "+marketMode, Result::Failure);
    if (table.empty())
        notifyError("Empty market?", Result::Failure);
    struct VerifyStats {
        int ocr_min_conf = 100;
        int ocr_max_conf = 0;
        int fuzzy_min_conf = 100;
        int fuzzy_max_conf = 0;
        int total_samples = 0;
    };
    std::map<const Commodity*,VerifyStats> verifyMap;
    int verifyUnrecognized = 0;

    std::deque<Commodity *> checkCommoditiesTable(table.begin(), table.end());
    int passed = 0;
    int failed = 0;
    if (shuffle) {
        std::srand(std::time({}));
    } else {
        for (int i=0; i < start_index; i++)
            checkCommoditiesTable.pop_front();
    }
    int left = checkCommoditiesTable.size();
    int checkIdx = 0;
    while (!checkCommoditiesTable.empty()) {
        if (shuffle)
            checkIdx = std::rand() % checkCommoditiesTable.size();
        Commodity *commodity = checkCommoditiesTable[checkIdx];
        std::vector<CommodityMatch> verify;
        bool ok = checkCommodity(commodity, marketMode, table, &verify);
        if (!ok)
            failed += 1;
        else
            passed += 1;
        left -= 1;
        notifyProgress("Test for commodity '"+commodity->name+"' "+(ok?" PASSED\n":" FAILED\n")+
                    "Progress: "+std::to_string(passed)+" passed and "+std::to_string(failed)+" failed\n"+
                    "left "+std::to_string(left)+" out of "+std::to_string(table.size()-start_index));
        std::erase(checkCommoditiesTable, commodity);
        for (auto& v : verify) {
            if (!v.commodity) {
                verifyUnrecognized += 1;
                continue;
            }
            auto& vs = verifyMap[v.commodity];
            if (v.ocr_conf < vs.ocr_min_conf)
                vs.ocr_min_conf = v.ocr_conf;
            if (v.ocr_conf > vs.ocr_max_conf)
                vs.ocr_max_conf = v.ocr_conf;
            if (v.fuzzy_conf < vs.fuzzy_min_conf)
                vs.fuzzy_min_conf = v.fuzzy_conf;
            if (v.fuzzy_conf > vs.fuzzy_max_conf)
                vs.fuzzy_max_conf = v.fuzzy_conf;
            vs.total_samples += 1;
        }
    }

    LOG(INFO) << "OCR/Fuzzy match statistic:";
    for (auto c : table) {
        auto& vs = verifyMap[c];
        LOG(INFO) << "  '" << c->name << "': ocr=" << vs.ocr_min_conf << ".." << vs.ocr_min_conf
                  << "; fuzzy=" << vs.fuzzy_min_conf << ".." << vs.fuzzy_max_conf;
    }
    LOG(INFO) << "  totally unrecognized: " << verifyUnrecognized;

    Sleep(1000);
    if (!checkCommoditiesTable.empty())
        notifyProgress("Cannot verify all commodities");
    notifyProgress("All commodities verified");
    return Result::Success;
}

bool TaskDebugFindAllCommodities::checkCommodity(Commodity *currCommodity, const std::string &marketMode,
                                                 const std::vector<Commodity *> &table,
                                                 std::vector<CommodityMatch> *verify) {
    for (;;) {
        mgr.detectEDState(DetectLevel::ListOcrFocusedRow);
        if (!mgr.uiState.match("scr-market:"+marketMode)) {
            notifyProgress("Not at market?");
            return false;
        }
        if (!mgr.master.approximateListOfCommodities("lst-goods", table, verify)) {
            notifyProgress("Cannot detect commodities in 'lst-goods', aborting");
            return false;
        }
        mgr.rEnv.classified = mgr.master.cEnv().classified; // TODO: evil hack
        const ClassifiedRect* focusedRow = nullptr;
        const Commodity* focusedCommodity = nullptr;
        for (auto &cr: mgr.rEnv.classified) {
            if (cr.cdt != ClsDetType::ListRow || cr.u.lrow.list->name != "lst-goods")
                continue;
            const Commodity* rowCommodity = cr.u.lrow.commodity;
            if (!rowCommodity)
                rowCommodity = mgr.cfg.getCommodityByName(cr.text, true);
            if (cr.u.lrow.ws == WState::Focused) {
                focusedRow = &cr;
                LOG(INFO) << "Focused row text: " << focusedRow->text;
                focusedCommodity = rowCommodity;
                if (focusedCommodity)
                    LOG(INFO) << "Focused commodity: " << focusedCommodity->name;
            }
            if (focusedCommodity == currCommodity) {
                LOG(INFO) << "Row with required commodity '" << focusedCommodity->name << "' found, focused";
                break;
            }
            if (rowCommodity == currCommodity) {
                LOG(INFO) << "Row with required commodity '" << rowCommodity->name << "' found, not focused";
                cv::Point mouse = (cr.detectedRect.tl() + cr.detectedRect.br()) / 2;
                sendMouseMove(mouse, 100);
                focusedCommodity = currCommodity;
                focusedRow = &cr;
                break;
            }
        }
        if (!focusedRow) {
            LOG(INFO) << "No focused row found, moving mouse to the list area";
            cv::Rect rect = mgr.master.resolveWidgetReferenceRect("lst-goods");
            int x = rect.x+rect.width/2;
            int y = rect.y - 20;
            sendMouseClick({x,y}, 0, 500);
            for (int i=0; i < 10; i++)
                sendMouseMove({0, 10}, 25, false);
            continue;
        }
        if (!focusedCommodity) {
            notifyProgress("Cannot detect commodities in 'lst-goods', aborting");
            return false;
        }
        if (focusedCommodity == currCommodity) {
            /*if (dlgCommodity == currCommodity)*/ {
                mgr.detectEDState(DetectLevel::ListOcrFocusedRow);
                cv::Rect r = mgr.rEnv.cvtReferenceToCaptured(focusedRow->detectedRect);
                saveOcrTrainingData(r, currCommodity, false);
            }
            hardcodedStep("[{key:'UI_Select', after:200},"
                          "{wait: 'scr-market:"+marketMode+":dlg-trade:*', during: 3000},"
                          "{goto:'btn-cancel', after:200}]",
                          DetectLevel::Buttons);
            const Commodity* dlgCommodity = mgr.master.getLabelCommodity("lbl-commodity");
            if (dlgCommodity != currCommodity) {
                notifyProgress("Dialog commodity mismatch");
                Sleep(3000);
            }
            hardcodedStep("[{ key: 'UI_Select' },"
                          "{ wait: 'scr-market:"+marketMode+"', during: 3000 }]",
                          DetectLevel::Buttons);
            return (dlgCommodity == currCommodity);
        }

        int focusedIdx = -1;
        int needIdx = -1;
        for (int idx = 0; idx < table.size(); idx++) {
            auto &c = table[idx];
            if (c == focusedCommodity)
                focusedIdx = idx;
            if (c == currCommodity)
                needIdx = idx;
        }
        if (needIdx >= 0 && focusedIdx >= 0) {
            LOG(INFO) << "Distance is "<<(needIdx - focusedIdx)<<" lines from focused '" << table[focusedIdx]->name << "' to '" << currCommodity->name << "'";
            if (needIdx < focusedIdx) {
                int count = focusedIdx - needIdx;
                for (int cnt=0; cnt < count; cnt++)
                    sendKey("up");
                count = std::min(3, needIdx);
                for (int cnt=0; cnt < count; cnt++)
                    sendKey("up");
                for (int cnt=0; cnt < count; cnt++)
                    sendKey("down");
            } else {
                int count = needIdx-focusedIdx;
                for (int cnt=0; cnt < needIdx-focusedIdx; cnt++)
                    sendKey("down");
                count = std::min(shuffle ? 3 : 10, int(table.size())-1-needIdx);
                for (int cnt=0; cnt < count; cnt++)
                    sendKey("down");
                for (int cnt=0; cnt < count; cnt++)
                    sendKey("up");
            }
            continue;
        }
        notifyProgress("Cannot detect commodities in 'lst-goods', aborting");
        return false;
    }
}

} // namespace ai

#include <tesseract/baseapi.h>
#include <leptonica/allheaders.h>
#include <filesystem>

namespace ai {

void TaskDebugFindAllCommodities::saveOcrTrainingData(cv::Rect rect, const Commodity* commodity, bool invert) {
    std::string lng;
    if (mgr.cfg.lng == Lang::RU)
        lng += "rus";
    else if (mgr.cfg.lng == Lang::EN)
        lng += "eng";
    else if (mgr.cfg.lng)
        lng += "xxx";
    std::string filename = std::format("testset-{}/{}-{:02d}-{}.png", lng, commodity->nameId, dump_index, lng);
    if (std::filesystem::exists(filename))
        return;

    tesseract::TessBaseAPI* tesseractApi = mgr.master.getTesseractApi();
    if (!tesseractApi)
        return;
    const cv::Mat& grayImage = mgr.master.cEnv().getGrayImage();
    cv::Mat rowImage(grayImage, rect);
    int outConf = 0;
    std::string text;
    if (!invert) {
        tesseractApi->SetImage(rowImage.data, rowImage.cols, rowImage.rows, 1, rowImage.step);
        tesseractApi->Recognize(nullptr);
        tesseract::ResultIterator* ri = tesseractApi->GetIterator();
        const char* outText = ri->GetUTF8Text(tesseract::PageIteratorLevel::RIL_TEXTLINE);
        outConf = ri->Confidence(tesseract::PageIteratorLevel::RIL_TEXTLINE);
        int left, top, right, bottom;
        ri->BoundingBox(tesseract::PageIteratorLevel::RIL_TEXTLINE, 2, &left, &top, &right, &bottom);
        text = trim(outText);
        delete[] outText;
        cv::Rect textRect = {rect.tl()+cv::Point(left,top), rect.tl()+cv::Point(right,bottom)};
        LOG(INFO) << "OCR Output: '" << text << "' words conf=" << outConf << "%" << " rect: " << textRect;
        if (textRect.empty() || outConf == 0)
            textRect = rect;
        {
            cv::Mat textImage(grayImage, textRect);
            cv::imwrite(filename, textImage);
            filename = std::format("testset-{}/{}-{:02d}-{}.gt.txt", lng, commodity->nameId, dump_index, lng);
            std::ofstream gt_txt(filename, std::ios::trunc | std::ios::binary);
            gt_txt << commodity->name;
            gt_txt.close();
            return;
        }
    }

//    if (invert) {
//        // try hard - detect background, and if it's dark - threshold and invert the image
//        int histSize = 256;
//        float range[]{0, 256}; //the upper boundary is exclusive
//        const float *histRange[]{range};
//        cv::Mat hist;
//        cv::calcHist(&rowImage, 1, nullptr, cv::Mat(), hist, 1, &histSize, histRange);
//        int maxLoc[4]{};
//        cv::minMaxIdx(hist, nullptr, nullptr, nullptr, maxLoc);
//        int background = maxLoc[0] + 5;
//        if (background > 127)
//            return 0;
//
//        cv::Mat invertedImage;
//        cv::bitwise_not(rowImage, invertedImage);
//        cv::Mat thrImage;
//        cv::threshold(invertedImage, thrImage, 255 - background, 255, cv::THRESH_BINARY);
//        mTesseractApiForMarket->SetImage(thrImage.data, thrImage.cols, thrImage.rows, 1, thrImage.step);
//        char *outText = mTesseractApiForMarket->GetUTF8Text();
//        text = trim(outText);
//        outConf = mTesseractApiForMarket->MeanTextConf();
//        delete[] outText;
//        if (outConf > 30) {
//            LOG(INFO) << "OCR Output: '" << text << "' words conf=" << outConf << "% (retried with negative)";
//            return outConf;
//        }
//    }
//    return outConf;
}

} // namespace ai