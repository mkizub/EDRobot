//
// Created by mkizub on 20.06.2025.
//

#include "../pch.h"

#include "AIManager.h"
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

using namespace std::chrono_literals;

namespace ai {

namespace {
    spTask activeTask;
    spTask lastTask;
    std::chrono::milliseconds nextDelay;
    DetectLevel nextDetectLevel{DetectLevel::None};

    std::thread taskThread;
    std::mutex taskMutex;
    std::condition_variable taskCond;

    std::atomic_bool isWorking;
    std::atomic_bool isLoopWaiting;
    std::atomic_bool isDebugPaused;
    std::atomic_bool isInterrupted;
}

void task_loop();
void task_step();

void initTemplates();

ResolvedEnv rEnv;
UIState uiState;
CompassInfo compassInfo;


void check_interrupted() {
    assert (taskThread.get_id() == std::this_thread::get_id());
    while (isDebugPaused) {
        if (isInterrupted) {
            isDebugPaused = false;
            throw interrupted_error();
        }
        Sleep(250);
        continue;
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

extern void init_ship_tracker();
extern void shutdown_ship_tracker();
bool init() {
    initTemplates();
    isWorking = true;
    taskThread = std::thread(&task_loop);
    init_ship_tracker();
    return true;
}

bool shutdown() {
    isWorking = false;
    interrupt();
    shutdown_ship_tracker();
    taskCond.notify_all();
    taskThread.join();
    return true;
}

bool active() {
    return !isInterrupted && activeTask;
}

void stop() {
    if (!activeTask)
        return;
    LOG(INFO) << "ai::stop() task " << activeTask->getName();
    UIManager::showToast("EDRobot stop", std::format("Stop task '{}'", activeTask->getName()));
    std::unique_lock<std::mutex> lock(taskMutex);
    isInterrupted = true;
    taskCond.notify_one();
    taskCond.wait_for(lock, std::chrono::milliseconds(1000)/*::max()*/, []() {
        return !isWorking || isLoopWaiting;
    });
    spTask oldTask;
    activeTask.swap(oldTask);
    taskCond.notify_one();
}

void interrupt() {
    if (isInterrupted || !activeTask)
        return;
    LOG(INFO) << "AIManager::interrupt() task " << activeTask->getName();
    UIManager::showToast("EDRobot paused", std::format("Paused task '{}'", activeTask->getName()));
    std::unique_lock<std::mutex> lock(taskMutex);
    isInterrupted = true;
    taskCond.notify_one();
    taskCond.wait_for(lock, std::chrono::milliseconds(1000)/*::max()*/, []() {
        return !isWorking || isLoopWaiting;
    });
}

void resume() {
    if (!isInterrupted) {
        LOG(INFO) << "AIManager::resume() - not interrupted";
        return;
    }
    if (!activeTask) {
        LOG(INFO) << "AIManager::resume() - no paused task";
        UIManager::showToast("EDRobot resume", "No paused task");
        return;
    }
    LOG(INFO) << "AIManager::resume() resuming task " << activeTask->getName();
    UIManager::showToast("EDRobot resume", std::format("Resuming task '{}'", activeTask->getName()));
    std::unique_lock<std::mutex> lock(taskMutex);
    isInterrupted = false;
    taskCond.notify_one();
    taskCond.wait_for(lock, std::chrono::milliseconds(1000)/*::max()*/, []() {
        return !isWorking || !isLoopWaiting;
    });
    Mgr.setGameForeground();
}

bool autopilot() {
    if (activeTask) {
        if (dynamic_cast<Autopilot*>(activeTask.get()))
            return true;
        return false;
    }
    auto templ = getTaskTemplate(ED_TASK_AUTOPILOT);
    return new_task(*templ);
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
    while (curr->currentSubStep)
        curr = curr->currentSubStep;
    return curr;
}

void throw_trouble(const char* msg) {
    spStep curr = curr_step();
    if (curr)
        curr->throw_trouble(msg);
}
void throw_trouble(const std::string& msg) {
    spStep curr = curr_step();
    if (curr)
        curr->throw_trouble(msg);
}
void throw_failed(const char* msg) {
    spStep curr = curr_step();
    if (curr)
        curr->throw_failed(msg);
}
void throw_failed(const std::string& msg) {
    spStep curr = curr_step();
    if (curr)
        curr->throw_failed(msg);
}


bool new_task(spTask&& task) {
    if (!task)
        return false;
    LOG(INFO) << "AIManager::new_task()";
    UIManager::showToast("EDRobot task", std::format("Starting task '{}'", task->getName()));
    std::unique_lock<std::mutex> lock(taskMutex);
    isInterrupted = true;
    lastTask.reset();
    activeTask.reset();
    taskCond.notify_one();
    taskCond.wait_for(lock, std::chrono::milliseconds(1000)/*::max()*/, []() {
        return !isWorking || isLoopWaiting;
    });
    LOG(INFO) << "AIManager::new_task(): activating " << task->getName();
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
    SetThreadDescription(GetCurrentThread(), L"AIManager task task_loop");

    LOG(INFO) << "Starting ai task task_loop";
    while (isWorking) {
        {
            std::unique_lock<std::mutex> lock(taskMutex);
            isLoopWaiting = true;
            // TODO: auto-resume after 30 seconds, need UI check-box and keyboard watchdog
            taskCond.wait_for(lock, 30s, []() {
                return !isWorking || (activeTask && !isInterrupted);
            });
            isLoopWaiting = false;
            if (!isWorking)
                break;
            if (isInterrupted) {
                LOG(INFO) << "ai::task_loop(): steel interrupted";
                continue;
            }
        }
        TRY {
            task_step();
        } CATCH(const std::exception& ex) {
            if (auto ir = dynamic_cast<const interrupted_error*>(&ex)) {
                //mark_interrupted();
            } else {
                LOG(ERROR) << "Exception in ai task_loop: " << ex.what() << std::endl << GET_EXCEPTION_STACK_TRACE;
                //mark_miss();
            }
        }
        kbd::reset_vJoy();
        disableAutoTurn();
        st::autopilot = {};
    }
    LOG(INFO) << "Exiting ai task task_loop";
}


void task_step() {
    if (!activeTask) {
        LOG(INFO) << "ai::task_loop(): no active task";
        //detectEDState(DetectLevel::Screen);
        return;
    }
    if (activeTask) {
        Mgr.setGameForeground();
        LOG(INFO) << "ai::task_loop(): executing active task: " << activeTask->getName();
        st::autopilot = {};
        disableAutoTurn();
        bool ok = activeTask->safe_run();
        if (ok) {
            lastTask.reset();
            lastTask.swap(activeTask);
        }
        LOG(INFO) << "ai::task_loop(): active task: " << (ok ? "passed" : "not complete");
        return;
    }
}

const bool detectEDState(DetectLevel level, cv::Mat* colorImage, cv::Mat* grayImage) {
    check_interrupted();
#ifdef NDEBUG
    std::chrono::milliseconds timeout = 2000ms;
#else
    std::chrono::milliseconds timeout = 5000ms;
#endif
    auto now = std::chrono::system_clock::now();
    auto until = now + timeout;
    uiState.valid = false;
    DetectRequest request { level, &uiState, &rEnv, &compassInfo, colorImage, grayImage };
    std::promise<bool> promise;
    std::future<bool> future = promise.get_future();
    Mgr.pushDetectRequest(std::move(promise), std::move(request));
    while (now < until) {
        check_interrupted();
        auto left = std::chrono::duration_cast<std::chrono::milliseconds>(until - now);
        if (left.count() < 5)
            break;
        auto duration = std::min(std::chrono::milliseconds(250), left);
        auto status = future.wait_for(duration);
        std::this_thread::sleep_for(duration);
        if (status == std::future_status::ready)
            break;
        now = std::chrono::system_clock::now();
    }
    if (!future.valid())
        return false;
    bool ok = future.get();
    return ok && uiState.valid;
}

} // namespace ai
