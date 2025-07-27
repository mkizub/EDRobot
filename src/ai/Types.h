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
    None,
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
    explicit nonlocal_return(Result result, Task* task)
        : result(result)
        , task(task)
        , std::exception()
    {}
    explicit nonlocal_return(Result result, Task* task, const char *arg)
        : result(result)
        , task(task)
        , std::exception(arg)
    {}
    explicit nonlocal_return(Result result, Task* task, const std::string& arg)
            : result(result)
            , task(task)
            , std::exception(arg.c_str())
    {}

    const Result result;
    Task * const task;
};

class interrupted_error : public std::exception {
public:
    explicit interrupted_error() = default;
};

} // namespace ai


namespace nav {

struct NavType {
    wchar_t charOCR;
    ai::POIType poiType;
    std::vector<std::string> navIcons;
    std::vector<std::string> typeAliases;
};

extern NavType STAR;
extern NavType BEACON;
extern NavType TOURIST_BEACON;
extern NavType BODY;
extern NavType LAND;
extern NavType BELT;
extern NavType ORBIS;
extern NavType CORIOLIS;
extern NavType MINER_BASE;
extern NavType OUTPOST;
extern NavType INSTALLATION;
extern NavType PORT;
extern NavType FACTORY;
extern NavType SETTLEMENT;
extern NavType CARRIER;
extern NavType STATION_MEGASHIP;
extern NavType MEGASHIP;
extern NavType ENGINEER;
extern NavType SIGNAL;
extern NavType WAR_ZONE;
extern NavType RES_SITE;
extern NavType SYSTEM;
extern NavType LOCATION;
extern NavType SHIELD1;
extern NavType SHIELD2;
extern NavType SHIELD3;

extern std::vector<NavType*> ALL_NAV_TYPES;

} // namespace nav


#endif //EDROBOT_TYPES_H
