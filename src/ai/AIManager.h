//
// Created by mkizub on 20.06.2025.
//

#pragma once

#ifndef EDROBOT_AIMANAGER_H
#define EDROBOT_AIMANAGER_H

#include "Types.h"
#include "TaskTemplate.h"
#include "Task.h"
#include "AutopilotTasks.h"

namespace ai {

class AIManager {
public:

    AIManager();
    ~AIManager();

    bool active();
    void stop();
    void interrupt();
    void resume();
    spTask curr_task();
    bool new_task(spTask&& task);
    bool new_task(const TaskTemplate& templ);

    const std::vector<TaskTemplate*>& getTaskTemplates();
    const TaskTemplate& getTaskTemplate(const std::string& name);

    enum class CheckResult {
        Failure,
        Resume,
        Replan,
    };

    const bool detectEDState(DetectLevel level, cv::Mat* colorImage = nullptr, cv::Mat* grayImage = nullptr);

    void loop();
    void step();

    Master& master;
    Configuration& cfg;

    void initTemplates();

    std::vector<TaskTemplate> AllImplementedTasks;
    std::vector<TaskTemplate*> AllImplementedTaskRefs;
    std::map<std::string,TaskTemplate*> AllImplementedTaskMap;

    spTask activeTask;
    std::vector<spTask> archivedTasks;
    std::chrono::milliseconds nextDelay;
    DetectLevel nextDetectLevel {DetectLevel::None};

    std::thread taskThread;
    std::mutex taskMutex;
    std::condition_variable taskCond;
    std::atomic_bool isWorking;
    std::atomic_bool isInterrupted;
    std::atomic_bool isLoopWaiting;
    ResolvedEnv rEnv;
    UIState uiState;
    CompassInfo compassInfo;
};

}

#endif //EDROBOT_AIMANAGER_H
