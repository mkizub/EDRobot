//
// Created by mkizub on 28.06.2025.
//

#pragma once

#ifndef EDROBOT_AUTOPILOTTASKS_H
#define EDROBOT_AUTOPILOTTASKS_H


#include <utility>
#include <span>

#include "Types.h"
#include "Task.h"
#include "NavList.h"

namespace ai {

bool setSpeed(int percents, bool force, const char* reason);
void disableAutoTurn();
int getNavRoutePosition(const std::shared_ptr<NavRoute>& navRoute);

struct CourseLocker {
    explicit CourseLocker(double pitch, bool without_roll=false);
    ~CourseLocker();

    void requestPitchRoll(double pitch, bool without_roll=false);
};

struct CourseLockerPause {
    CourseLockerPause();
    ~CourseLockerPause();
    const bool wasActive;
};

struct ExpectSceeenLocker {
    explicit ExpectSceeenLocker(const char* scr) {
        prev_screen = st::autopilot.expect_screen;
        st::autopilot.expect_screen = scr;
    }
    ~ExpectSceeenLocker() {
        st::autopilot.expect_screen = prev_screen;
    }
    std::string prev_screen;
};

class BaseAutopilotTask : public Task {
protected:
    explicit BaseAutopilotTask(const TaskTemplate& templ_) : Task(templ_) {}

public:
    void relogin();

    void orientRollStep(double delta, int max_time_ms=5000);
    void orientPitchStep(double delta, int max_time_ms=5000);
    void orientYawStep(double delta, int max_time_ms=5000);
    bool orientTowardTargetStep(double precision, int max_time_ms=5000);
    bool orientTowardTarget(double precision);
    bool orientAwayFromTargetStep(double precision, int max_time_ms=5000);
    bool orientAwayFromTarget(double precision);
    bool orientRollByTarget(double roll, double precision, int max_time_ms=5000);
    void initNavFilter();

    NavList nl;
};

class BaseAutopilotStep : public Step {
protected:
    BaseAutopilotStep();
    BaseAutopilotTask* task;
    utc_timer timer;
};

class TaskDebugAutopilot : public BaseAutopilotTask {
public:
    explicit TaskDebugAutopilot(const TaskTemplate& templ);
    bool run() final;

    std::string test;
    std::string target;
};

class TaskDebugShipStats : public BaseAutopilotTask {
public:
    explicit TaskDebugShipStats(const TaskTemplate& templ);
    bool run() final;

    bool accelForward();
    bool accelReverse();
    bool forwardDist();
    bool rotateAxis(Axis& axis);
    bool rotateCurve(Axis& axis);
    bool rotateCurveTest(Axis& axis, const std::vector<float>& speedPercents, std::vector<float>& rotScaleTable);

    std::string test;
    std::optional<double> value;
    std::optional<double> duration;
    std::optional<int> speed;
};


class DepartureStep : public BaseAutopilotStep {
public:
    explicit DepartureStep() = default;
    bool run() final;

    std::string getTitle() override;
    std::string getStatus() override;
    enum {
        READY, GOING_TO_DOCK, REFUEL, TAKEOFF, WAIT_AUTOPILOT, AUTOPILOT, ORIENT_AWAY, LEAVE_DEPOT, MASSLOCKED, FLYAWAY, RELOGIN, DONE
    } status {READY};

    gal::spEntity fromDock;
    std::string fromDockName;
    int notAutoPilotCounter {};
    float pitchAfterAutopilot {};
};

class EnterCruiseStep : public BaseAutopilotStep {
public:
    explicit EnterCruiseStep(bool enterSimple=false) : enterSimple(enterSimple) {}
    bool run() final;

    std::string getTitle() override;
    std::string getStatus() override;
    enum {
        READY, LOCK_BODY, LOCK_TARGET, ORIENT, MASSLOCKED, PREPARE, FSD_COOLDOWN, ENTER_CRUISE, FLY_AWAY, DONE
    } status {READY};

    bool enterSimple;
};

class HyperJumpStep : public BaseAutopilotStep {
public:
    explicit HyperJumpStep() = default;
    bool run() final;

    std::string getTitle() override;
    std::string getStatus() override;
    enum {
        READY, MASSLOCKED, CHARGE, HYPERSPACE, AVOID_STAR, FLY_AWAY, DONE
    } status {READY};

    std::string destSystem;
};

class LeaveBodyStep : public BaseAutopilotStep {
public:
    explicit LeaveBodyStep() = default;
    bool run() final;

    std::string getTitle() override;
    std::string getStatus() override;
    enum {
        READY, LOCK_BODY, ORIENT, MASSLOCKED, PREPARE, FSD_COOLDOWN, ENTER_CRUISE, LEAVING_BODY, FLY_AWAY, DONE
    } status {READY};

    std::string fromBody;
};

class BaseDockStep : public BaseAutopilotStep {
public:
    BaseDockStep() = default;

    bool canDock();
    spGameEvent requestDockingPermit();
    bool autopilot();
    void flyTowardsStep();
    void sleep_waiting_dist(int period);

    std::string getTitle() override;
    std::string getStatus() override;
    enum {
        READY, PREPARE, APPROACH, REQUEST, WAITING, AUTOPILOT, REFUEL, DONE
    } status {READY};

