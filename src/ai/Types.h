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

void check_interrupted();
void sleep(int milliseconds, bool precise=false);

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
extern NavType SPACE_OUTPOST;
extern NavType SPACE_INSTALLATION;
extern NavType PLANETARY_PORT;
extern NavType PLANETARY_INSTALLATION;
extern NavType ODYSSEY_SETTLEMENT;
extern NavType FLEET_CARRIER;
extern NavType SQUADRON_CARRIER;
extern NavType STATION_MEGASHIP;
extern NavType MEGASHIP;
extern NavType ENGINEER;
extern NavType SIGNAL;
extern NavType WAR_ZONE;
extern NavType RES_SITE;
extern NavType STAR_SYSTEM;
extern NavType ERROR;
extern NavType LOCATION;
extern NavType SHIELD1;
extern NavType SHIELD2;
extern NavType SHIELD3;

extern std::vector<NavType*> ALL_NAV_TYPES;

} // namespace nav


#endif //EDROBOT_AI_TYPES_H
