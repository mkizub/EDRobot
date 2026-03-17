//
// Created by mkizub on 20.06.2025.
//

#include "../pch.h"

#include "AIManager.h"
#include "ContactsTasks.h"
#include "../ui/UIManager.h"
#include "../Keyboard.h"

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

std::thread taskThread;
std::mutex taskMutex;
std::condition_variable taskCond;

std::thread turnThread;
std::mutex turnMutex;
std::condition_variable turnCond;

namespace {
    spTask activeTask;
    spTask lastTask;

    std::atomic_bool isWorking;
    std::atomic_bool isLoopWaiting;
    std::atomic_bool isDebugPaused;
    std::atomic_bool isInterrupted;

    InterruptReason interruptReason { InterruptReason::UNKNOWN };
}

void task_loop();
void task_step();

void initTemplates();

ResolvedEnv rEnv;
UIState uiState;
CompassInfo compassInfo;


void check_interrupted() {
    assert (taskThread.get_id() == std::this_thread::get_id() || turnThread.get_id() == std::this_thread::get_id());
    while (isDebugPaused) {
        if (isInterrupted) {
            isDebugPaused = false;
            throw interrupted_error();
        }
        Sleep(250);
    }
    if (isInterrupted)
        throw interrupted_error();
}

void toggleDebugPause() {
    isDebugPaused = !isDebugPaused;
}
bool isDebugPause() {
    return isDebugPaused;
}

bool init() {
    initTemplates();
    isWorking = true;
    taskThread = std::thread(&task_loop);
    return true;
}

bool shutdown() {
    isWorking = false;
    interrupt(InterruptReason::SHUTDOWN);
    taskCond.notify_all();
    if (taskThread.joinable())
        taskThread.join();
    return true;
}

bool active() {
    return !isInterrupted && activeTask;
}

void stop() {
    if (!activeTask)
        return;
    LOG(INFO) << "ai::stop() task " << activeTask->getTitle();
    UIManager::showToast(_gt("EDRobot stop"), lc_format("Stop task '{}'", activeTask->getTitle()));
    std::unique_lock<std::mutex> lock(taskMutex);
    interruptReason = InterruptReason::UNKNOWN;
    isInterrupted = true;
    taskCond.notify_one();
    taskCond.wait_for(lock, std::chrono::milliseconds(1000)/*::max()*/, []() {
        return !isWorking || isLoopWaiting;
    });
    lastTask.reset();
    activeTask.swap(lastTask);
    taskCond.notify_one();
}

void interrupt(InterruptReason reason) {
    interruptReason = reason;
    if (isInterrupted || !activeTask)
        return;
    LOG(INFO) << "AIManager::interrupt() task " << activeTask->getTitle();
    UIManager::showToast(_gt("EDRobot paused"), lc_format("Paused task '{}'", activeTask->getTitle()));
    std::unique_lock<std::mutex> lock(taskMutex);
    isInterrupted = true;
    taskCond.notify_one();
    taskCond.wait_for(lock, std::chrono::milliseconds(1000)/*::max()*/, []() {
        return !isWorking || isLoopWaiting;
    });
}

void resume() {
    isDebugPaused = false;
    if (!activeTask) {
        LOG(INFO) << "AIManager::resume() - no paused task";
        UIManager::showToast(_gt("EDRobot resume"), _gt("No paused task"));
        return;
    }
    LOG(INFO) << "AIManager::resume() resuming task " << activeTask->getTitle();
    UIManager::showToast(_gt("EDRobot resume"), lc_format("Resuming task '{}'", activeTask->getTitle()));
    if (isInterrupted) {
        std::unique_lock<std::mutex> lock(taskMutex);
        isInterrupted = false;
        taskCond.notify_one();
        taskCond.wait_for(lock, std::chrono::milliseconds(1000)/*::max()*/, []() {
            return !isWorking || !isLoopWaiting;
        });
    }
    Mgr.setGameForeground();
}

