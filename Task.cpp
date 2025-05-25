//
// Created by mkizub on 23.05.2025.
//

#include "Task.h"
#include "Keyboard.h"
#include "easylogging++.h"
#include <synchapi.h>

void Task::sleep(int milliseconds) {
    check_completed();
    if (milliseconds <= 0)
        return;

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

bool Task::sendKey(const std::string& name, int delay_ms, int pause_ms) {
    try {
        if (!keyboard::sendDown(name))
            return false;
        sleep(delay_ms > 0 ? delay_ms : master.getDefaultKeyHoldTime());
        bool ok = keyboard::sendUp(name);
        sleep(pause_ms > 0 ? pause_ms : master.getDefaultKeyAfterTime());
        return ok;
    } catch (...) {
        keyboard::sendUp(name);
        throw;
    }
}

bool Task::executeAction(const std::string& actionName, const json5pp::value& args, std::string& expect_goto) {
    const json5pp::value& action = master.getAction(actionName);
    std::string fromState = action.at("from").as_string();
    std::string gotoState = action.at("goto").as_string();
    const json5pp::value& execute = action.at("exec");

    if (executeStep(execute, args)) {
        expect_goto = gotoState;
        return true;
    }
    return false;
}

int get_int(const json5pp::value& val, const json5pp::value& args, int dflt = -1) {
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
        if (step.as_object().contains("key")) {
            LOG(INFO) << "action step key: " << step;
            const json5pp::value& key = step.at("key");
            int hold = get_int(step.at("hold"), args, master.getDefaultKeyHoldTime());
            int after = get_int(step.at("after"), args, master.getDefaultKeyAfterTime());
            return sendKey(key.as_string(), hold, after);
        }
        if (step.as_object().contains("loop")) {
            LOG(INFO) << "action step loop: " << step;
            const json5pp::value& loop = step.at("loop");
            const json5pp::value& action = step.at("action");
            int count = get_int(loop, args);
            if (count < 0) {
                LOG(ERROR) << "bad loop counter value: " << step << " with args: " << args;
                return false;
            }
            for (int i=0; i < count; i++) {
                bool ok = executeStep(action, args);
                if (!ok)
                    return false;
            }
            return true;
        }
        if (step.as_object().contains("sleep")) {
            LOG(INFO) << "action step sleep: " << step;
            int duration = get_int(step.at("sleep"), args);
            sleep(duration);
            return true;
        }
        if (step.as_object().contains("check")) {
            LOG(INFO) << "action step check: " << step;
            const json5pp::value& state = step.at("check");
            return master.getEDState("") == state.as_string();
        }
        // fall through
    }
    LOG(ERROR) << "Unknown action step: " << step;
    return false;
}

bool TaskSell::run() {
    if (done) {
        LOG(INFO) << "done at start";
        return false;
    }
    try {
        std::string expected;
        std::string edState = master.getEDState(expected);
        expected = edState;
        auto actionArgs = json5pp::object({{"$items", mItems}});
        while (mSells > 0) {
            edState = master.getEDState(expected);
            if (edState.empty() && expected != edState) {
                LOG(ERROR) << "Unknown state, aborting trade task";
                done = true;
                return false;
            }
            if (edState == "scr-market:mode-sell:dlg-trade" && expected != edState) {
                LOG(INFO) << "At market sell dialog, execute action 'sell-recover'";
                bool ok = executeAction("sell-recover", actionArgs, expected);
                if (!ok)
                    return false;
                continue;
            }
            if (edState == "scr-market:mode-sell" && expected != edState) {
                LOG(INFO) << "At market sell, execute action 'sell-restart'";
                bool ok = executeAction("sell-restart", actionArgs, expected);
                if (!ok)
                    return false;
                continue;
            }
            if (edState == "scr-market:mode-sell" && (expected.empty() || expected == edState)) {
                LOG(INFO) << "At market sell, execute action 'sell-start'";
                 executeAction("sell-start", actionArgs, expected);
                continue;
            }
            if (edState == "scr-market:mode-sell:dlg-trade" && (expected.empty() || expected == edState)) {
                LOG(INFO) << "At market sell dialog, execute action 'sell-some'";
                bool ok = executeAction("sell-some", actionArgs, expected);
                if (ok)
                    mSells -= 1;
                continue;
            }
            LOG(ERROR) << "Unknown state, aborting trade task";
            done = true;
            return false;
        }
    } catch (completed_error& e) {
        return false;
    }
    done = true;
    return true;
}
