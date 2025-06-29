//
// Created by mkizub on 21.06.2025.
//

#pragma once

#ifndef EDROBOT_TYPES_H
#define EDROBOT_TYPES_H

namespace ai {

class Task;
class AIManager;
typedef std::unique_ptr<Task> upTask;

enum class FlyState {
    Void, Space, Jump, Cruise, CruiseInt, CruiseJet, Docked, Landed, Depart, BootScreen, DeathScreen
};

enum class ViewMode {
    Norm, Left, Right, Down, Chat, Dock, Station, GalMap, SysMap
};

enum class POIType {
    Star, // star in current star system
    AsteroidBelt,
    Planet, // planets and moons
    Port, // ports, settlements
    Station,
    FleetCarrier,
    Place, // interesting places
    Signal, // signal source
    System, // system to jump to
    Ship, // player/NPC ship
    Equipment, // turret, power plant, etc.
    Asteroid,
    Salvage,
};

struct CompassInfo {
    double pitchToTarget; // in degrees, left -180...+180 right
    double yawToTarget;   // in degrees, down -180...+180 up
    double rollToTarget;  // in degrees, counterclockwise -90...+90 clockwise
    bool frontHemisphere;
};

struct EDState {
    FlyState flyState;
    ViewMode viewMode;

    CompassInfo compass;
    const widget::Widget* uiWidget;
    const widget::Widget* uiFocused;
};

enum class Result {
    Created,    // just created, need planning and execution
    Started,    // already started, maybe was interrupted/suspended
    Trouble,    // failed, but may try again (stop if failed many times)
    Failure,    // completely failed
    Partly,     // partial success, i.e. bought something, but not all required, and market is empty now
    Success,    // all is done successfully
};

struct Param {
    enum Type { Bool, Int, Real, String, Star, POI, Dock, Commodity };

    const Type        type;
    const std::string name;
    std::variant<bool, int64_t,double,std::string> value;
};

struct ReqState {
    std::optional<FlyState> flyState;
    std::optional<ViewMode> viewMode;
};

struct TaskTemplate {
    const std::string name;
    std::vector<Param> params;
    std::vector<TaskTemplate> steps;
    std::vector<ReqState> requiredStartStates;
    std::vector<ReqState> workingState;
    std::vector<ReqState> expectedFinalStates;

    int maxMisses;

    bool operator<(const TaskTemplate& other) const {
        return name < other.name;
    }
    bool set(const string& param, bool value);
    bool set(const string& param, int64_t value);
    bool set(const string& param, int32_t value) { return set(param, (int64_t)value); }
    bool set(const string& param, double value);
    bool set(const string& param, float value) { return set(param, (double)value); }
    bool set(const string& param, const string& value);
};

class nonlocal_return : public std::exception {
public:
    explicit nonlocal_return(Result result, const Task * const task)
        : result(result)
        , task(task)
        , std::exception()
    {}
    explicit nonlocal_return(Result result, const Task * const task, const char *arg)
        : result(result)
        , task(task)
        , std::exception(arg)
    {}
    explicit nonlocal_return(Result result, const Task * const task, const std::string& arg)
            : result(result)
            , task(task)
            , std::exception(arg.c_str())
    {}

    const Result result;
    const Task * const task;
};

class interrupted_error : public std::exception {
public:
    explicit interrupted_error() = default;
};



} // namespace ai

#endif //EDROBOT_TYPES_H
