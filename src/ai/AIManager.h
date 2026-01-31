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
bool init_ship_tracker();
bool shutdown();
bool shutdown_ship_tracker();
bool active();
void stop();
void interrupt(InterruptReason reason);
void resume();
bool autopilot();
spTask curr_task();
spTask last_task();
spStep curr_step();
bool new_task(spTask&& task);
bool new_task(const TaskTemplate& templ);

void reportCompassDetect(CompassInfo& compass);
void resetCompassDetects();

void notify_progress_(MessageSeverity severity, const std::string_view msg);
[[noreturn]] void throw_trouble_(const std::string_view msg);
[[noreturn]] void throw_failed_(const std::string_view msg);

inline void notify_info(const std::string_view msg) {
    notify_progress_(MSG_INFO, gettext(msg.data()));
}
inline void notify_warn(const std::string_view msg) {
    notify_progress_(MSG_WARN, gettext(msg.data()));
}
inline void notify_error(const std::string_view msg) {
    notify_progress_(MSG_ERROR, gettext(msg.data()));
}
template <class... _Types>
void notify_info(const std::format_string<_Types...> _Fmt, _Types&&... _Args) {
    auto lc_fmt = gettext(_Fmt.get().data());
    notify_progress_(MSG_INFO, std::vformat(lc_fmt, std::make_format_args(_Args...)).c_str());
}
template <class... _Types>
void notify_warn(const std::format_string<_Types...> _Fmt, _Types&&... _Args) {
    auto lc_fmt = gettext(_Fmt.get().data());
    notify_progress_(MSG_WARN, std::vformat(lc_fmt, std::make_format_args(_Args...)).c_str());
}
template <class... _Types>
void notify_error(const std::format_string<_Types...> _Fmt, _Types&&... _Args) {
    auto lc_fmt = gettext(_Fmt.get().data());
    notify_progress_(MSG_ERROR, std::vformat(lc_fmt, std::make_format_args(_Args...)).c_str());
}

[[noreturn]] inline void throw_trouble(const std::string_view msg) {
    throw_trouble_(gettext(msg.data()));
}
template <class... _Types>
[[noreturn]] void throw_trouble(const std::format_string<_Types...> _Fmt, _Types&&... _Args) {
    auto lc_fmt = gettext(_Fmt.get().data());
    throw_trouble_(std::vformat(lc_fmt, std::make_format_args(_Args...)).c_str());
}
[[noreturn]] inline void throw_failed(const std::string_view msg) {
    throw_failed_(gettext(msg.data()));
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

bool detectEDState(DetectLevel level);
bool detectEDStateGrayIm(DetectLevel level, cv::Mat& grayImage);

bool gotoNavPage(const std::string &page_name, bool required=true);
void rollBlindCompass();


extern ResolvedEnv rEnv;
extern UIState uiState;
extern CompassInfo compassInfo;

}

#endif //EDROBOT_AIMANAGER_H