bool autopilot() {
    if (activeTask) {
        if (dynamic_cast<Autopilot*>(activeTask.get()))
            return true;
        return false;
    }
    auto& templ = getTemplate(ED_TASK_AUTOPILOT);
    return new_task(templ);
}

spTask curr_task() {
    return activeTask;
}

spTask last_task() {
    return lastTask;
}

spStep curr_step() {
    spStep curr = activeTask;
    if (!curr)
        return {};
    while (curr->currSubStep)
        curr = curr->currSubStep;
    return curr;
}

void notify_progress_(MessageSeverity severity, std::string_view msg) {
    if (msg.empty())
        return;
    spStep curr = curr_step();
    if (curr)
        curr->addMessage(severity, msg);
    switch (severity) {
    case MSG_INFO:
        LOG(INFO) << msg;
        break;
    case MSG_WARN:
        LOG(WARNING) << msg;
        break;
    case MSG_ERROR:
        LOG(ERROR) << msg;
        break;
    case MSG_FATAL:
        LOG(ERROR) << msg;
        break;
    }
}

void throw_trouble_(const string_view msg) {
    assert (taskThread.get_id() == std::this_thread::get_id());
    if (!msg.empty()) {
        spStep curr = curr_step();
        if (curr)
            curr->addMessage(MSG_WARN, msg);
        LOG(WARNING) << msg;
    }
    throw nonlocal_return(TaskExitReason::ONGOING, msg);
}
void throw_failed_(const string_view msg) {
    assert (taskThread.get_id() == std::this_thread::get_id());
    if (!msg.empty()) {
        spTask curr = curr_task();
        if (curr) {
            curr->progress = TaskExitReason::FAILED;
            curr->addMessage(MSG_FATAL, msg);
        }
        LOG(ERROR) << msg;
    }
    throw nonlocal_return(TaskExitReason::FAILED, msg);
}
void throw_complete_(const string_view msg) {
    assert (taskThread.get_id() == std::this_thread::get_id());
    if (!msg.empty()) {
        spTask curr = curr_task();
        if (curr) {
            curr->progress = TaskExitReason::COMPLETE;
            curr->addMessage(MSG_FATAL, msg);
        }
        LOG(ERROR) << msg;
    }
    throw nonlocal_return(TaskExitReason::COMPLETE, msg);
}


bool new_task(spTask&& task) {
    if (!task)
        return false;
    LOG(INFO) << "AIManager::new_task()";
    UIManager::showToast(_gt("EDRobot start"), lc_format("Starting task '{}'", task->getTitle()));
    std::unique_lock<std::mutex> lock(taskMutex);
    isInterrupted = true;
    isDebugPaused = false;
    lastTask.reset();
    taskCond.notify_one();
    taskCond.wait_for(lock, std::chrono::milliseconds(1000)/*::max()*/, []() {
        return !isWorking || isLoopWaiting;
    });
    LOG(INFO) << "AIManager::new_task(): activating " << task->getTitle();
    lastTask.swap(activeTask);
    activeTask.swap(task);
    isInterrupted = false;
    taskCond.notify_one();
    taskCond.wait_for(lock, std::chrono::milliseconds(1000)/*::max()*/, []() {
        return !isWorking || !isLoopWaiting;
    });
    Mgr.setGameForeground();
    return true;
}

bool new_task(const TaskTemplate& templ) {
    if (templ.id.empty())
        return false;
    spTask task(templ.factory(templ));
    if (task->parent)
        const_cast<Step*&>(task->parent) = nullptr;
    LOG_IF(!task,ERROR) << "Task not known or not implemented: " << templ.id;
    return new_task(std::move(task));
}