    std::string toDock;
    std::string lastDockingStatus;

    const dist_t dock_req_dist = 7500_m;
    dist_t safe_dist = dock_req_dist - 200_m;
};

class DockSpaceStation : public BaseDockStep {
public:
    DockSpaceStation() = default;
    bool run() final;

    void updateSafeDist();
    bool getDockDistance();
    void flyTowardsTarget();
};

class DockPlanetPort : public BaseDockStep {
public:
    DockPlanetPort() = default;
    bool run() final;

    dist_t getDockDistance(bool force);
    bool normalizeOrientation();
    bool flyTowardsTarget(dist_t dist);
    bool flyAlongSurface();

    bool surface_aligned = false;
};

class NavDockSelect : public BaseAutopilotStep {
public:
    explicit NavDockSelect(gal::spEntity dock={}) : dock(std::move(dock)) {}
    bool run() override;
    std::string getTitle() override;

    enum {
        READY, SELECTING, FAILED, DONE
    } status {READY};
    gal::spEntity dock;
};

class NavBodySelect : public BaseAutopilotStep {
public:
    explicit NavBodySelect(gal::spEntity body={}, bool dockBody=false) : body(std::move(body)), checkDockBody(dockBody) {}
    std::string getTitle() override;
    bool run() override;

    enum {
        READY, SELECTING, FAILED, DONE
    } status {READY};
    gal::spEntity body;
    bool checkDockBody;
};

class BaseCruiseStep : public BaseAutopilotStep {
public:
    BaseCruiseStep() = default;

    bool gotDistance(dist_t dist);

    std::string getTitle() override;
    std::string getStatus() override;
    enum {
        READY, DIST_BAD, DIST_HUGE, DIST_FAR, DIST_CLOSE, DIST_NEAR, DIST_STOP, DONE
    } status {READY};

    std::string destName;
    dist_t requiredDist;
    dist_t currentDist;
    bool useNavList {};
    bool flyAway {};
    int failCount {};
};


class CruiseToSignal : public BaseCruiseStep {
public:
    explicit CruiseToSignal(dist_t req_dist) {
        requiredDist = req_dist;
    }
    bool run() override;
};

class CruiseToDistStep : public BaseCruiseStep {
public:
    explicit CruiseToDistStep(dist_t min_dist, dist_t max_dist)
            : minDist(min_dist)
            , maxDist(max_dist)
    {}
    bool run() override;

    dist_t minDist;
    dist_t maxDist;
};

class DiveUnderPlanetStep : public BaseAutopilotStep {
public:
    DiveUnderPlanetStep() = default;
    bool run() override;
    bool orient_pitch(float pitchGoal);
    bool fly_dive(float pitchGoal);

    bool toPort {};
    int keepCruisePitch {};

    std::string getTitle() override;
    std::string getStatus() override;
    enum {
        READY, ORIENT_BODY, DIST_BODY, ORIENT_DOCK, DIST_DOCK, ORIENT_DIVE, FLY_DIVE, DONE
    } status {READY};
};

class ExitCruiseToSpace : public BaseAutopilotStep {
public:
    explicit ExitCruiseToSpace(int pitch=0) : keepPitch(pitch) {}
    bool run() override;

    std::string getTitle() override;
    std::string getStatus() override;
    enum {
        READY, ORIENT, APPROACH, EXITING, CONFIRM, DONE,
    } status {READY};
    int dist_fails {};
    int exit_confirm {};
    int keepPitch;
};

class ExitCruiseToPlanet : public BaseAutopilotStep {
public:
    explicit ExitCruiseToPlanet(int pitch=0) : keepPitch(pitch) {}
    bool run() override;

    std::string getTitle() override;
    std::string getStatus() override;
    enum {
        READY, ORIENT, FLY_TO_BODY, APPROACH, EXITING, CONFIRM, DONE,
    } status {READY};
    int keepPitch;
};

class CompleteNavRoute : public BaseAutopilotStep {
public:
    CompleteNavRoute() = default;
    bool run() override;

    void targetNextNavRoute(int routeIdx);

    std::string getTitle() override;
    std::string getStatus() override;
    enum {
        READY, ORIENT, ENTER_CRUISE, LEAVE_BODY, FLY_AWAY, JUMP, DONE
    } status {READY};

    std::shared_ptr<NavRoute> lastNavRoute;
};

class CruiseAndDock : public BaseAutopilotStep {
public:
    CruiseAndDock() = default;
    bool run() override;

    std::string getTitle() override;
    std::string getStatus() override;
    enum {
        READY, DEPARTURE, ENTER_CRUISE, LEAVE_BODY, APPROACH, DIVE, LEAVE_CRUISE, DOCK, DONE
    } status {READY};
};

class TaskTravel : public BaseAutopilotTask {
public:
    explicit TaskTravel(const TaskTemplate& templ);
    std::string getTitle() override;
    bool run() final;

    std::string destSystemName;
    std::string destDockName;
};

class Autopilot : public BaseAutopilotTask {
public:
    explicit Autopilot(const TaskTemplate& templ);
    std::string getTitle() override;
    bool run() final;

    std::string destName;
};

} // namespace ai

#endif //EDROBOT_AUTOPILOTTASKS_H
