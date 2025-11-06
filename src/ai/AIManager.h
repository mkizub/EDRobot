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

bool init();
bool shutdown();
bool active();
void stop();
void interrupt();
void resume();
bool autopilot();
spTask curr_task();
spTask last_task();
spStep curr_step();
bool new_task(spTask&& task);
bool new_task(const TaskTemplate& templ);

[[noreturn]] void throw_trouble(const char* msg);
[[noreturn]] void throw_trouble(const std::string& msg);
[[noreturn]] void throw_failed(const char* msg);
[[noreturn]] void throw_failed(const std::string& msg);

const std::list<TaskTemplate>& getUserTasks();
const std::list<TaskTemplate>& getTemplates();
const TaskTemplate* getTaskTemplate(const std::string& name);

enum class CheckResult {
    Failure,
    Resume,
    Replan,
};

const bool detectEDState(DetectLevel level, cv::Mat* colorImage = nullptr, cv::Mat* grayImage = nullptr);

bool gotoNavPage(const std::string &page_name, bool required=true);
void rollBlindCompass();


extern ResolvedEnv rEnv;
extern UIState uiState;
extern CompassInfo compassInfo;

}

#endif //EDROBOT_AIMANAGER_H
