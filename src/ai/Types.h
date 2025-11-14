//
// Created by mkizub on 21.06.2025.
//

#pragma once

#ifndef EDROBOT_AI_TYPES_H
#define EDROBOT_AI_TYPES_H

namespace ai {

class Step;
class Task;
typedef std::shared_ptr<Step> spStep;
typedef std::shared_ptr<Task> spTask;

struct OrientationRequest {
    int pitch;
    int yaw;
    int roll;


};

enum class FlyState {
    Void, Space, Jump, Cruise, CruiseInt, CruiseJet, Docked, Landed, Depart, BootScreen, DeathScreen
};

enum class ViewMode {
    Norm, Left, Right, Down, Chat, Dock, Station, GalMap, SysMap
};

struct Param {
    enum Type { Bool, Enum, Int, Real, String, System, POI, Dock, Commodity };

    const Type        type;
    const std::string name;
    std::variant<bool,int64_t,double,std::string> value;
    const std::string meta {};
    const bool        optional {false};
};

struct ReqState {
    std::optional<FlyState> flyState;
    std::optional<ViewMode> viewMode;
};

struct TaskTemplate {
    const std::string id;
    std::string name;
    std::vector<Param> params;
    std::vector<TaskTemplate> steps;
    std::function<Task*(const TaskTemplate& templ)> factory;

    bool set(const string& param, bool value);
    bool set(const string& param, int64_t value);
    bool set(const string& param, int32_t value) { return set(param, (int64_t)value); }
    bool set(const string& param, double value);
    bool set(const string& param, float value) { return set(param, (double)value); }
    bool set(const string& param, const string& value);

    bool validate();
};

class nonlocal_return : public std::exception {
public:
    explicit nonlocal_return(bool failed)
        : failed(failed)
        , std::exception()
    {}
    explicit nonlocal_return(bool failed, const char *message)
        : failed(failed)
        , std::exception(message)
    {}
    explicit nonlocal_return(bool failed, const std::string& message)
        : failed(failed)
        , std::exception(message.c_str())
    {}

    const bool failed;
};

class interrupted_error : public std::exception {
public:
    explicit interrupted_error() = default;
};

enum MessageSeverity { MSG_INFO, MSG_WARN, MSG_ERROR, MSG_FATAL };

void check_interrupted();
void sleep(int milliseconds, bool precise=false);

} // namespace ai



#endif //EDROBOT_AI_TYPES_H
