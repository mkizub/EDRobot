//
// Created by mkizub on 23.05.2025.
//

#include "pch.h"

#include "Task.h"
#include "Keyboard.h"
#include "Template.h"
#include "UI.h"
#include <synchapi.h>
#include <timeapi.h>
#include "magic_enum/magic_enum.hpp"

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

bool Task::sendMouseMove(const cv::Point& point, int pause_ms) const {
    cv::Point screen = master.lastEDState().cEnv.cvtReferenceToDesktop(point);
    bool virtualDesktop = (GetSystemMetrics(SM_CMONITORS) > 1);
    screen = master.lastEDState().cEnv.cvtReferenceToDesktop(point);
    //LOG(INFO) << "sendMouseMove recalculated from reference " << point << " to screen " << screen;
    if (!keyboard::sendMouseMoveTo(screen.x, screen.y, true, virtualDesktop))
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
        cv::Rect rect = master.resolveWidgetRect(master.lastEDState().path() + ":" + pos.as_string());
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
        master.detectEDState();
        ok = master.isEDStateMatch(state.as_string());
        if (ok) {
            bool ok_focus = true;
            if (focus.is_string()) {
                auto& cbs = master.lastEDState().cEnv.classifiedButtonStates;
                auto it = cbs.find(focus.as_string());
                ok_focus = (it != cbs.end() && it->second == ButtonState::Focused);
            }
            bool ok_disabled = true;
            if (disabled.is_string()) {
                auto& cbs = master.lastEDState().cEnv.classifiedButtonStates;
                auto it = cbs.find(disabled.as_string());
                ok_disabled = (it != cbs.end() && it->second == ButtonState::Disabled);
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
            master.detectEDState();
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

void Task::notifyProgress(const std::string& msg) {
    LOG(INFO) << msg;
    if (taskName.empty())
        taskName = "EDRobot";
    UI::showToast(taskName, msg);
}
void Task::notifyError(const std::string& msg) {
    LOG(ERROR) << msg;
    if (taskName.empty())
        taskName = "EDRobot";
    UI::showToast(taskName, msg);
}

TaskCalibrate::TaskCalibrate()
    : mDetector(HistogramTemplate::CompareMode::RGB, cv::Rect(), cv::Vec3b())
{
    taskName = "Calibration";
}

void TaskCalibrate::recordLuv(const char* button, ButtonState bs) {
    cv::Rect rect = master.resolveWidgetRect(master.lastEDState().path()+":"+button);
    if (rect.empty()) {
        LOG(ERROR) << "Cannot get rect of button '" << button << "'";
        return;
    }
    mDetector.mRect = rect;
    ClassifyEnv cEnv = master.lastEDState().cEnv; // copy
    mDetector.match(cEnv);
    cv::Vec3b rgb = mDetector.mLastColor;
    mRGBColors[int(bs)].push_back(rgb);
    mLuvColors[int(bs)].push_back(rgb2luv(rgb));
    imageCounter += 1;
    const char* names[] = {"Normal   ", "Focused  ", "Activated", "Disabled "};
    LOG(INFO) << names[int(bs)] << " button: luv=" << mLuvColors[int(bs)].back()
              << " rgb=" << rgb << std::format(" 0x{:06x}", decodeRGB(rgb));
    //cv::Mat m(cEnv.imageColor,rect);
    //std::string bname = master.lastEDState().path()+"-"+button;
    //std::transform(bname.begin(), bname.end(), bname.begin(),[](unsigned char c){ return c==':' ? '~' : c; });
    //std::string fname = "calibration-"+std::format("{:02d}", imageCounter)+"-"+toLower(trim(names[int(bs)]))+"-"+bname+".png";
    //cv::imwrite(fname, m);
}

void TaskCalibrate::hardcodedStep(const char* step) {
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
    master.detectEDState();
}

bool TaskCalibrate::calculateAverage() {
    bool success = true;
    for(auto bs : magic_enum::enum_values<ButtonState>()) {
        auto& luvState = mLuvColors[int(bs)];
        auto& rgbState = mRGBColors[int(bs)];
        int len = (int)luvState.size();
        if (!len) {
            LOG(INFO) << "No samples for " << magic_enum::enum_name(bs) << " button color";
            return false;
        }
        cv::Mat colorsMatrix(len, 1, CV_8UC3);
        for (int j=0; j < len; j++)
            colorsMatrix.at<cv::Vec3b>(j) = luvState[j];
        cv::Scalar meanS;
        cv::Scalar stddevS;
        cv::meanStdDev(colorsMatrix, meanS, stddevS);
        cv::Vec3b mean(meanS[0], meanS[1], meanS[2]);
        cv::Vec3d stddev(stddevS[0], stddevS[1], stddevS[2]);
        LOG(INFO) << "Luv color for " << magic_enum::enum_name(bs) << " mean " << mean << " stddev " << stddev << " over " << len << " samples";
        mLuvAverage[int(bs)] = mean;
        if (stddevS[0] > 3 || stddevS[1] > 3 || stddevS[2] > 3) {
            success = false;
            LOG(ERROR) << "Luv color for " << magic_enum::enum_name(bs) << ", has too high deviation " << stddev;
        }
    }
    if (success) {
        master.setCalibrationResult(mLuvAverage);
        master.getConfiguration()->saveCalibration();
    }
    return success;
}

bool TaskCalibrate::run() {
    if (done) {
        LOG(WARNING) << "done at start";
        return false;
    }
    try {
        master.detectEDState();
        if (!master.isEDStateMatch("scr-market:mod-sell")) {
            notifyError(_("Not at market sell, calibration fails"));
            return false;
        }

        notifyProgress(_("Calibration started"));

        hardcodedStep("{goto:'btn-exit', after: 500}");
        LOG(INFO) << "State " << master.lastEDState() << " activated 'btn-exit'";

        recordLuv("btn-help", ButtonState::Normal);
        recordLuv("btn-exit", ButtonState::Focused);
        recordLuv("btn-to-sell", ButtonState::Activated);

        hardcodedStep("[{goto:'lst-goods', after:500},"
                      "{key:'space', after:1000},"
                      "{check:'scr-market:mod-sell:dlg-trade:*'},"
                      "{goto:'btn-more', after:500}]");
        LOG(INFO) << "State " << master.lastEDState();

        recordLuv("btn-exit", ButtonState::Normal);
        recordLuv("btn-more", ButtonState::Focused);
        recordLuv("btn-commit", ButtonState::Disabled);

        hardcodedStep("[{key:'left'},"
                      "{key:'space', after:1000},"
                      "{check:'scr-market:mod-sell'},"
                      /*"{goto:[0,0], after:1000},"*/
                      "{click:'btn-to-buy', after:1000},"
                      "{check:'scr-market:mod-buy'},"
                      "{goto:'btn-help', after:500}]");
        LOG(INFO) << "State " << master.lastEDState() << " activated 'btn-help'";

        recordLuv("btn-exit", ButtonState::Normal);
        recordLuv("btn-help", ButtonState::Focused);
        recordLuv("btn-to-buy", ButtonState::Activated);

        hardcodedStep("[{goto:'lst-goods', after:500},"
                      "{key:'space', after:1000},"
                      "{check:'scr-market:mod-buy:dlg-trade:*'},"
                      "{goto:'btn-more', after:1000}]");
        LOG(INFO) << "State " << master.lastEDState();

        recordLuv("btn-exit", ButtonState::Normal);
        recordLuv("btn-more", ButtonState::Focused);
        recordLuv("btn-commit", ButtonState::Disabled);

        hardcodedStep("[{key:'left'},"
                      "{key:'space', after:500},"
                      "{check:'scr-market:mod-buy'},"
                      "{click:'btn-to-sell', after:500},"
                      "{check:'scr-market:mod-sell'},"
                      "{goto:'btn-exit', after:500}]");

        LOG(INFO) << "State " << master.lastEDState();

        if (calculateAverage())
            notifyProgress(_("Calibration completed successfully!"));
        else
            notifyError(_("Failed to calibrate button state detector"));
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
        notifyProgress(std_format(_("Start selling {} times by {} item(s)"), mSells, mItems));
        auto actionArgs = json5pp::object({{"$items", mItems}});
        while (mSells > 0) {
            master.detectEDState();
            if (master.isEDStateMatch("scr-market:mod-sell")) {
                LOG(INFO) << "At market sell, execute action 'start'";
                bool ok = executeAction("start");
                if (!ok) {
                    notifyError(_("Step 'sell-start' failed, aborting"));
                    break;
                }
                continue;
            }
            else if (master.isEDStateMatch("scr-market:mod-sell:dlg-trade:*")) {
                LOG(INFO) << "At market sell dialog, execute action 'sell-some'";
                bool ok = executeAction("sell-some", actionArgs);
                if (!ok) {
                    LOG(WARNING) << "Step 'sell-some' not successful, recovering";
                    for (int i=0; i < 3; i++) {
                        master.detectEDState();
                        if (master.isEDStateMatch("scr-market:mod-sell:dlg-trade:*")) {
                            LOG(WARNING) << "Step 'sell-some' not successful, retrying";
                            executeAction("restart");
                        }
                        else if (master.isEDStateMatch("scr-market:mod-sell")) {
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
                if (ok)
                    mSells -= 1;
                continue;
            }
            else {
                notifyError(std_format(_("Unknown state '{}', aborting trade task"), master.lastEDState().to_string()));
                done = true;
                return false;
            }
        }
    } catch (completed_error& e) {
        return false;
    }
    done = true;
    return true;
}