void task_loop() {
    SetThreadDescription(GetCurrentThread(), L"AIManager task loop");

    LOG(INFO) << "Starting ai task loop";
    while (isWorking) {
        {
            std::unique_lock<std::mutex> lock(taskMutex);
            isLoopWaiting = true;
            bool wasActive = active(); // auto-stop capturing
            // TODO: auto-resume after 30 seconds, need UI check-box and keyboard watchdog
            taskCond.wait_for(lock, 30s, []() {
                return !isWorking || (activeTask && !isInterrupted);
            });
            isLoopWaiting = false;
            if (!isWorking)
                break;
            if (!wasActive && !active())
                Mgr.pushCommand(Command::ResetCapturer);
            if (isInterrupted && interruptReason == InterruptReason::DEATH) {
                if (st::isDead && activeTask) {
                    lastTask.reset();
                    activeTask.swap(lastTask);
                    TaskTemplate tt {ED_TASK_RESURRECT, _lc("Resurrect"), [](const TaskTemplate &templ) { return new TaskResurrect(templ); }};
                    spTask task(new TaskResurrect(tt));
                    activeTask.swap(task);
                    isInterrupted = false;
                }
                interruptReason = InterruptReason::UNKNOWN;
            }
            if (isInterrupted) {
                LOG(INFO) << "ai::task_loop(): steel interrupted";
                continue;
            }
        }
        st::autopilot = {};
        if (activeTask) {
            task_step();
        } else {
            LOG(INFO) << "ai::task_loop(): no active task";
        }
    }
    LOG(INFO) << "Exiting ai task loop";
}


void task_step() {
    Mgr.setGameForeground();
    disableAutoTurn();
    LOG(INFO) << "ai::task_loop(): executing active task: " << activeTask->getTitle();
    bool ok = false;
    TaskExitReason reason = TaskExitReason::ONGOING;
    TRY {
        activeTask->prevSubStep.reset();
        activeTask->currSubStep.reset();
        ok = activeTask->run();
    } CATCH (const std::exception& ex) {
        kbd::reset_vJoy();
        if (auto nlr = dynamic_cast<const nonlocal_return*>(&ex))
            activeTask->progress = reason = nlr->reason;
        else if (dynamic_cast<const interrupted_error*>(&ex))
            ;
        else
            LOG(ERROR) << "Exception during task execution: " << ex.what() << std::endl << GET_EXCEPTION_STACK_TRACE;
    }
    if (ok || reason > TaskExitReason::ONGOING) {
        if (ok && activeTask && activeTask->templ.id == ED_TASK_RESURRECT) {
            lastTask.swap(activeTask);
        } else {
            lastTask.reset();
            lastTask.swap(activeTask);
        }
    }
    LOG(INFO) << "ai::task_loop(): active task: " << (
            isInterrupted ? "interrupted" :
            reason == TaskExitReason::FAILED ? "failed" :
            ok || reason == TaskExitReason::COMPLETE ? "passed" :
            "not complete");
    kbd::reset_vJoy();
    disableAutoTurn();
    st::autopilot = {};
}

bool detectEDStateReq(DetectRequest&& req) {
    check_interrupted();
#ifdef NDEBUG
    std::chrono::milliseconds timeout = 1000ms;
#else
    std::chrono::milliseconds timeout = 3000ms;
#endif
    auto now = std::chrono::system_clock::now();
    auto until = now + timeout;
    uiState.valid = false;
    std::promise<bool> promise;
    std::future<bool> future = promise.get_future();

    Mgr.pushDetectRequest(std::move(promise), std::move(req));
    while (now < until) {
        check_interrupted();
        auto left = std::chrono::duration_cast<std::chrono::milliseconds>(until - now);
        if (left.count() < 5)
            break;
        auto status = future.wait_for(timeout);
        if (status == std::future_status::ready)
            break;
        now = std::chrono::system_clock::now();
    }
    if (!future.valid())
        return false;
    bool ok = future.get();
    return ok && uiState.valid;
}

bool detectEDState(DetectLevel level) {
    return detectEDStateReq({level, &ai::uiState, &ai::rEnv, &compassInfo});
}

bool detectEDStateGrayIm(DetectLevel level, cv::Mat& grayImage) {
    cv::Mat colorImage;
    if (!detectEDStateReq({level, &ai::uiState, &ai::rEnv, &compassInfo, &colorImage}))
        return false;
    cv::cvtColor(colorImage, grayImage, cv::COLOR_BGR2GRAY);
    return true;
}

} // namespace ai
