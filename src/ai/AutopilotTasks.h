//
// Created by mkizub on 28.06.2025.
//

#ifndef EDROBOT_AUTOPILOTTASKS_H
#define EDROBOT_AUTOPILOTTASKS_H


#include <utility>

#include "Types.h"
#include "Task.h"
#include "NavList.h"

namespace gal {
class Item;
class Body;
class Site;
typedef std::shared_ptr<Item> spItem;
typedef std::shared_ptr<Body> spBody;
typedef std::shared_ptr<Site> spSite;
}

namespace ai {

class BaseAutopilotTask : public Task {
public:
    BaseAutopilotTask(Task* parent, AIManager& mgr, const TaskTemplate& templ);
    void relogin();

    bool setSpeed(int percents);
    void orientRollStep(double requiredRoll, int max_time_ms=5000);
    void orientPitchStep(double requiredPitch, int max_time_ms=5000);
    void orientYawStep(double requiredYaw, int max_time_ms=5000);
    bool orientTowardTargetStep(double precision, int max_time_ms=5000);
    bool orientTowardTarget(double precision);
    bool orientAwayFromTargetStep(double precision, int max_time_ms=5000);
    bool orientAwayFromTarget(double precision);

    gal::spBody destBody;
    gal::spSite destDock;

    int speed_set_to {0};
    dist_t distanceToBody; // distance to the body
    dist_t distanceToDock; // distance to the dock

    nav::NavList nl;
};

class BaseAutopilotStep : public Step {
protected:
    BaseAutopilotStep(Step* parent);
    BaseAutopilotTask* task;
    utc_timer timer;
};

class TaskDebugAutopilot : public BaseAutopilotTask {
public:
    TaskDebugAutopilot(Task* parent, AIManager& mgr, const TaskTemplate& templ);
    Result run() final;

    std::string test;
    std::string target;
    double orient_precision {0.5};
};


class DepartureStep : public BaseAutopilotStep {
public:
    explicit DepartureStep(Step* parent);
    bool step() final;
    const char* getName() override { return "Departure"; }

    std::string getStatus() override;
    enum {
        READY, GOING_TO_DOCK, REFUEL, TAKEOFF, WAIT_AUTOPILOT, AUTOPILOT, MASSLOCKED, FLYAWAY, RELOGIN
    } status {READY};
    int notAutoPilotCounter {};
};

class EnterCruiseStep : public BaseAutopilotStep {
public:
    explicit EnterCruiseStep(Step* parent) : BaseAutopilotStep(parent) {}
    bool step() final;
    const char* getName() override { return "EnterCruiseStep"; }

    std::string getStatus() override;
    enum {
        READY, LOCK_BODY, LOCK_TARGET, ORIENT, MASSLOCKED, PREPARE, FSD_COOLDOWN, ENTER_CRUISE,
    } status {READY};
};

class LeaveBodyStep : public BaseAutopilotStep {
public:
    explicit LeaveBodyStep(Step* parent) : BaseAutopilotStep(parent) {}
    bool step() final;
    const char* getName() override { return "LeaveBodyStep"; }

    std::string getStatus() override;
    enum {
        READY, LOCK_BODY, ORIENT, MASSLOCKED, PREPARE, FSD_COOLDOWN, ENTER_CRUISE, LEAVING_BODY
    } status {READY};
};

class DockStep : public BaseAutopilotStep {
public:
    DockStep(Task* parent) : BaseAutopilotStep(parent) {}
    bool step() final;
    const char* getName() override { return "DockStep"; }

    spGameEvent requestDockingPermit();
    bool getDockDistance();
    bool flyTowardsTarget();
    bool flyTowardsStep();

    std::string getStatus() override;
    enum {
        READY, PREPARE, APPROACH, REQUEST, AUTOPILOT
    } status {READY};
};

class NavDockSelect : public BaseAutopilotStep {
public:
    NavDockSelect(Step* parent, gal::spSite dock) : BaseAutopilotStep(parent), dock(dock) {}
    const char* getName() override { return "NavDockSelect"; }
    bool step() override;

    gal::spSite dock;
};

class NavBodySelect : public BaseAutopilotStep {
public:
    NavBodySelect(Step* parent, gal::spBody body) : BaseAutopilotStep(parent), body(body) {}
    const char* getName() override { return "NavBodySelect"; }
    bool step() override;

    gal::spBody body;
};

class CruiseToDistStep : public BaseAutopilotStep {
public:
    CruiseToDistStep(Step* parent, double dist_km)
            : BaseAutopilotStep(parent)
            , requiredDist_km(dist_t::KM,dist_km)
            , currentDist_km(dist_t::X,0)
    {}
    const char* getName() override { return "CruiseToDistStep"; }
    bool step() override;

    std::string getStatus() override;
    enum {
        READY, BAD_ROW, BAD_COMPASS, DIST_FAR, DIST_NEAR, DIST_STOP,
    } status {READY};

    dist_t requiredDist_km;
    dist_t currentDist_km;
};

class DiveUnderPlanetStep : public BaseAutopilotStep {
public:
    DiveUnderPlanetStep(Step* parent)
            : BaseAutopilotStep(parent)
    {}
    const char* getName() override { return "DiveUnderPlanetStep"; }
    bool step() override;
    bool get_dist_body();
    bool get_dist_dock();
    bool orient_roll(float requiredRoll);
    bool orient_pitch(int pitchGoal);
    bool fly_dive(int pitchGoal);

    std::string getStatus() override;
    enum {
        READY, ORIENT_BODY, DIST_BODY, ORIENT_DOCK, DIST_DOCK, ORIENT_DIVE, FLY_DIVE
    } status {READY};
};

class ExitCruiseToStationStep : public BaseAutopilotStep {
public:
    ExitCruiseToStationStep(Step* parent)
            : BaseAutopilotStep(parent)
    {}
    const char* getName() override { return "ExitCruiseToStation"; }
    bool step() override;

    std::string getStatus() override;
    enum {
        READY, ORIENT, APPROACH, EXITING, CONFIRM,
    } status {READY};
    bool use_nav_panel {false};
    int dist_fails {};
    int exit_confirm {};
};

class TaskTravel : public BaseAutopilotTask {
public:
    TaskTravel(Task* parent, AIManager& mgr, const TaskTemplate& templ);
    Result run() final;

    void plan();

    std::string destSystem;
    std::string destDock;
};

class Autopilot : public BaseAutopilotTask {
public:
    Autopilot(Task* parent, AIManager& mgr, const TaskTemplate& templ);
    Result run() final;
};

} // namespace ai

#endif //EDROBOT_AUTOPILOTTASKS_H
