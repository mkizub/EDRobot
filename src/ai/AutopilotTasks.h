//
// Created by mkizub on 28.06.2025.
//

#ifndef EDROBOT_AUTOPILOTTASKS_H
#define EDROBOT_AUTOPILOTTASKS_H


#include <utility>

#include "Types.h"
#include "Task.h"

namespace gal {
class Item;
class Body;
class Site;
typedef std::shared_ptr<Item> spItem;
typedef std::shared_ptr<Body> spBody;
typedef std::shared_ptr<Site> spSite;
}

namespace ai {

struct NavListEntry {
    wchar_t icon {L'\0'};  // ✦ / ☄ / ✇ / etc.
    bool isTarget {false}; // < Name >
    bool isMarked {false}; // Name∇
    uint8_t indent {0};
    uint8_t portSize {0}; // Name ++
    wchar_t portDanger {L'\0'}; // ◇ / ⬖ / ◆
    std::wstring name;
    dist_t dist;
    const nav::NavType* navType {nullptr};
    gal::spItem item {};
    int ocr_conf {};
    bool focused {};
    bool parsed {};
    int8_t confirmed {};
};

class NavList {
public:

    NavList() = default;
    bool init(Task* task, const gal::spSite& dock, const gal::spBody& body, st::NavPanelFilters filters);
    std::vector<ClassifiedRect*> initNavList(cv::Mat& grayImage);
    std::vector<ClassifiedRect*> recognizeWholePage(cv::Mat& grayImage);
    gal::spItem guessNavItem(NavListEntry &nle);
    bool fixupNavList();

    bool focusDestDock();
    bool focusDestBody();
    bool focusNearestBody();
    bool focusTopEntry();

    bool selectFocused();

    dist_t getFocusedDist();

    NavListEntry* getFocusedEntry() {
        if (focusIdx < 0 || focusIdx >= list.size())
            return nullptr;
        return &list[focusIdx];
    }

    st::NavPanelFilters locationFilters;
    std::vector<NavListEntry> list;

    Task* task;
    gal::spBody destBody;
    gal::spSite destDock;

    int focusIdx {-1};
    bool isDestDockFocused {};
    bool isDestBodyFocused {};
    bool isNearestBodyFocused {};
    bool isTopEntryFocused {};

    bool badBodyHierarchy {};
};


class BaseAutopilotTask : public Task {
public:
    BaseAutopilotTask(Task* parent, AIManager& mgr, const TaskTemplate& templ);
    void relogin();

    bool getFocusedNavRow(NavListEntry& rowInfo);

    bool setSpeed(int percents);
    void orientRollStep(double requiredRoll, int max_time_ms=5000);
    void orientPitchStep(double requiredPitch, int max_time_ms=5000);
    void orientYawStep(double requiredYaw, int max_time_ms=5000);
    bool orientTowardTargetStep(double precision, int max_time_ms=5000);
    bool orientTowardTarget(double precision);
    bool orientAwayFromTargetStep(double precision);
    bool orientAwayFromTarget(double precision);

    gal::spBody destBody;
    gal::spSite destDock;

    int speed_set_to {0};
    dist_t distanceToBody; // distance to the body
    dist_t distanceToDock; // distance to the dock

    NavList nl;
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

class DockStep : public BaseAutopilotStep {
public:
    DockStep(Task* parent) : BaseAutopilotStep(parent) {}
    bool step() final;
    const char* getName() override { return "DockStep"; }

    spGameEvent requestDockingPermit();
    bool flyTowardsTarget();

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

class DockAndBodyDist : public BaseAutopilotStep {
public:
    DockAndBodyDist(Step* parent) : BaseAutopilotStep(parent) {}
    const char* getName() override { return "DockAndBodyDist"; }
    bool step() override;

    std::string getStatus() override;
    enum {
        READY, DIST_DOCK, DIST_BODY,
    } status {READY};
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
    bool orient(int pitchGoal);
    bool fly(int pitchGoal);
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
