//
// Created by mkizub on 23.05.2025.
//

#include "../pch.h"

#include "Task.h"
#include "AIManager.h"
#include "../Keyboard.h"
#include "../widget/EDWidget.h"
#include "../js/parser.h"
#include <synchapi.h>

#ifndef NDEBUG
#include <cpptrace/cpptrace.hpp>
#include "cpptrace/from_current.hpp"
#endif

#ifdef CPPTRACE_TRY
# define TRY CPPTRACE_TRY
# define CATCH(param) CPPTRACE_CATCH(param)
# define GET_EXCEPTION_STACK_TRACE cpptrace::from_current_exception().to_string()
#else
# define TRY try
# define CATCH(param) catch(param)
# include <stacktrace>
# define GET_EXCEPTION_STACK_TRACE std::stacktrace::current()
#endif

namespace ai {

Step::Step()
    : parent(ai::curr_step().get())
{
}

bool Step::run_sub_step(spStep step) {
    assert (!currSubStep);
    assert (!step->currSubStep);
    currSubStep = step;
    bool ok = false;
    TRY {
        check_interrupted();
        ok = step->run();
    } CATCH (const std::exception& ex) {
        kbd::reset_vJoy();
        prevSubStep = currSubStep;
        currSubStep.reset();
        if (dynamic_cast<const nonlocal_return*>(&ex)) {
            throw;
        }
        else if (dynamic_cast<const interrupted_error*>(&ex)) {
            throw;
        }
        else {
            LOG(ERROR) << "Exception during task task_step execution: " << ex.what() << std::endl << GET_EXCEPTION_STACK_TRACE;
            return false;
        }
    }
    prevSubStep = currSubStep;
    currSubStep.reset();
    return ok;
}

Task::Task(const TaskTemplate& templ_)
    : templ(templ_)
{
}

std::string Task::getTitle() {
    return templ.name();
}

void sleep(int milliseconds, bool precise) {
    check_interrupted();
    if (milliseconds <= 0)
        return;
    auto now = std::chrono::high_resolution_clock::now();
    auto until = now + std::chrono::milliseconds(milliseconds);
    if (milliseconds >= 75 || !precise) {
        auto until_rough = until;
        if (precise)
            until_rough -= 50ms;
        while (now < until_rough) {
            check_interrupted();
            auto left = std::chrono::duration_cast<std::chrono::milliseconds>(until_rough - now);
            if (left.count() < 5)
                break;
            auto duration = std::min(500ms, left);
            std::this_thread::sleep_for(duration);
            now = std::chrono::high_resolution_clock::now();
        }
    }
    if (precise) {
        while (now < until) {
            check_interrupted();
            now = std::chrono::high_resolution_clock::now();
        }
    }
}

static int get_int(const js::value& val, const js::value& args, int dflt = -1) {
    if (val.is_null() && dflt >= 0)
        return dflt;
    if (val.is_int())
        return val.as_int();
    if (val.is_string()) {
        const js::value& resolved = args.at(val.as_string());
        if (resolved.is_int())
            return resolved.as_int();
    }
    LOG(ERROR) << "integer value expected, but got: " << val << " with args: " << args;
    return 0;
}

bool Task::decodePosition(const js::value& pos, cv::Point& point, const js::value& args) const {
    if (pos.is_string()) {
        cv::Rect rect = Mgr.resolveWidgetReferenceRect(pos.as_string(), ai::rEnv);
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

bool Task::executeWait(const js::value& step, const js::value& args) {
    LOG(DEBUG) << "action task_step wait: " << step;
    const js::value& state = step.at("wait");
    const js::value& focus = step.at("focus");
    const js::value& disabled = step.at("disabled");
    auto start = std::chrono::system_clock::now();
    auto now = start;
    int during = 3000;
    int period = 250;
    if (step.at("during").is_int())
        during = std::max(100, (int)step.at("during").as_int());
    if (step.at("period").is_int())
        period = std::max(100, (int)step.at("period").as_int());
    auto until = now + std::chrono::milliseconds(during);
    LOG(INFO) << "Step 'wait' #0 duration " << during << " left " << std::chrono::duration_cast<std::chrono::milliseconds>(until - now).count();
    bool ok;
    for (int counter=1; now < until; counter++) {
        ok = ai::detectEDState(DetectLevel::Buttons);
        if (ok && ai::uiState.match(state.as_string())) {
            bool ok_focus = true;
            if (focus.is_string()) {
                ok_focus = false;
                for (auto& cr : ai::rEnv.classified) {
                    if (cr.cdt == ClsDetType::Widget && cr.u.widg.ws == WState::Focused && cr.u.widg.widget->name == focus.as_string()) {
                        ok_focus = true;
                        break;
                    }
                }
            }
            bool ok_disabled = true;
            if (disabled.is_string()) {
                ok_disabled = false;
                for (auto& cr : ai::rEnv.classified) {
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
        std::this_thread::sleep_for(std::chrono::milliseconds(period));
        now = std::chrono::system_clock::now();
        LOG_IF(!ok,INFO) << "Step 'wait' #"<<counter<<" duration " << during << " left " << std::chrono::duration_cast<std::chrono::milliseconds>(until - now).count();
    }
    LOG_IF(!ok,ERROR) << "Step " << step << " failed - wait time expired, current state is " << ai::uiState;
    LOG_IF(ok,INFO) << "Step " << step << " successful, waited " << std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count();
    return ok;
}

bool Task::executeStep(const js::value& step, const js::value& args) {
    if (step.is_array()) {
        for (auto& s : step.as_array()) {
            bool ok = executeStep(s, args);
            if (!ok)
                return false;
        }
        return true;
    }
    if (step.is_object()) {
        if (step.as_object().contains("task_loop")) {
            LOG(DEBUG) << "action task_step task_loop: " << step;
            const js::value& loop = step.at("task_loop");
            const js::value& action = step.at("action");
            int count = get_int(loop, args);
            if (count < 0) {
                LOG(ERROR) << "bad task_loop counter value: " << step << " with args: " << args;
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
            LOG(DEBUG) << "action task_step check: " << step;
            const js::value& state = step.at("check");
            ai::detectEDState(DetectLevel::Buttons);
            bool ok = ai::uiState.match(state.as_string());
            if (ok) {
                const js::value &focus = step.at("focus");
                if (focus) {
                    const widget::Widget* fw = ai::uiState.focused;
                    std::string fn = fw ? fw->name : "";
                    ok = focus.is_string() && fn == focus.as_string();
                    LOG_IF(!ok,ERROR) << "Step failed, current focus at '" << fn << "', but '" << focus << "' required";
                }
            }
            LOG_IF(!ok,ERROR) << "Step " << step << " failed, current state is " << ai::uiState;
            return ok;
        }
        if (step.as_object().contains("key")) {
            LOG(DEBUG) << "action task_step key: " << step;
            const js::value& key = step.at("key");
            bool ok;
            if (step.at("hold").is_object() || step.at("hold").is_array()) {
                const KeyBindings& keyBindings = Cfg.getGameKeyBindings(key.as_string());
                const GameKey* gk = nullptr;
                if (keyBindings.primary.device != GameKey::Void)
                    gk = &keyBindings.primary;
                else if (keyBindings.secondary.device != GameKey::Void)
                    gk = &keyBindings.secondary;
                else {
                    LOG(DEBUG) << "key '" << key.as_string() << "' not bound, action failed";
                    return false;
                }
                unsigned inputId = kbd::post(*gk, 60000);
                ok = executeStep(step.at("hold"), args);
                kbd::clearInput(inputId);
                if (ok) {
                    int after = get_int(step.at("after"), args, Cfg.getDefaultKeyAfterTime());
                    sleep(after);
                }
            } else {
                int hold = get_int(step.at("hold"), args, Cfg.getDefaultKeyHoldTime());
                int after = get_int(step.at("after"), args, Cfg.getDefaultKeyAfterTime());
                ok = kbd::send(key.as_string(), hold, after);
            }
            LOG_IF(!ok,ERROR) << "Step " << step << " failed";
            return ok;
        }
        if (step.as_object().contains("goto")) {
            LOG(DEBUG) << "action goto: " << step;
            const js::value& widget = step.at("goto");
            cv::Point pos;
            if (!decodePosition(widget, pos, args)) {
                LOG(ERROR) << "Step " << step << " failed";
                return false;
            }
            int after = get_int(step.at("after"), args, Cfg.getDefaultKeyAfterTime());
            bool ok = kbd::sendMouseMove(pos, after);
            LOG_IF(!ok,ERROR) << "Step " << step << " failed";
            return ok;
        }
        if (step.as_object().contains("click")) {
            LOG(DEBUG) << "action click: " << step;
            const js::value& widget = step.at("click");
            cv::Point pos;
            if (!decodePosition(widget, pos, args)) {
                LOG(ERROR) << "Step " << step << " failed";
                return false;
            }
            int hold = get_int(step.at("hold"), args, Cfg.getDefaultKeyHoldTime());
            int after = get_int(step.at("after"), args, Cfg.getDefaultKeyAfterTime());
            bool ok = kbd::sendMouseClick(pos, hold, after);
            LOG_IF(!ok,ERROR) << "Step " << step << " failed";
            return ok;
        }
        if (step.as_object().contains("sleep")) {
            LOG(DEBUG) << "action task_step sleep: " << step;
            int duration = get_int(step.at("sleep"), args);
            sleep(duration);
            return true;
        }
        // fall through
    }
    LOG(ERROR) << "Unknown action task_step: " << step;
    return false;
}

void Task::hardcodedStep(const std::string& step, DetectLevel level, cv::Mat* grayImage) {
    js::value parsed, args;
    try {
        std::stringstream in(step);
        in >> js::rule::json5() >> parsed;
    } catch (...) {
        LOG(ERROR) << "Failed to parse json " << step;
        throw nonlocal_return(TaskExitReason::FAILED, "hardcoded task_step parse failed");
    }
    if (!executeStep(parsed, args)) {
        LOG(ERROR) << "Failed to execute " << step;
        throw nonlocal_return(TaskExitReason::ONGOING, "hardcoded task_step failed");
    }
    if (grayImage)
        ai::detectEDStateGrayIm(level, *grayImage);
    else
        ai::detectEDState(level);
}

bool Step::Message::expired() const {
    auto now = std::chrono::steady_clock::now();
    switch (severity) {
    default:
    case MSG_INFO:
        return (now - timestamp) > 5s;
    case MSG_WARN:
        return (now - timestamp) > 10s;
    case MSG_ERROR:
        return (now - timestamp) > 30s;
    case MSG_FATAL:
        return false;
    }
}

void Step::addMessage(MessageSeverity severity, const std::string_view msg) {
    if (msg.empty())
        return;
    std::scoped_lock<std::mutex> lock(messagesMutex);
    while (!messages.empty() && messages.front().expired() || messages.size() > 4)
        messages.pop_front();
    messages.emplace_back(std::chrono::steady_clock::now(), severity, std::string(msg));
}

std::vector<std::string> Step::getMessages() {
    std::scoped_lock<std::mutex> lock(messagesMutex);
    std::vector<std::string> out;
    for (auto& msg : messages)
        if (!msg.expired())
            out.push_back(msg.message);
    return out;
}

std::string Step::getStatus() {
    return {};
}


TaskRepeat::TaskRepeat(const TaskTemplate& templ_)
    : Task(templ_)
{
    assert (templ.id == ED_TASK_REPEAT);
    for (auto& p : templ.params) {
        if (p.id == "count")
            mTotal = p.as_integer();
        else if (p.id == "duration")
            mDuration = p.as_integer();
        else if (p.id == "tasks") {
            if (p.value.is_array()) {
                for (auto task : p.value.as_array()) {
                    TaskTemplate tt = TaskTemplate::loadTask(task);
                    if (!tt.id.empty())
                        steps.emplace_back(std::move(tt));
                }
            }
        }
    }
}

std::string TaskRepeat::getTitle() {
    std::string name = templ.nm.empty() ? _gt("Repeat") : templ.name();
    if (mTotal)
        return std::format("{}: {} / {} ", name, mCompleted+1, mTotal);
    return std::format("{}: {} ", name, mCompleted+1);
}

bool TaskRepeat::run() {
    if (mTotal && mTotal - mCompleted <= 0)
        return true;
    if (steps.empty())
        return true;

    if (mDuration && !timer.started())
        timer = utc_timer(std::chrono::minutes(mDuration));

    if (mDuration && timer.expired() || mTotal && mCompleted >= mTotal)
        return true;
    if (!mStarted) {
        // skip to travel entry if we are docked
        for (int i=0; i < steps.size(); i++) {
            TaskTemplate &step_templ = steps[i];
            if (step_templ.id == ED_TASK_TRAVEL && st::ship.flags.docked) {
                std::string destDockName;
                for (auto& p : step_templ.params) {
                    if (p.id == "dock")
                        destDockName = p.as_string();
                }
                if (st::dockedAt.stationName == destDockName) {
                    mStepIdx = i + 1;
                    break;
                }
            }
        }
    }
    mStarted = true;

    for (;;) {
        if (mTotal && mCompleted >= mTotal)
            return true;
        for (; mStepIdx < steps.size(); mStepIdx++) {
            if (mDuration && timer.expired())
                return true;
            TaskTemplate &step_templ = steps[mStepIdx];
            auto step = spStep(step_templ.factory(step_templ));
            if (!step)
                throw_failed("Cannot create task for next step");
            if (!run_sub_step(step))
                return false;
        }
        mCompleted += 1;
        mStepIdx = 0;
    }
    return true;
}

} // namespace ai