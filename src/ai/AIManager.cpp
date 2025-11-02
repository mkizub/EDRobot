//
// Created by mkizub on 20.06.2025.
//

#include "../pch.h"

#include "AIManager.h"
#include "TaskCalibrate.h"
#include "TradeTasks.h"
#include "TaskDebug.h"
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

static std::thread taskThread;
static std::mutex taskMutex;
static std::condition_variable taskCond;

static std::atomic_bool isWorking;
static std::atomic_bool isLoopWaiting;
static std::atomic_bool isDebugPaused;
static std::atomic_bool isInterrupted;

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


AIManager::AIManager()
    : master(Master::getInstance())
    , cfg(Cfg)
{
    initTemplates();
    isWorking = true;
    taskThread = std::thread(&AIManager::loop, this);
}

AIManager::~AIManager() {
    isWorking = false;
    interrupt();
    taskCond.notify_all();
    taskThread.join();
}

bool AIManager::active() {
    return !isInterrupted && activeTask;
}

void AIManager::stop() {
    if (!activeTask)
        return;
    LOG(INFO) << "AIManager::stop() task " << activeTask->getName();
    UIManager::showToast("EDRobot stop", std::format("Stop task '{}'", activeTask->getName()));
    std::unique_lock<std::mutex> lock(taskMutex);
    isInterrupted = true;
    taskCond.notify_one();
    taskCond.wait_for(lock, std::chrono::milliseconds(1000)/*::max()*/, [this]() {
        return !isWorking || isLoopWaiting;
    });
    spTask oldTask;
    activeTask.swap(oldTask);
    taskCond.notify_one();
}

void AIManager::interrupt() {
    if (isInterrupted || !activeTask)
        return;
    LOG(INFO) << "AIManager::interrupt() task " << activeTask->getName();
    UIManager::showToast("EDRobot paused", std::format("Paused task '{}'", activeTask->getName()));
    std::unique_lock<std::mutex> lock(taskMutex);
    isInterrupted = true;
    taskCond.notify_one();
    taskCond.wait_for(lock, std::chrono::milliseconds(1000)/*::max()*/, [this]() {
        return !isWorking || isLoopWaiting;
    });
}

void AIManager::resume() {
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
    taskCond.wait_for(lock, std::chrono::milliseconds(1000)/*::max()*/, [this]() {
        return !isWorking || !isLoopWaiting;
    });
    master.setGameForeground();
}

bool AIManager::autopilot() {
    if (activeTask) {
        if (dynamic_cast<Autopilot*>(activeTask.get()))
            return true;
        return false;
    }
    auto templ = getTaskTemplate(ED_TASK_AUTOPILOT);
    return new_task(*templ);
}

spTask AIManager::curr_task() {
    return activeTask;
}

bool AIManager::new_task(spTask&& task) {
    if (!task)
        return false;
    LOG(INFO) << "AIManager::new_task()";
    UIManager::showToast("EDRobot task", std::format("Starting task '{}'", task->getName()));
    std::unique_lock<std::mutex> lock(taskMutex);
    isInterrupted = true;
    lastTask.reset();
    activeTask.reset();
    taskCond.notify_one();
    taskCond.wait_for(lock, std::chrono::milliseconds(1000)/*::max()*/, [this]() {
        return !isWorking || isLoopWaiting;
    });
    LOG(INFO) << "AIManager::new_task(): activating " << task->getName();
    activeTask.swap(task);
    isInterrupted = false;
    taskCond.notify_one();
    taskCond.wait_for(lock, std::chrono::milliseconds(1000)/*::max()*/, [this]() {
        return !isWorking || !isLoopWaiting;
    });
    master.setGameForeground();
    return true;
}

bool AIManager::new_task(const TaskTemplate& templ) {
    if (templ.id.empty())
        return false;
    spTask task(templ.factory(nullptr,templ));
    LOG_IF(!task,ERROR) << "Task not known or not implemented: " << templ.id;
    return new_task(std::move(task));
}


void AIManager::loop() {
    SetThreadDescription(GetCurrentThread(), L"AIManager task loop");

    LOG(INFO) << "Starting AIManager task loop";
    while (isWorking) {
        {
            std::unique_lock<std::mutex> lock(taskMutex);
            isLoopWaiting = true;
            // TODO: auto-resume after 30 seconds, need UI check-box and keyboard watchdog
            taskCond.wait_for(lock, std::chrono::milliseconds(30000)/*::max()*/, [this]() {
                return !isWorking || (activeTask && !isInterrupted);
            });
            isLoopWaiting = false;
            if (!isWorking)
                break;
            if (isInterrupted) {
                LOG(INFO) << "AIManager::loop(): steel interrupted";
                continue;
            }
        }
        TRY {
            step();
        } CATCH(const std::exception& ex) {
            kbd::reset_vJoy();
            if (auto ir = dynamic_cast<const interrupted_error*>(&ex)) {
                //mark_interrupted();
            } else {
                LOG(ERROR) << "Exception in ai loop: " << ex.what() << std::endl << GET_EXCEPTION_STACK_TRACE;
                //mark_miss();
            }
        }
    }
    LOG(INFO) << "Exiting AIManager task loop";
}


void AIManager::step() {
    if (!activeTask) {
        LOG(INFO) << "AIManager::loop(): no active task";
        //detectEDState(DetectLevel::Screen);
        return;
    }
    if (activeTask) {
        master.setGameForeground();
        LOG(INFO) << "AIManager::loop(): executing active task: " << activeTask->getName();
        bool ok = activeTask->safe_run();
        if (ok) {
            lastTask.reset();
            lastTask.swap(activeTask);
        }
        LOG(INFO) << "AIManager::loop(): active task: " << (ok ? "passed" : "not complete");
        return;
    }
}

const bool AIManager::detectEDState(DetectLevel level, cv::Mat* colorImage, cv::Mat* grayImage) {
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
    master.pushDetectRequest(std::move(promise), std::move(request));
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
