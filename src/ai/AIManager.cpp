//
// Created by mkizub on 20.06.2025.
//

#include "../pch.h"

#include "AIManager.h"
#include "TaskCalibrate.h"
#include "TaskSell.h"
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
# define GET_STACK_TRACE std::stacktrace::current().to_string()
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
    LOG(INFO) << "AIManager::stop() task " << activeTask->taskName;
    UIManager::showToast("EDRobot stop", std::format("Stop task '{}'", activeTask->taskName));
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
    LOG(INFO) << "AIManager::interrupt() task " << activeTask->taskName;
    UIManager::showToast("EDRobot paused", std::format("Paused task '{}'", activeTask->taskName));
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
    LOG(INFO) << "AIManager::resume() resuming task " << activeTask->taskName;
    UIManager::showToast("EDRobot resume", std::format("Resuming task '{}'", activeTask->taskName));
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
    return new_task(getTaskTemplate(ED_TASK_AUTOPILOT));
}

spTask AIManager::curr_task() {
    return activeTask;
}

bool AIManager::new_task(spTask&& task) {
    if (!task)
        return false;
    LOG(INFO) << "AIManager::new_task()";
    UIManager::showToast("EDRobot task", std::format("Starting task '{}'", task->taskName));
    std::unique_lock<std::mutex> lock(taskMutex);
    isInterrupted = true;
    spTask oldTask;
    activeTask.swap(oldTask);
    taskCond.notify_one();
    taskCond.wait_for(lock, std::chrono::milliseconds(1000)/*::max()*/, [this]() {
        return !isWorking || isLoopWaiting;
    });
    if (oldTask) {
        LOG(INFO) << "AIManager::new_task(): suspending " << oldTask->taskName;
        archivedTasks.emplace_back(std::move(oldTask));
    }
    LOG(INFO) << "AIManager::new_task(): activating " << task->taskName;
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
    if (templ.name.empty())
        return false;
    spTask task;
    if (templ.name == ED_TASK_CALIBRATE)
        task.reset(new TaskCalibrate(nullptr, *this, templ));
    else if (templ.name == ED_TASK_DEBUG_FIND_ALL_COMMODITIES)
        task.reset(new TaskDebugFindAllCommodities(nullptr, *this, templ));
    else if (templ.name == ED_TASK_DEBUG_FIND_ALL_NAV_POINTS)
        task.reset(new TaskDebugFindAllNavPoints(nullptr, *this, templ));
    else if (templ.name == ED_TASK_MARKET_SELL_ALL)
        task.reset(new TaskSellAll(nullptr, *this, templ));
    else if (templ.name == ED_TASK_MARKET_SELL)
        task.reset(new TaskSell(nullptr, *this, templ));
    else if (templ.name == ED_TASK_AUTOPILOT)
        task.reset(new Autopilot(nullptr, *this, templ));
    else if (templ.name == ED_TASK_TRAVEL)
        task.reset(new TaskTravel(nullptr, *this, templ));
    else if (templ.name == ED_TASK_DEBUG_AUTOPILOT)
        task.reset(new TaskDebugAutopilot(nullptr, *this, templ));
    LOG_IF(!task,ERROR) << "Task not known or not implemented: " << templ.name;
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
            keyboard::reset_vJoy();
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
        switch (activeTask->result) {
        case Result::Failure:
        case Result::Partly:
        case Result::Success:
            archivedTasks.emplace_back(std::move(activeTask));
            return;
        case Result::Trouble:
            if (activeTask->missCount >= activeTask->templ.maxMisses) {
                UIManager::showToast("EDRobot task", std::format("Too many failures ({}), task '{}' aborted",
                                                                 activeTask->missCount, activeTask->taskName));
                activeTask->result = Result::Failure;
                archivedTasks.emplace_back(std::move(activeTask));
                return;
            }
            activeTask->result = Result::Started;
            break;
        case Result::Created:
        case Result::Started:
            break;
        }
    }

    if (activeTask) {
        master.setGameForeground();
        LOG(INFO) << "AIManager::loop(): executing active task: " << activeTask->taskName;
        activeTask->safe_run();
        LOG(INFO) << "AIManager::loop(): active task result: " << enum_name<Result>(activeTask->result);
        UIManager::showToast("EDRobot task", std::format("Active task '{}' result {}", activeTask->taskName, enum_name<Result>(activeTask->result)));
        return;
    }
}

//AIManager::CheckResult AIManager::checkTaskReqMatch(TaskState& ts) {
//    if (!ts.task)
//        return CheckResult::Failure;
//
//    FlyState flyState = edState.flyState;
//    ViewMode viewMode = edState.viewMode;
//
//    if (!ts.started) {
//        if (ts.task->workingState.empty())
//            return CheckResult::Resume;
//        for (auto &st: ts.task->requiredStartStates) {
//            if ((!st.flyState.has_value() || st.flyState.value() == flyState) &&
//                (!st.viewMode.has_value() || st.viewMode.value() == viewMode)) {
//                return CheckResult::Resume;
//            }
//        }
//    } else {
//        if (ts.task->workingState.empty())
//            return CheckResult::Resume;
//        for (auto &st: ts.task->workingState) {
//            if ((!st.flyState.has_value() || st.flyState.value() == flyState) &&
//                (!st.viewMode.has_value() || st.viewMode.value() == viewMode)) {
//                return CheckResult::Resume;
//            }
//        }
//        ts.result = Result::Interrupt;
//    }
//    return CheckResult::Replan;
//}

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
