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

void notify_progress_(MessageSeverity severity, const char* msg);
[[noreturn]] void throw_trouble_(const char* msg);
[[noreturn]] void throw_failed_(const char* msg);

inline void notify_progress(MessageSeverity severity, const char* msg) {
    notify_progress_(severity, gettext(msg));
}
inline void notify_progress(MessageSeverity severity, const std::string& msg) {
    notify_progress_(severity, gettext(msg.c_str()));
}
template <class... _Types>
void notify_progress(MessageSeverity severity, const std::format_string<_Types...> _Fmt, _Types&&... _Args) {
    auto lc_fmt = gettext(_Fmt.get().data());
    notify_progress_(severity, std::vformat(lc_fmt, std::make_format_args(_Args...)).c_str());
}

[[noreturn]] inline void throw_trouble(const std::string& msg) {
    throw_trouble_(gettext(msg.c_str()));
}
[[noreturn]] inline void throw_trouble(const char* msg) {
    throw_trouble_(gettext(msg));
}
template <class... _Types>
[[noreturn]] void throw_trouble(const std::format_string<_Types...> _Fmt, _Types&&... _Args) {
    auto lc_fmt = gettext(_Fmt.get().data());
    throw_trouble_(std::vformat(lc_fmt, std::make_format_args(_Args...)).c_str());
}
[[noreturn]] inline void throw_failed(const std::string& msg) {
    throw_failed_(gettext(msg.c_str()));
}
[[noreturn]] inline void throw_failed(const char* msg) {
    throw_failed_(gettext(msg));
}
template <class... _Types>
[[noreturn]] void throw_failed(const std::format_string<_Types...> _Fmt, _Types&&... _Args) {
    auto lc_fmt = gettext(_Fmt.get().data());
    throw_failed_(std::vformat(lc_fmt, std::make_format_args(_Args...)).c_str());
}

const std::list<TaskTemplate>& getUserTasks();
const std::list<TaskTemplate>& getTemplates();
const TaskTemplate& getTemplate(const std::string& id);
bool saveUserTask(TaskTemplate& templ);
bool delUserTask(TaskTemplate& templ);
bool delUserTask(int index);

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
