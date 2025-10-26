//
// Created by mkizub on 23.05.2025.
//

#include "../pch.h"

#include "Task.h"
#include "AIManager.h"
#include "../Keyboard.h"
#include "../ui/UIManager.h"
#include "../EDWidget.h"
#include <synchapi.h>

#ifndef NDEBUG
#include <cpptrace/cpptrace.hpp>
#include "cpptrace/from_current.hpp"
#endif

#ifdef CPPTRACE_TRY
# define TRY CPPTRACE_TRY
# define CATCH(param) CPPTRACE_CATCH(param)
# define GET_STACK_TRACE std::stacktrace::current().to_string()
# define GET_EXCEPTION_STACK_TRACE cpptrace::from_current_exception().to_string()
#else
# define TRY try
# define CATCH(param) catch(param)
# include <stacktrace>
# define GET_EXCEPTION_STACK_TRACE std::stacktrace::current()
#endif

namespace ai {

Step::Step(Step* parent, AIManager& mgr)
    : parent(parent)
    , mgr(mgr)
{
}

Task* Step::getTask() {
    for (Step* s = this; s; s = (Step*)s->parent) {
        auto t = dynamic_cast<Task*>(s);
        if (t)
            return t;
    }
    LOG(ERROR) << "Step needs parent Task";
    throw std::runtime_error("Step needs parent Task");
}


bool Step::run_sub_step(spStep step) {
    currentSubStep = step;
    bool ok = false;
    TRY {
        check_interrupted();
        ok = step->step();
    } CATCH (const std::exception& ex) {
        kbd::reset_vJoy();
        if (dynamic_cast<const nonlocal_return*>(&ex)) {
            throw;
        }
        else if (dynamic_cast<const interrupted_error*>(&ex)) {
            throw;
        }
        else {
            LOG(ERROR) << "Exception during task step execution: " << ex.what() << std::endl << GET_EXCEPTION_STACK_TRACE;
            return false;
        }
    }
    currentSubStep.reset();
    return ok;
}

Task::Task(Task* parent, AIManager& mgr, const TaskTemplate& templ_)
    : Step(parent, mgr)
    , templ(templ_)
    , taskName(templ.name)
    , maxMisses(templ.maxMisses)
{
}

const char* Task::getName() {
    return taskName.c_str();
}

bool Task::step() {
    if (this->result == Result::Created)
        this->result = Result::Started;
    for (int i=missCount; i <= maxMisses; i++) {
        this->result = safe_run();
        if (this->result == Result::Trouble) {
            this->missCount += 1;
            continue;
        }
    }
    return (result >= Result::Partly);
}

Result Task::safe_run() {
    TRY {
        currentSubStep.reset();
        check_interrupted();
        this->result = this->run();
        if (this->result == Result::Trouble)
            this->missCount += 1;
    } CATCH (const std::exception& ex) {
        kbd::reset_vJoy();
        if (auto nlr = dynamic_cast<const nonlocal_return*>(&ex)) {
            if (nlr->task) {
                nlr->task->result = nlr->result;
                if (nlr->task->result == Result::Trouble)
                    nlr->task->missCount += 1;
            }
            else if (nlr->result == Result::Trouble) {
                this->missCount += 1;
            }
            return nlr->result;
        }
        else if (dynamic_cast<const interrupted_error*>(&ex)) {
            throw;
        }
        else {
            LOG(ERROR) << "Exception during task execution: " << ex.what() << std::endl << GET_EXCEPTION_STACK_TRACE;
            this->result = Result::Failure;
            return Result::Failure;
        }
    }
    return this->result;
}

void ai_sleep(int milliseconds, bool precise) {
    check_interrupted();
    if (milliseconds <= 0)
        return;
    if (milliseconds >= 75 && !precise) {
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
                else {
                    LOG(DEBUG) << "key '" << key.as_string() << "' not bound, action failed";
                    return false;
                }
                unsigned inputId = kbd::post(*gk, 60000);
                ok = executeStep(step.at("hold"), args);
                kbd::clearInput(inputId);
                if (ok) {
                    int after = get_int(step.at("after"), args, mgr.cfg.getDefaultKeyAfterTime());
                    sleep(after);
                }
            } else {
                int hold = get_int(step.at("hold"), args, mgr.cfg.getDefaultKeyHoldTime());
                int after = get_int(step.at("after"), args, mgr.cfg.getDefaultKeyAfterTime());
                ok = kbd::send(key.as_string(), hold, after);
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
            bool ok = kbd::sendMouseMove(pos, after);
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
            bool ok = kbd::sendMouseClick(pos, hold, after);
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

void Task::hardcodedStep(const std::string& step, DetectLevel level, cv::Mat* colorImage, cv::Mat* grayImage) {
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
    mgr.detectEDState(level, colorImage, grayImage);
}

void Step::addMessage(const char* msg) {
    if (msg)
        addMessage(std::string(msg));
}
void Step::addMessage(const std::string& msg) {
    std::scoped_lock<std::mutex> lock(messagesMutex);
    auto now = std::chrono::steady_clock::now();
    auto expired = now - std::chrono::seconds(5);
    while (!messages.empty() && messages.front().timestamp < expired || messages.size() > 4)
        messages.pop_front();
    messages.emplace_back(now, msg);
}

std::vector<std::string> Step::getMessages() {
    std::scoped_lock<std::mutex> lock(messagesMutex);
    auto expired = std::chrono::steady_clock::now() - std::chrono::seconds(60);
    std::vector<std::string> out;
    for (auto& msg : messages)
        if (msg.timestamp > expired)
            out.push_back(msg.message);
    return out;
}

std::string Step::getStatus() {
    return "----";
}

void Step::notifyProgress(const char* msg) {
    addMessage(msg);
    LOG(INFO) << msg;
    UIManager::showToast(getName(), msg);
}
void Step::notifyProgress(const std::string& msg) {
    addMessage(msg);
    LOG(INFO) << msg;
    UIManager::showToast(getName(), msg);
}
void Step::notifyError(const char* msg, Result res) {
    addMessage(msg);
    LOG(ERROR) << msg;
    UIManager::showToast(getName(), msg);
    throw nonlocal_return(res, getTask(), msg);
}
void Step::notifyError(const std::string& msg, Result res) {
    addMessage(msg);
    LOG(ERROR) << msg;
    UIManager::showToast(getName(), msg);
    throw nonlocal_return(res, getTask(), msg);
}

void Step::task_return(Result res) {
    throw nonlocal_return(res, getTask());
}

void Step::task_return(Result res, const char* msg) {
    throw nonlocal_return(res, getTask(), msg);
}

} // namespace ai