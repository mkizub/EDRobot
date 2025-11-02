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
    bool autopilot();
    spTask curr_task();
    bool new_task(spTask&& task);
    bool new_task(const TaskTemplate& templ);

    const std::list<TaskTemplate>& getUserTasks();
    const std::list<TaskTemplate>& getTemplates();
    const TaskTemplate* getTaskTemplate(const std::string& name);

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
    TaskTemplate loadTemplate(const json5pp::value& j_task);
    void loadSavedTasks();

    std::list<TaskTemplate> AllTasks;
    std::list<TaskTemplate> AllTaskTemplates;
    std::map<std::string,TaskTemplate*> TaskTemplateMap;

    spTask activeTask;
    spTask lastTask;
    std::chrono::milliseconds nextDelay;
    DetectLevel nextDetectLevel {DetectLevel::None};

    ResolvedEnv rEnv;
    UIState uiState;
    CompassInfo compassInfo;
};

}

#endif //EDROBOT_AIMANAGER_H
