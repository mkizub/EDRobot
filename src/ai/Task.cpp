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
# ifdef _GLIBCXX_HAVE_STACKTRACE
#  include <stacktrace>
#  define GET_EXCEPTION_STACK_TRACE std::stacktrace::current()
# else
#  define GET_EXCEPTION_STACK_TRACE "(stack trace unavailable)"
# endif
#endif

namespace ai {

Task::Task(Task* parent, AIManager& mgr, const TaskTemplate& templ_)
    : parent(parent)
    , mgr(mgr)
    , templ(templ_)
    , taskName(templ.name)
    , maxMisses(templ.maxMisses)
{
}

Result Task::safe_run() {
    TRY {
        currentSubTask.reset();
        this->result = this->run();
        if (this->result == Result::Trouble)
            this->missCount += 1;
    } CATCH (const std::exception& ex) {
        keyboard::reset_vJoy();
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

Result Task::run_sub_task(spTask& pTask) {
    Task* task  = pTask.get();
    if (!task)
        return Result::Failure;
    currentSubTask = pTask;
    Result res = task->safe_run();
    currentSubTask.reset();
    return res;
}

void Task::check_interrupted() const {
    if (mgr.isInterrupted)
        throw interrupted_error();
}

void Task::sleep(int milliseconds, bool precise) const {
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

bool Task::sendKey(const std::string& name, int delay_ms, int pause_ms, bool precise) const {
    if (!mgr.master.setGameForeground())
        return false;
    if (delay_ms <= 0)
        delay_ms = mgr.cfg.getDefaultKeyHoldTime();
    if (pause_ms <= 0)
        pause_ms = mgr.cfg.getDefaultKeyAfterTime();
    LOG(INFO) << "sendKey('" << name << "'," << delay_ms << "," << pause_ms << ")";
    const KeyBindings& keyBindings = mgr.cfg.getGameKeyBindings(name);
    if (keyBindings.primary.device != GameKey::Void) {
        if (!keyboard::sendKeyDown(keyBindings.primary, delay_ms, pause_ms, nullptr))
            return false;
        sleep(delay_ms + pause_ms, precise);
        return true;
    }
    else if (keyBindings.secondary.device != GameKey::Void) {
        if (!keyboard::sendKeyDown(keyBindings.secondary, delay_ms, pause_ms, nullptr))
            return false;
        sleep(delay_ms + pause_ms, precise);
        return true;
    }
    int code = keyboard::getScanCode(name);
    if (!code)
        return false;
    GameKey tmp {GameKey::Keyboard, name, code};
    if (!keyboard::sendKeyDown(tmp, delay_ms, pause_ms, nullptr))
        return false;
    sleep(delay_ms + pause_ms, precise);
    return true;
}

bool Task::hasAxis(const std::string& name) const {
    return keyboard::getJoyAxis(name);
}

bool Task::sendAxis(const std::string& name, double value) const {
    if (!mgr.master.setGameForeground())
        return false;
    return keyboard::sendJoyAxis(name, value);
}

bool Task::sendAxis(const KeyBindings& bindings, double value) const {
    if (!mgr.master.setGameForeground())
        return false;
    return keyboard::sendJoyAxis(bindings, value);
}

bool Task::sendMouseMove(const cv::Point& point, int pause_ms, bool absolute) const {
    if (!mgr.master.setGameMouseCapture())
        return false;
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
    if (!mgr.master.setGameMouseCapture())
        return false;
    cv::Point screen = mgr.rEnv.cvtReferenceToDesktop(point);
    bool virtualDesktop = (GetSystemMetrics(SM_CMONITORS) > 1);
    //LOG(INFO) << "sendMouseClick recalculated from reference " << point << " to screen " << screen;
    if (!keyboard::sendMouseMoveTo(screen.x, screen.y, true, virtualDesktop))
        return false;
    GameKey tmp {GameKey::Mouse, "", keyboard::MOUSE_L_BUTTON};
    if (!keyboard::sendKeyDown(tmp, delay_ms, pause_ms, nullptr))
        return false;
    sleep(delay_ms + pause_ms);
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

bool Task::executeAction(const std::string& actionName, const json5pp::value& args) {
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
                else {
                    LOG(DEBUG) << "key '" << key.as_string() << "' not bound, action failed";
                    return false;
                }
                unsigned inputId = keyboard::sendKeyDown(*gk, 60000, 0, nullptr);
                ok = executeStep(step.at("hold"), args);
                keyboard::clearInput(inputId);
                if (ok) {
                    int after = get_int(step.at("after"), args, mgr.cfg.getDefaultKeyAfterTime());
                    sleep(after);
                }
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

void Task::addMessage(const char* msg) {
    if (msg)
        addMessage(std::string(msg));
}
void Task::addMessage(const std::string& msg) {
    std::scoped_lock<std::mutex> lock(messagesMutex);
    auto now = std::chrono::steady_clock::now();
    auto expired = now - std::chrono::seconds(5);
    while (!messages.empty() && messages.front().timestamp < expired || messages.size() > 4)
        messages.pop_front();
    messages.emplace_back(now, msg);
}

std::vector<std::string> Task::getMessages() {
    std::scoped_lock<std::mutex> lock(messagesMutex);
    auto expired = std::chrono::steady_clock::now() - std::chrono::seconds(60);
    std::vector<std::string> out;
    for (auto& msg : messages)
        if (msg.timestamp > expired)
            out.push_back(msg.message);
    return out;
}

void Task::notifyProgress(const char* msg) {
    addMessage(msg);
    LOG(INFO) << msg;
    UIManager::showToast(taskName, msg);
}
void Task::notifyProgress(const std::string& msg) {
    addMessage(msg);
    LOG(INFO) << msg;
    UIManager::showToast(taskName, msg);
}
void Task::notifyError(const char* msg, Result res) {
    addMessage(msg);
    LOG(ERROR) << msg;
    UIManager::showToast(taskName, msg);
    throw nonlocal_return(res, this, msg);
}
void Task::notifyError(const std::string& msg, Result res) {
    addMessage(msg);
    LOG(ERROR) << msg;
    UIManager::showToast(taskName, msg);
    throw nonlocal_return(res, this, msg);
}

void Task::task_return(Result res) {
    throw nonlocal_return(res, this);
}

void Task::task_return(Result res, const char* msg) {
    throw nonlocal_return(res, this, msg);
}

} // namespace ai