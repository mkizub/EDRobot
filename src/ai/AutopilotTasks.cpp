//
// Created by mkizub on 28.06.2025.
//

#include "../pch.h"

#include "Task.h"
#include "AIManager.h"
#include "AutopilotTasks.h"
#include "../Galaxy.h"
#include "../Keyboard.h"
#include "../ShipStats.h"
#include "../EDWidget.h"

namespace ai {

// TODO: departure from planet - orient away before mass-locked
// TODO: complete nav route - leave body first
// TODO: autopilot to planet port - dive, check direct visibility before going far from planet
// TODO: autopilot to planet port - orient roll to planet center before flying to port
// TODO: restart frame capturing if many fails

static enum BlindMode {
    ROLL_BLIND_ROLL,
    ROLL_BLIND_NONE,
} blindMode {ROLL_BLIND_ROLL};
struct BlindLock {
    BlindLock(BlindMode mode) : saved(blindMode) {
        blindMode = mode;
    }
    ~BlindLock() {
        blindMode = saved;
    }
    BlindMode saved;
};

void rollBlindCompass() {
    if (blindMode != ROLL_BLIND_ROLL) {
        notify_progress(MSG_WARN, "Glare, but rolling disabled");
        return;
    }
    notify_progress(MSG_INFO, "Anti-glare rolling");
    const KeyBindings& bind = Cfg.getGameKeyBindings("RollAxisRaw");
    if ((bind.mode == KeyBindings::Axis || bind.mode == KeyBindings::AxisInv) && bind.primary.device == GameKey::vJoy) {
        kbd::axis(bind, 1);
        ai::sleep(1000);
        kbd::axis(bind, 0);
        ai::sleep(1000);
    } else {
        kbd::send("RollRightButton", 1000, 1000);
    }
}

int getNavPageIndex(const std::string &page_name) {
    int pageIndex = -1;
    if (page_name == "mod-sysinfo")
        pageIndex = 0;
    else if (page_name == "mod-nav-list")
        pageIndex = 1;
    else if (page_name == "mod-transact")
        pageIndex = 2;
    else if (page_name == "mod-contacts")
        pageIndex = 3;
    else if (page_name == "mod-target")
        pageIndex = 4;
    else
        LOG(ERROR) << "Nav page '" << page_name << "' not known";
    return pageIndex;
}

bool gotoNavPage(const std::string &page_name, bool required) {
    int targetPageIndex = getNavPageIndex(page_name);
    if (targetPageIndex < 0)
        ai::throw_failed("Bad nav panel page: {}", page_name);

    for (int i = 0; i < 6 && !ai::uiState.match("scr-left-panel:" + page_name); i++) {
        ai::detectEDState(DetectLevel::Buttons);
        LOG(DEBUG) << "Goto '" << page_name << "'...";

        if (ai::uiState.guiFocus == GuiFocus::None) {
            LOG(DEBUG) << "FocusLeftPanel...";
            kbd::send("FocusLeftPanel", 0, 1500);
            continue;
        }
        if (st::ship.flags.fsd_charging) {
            notify_progress(MSG_WARN, "Unexpected fsd charging");
            kbd::send("HyperSuperCombination", 100, 1000);
            continue;
        }
        if (ai::uiState.guiFocus == GuiFocus::Left && !ai::uiState.screen) {
            if ((i & 1) == 0)
                rollBlindCompass();
            else
                kbd::send("UI_Back", 0, 500);
            continue;
        }
        if (!ai::uiState.match("scr-left-panel:*")) {
            LOG(DEBUG) << "FocusLeftPanel...";
            kbd::send("FocusLeftPanel", 0, 1500);
            continue;
        }
        if (ai::uiState.match("scr-left-panel:dlg-nav-select") ||
            ai::uiState.match("scr-left-panel:dlg-filters")) {
            kbd::send("UI_Back", 0, 500);
            continue;
        }
        std::vector<std::string> segments = ai::uiState.splitPath();
        if (segments.size() < 2)
            ai::throw_failed("Expecting 2 segments in {}", ai::uiState.to_string());
        int currentPageIndex = getNavPageIndex(segments[1]);
        if (currentPageIndex < 0)
            ai::throw_failed("Bad nav panel page: {}", segments[1]);
        int dist = currentPageIndex - targetPageIndex;
        if (dist >= 0) {
            for (int j = 0; j < dist; j++)
                kbd::send("CycleNextPanel", 0, 250);
        } else {
            for (int j = 0; j < -dist; j++)
                kbd::send("CyclePreviousPanel", 0, 250);
        }
    }
    if (!ai::uiState.match("scr-left-panel:" + page_name)) {
        if (required)
            ai::throw_trouble("Unexpected scr-left-panel: {}", ai::uiState.to_string());
        return false;
    }
    return true;
}

int getNavRoutePosition() {
    auto navRoute = st::currentNavRoute;
    if (!navRoute || navRoute->route.empty())
        return -1;

    auto starSystem = gal::getCurrentStarSystem();
    if (!starSystem)
        throw_failed("Current star system not known");

    int currIdx = -1;
    for (int i = 0; i < navRoute->route.size(); i++) {
        if (navRoute->route[i].systemAddress == starSystem->systemAddress) {
            currIdx = i;
            break;
        }
    }
    currIdx += 1;
    if (currIdx >= navRoute->route.size())
        return -1;
    return currIdx;
}

bool clickWidget(const char* btn, int delay_ms, int pause_ms) {
    cv::Rect rect = Mgr.resolveWidgetReferenceRect(btn);
    if (rect.empty())
        return false;
    cv::Point pos = (rect.tl() + rect.br()) * 0.5;
    return kbd::sendMouseClick(pos, delay_ms, pause_ms);
}

bool selectOnGalaxyMap(const std::string& systemName) {
    ai::detectEDState(DetectLevel::Buttons);
    if (ai::uiState.guiFocus != GuiFocus::GalaxyMap || !ai::uiState.match("scr-galaxy")) {
        kbd::send("GalaxyMapOpen", 100, 1000);
        utc_timer timer(10s);
        do {
            ai::sleep(1000);
            ai::detectEDState(DetectLevel::Buttons);
            if (ai::uiState.guiFocus == GuiFocus::GalaxyMap && ai::uiState.match("scr-galaxy"))
                break;
        } while (!timer.expired());
        if (ai::uiState.guiFocus != GuiFocus::GalaxyMap || !ai::uiState.match("scr-galaxy"))
            return false;
    }
    if (!clickWidget("lbl-search",100,500))
        return false;
    kbd::send("BackSpace", 100, 500);

    pasteToClipboard(systemName);

    int handle = kbd::post("CtrlLeft", 500);
    ai::sleep(100);
    kbd::send("v", 100, 300);
    kbd::clearInput(handle);
    ai::sleep(1000);

    for (int entry=0; entry < 20; entry++) {
        cv::Rect dd_rect = Mgr.resolveWidgetReferenceRect("lbl-drop-down");
        cv::Point pos;
        pos.x = dd_rect.x + dd_rect.width / 2;
        pos.y = dd_rect.y + (entry+0.5)*28.65;
        kbd::sendMouseClick(pos, 300, 100);
        ai::sleep(3000);

        clickWidget("btn-tgt-copy",300,500);
        auto name = textFromClipboard();
        if (name == systemName) {
            clickWidget("btn-tgt-nav-to", 300, 1000);
            clickWidget("btn-exit", 300, 500);
            return true;
        }

        cv::Rect rect = Mgr.resolveWidgetReferenceRect("lbl-search");
        pos = (rect.tl() + rect.br()) * 0.5;
        kbd::sendMouseMove(pos, 500);
    }
    return false;
}


BaseAutopilotStep::BaseAutopilotStep()
    : task(nullptr)
{
    for (Step* s=parent; s && !task; s=s->parent) {
        task = dynamic_cast<BaseAutopilotTask*>(s);
    }
    if (!task) {
        LOG(ERROR) << "BaseAutopilotStep needs BaseAutopilotTask";
        throw std::runtime_error("BaseAutopilotStep needs BaseAutopilotTask");
    }
}


void BaseAutopilotTask::relogin() {
    // something is really wrong, logout and login again
    notify_progress(MSG_WARN, "Something is wrong with departure, trying to re-login");
    kbd::send("Pause", 0, 1000);
    kbd::send("UI_Up", 0, 100); // go to Exit button
    kbd::send("UI_Select", 0, 1000); // logout
    kbd::send("UI_Select", 0, 8000); // logout to main menu
    notify_progress(MSG_WARN, "Login to Solo...");
    kbd::send("UI_Select", 0, 3000); // login, select mode screen
    kbd::send("UI_Right", 0, 100);
    kbd::send("UI_Right", 0, 500);  // choose Solo
    kbd::send("UI_Select", 0, 12000); // login
    throw_trouble("Finished re-login");
}

bool setSpeed(int percents, bool force) {
    percents = std::clamp(percents, -100, +100);
    if (st::autopilot.speed_set_to.has_value() && percents == st::autopilot.speed_set_to.value() && !force)
        return true;
    switch (percents / 25) {
    case 4:
        kbd::send("SetSpeed100", 50);
        st::autopilot.speed_set_to = 100;
        break;
    case 3:
        kbd::send("SetSpeed75", 50);
        st::autopilot.speed_set_to = 75;
        break;
    case 2:
        kbd::send("SetSpeed50", 50);
        st::autopilot.speed_set_to = 50;
        break;
    case 1:
        kbd::send("SetSpeed25", 50);
        st::autopilot.speed_set_to = 25;
        break;
    case 0:
        kbd::send("SetSpeedZero", 50);
        st::autopilot.speed_set_to = 0;
        break;
    case -1:
        kbd::send("SetSpeedMinus25", 50);
        st::autopilot.speed_set_to = -25;
        break;
    case -2:
        kbd::send("SetSpeedMinus50", 50);
        st::autopilot.speed_set_to = -50;
        break;
    case -3:
        kbd::send("SetSpeedMinus75", 50);
        st::autopilot.speed_set_to = -75;
        break;
    case -4:
        kbd::send("SetSpeedMinus100", 50);
        st::autopilot.speed_set_to = -100;
        break;
    }
    return true;
}

static int getDuration(double angle, double speed) {
    angle = std::abs(angle);
    if (angle < 0.7)
        return 100;
    int duration1 = 1000*(angle-3)/speed;
    int duration2 = 150 + 1000*angle/(2*speed+3);
    int duration = std::max(duration1, duration2);
    return duration;
}

static void sendOrientAxis(const KeyBindings& bind, double speed, double delta, int max_time_ms) {
    double seconds = delta / speed;
    double value = std::copysign(1.0, seconds);
    int duration = (int) std::abs(1000*seconds);
    if (max_time_ms >= 1000 && duration >= 1000) {
        duration = std::min(duration, max_time_ms);
    } else {
        if (duration < 1000)
            value *= duration / 1500.0;
        duration = 1000;
    }
    int pause = std::min(1000,int(std::abs(40*value*speed)));
    kbd::axis(bind, value);
    ai::sleep(duration, true);
    kbd::axis(bind, 0);
    ai::sleep(pause);
}

static void sendOrientKeys(const char* pos, const char* neg, double speed, double delta, int max_time_ms) {
    if (std::abs(delta) < 0.1)
        return;
    int duration = std::min(max_time_ms, getDuration(delta, speed));
    int pause = duration < max_time_ms ? duration : 1000;
    kbd::send(delta > 0 ? pos : neg, duration, pause);
}

void BaseAutopilotTask::orientRollStep(double delta, int max_time_ms) {
    if (delta > 180) delta = 360-delta;
    if (delta < -180) delta = 360+delta;
    auto shipStats = eddb::getShipStats();
    if (!shipStats)
        throw_failed("Unsupported or unknown ship");
    if (!st::autopilot.speed_set_to.has_value())
        setSpeed(0, true);
    double speed = shipStats->getRollSpeed(st::autopilot.speed_set_to.value());
    const KeyBindings& bind = Cfg.getGameKeyBindings("RollAxisRaw");
    if ((bind.mode == KeyBindings::Axis || bind.mode == KeyBindings::AxisInv) && bind.primary.device == GameKey::vJoy) {
        sendOrientAxis(bind, speed, delta, max_time_ms);
    } else {
        sendOrientKeys("RollRightButton", "RollLeftButton", speed, delta, max_time_ms);
    }
}

void BaseAutopilotTask::orientPitchStep(double delta, int max_time_ms) {
    if (delta > 180) delta = 360-delta;
    if (delta < -180) delta = 360+delta;
    auto shipStats = eddb::getShipStats();
    if (!shipStats)
        throw_failed("Unsupported or unknown ship");
    if (!st::autopilot.speed_set_to.has_value())
        setSpeed(0, true);
    double speed = shipStats->getPitchSpeed(st::autopilot.speed_set_to.value());
    const KeyBindings& bind = Cfg.getGameKeyBindings("PitchAxisRaw");
    if ((bind.mode == KeyBindings::Axis || bind.mode == KeyBindings::AxisInv) && bind.primary.device == GameKey::vJoy) {
        sendOrientAxis(bind, speed, -delta, max_time_ms);
    } else {
        sendOrientKeys("PitchUpButton", "PitchDownButton", speed, delta, max_time_ms);
    }
}

void BaseAutopilotTask::orientYawStep(double delta, int max_time_ms) {
    if (delta > 180) delta = 360-delta;
    if (delta < -180) delta = 360+delta;
    auto shipStats = eddb::getShipStats();
    if (!shipStats)
        throw_failed("Unsupported or unknown ship");
    if (!st::autopilot.speed_set_to.has_value())
        setSpeed(0, true);
    double speed = shipStats->getYawSpeed(st::autopilot.speed_set_to.value());
    const KeyBindings& bind = Cfg.getGameKeyBindings("YawAxisRaw");
    if ((bind.mode == KeyBindings::Axis || bind.mode == KeyBindings::AxisInv) && bind.primary.device == GameKey::vJoy) {
        sendOrientAxis(bind, speed, delta, max_time_ms);
    } else {
        sendOrientKeys("YawRightButton", "YawLeftButton", speed, delta, max_time_ms);
    }
}

bool BaseAutopilotTask::orientTowardTargetStep(double precision, int max_time_ms) {
    check_interrupted();
    if (!ai::compassInfo.has_nav_target)
        precision = std::max(3.0, precision);
    bool front = ai::compassInfo.hemisphere > 0;
    int hemiYaw = ai::compassInfo.targetYaw;
    if (!front) {
        if (hemiYaw > 0)
            hemiYaw = 180 - hemiYaw;
        else
            hemiYaw = -180 - hemiYaw;
    }
    if (std::abs(hemiYaw) > 20) {
        float roll = ai::compassInfo.targetRoll;
        if (std::abs(roll) <= 90)
            orientRollStep(roll, max_time_ms);
        else
            orientRollStep(roll-180, max_time_ms);
        return false;
    }

    float pitch = ai::compassInfo.targetPitch;
    float yaw = ai::compassInfo.targetYaw;

    if (std::abs(pitch) > precision) {
        orientPitchStep(pitch, max_time_ms);
        return false;
    }

    if (std::abs(yaw) > precision) {
        orientYawStep(yaw, max_time_ms);
        return false;
    }
    return true;
}

bool BaseAutopilotTask::orientTowardTarget(double precision) {
    CourseLockerPause lockPause;
    if (st::guiFocus != GuiFocus::None) {
        notify_progress(MSG_INFO, "Orientation: goto compass");
        kbd::send("UI_Back", 0, 1500);
    }
    for (int fails=0; fails < 10; fails++) {
        check_interrupted();
        if (fails > 2)
            setSpeed(0);
        ai::detectEDState(DetectLevel::Screen);
        if (ai::uiState.guiFocus != GuiFocus::None) {
            notify_progress(MSG_WARN, std::format("Unexpected ui mode {}", ai::uiState.to_string()));
            kbd::send("UI_Back", 0, 1500);
            continue;
        }
        if (!ai::compassInfo.hemisphere) {
            notify_progress(MSG_WARN, std::format("Compass not detected, fails {}", fails));
            rollBlindCompass();
            continue;
        }
        fails = 0;
        if (orientTowardTargetStep(precision))
            return true;
    }
    notify_progress(MSG_ERROR, "Compass not detected");
    return false;
}

bool BaseAutopilotTask::orientAwayFromTargetStep(double precision, int max_time_ms) {
    check_interrupted();
    precision = std::max(5.0, precision);
    bool front = ai::compassInfo.hemisphere > 0;
    int hemiYaw = ai::compassInfo.targetYaw;
    if (!front) {
        if (hemiYaw > 0)
            hemiYaw = 180 - hemiYaw;
        else
            hemiYaw = -180 - hemiYaw;
    }
    if (std::abs(hemiYaw) > 20) {
        float roll = ai::compassInfo.targetRoll;
        if (std::abs(roll) <= 90)
            orientRollStep(roll, max_time_ms);
        else
            orientRollStep(roll-180, max_time_ms);
        return false;
    }

    float pitch = ai::compassInfo.targetPitch;
    float yaw = ai::compassInfo.targetYaw;

    if (180-std::abs(pitch) > precision) {
        orientPitchStep(pitch-180);
        return false;
    }

    if (180-std::abs(yaw) > precision) {
        orientYawStep(yaw-180);
        return false;
    }
    return true;
}

bool BaseAutopilotTask::orientAwayFromTarget(double precision) {
    CourseLockerPause lockPause;
    if (st::guiFocus != GuiFocus::None) {
        notify_progress(MSG_INFO, "Orientation: goto compass");
        kbd::send("UI_Back", 0, 1500);
    }
    int speedDropped = 0;
    for (int fails=0; fails < 10; fails++) {
        check_interrupted();
        if (fails > 2) {
            setSpeed(0);
            continue;
        }
        ai::detectEDState(DetectLevel::Screen);
        if (ai::uiState.guiFocus != GuiFocus::None) {
            notify_progress(MSG_WARN, std::format("Unexpected ui mode {}", ai::uiState.to_string()));
            kbd::send("UI_Back", 0, 1500);
            continue;
        }
        if (!ai::compassInfo.hemisphere) {
            notify_progress(MSG_WARN, std::format("Compass not detected, fails {}", fails));
            rollBlindCompass();
            continue;
        }
        fails = 0;
        if (orientAwayFromTargetStep(precision))
            return true;
    }
    notify_progress(MSG_ERROR, "Compass not detected");
    return false;
}

bool BaseAutopilotTask::orientRollByTarget(double reqRoll, double precision, int max_time_ms) {
    CourseLockerPause lockPause;
    if (st::guiFocus != GuiFocus::None) {
        notify_progress(MSG_INFO, "Orientation: goto compass");
        kbd::send("UI_Back", 0, 1500);
    }
    for (int fails=0; fails < 10; fails++) {
        check_interrupted();
        if (fails > 2) {
            setSpeed(0);
            continue;
        }
        ai::detectEDState(DetectLevel::Screen);
        if (ai::uiState.guiFocus != GuiFocus::None) {
            notify_progress(MSG_WARN, std::format("Unexpected ui mode {}", ai::uiState.to_string()));
            kbd::send("UI_Back", 0, 1500);
            continue;
        }
        if (!ai::compassInfo.hemisphere) {
            notify_progress(MSG_WARN, std::format("Compass not detected, fails {}", fails));
            rollBlindCompass();
            continue;
        }
        fails = 0;
        if (ai::compassInfo.targetAngle < 3)
            return true;
        if (!ai::compassInfo.has_nav_target) {
            if (ai::compassInfo.targetAngle < 10)
                precision = std::max(precision, 10.);
            else if (ai::compassInfo.targetAngle < 20)
                precision = std::max(precision, 8.);
        }
        float roll = ai::compassInfo.targetRoll;
        double delta = roll - reqRoll;
        if (std::abs(delta) <= precision)
            return true;
        orientRollStep(delta, max_time_ms);
    }
    notify_progress(MSG_ERROR, "Compass not detected");
    return false;
}

void BaseAutopilotTask::initNavFilter() {
    st::NavPanelFilters filters{};
    filters.star = true;
    filters.planetOrMoon = true;
    filters.landablePlanetOrMoon = true;
    filters.station = true;
    TypeNav nt = TypeNav::Other;
    if (st::autopilot.destDock)
        nt = st::autopilot.destDock->type;
    else if (st::autopilot.destBody)
        nt = st::autopilot.destBody->type;
    switch (nt) {
    case TypeNav::Body:
    case TypeNav::Barycenter:
    case TypeNav::Star:
    case TypeNav::Planet:
    case TypeNav::NavBeacon:
    case TypeNav::SpaceStation:
    case TypeNav::Orbis:
    case TypeNav::Ocellus:
    case TypeNav::Coriolis:
    case TypeNav::AsteroidBase:
    case TypeNav::SpaceConstrDepot:
    case TypeNav::StationMegaShip:
    case TypeNav::StrongholdCarrier:
    case TypeNav::ColonisationShip:
    case TypeNav::PlanetaryConstrDepot:
        break;

    case TypeNav::Other:
    case TypeNav::Error:
    case TypeNav::NotExplored:
    case TypeNav::Signal:
        filters.settlement = true;
        filters.signalSource = true;
        filters.asteroidCluster = true;
        filters.pointOfInterest = true;
        break;

    case TypeNav::WarZone:
    case TypeNav::ResSite:
        filters.signalSource = true;
        break;
    case TypeNav::StarSystem:
        filters.system = true;
        break;
    case TypeNav::Ring:
        filters.signalSource = true;
        break;
    case TypeNav::AsteroidCluster:
        filters.asteroidCluster = true;
        break;

    case TypeNav::SpaceThing:
    case TypeNav::SpaceInstallation:
    //case TypeNav::TrailblazerDream:
    case TypeNav::Megaship:
    case TypeNav::TouristBeacon:
        filters.pointOfInterest = true;
        break;
    case TypeNav::FleetCarrier:
        if (st::autopilot.destDock->marketId != st::cmdr.carrierId)
            filters.fleetCarrier = true;
        break;
    case TypeNav::SquadronCarrier:
        filters.fleetCarrier = true;
        break;
    case TypeNav::PlanetaryThing:
    case TypeNav::PlanetaryStation:
    case TypeNav::PlanetaryPort:
    case TypeNav::EngineerPort:
    case TypeNav::Settlement:
    case TypeNav::PlanetaryInstallation:
        filters.settlement = true;
        break;
    }
    nl.init(filters);
}


TaskDebugAutopilot::TaskDebugAutopilot(const ai::TaskTemplate &templ)
        : BaseAutopilotTask(templ)
{
    assert (templ.id == ED_TASK_DEBUG_AUTOPILOT);
    for (auto& p : templ.params) {
        if (p.id == "test")
            test = p.as_string();
        if (p.id == "target")
            target = p.as_string();
        if (p.id == "value")
            value = p.as_number();
    }
}


bool TaskDebugAutopilot::run() {
    auto starSystem = gal::getCurrentStarSystem();
    if (target.empty())
        target = st::destination.name;
    st::autopilot.setDestDock(starSystem->getDock(target));
    if (st::autopilot.destDock) {
        auto body = starSystem->getBodyById(st::autopilot.destDock->parentBodyId);
        st::autopilot.setDestBody(body);
    } else {
        st::autopilot.setDestBody(starSystem->getBody(target));
    }

    initNavFilter();
    setSpeed(0);
    kbd::send("UI_Back");

    if (test == "OrientTowards") {
        setSpeed(0);
        orientTowardTarget(value);
        setSpeed(0);
    }
    else if (test == "OrientAway") {
        setSpeed(0);
        orientAwayFromTarget(value);
        setSpeed(0);
    }
    else if (test == "KeepCourse") {
        setSpeed(50);
        CourseLocker course(0, true);
        for (;;) {
            sleep(1000);
            LOG(INFO) << std::format("KeepCourse: pitch: {:.1f}, yaw: {:.1f}, roll: {:.1f}, angle: {:.1f}",
                                     st::compass.targetPitch, st::compass.targetYaw,
                                     st::compass.targetRoll, st::compass.targetAngle);
        }
    }
    else if (test == "Departure") {
        run_sub_step(new DepartureStep);
    }
    else if (test == "DockSpaceStation") {
        run_sub_step(new DockSpaceStation);
    }
    else if (test == "DockPlanetPort") {
        run_sub_step(new DockPlanetPort);
    }
    else if (test == "EnterCruise") {
        run_sub_step(new EnterCruiseStep);
    }
    else if (test == "HyperJump") {
        run_sub_step(new HyperJumpStep);
    }
    else if (test == "LeaveBody") {
        run_sub_step(new LeaveBodyStep);
    }
    else if (test == "FocusDestDock") {
        nl.focusDestDock();
    }
    else if (test == "FocusDestBody") {
        nl.focusDestBody();
    }
    else if (test == "FocusNearestBody") {
        nl.focusNearestBody();
    }
    else if (test == "FocusTopEntry") {
        nl.focusTopEntry();
    }
    else if (test == "GalMapNavRoute") {
        selectOnGalaxyMap(target);
    }
    else if (test == "NavDockSelect") {
        run_sub_step(new NavDockSelect);
    }
    else if (test == "NavBodySelect") {
        run_sub_step(new NavBodySelect);
    }
    else if (test == "CruiseToDist") {
        dist_t min_dist(dist_t::MM, 20);
        dist_t max_dist(dist_t::MM, 50);
        if (st::autopilot.destBody && st::autopilot.destBody->radius > 0) {
            min_dist = dist_t(dist_t::KM, st::autopilot.destBody->radius * 20);
            max_dist = dist_t(dist_t::KM, st::autopilot.destBody->radius * 50);
        }
        run_sub_step(new CruiseToDistStep(min_dist, max_dist));
    }
    else if (test == "DiveUnderPlanet") {
        run_sub_step(new DiveUnderPlanetStep);
    }
    else if (test == "ExitCruiseToSpace") {
        run_sub_step(new ExitCruiseToSpace);
    }
    else if (test == "ExitCruiseToPlanet") {
        run_sub_step(new ExitCruiseToPlanet);
    }
    else if (test == "ForwardAccelerate") {
        accelForward(value);
    }
    else if (test == "ReverseAccelerate") {
        accelReverse(value);
    }
    return true;
}

bool TaskDebugAutopilot::accelForward(double seconds) {
    auto ship = eddb::getShipStats();

    double fwdacc = ship->getForwardAccel();
    setSpeed(100);
    LOG(INFO) << "Start acceleration";
    ai::sleep(int(seconds * 1000), true);
    setSpeed(0);
    LOG(INFO) << "End acceleration";
    ai::sleep(1000);
    LOG(INFO) << "Expected speed: " << (fwdacc * seconds);
    return true;
}

bool TaskDebugAutopilot::accelReverse(double seconds) {
    auto ship = eddb::getShipStats();

    double revacc = ship->getReverseAccel();
    setSpeed(-100);
    LOG(INFO) << "Start acceleration";
    ai::sleep(int(seconds * 1000), true);
    setSpeed(0);
    LOG(INFO) << "End acceleration";
    ai::sleep(1000);
    double max_speed = ship->getThrustSpeed();
    LOG(INFO) << "Expected speed (if dropped from max): " << (max_speed - revacc * seconds);
    return true;
}

bool DepartureStep::run() {
    if (fromDock.empty())
        fromDock = st::dockedAt.stationName;

    bool fromSpaceConstruction = false; // need UpThrustButton
    if (st::dockedAt.stationType == "SpaceConstructionDepot") {
        fromSpaceConstruction = true;
    }

    if (st::ship.flags.docked) {
        if (st::guiFocus != GuiFocus::None) {
            LOG(INFO) << "Going to landing pad.";
            status = GOING_TO_DOCK;
            for (int i = 0; i < 10 && st::guiFocus != GuiFocus::None; i++) {
                kbd::send("UI_Back", 0, 1000);
                ai::detectEDState(DetectLevel::Screen);
            }
        }
        ai::detectEDState(DetectLevel::Screen);
        if (st::guiFocus != GuiFocus::None)
            throw_trouble("Cannot get to landing pad");

        LOG(INFO) << "Refuel...";
        status = REFUEL;
        sleep(500);
        for (int i = 0; i < 4; i++)
            kbd::send("UI_Up");
        kbd::send("UI_Select", 0, 500); // refuel
        kbd::send("UI_Right");
        kbd::send("UI_Select", 0, 500); // repair
        kbd::send("UI_Right");
        kbd::send("UI_Select", 0, 500); // rearm
        kbd::send("UI_Down");
        kbd::send("UI_Down");
        kbd::send("UI_Select");

        LOG(INFO) << "Takeoff...";
        // 20 seconds to leave landing pad
        timer = utc_timer(25s);
        status = TAKEOFF;
        while (st::ship.flags.docked && !timer.expired()) {
            sleep(1000);
            ai::detectEDState(DetectLevel::Screen);
            if (ai::uiState.autopilot)
                break;
        }
        if (st::ship.flags.docked && !ai::uiState.autopilot)
            throw_trouble("Takeoff failed");
    }
    if (!ai::uiState.autopilot) {
        LOG(INFO) << "Departure autopilot waiting...";
        // 15 seconds wait autopilot
        timer = utc_timer(15s);
        status = WAIT_AUTOPILOT;
        // wait at least 15 seconds for autopilot to departure
        while (!ai::uiState.autopilot && !timer.expired()) {
            sleep(250);
            ai::detectEDState(DetectLevel::Screen);
        }
    }
    // 4 minutes for departure
    timer = utc_timer(4min);
    status = AUTOPILOT;
    setSpeed(0, true);
    unsigned speedeZeroHandle = kbd::post("SetSpeedZero", 4*60*1000);
    notAutoPilotCounter = 0;
    try {
        for (;;) {
            if (timer.expired()) {
                kbd::clearInput(speedeZeroHandle);
                notify_progress(MSG_ERROR, "Autopilot time expired");
                status = RELOGIN;
                task->relogin();
                return false;
            }
            sleep(250);
            ai::detectEDState(DetectLevel::Screen);
            if (ai::uiState.autopilot) {
                notAutoPilotCounter = 0;
                continue;
            }
            if (++notAutoPilotCounter > 4) {
                notify_progress(MSG_INFO, "Departure complete (autopilot off)");
                break;
            } else {
                notify_progress(MSG_INFO, std::format("Auto-pilot off counter: {}", notAutoPilotCounter));
            }
        }
    } catch (...) {
        kbd::clearInput(speedeZeroHandle);
        throw;
    }
    kbd::clearInput(speedeZeroHandle);

    ai::detectEDState(DetectLevel::Screen);
    if (ai::compassInfo.hemisphere)
        pitchBeforeAutopilot = ai::compassInfo.targetPitch;
    else
        pitchBeforeAutopilot = std::numeric_limits<float>::quiet_NaN();

    if (st::shipAtBody.nearBody) {
        status = ORIENT_AWAY;
        task->orientPitchStep(90, 7000);
    }
    else if (fromSpaceConstruction) {
        timer = utc_timer(15s);
        status = LEAVE_DEPOT;
        setSpeed(0,true);
        kbd::send("UpThrustButton", 15000);
    }
    if (st::ship.flags.fsd_masslocked) {
        timer = utc_timer(1min);
        status = MASSLOCKED;
        notify_progress(MSG_INFO, "Mass-locked, flying away");
        setSpeed(100, true);
        for (int cnt=0; st::ship.flags.fsd_masslocked; cnt++) {
            sleep(1000);
            if (cnt > 10) {
                setSpeed(100, true);
                cnt = 0;
            }
        }
    }
    notify_progress(MSG_INFO, "Ready to jump, flying away");
    timer = utc_timer(15s);
    status = FLYAWAY;
    while (!timer.expired()) {
        sleep(1000);
    }
    setSpeed(50);
    prevSubStep.reset();
    currSubStep.reset();
    status = DONE;
    return true;
}

std::string DepartureStep::getTitle() {
    if (status == DONE)
        return lc_format("Departed from: {}", fromDock);
    return lc_format("Departing from: {}", fromDock);
}

std::string DepartureStep::getStatus() {
    switch (status) {
    case READY:
    case DONE:
        return {};
    case GOING_TO_DOCK:
        return _gt("Going to landing pad");
    case REFUEL:
        return _gt("Refuel/repair/rearm");
    case TAKEOFF:
        return lc_format("Takeoff: {}", timer.left());
    case WAIT_AUTOPILOT:
        return std::format("Wait for autopilot: {}", timer.left());
    case AUTOPILOT:
        if (notAutoPilotCounter > 0)
            return lc_format("Autopilot exiting: {}", notAutoPilotCounter);
        else
            return lc_format("Autopilot: {}", timer.left());
    case ORIENT_AWAY:
        return _gt("Orient away from planet");
    case LEAVE_DEPOT:
        return lc_format("Leaving depot: {}s", timer.left());
    case MASSLOCKED:
        return lc_format("Mass-locked: {}", timer.passed());
    case FLYAWAY:
        return lc_format("Fly away: {}", timer.left());
    case RELOGIN:
        return _gt("Re-login");
    }
    return {};
}


bool EnterCruiseStep::run() {
    if (st::ship.flags.cruise) {
        prevSubStep.reset();
        currSubStep.reset();
        status = DONE;
        return true;
    }

    bool flyAway = false;
    status = LOCK_BODY;
    for (int retry=0; retry < 3; retry++) {
        dist_t dist;
        gal::spEntity body = task->nl.focusNearestBody(&dist);
        if (body && body->radius > 0 && dist.valid()) {
            if (body->type == TypeNav::Star) {
                if (dist.get(dist_t::LS) > 20 || dist.get(dist_t::KM) / body->radius > 10)
                    break;
            } else {
                if (dist.get(dist_t::KM) / body->radius > 4)
                    break;
            }
        }
        if (body && dist.valid() || st::space.bodyType == "Star") {
            flyAway = true;
            task->nl.selectFocused();
            status = ORIENT;
            kbd::send("UI_Back", 0, 500);
            task->orientAwayFromTarget(10);
            break;
        }
    }
    kbd::send("UI_Back");

    setSpeed(100);
    sleep(500);
    while (st::ship.flags.fsd_masslocked) {
        timer = utc_timer(60s);
        status = MASSLOCKED;
        //notifyProgress("Mass-locked, flying away");
        sleep(1000);
    }

    if (st::ship.flags.cargo_scoop_on || st::ship.flags.weapon_on || st::ship.flags.landing_gear_down) {
        status = PREPARE;
        if (st::ship.flags.cargo_scoop_on)
            kbd::send("ToggleCargoScoop");
        if (st::ship.flags.weapon_on)
            kbd::send("DeployHardpointToggle");
        if (st::ship.flags.landing_gear_down)
            kbd::send("LandingGearToggle");
        sleep(1000);
    }

    while (st::ship.flags.fsd_cooldown) {
        timer = utc_timer(20s);
        status = FSD_COOLDOWN;
        sleep(1000);
    }

    timer = utc_timer(20s);
    status = ENTER_CRUISE;
    kbd::send("Supercruise", 100, 1000);
    if (!(st::ship.flags.fsd_charging || st::ship.flags.fsd_jump)) {
        notify_progress(MSG_ERROR, "Entering supercruise failed");
        return false;
    }

    if (!st::ship.flags.cruise && (st::ship.flags.fsd_charging || st::ship.flags.fsd_jump)) {
        CourseLocker course(0);
        while (!st::ship.flags.cruise && (st::ship.flags.fsd_charging || st::ship.flags.fsd_jump) && !timer.expired()) {
            if (st::guiFocus != GuiFocus::None)
                kbd::send("UI_Back", 0, 500);
            sleep(500);
        }
    }

    if (!st::ship.flags.cruise) {
        notify_progress(MSG_ERROR, "Entering supercruise failed");
        return false;
    }

    if (flyAway) {
        setSpeed(100);
        timer = utc_timer(10s);
        status = FLY_AWAY;
        while (!timer.expired()) {
            sleep(1000);
            if (!st::ship.flags.cruise) {
                setSpeed(0);
                throw_trouble("Unexpected cruise exit");
            }
        }
    }

    if (task->nl.focusDestDock() || task->nl.focusDestBody())
        task->nl.selectFocused();

    prevSubStep.reset();
    currSubStep.reset();
    status = DONE;
    return true;
}

std::string EnterCruiseStep::getTitle() {
    if (status == DONE)
        return _gt("Entered cruise");
    return _gt("Entering cruise");
}

std::string EnterCruiseStep::getStatus() {
    switch (status) {
    case DONE:
    case READY:
        return {};
    case LOCK_BODY:
        return _gt("Locking body");
    case LOCK_TARGET:
        return _gt("Locking target");
    case ORIENT:
        return _gt("Orienting");
    case MASSLOCKED:
        return lc_format("Mass-locked: {}", timer.passed());
    case PREPARE:
        return lc_format("Preparing");
    case FSD_COOLDOWN:
        return lc_format("FSD Cooldown: {}", timer.passed());
    case ENTER_CRUISE:
        return lc_format("Entering cruise: {}", timer.left());
    case FLY_AWAY:
        return lc_format("Fly away: {}", timer.left());
    }
    return {};
}

bool HyperJumpStep::run() {
    destSystem = st::destination.name;
    if (st::ship.flags.cargo_scoop_on || st::ship.flags.weapon_on || st::ship.flags.landing_gear_down) {
        if (st::ship.flags.cargo_scoop_on)
            kbd::send("ToggleCargoScoop");
        if (st::ship.flags.weapon_on)
            kbd::send("DeployHardpointToggle");
        if (st::ship.flags.landing_gear_down)
            kbd::send("LandingGearToggle");
        sleep(1000);
    }

    // select next jump system and jump
    timer = utc_timer(15s);
    status = CHARGE;
    if (!(st::ship.flags.fsd_charging || st::ship.flags.fsd_jump)) {
        kbd::send("HyperSuperCombination", 100, 1000);
        if (!(st::ship.flags.fsd_charging || st::ship.flags.fsd_jump)) {
            notify_progress(MSG_ERROR, "Entering jump failed");
            return false;
        }
    }
    {
        setSpeed(50);
        CourseLocker course(0);
        while (timer.sec_passed() < 9) {
            if (st::compass.has_nav_target && st::compass.targetAngle < 3)
                setSpeed(25);
            sleep(500);
        }
        setSpeed(100);
        while (!st::ship.flags.fsd_jump) {
            if (!st::ship.flags.fsd_charging || timer.sec_passed() > 60) {
                notify_progress(MSG_ERROR, "Jump failed");
                if (st::ship.flags.fsd_charging || st::ship.flags2.fsd_hyperdrive_charging)
                    kbd::send("HyperSuperCombination");
                setSpeed(0);
                return false;
            }
            sleep(500);
        }
    }

    status = HYPERSPACE;
    timer = utc_timer(20s);
    for (;;) {
        if (!st::ship.flags.fsd_jump)
            break;
        if (timer.expired()) {
            notify_progress(MSG_ERROR, "Jump failed");
            return false;
        }
        sleep(250);
    }

    // avoid star
    status = AVOID_STAR;
    int rotate_speed = 25;
    auto fly_away_time = 10s;
    if (auto ss = gal::getCurrentStarSystem()) {
        auto star = ss->getMainStar();
        if (star && !star->code.empty()) {
            switch (star->code[0]) {
            case 'L': case 'T': case 'Y': // Brown Dwarfs
            case 'D': case 'H': case 'X': // White Dwarfs, Neutron, Black Hole
            //case 'C': case 'M': case 'S': // Carbon Stars
                rotate_speed = 0;
                fly_away_time = 15s;
                break;
            }
        }
    }
    setSpeed(rotate_speed);
    task->orientPitchStep(100, 20000);

    status = FLY_AWAY;
    setSpeed(100);
    timer = utc_timer(fly_away_time);
    while (!timer.expired()) {
        if (timer.sec_passed() > 2) {
            ai::detectEDState(DetectLevel::Screen);
            if (ai::compassInfo.hemisphere) {
                int roll = ai::compassInfo.targetRoll;
                int delta = roll;
                if (roll > +90)
                    delta = -180+roll;
                else if (roll < -90)
                    delta = 180+roll;
                if (std::abs(delta) > 10) {
                    task->orientRollStep(delta, 1000);
                    continue;
                }
            }
        }
        sleep(1000);
    }
    prevSubStep.reset();
    currSubStep.reset();
    status = DONE;
    return true;
}

std::string HyperJumpStep::getTitle() {
    if (status == DONE)
        return lc_format("Jumped to: {}", destSystem);
    return lc_format("Jumping to: {}", destSystem);
}

std::string HyperJumpStep::getStatus() {
    switch (status) {
    case DONE:
    case READY:
        return {};
    case CHARGE:
        return lc_format("Charging: {}", timer.passed());
    case HYPERSPACE:
        return "Hyperspace";
        return lc_format("Hyperspace: {}", timer.left());
    case AVOID_STAR:
        return "Avoid star";
    case FLY_AWAY:
        return lc_format("Fly away: {}", timer.left());
    }
    return {};
}


bool LeaveBodyStep::run() {
    if (st::ship.flags.cruise && !st::shipAtBody.approachBody && !st::shipAtBody.nearBody) {
        prevSubStep.reset();
        currSubStep.reset();
        status = DONE;
        return true;
    }

    status = LOCK_BODY;
    if (!task->nl.focusNearestBody())
        throw_trouble("Cannot focus nearest body");
    if (!task->nl.selectFocused())
        throw_trouble("Cannot select focused nearest body");
    fromBody = st::destination.name;
    status = ORIENT;
    kbd::send("UI_Back", 0, 500);
    task->orientAwayFromTarget(10);
    kbd::send("UI_Back");

    setSpeed(100);
    sleep(500);
    if (!st::ship.flags.cruise) {
        if (st::ship.flags.fsd_masslocked) {
            timer = utc_timer(60s);
            status = MASSLOCKED;
            notify_progress(MSG_INFO, "Mass-locked, flying away");
            while (st::ship.flags.fsd_masslocked && !timer.expired()) {
                sleep(1000);
            }
            if (st::ship.flags.fsd_masslocked)
                throw_trouble("Cannot leave mass-locked area");
        }

        if (st::ship.flags.cargo_scoop_on || st::ship.flags.weapon_on || st::ship.flags.landing_gear_down) {
            status = PREPARE;
            if (st::ship.flags.cargo_scoop_on)
                kbd::send("ToggleCargoScoop");
            if (st::ship.flags.weapon_on)
                kbd::send("DeployHardpointToggle");
            if (st::ship.flags.landing_gear_down)
                kbd::send("LandingGearToggle");
            sleep(1000);
        }

        if (st::ship.flags.fsd_cooldown) {
            timer = utc_timer(30s);
            status = FSD_COOLDOWN;
            while (st::ship.flags.fsd_cooldown && !timer.expired())
                sleep(1000);
        }

        timer = utc_timer(20s);
        status = ENTER_CRUISE;
        kbd::send("Supercruise", 100, 1000);
        if (!(st::ship.flags.fsd_charging || st::ship.flags.fsd_jump)) {
            notify_progress(MSG_ERROR, "Entering supercruise failed");
            return false;
        }
    }

    if (!st::ship.flags.cruise && (st::ship.flags.fsd_charging || st::ship.flags.fsd_jump)) {
        //CourseLocker course(0);
        while (!st::ship.flags.cruise && (st::ship.flags.fsd_charging || st::ship.flags.fsd_jump) && !timer.expired()) {
            if (st::guiFocus != GuiFocus::None)
                kbd::send("UI_Back", 0, 500);
            ai::detectEDState(DetectLevel::Screen);
            if (ai::compassInfo.hemisphere > 1) {
                // need align to exit course
                task->orientTowardTargetStep(5, 1000);
            }
            sleep(500);
        }
    }

    if (!st::ship.flags.cruise) {
        notify_progress(MSG_ERROR, "Entering supercruise failed");
        return false;
    }

    if (st::shipAtBody.approachBody || st::shipAtBody.nearBody) {
        timer = utc_timer(60s);
        status = LEAVING_BODY;
        while ((st::shipAtBody.approachBody || st::shipAtBody.nearBody) && !timer.expired()) {
            if (st::guiFocus != GuiFocus::None)
                kbd::send("UI_Back", 0, 500);
            if (!st::ship.flags.cruise)
                throw_trouble("Unexpected cruise exit");
            sleep(500);
        }
    }

    timer = utc_timer(15s);
    setSpeed(100);
    status = FLY_AWAY;
    while (!timer.expired()) {
        if (!st::ship.flags.cruise) {
            setSpeed(0);
            throw_trouble("Unexpected cruise exit");
        }
        sleep(500);
    }

    status = PREPARE;
    if (task->nl.focusDestDock() || task->nl.focusDestBody())
        task->nl.selectFocused();

    prevSubStep.reset();
    currSubStep.reset();
    status = DONE;
    return true;
}

std::string LeaveBodyStep::getTitle() {
    if (status == DONE)
        return lc_format("Leaved: {}", fromBody);
    return lc_format("Leaving: {}", fromBody);
}

std::string LeaveBodyStep::getStatus() {
    switch (status) {
    case DONE:
    case READY:
        return {};
    case LOCK_BODY:
        return _gt("Locking body");
    case ORIENT:
        return _gt("Orienting");
    case MASSLOCKED:
        return lc_format("Mass-locked: {}", timer.passed());
    case PREPARE:
        return lc_format("Preparing");
    case FSD_COOLDOWN:
        return lc_format("FSD Cooldown: {}", timer.passed());
    case ENTER_CRUISE:
        return lc_format("Entering cruise: {}", timer.left());
    case LEAVING_BODY:
        return lc_format("Leaving body: {}", timer.passed());
    case FLY_AWAY:
        return lc_format("Fly away {}", timer.passed());
    }
    return {};
}

bool BaseDockStep::canDock() {
    if (!st::autopilot.destDock)
        return false;
    switch (st::autopilot.destDock->type) {
    case TypeNav::SpaceStation:
    case TypeNav::Orbis:
    case TypeNav::Ocellus:
    case TypeNav::Coriolis:
    case TypeNav::AsteroidBase:
    case TypeNav::SpaceOutpost:
    case TypeNav::SpaceConstrDepot:
    case TypeNav::StationMegaShip:
    case TypeNav::FleetCarrier:
    case TypeNav::SquadronCarrier:
    case TypeNav::StrongholdCarrier:
    case TypeNav::ColonisationShip:
    case TypeNav::PlanetaryStation:
    case TypeNav::PlanetaryPort:
    case TypeNav::EngineerPort:
    case TypeNav::Settlement:
    case TypeNav::PlanetaryConstrDepot:
        return true;
    default:
        break;
    }
    return false;
}
spGameEvent BaseDockStep::requestDockingPermit() {
    lastDockingStatus.clear();
    status = REQUEST;
    for (int retry=0; retry < 3; retry++) {
        setSpeed(0);
        gotoNavPage("mod-contacts");

        if (ai::uiState.focused_name() != "btn-landing") {
            bool have_btn_landing = false;
            for (auto& cr : ai::rEnv.classified) {
                if (cr.cdt == ClsDetType::Widget && cr.text == "btn-landing") {
                    have_btn_landing = true;
                    break;
                }
            }
            if (!have_btn_landing) {
                kbd::send("UI_Down");
                kbd::send("UI_Up", 1500);
            }
            kbd::send("UI_Right", 500);

            ai::detectEDState(DetectLevel::Buttons);
            if (ai::uiState.focused_name() != "btn-landing") {
                gotoNavPage("mod-nav-list");
                continue;
            }
        }

        LOG(INFO) << "TaskDock requesting landing permission";
        Cfg.dockingEvent.reset();
        // poll for docking event
        timer = utc_timer(5s);
        kbd::send("UI_Right");
        kbd::send("UI_Select", 0, 800);
        kbd::send("UI_Select");
        while (!timer.expired()) {
            auto de = Cfg.dockingEvent;
            if (!de) {
                sleep(250);
                continue;
            }
            if (de->event == "DockingRequested") {
                sleep(250);
                continue;
            }
            return de;
        }
    }
    return {};
}

bool BaseDockStep::autopilot() {
    // 8 minutes for docking
    timer = utc_timer(8min);
    status = AUTOPILOT;
    setSpeed(0); // set speed to 0 to start autopilot
    kbd::send("UI_Back", 0, 1500);

    // wait at least 5 seconds for autopilot to start docking
    for (int i=0; i < 40; i++) {
        sleep(250);
        ai::detectEDState(DetectLevel::Screen);
        if (ai::uiState.autopilot) {
            LOG(INFO) << "Docking autopilot started";
            break;
        }
        LOG(INFO) << "Docking autopilot waiting...";
    }
    int notAutoPilotCounter = 0;
    for (;;) {
        if (timer.expired()) {
            LOG(ERROR) << "Autopilot time expired";
            task->relogin();
        }
        sleep(250);
        ai::detectEDState(DetectLevel::Screen);
        if (st::ship.flags.docked) {
            LOG(INFO) << "Docking complete, status docked: " << st::ship.flags.docked
                      << ", docking event: " << (Cfg.dockingEvent ? Cfg.dockingEvent->event : "null");
            break;
        }
        auto de = Cfg.dockingEvent;
        if (!de || !(de->event == "DockingGranted" || de->event == "Docked")) {
            LOG(ERROR) << "Docking permission revoked, docking event: " << (Cfg.dockingEvent ? Cfg.dockingEvent->event : "null");
            return false;
        }
    }

    if (st::ship.flags.docked) {
        status = REFUEL;
        sleep(2000);
        ai::detectEDState(DetectLevel::Screen);
        if (ai::uiState.guiFocus == GuiFocus::None) {
            LOG(INFO) << "Refuel...";
            for (int i = 0; i < 4; i++)
                kbd::send("UI_Up");
            kbd::send("UI_Select", 0, 500); // refuel
            kbd::send("UI_Right");
            kbd::send("UI_Select", 0, 500); // repair
            kbd::send("UI_Right");
            kbd::send("UI_Select", 0, 500); // rearm
        }
    }

    return true;
}

std::string BaseDockStep::getTitle() {
    if (status == DONE)
        return lc_format("Docked to: {}", toDock);
    return lc_format("Docking to: {}", toDock);
}

std::string BaseDockStep::getStatus() {
    switch (status) {
    default:
        return {};
    case PREPARE:
        return _gt("Prepare docking");
    case APPROACH:
        if (!lastDockingStatus.empty())
            return lc_format("{}\nApproach\n  dist {}", lastDockingStatus, st::autopilot.distanceToDock.to_string());
        return lc_format("Approach\n  dist {}", st::autopilot.distanceToDock.to_string());
    case REQUEST:
        if (!lastDockingStatus.empty())
            return lc_format("{}\nRequesting permit", lastDockingStatus);
        return _gt("Requesting permit");
    case WAITING:
        if (!lastDockingStatus.empty())
            return lc_format("{}\nWaiting", lastDockingStatus);
        return "Waiting";
    case AUTOPILOT:
        return lc_format("Autopilot {}", timer.left());
    case REFUEL:
        return _gt("Refuel");
    }
}

void DockSpaceStation::updateSafeDist() {
    if (st::autopilot.destDock) {
        switch (st::autopilot.destDock->type) {
        case TypeNav::SpaceOutpost:
        case TypeNav::FleetCarrier:
        case TypeNav::SquadronCarrier:
        case TypeNav::ColonisationShip:
        case TypeNav::Megaship:
        //case TypeNav::TrailblazerDream:
        case TypeNav::SpaceConstrDepot:
            safe_dist = 6500;
            return;
        default:
            break;
        }
    }
    if (st::space.stationType == "SpaceConstructionDepot" ||
        st::space.stationType == "TrailblazerDream" ||
        st::space.stationType == "FleetCarrier")
        safe_dist = 6500;
}

bool DockSpaceStation::run() {
    if (st::ship.flags.cruise)
        throw_trouble("Docking not possible in super-cruise mode");
    if (st::ship.flags.docked) {
        notify_progress(MSG_INFO, "Docking - already docked");
        prevSubStep.reset();
        currSubStep.reset();
        status = DONE;
        return true;
    }
    if (!canDock())
        throw_failed("Destination is not dockable");
    ai::detectEDState(DetectLevel::Screen);
    if (ai::uiState.autopilot)
        throw_trouble("Docking request while autopilot is active");

    setSpeed(0);
    updateSafeDist();

    status = PREPARE;
    // leave all UI panels
    if (st::guiFocus != GuiFocus::None) {
        for (int cnt = 0; cnt < 3; cnt++) {
            if (st::guiFocus == GuiFocus::None)
                break;
            kbd::send("UI_Back", 0, 1500);
            ai::detectEDState(DetectLevel::Screen);
        }
        if (st::guiFocus != GuiFocus::None)
            throw_trouble("Cannot enter cockpit mode");
    }

    if (!run_sub_step(new NavDockSelect))
        throw_trouble("Cannot target destination dock");
    if (!task->orientTowardTarget(5))
        throw_trouble("Cannot orient ship towards dock");

    // clear expired docking event
    auto de = Cfg.dockingEvent;
    if (de) {
        if ((de->timestamp - std::chrono::utc_clock::now()) > 15min) {
            Cfg.dockingEvent.reset();
            de.reset();
        }
    }
    // try to dock, retry if something goes wrong
    for (int cnt=0; cnt < 10; cnt++) {
        status = WAITING;
        de = Cfg.dockingEvent;
        // end loop if we granted to tock
        if (de && (de->event == "DockingGranted" || de->event == "Docked"))
            break;
        // if we are close enough (or don't know the distance) - request docking permit
        if (!st::autopilot.distanceToDock.valid() || st::autopilot.distanceToDock.get(dist_t::M) >= dock_req_dist) {
            flyTowardsTarget();
            continue;
        }
        de = requestDockingPermit();
        LOG(INFO) << "Docking status: " << (de ? de->event : "null");
        if (de) {
            if (de->event == "DockingDenied")
                lastDockingStatus = de->event + ": " + de->data["Reason"].as_string();
            else
                lastDockingStatus = de->event;
            updateSafeDist();
        }
        if (de && (de->event == "DockingGranted" || de->event == "Docked"))
            break;
        if (!de || de->event == "DockingRequested") {
            // need to wait a bit
            sleep(2000);
            continue;
        }
        if (de->event == "DockingCancelled") {
            // oops, we canceled docking, try again
            sleep(2000);
            continue;
        }
        if (de->event == "DockingTimeout") {
            // have not completed docking in time, try docking again
            continue;
        }
        // NoSpace, TooLarge, Hostile, Offences, Distance, ActiveFighter, NoReason, etc.
        if (de->event == "DockingDenied") {
            auto reason = de->data["Reason"].as_string();
            if (reason == "NoSpace") {
                LOG(ERROR) << "DockingDenied reason: NoSpace, waiting...";
                sleep(5000);
                cnt = 0;
                continue;
            }
            if (reason == "Distance") {
                LOG(ERROR) << "DockingDenied reason: Distance, flying towards station...";
                // need to get close
                flyTowardsTarget();
                continue;
            }
        }
        // all others are fatal
        throw_failed("Unknown docking event: {}", de->event);
    }
    if (st::ship.flags.docked || (de && de->event == "Docked")) {
        LOG(ERROR) << "Docking - already docked";
        prevSubStep.reset();
        currSubStep.reset();
        status = DONE;
        return true;
    }
    if (!de || de->event != "DockingGranted") {
        LOG(ERROR) << "Docking not granted";
        return false;
    }

    if (autopilot()) {
        prevSubStep.reset();
        currSubStep.reset();
        status = DONE;
        return true;
    }
    return false;
}

bool DockSpaceStation::getDockDistance() {
    if (st::autopilot.distanceToDock.valid()) {
        auto d = st::autopilot.distanceToDock.get(dist_t::M);
        if (d > 6000 && d < 10000)
            return true;
    }
    std::array<dist_t,4> dist;
    int di = 0;
    for (int fails=0; fails < 10; fails++) {
        if (fails % 3 == 2) {
            if (st::guiFocus != GuiFocus::None)
                kbd::send("UI_Back", 100, 500);
            rollBlindCompass();
        }
        if (st::guiFocus == GuiFocus::None) {
            if (!dist[di].valid() || dist[di].get(dist_t::M) < 1) {
                ai::detectEDState(DetectLevel::Screen);
                dist_t d = ai::compassInfo.nav_target_dist;
                if (d.valid() && d.get(dist_t::M) > 100) {
                    if (d.get(dist_t::M) > 6000 && d.get(dist_t::M) < 10000) {
                        st::autopilot.distanceToDock = d;
                        return true;
                    }
                    dist[di++] = d;
                }
            }
            if (!dist[di].valid() || dist[di].get(dist_t::M) < 1) {
                dist_t d = task->nl.getFocusedDist(2);
                kbd::send("UI_Back", 100, 1000);
                if (d.valid() && d.get(dist_t::M) > 100) {
                    if (d.get(dist_t::M) > 6000 && d.get(dist_t::M) < 10000) {
                        st::autopilot.distanceToDock = d;
                        return true;
                    }
                    dist[di++] = d;
                }
            }
        } else {
            if (!dist[di].valid() || dist[di].get(dist_t::M) < 1) {
                dist_t d = task->nl.getFocusedDist(2);
                kbd::send("UI_Back", 100, 1000);
                if (d.valid() && d.get(dist_t::M) > 100) {
                    if (d.get(dist_t::M) > 6000 && d.get(dist_t::M) < 10000) {
                        st::autopilot.distanceToDock = d;
                        return true;
                    }
                    dist[di++] = d;
                }
            }
            if (!dist[di].valid() || dist[di].get(dist_t::M) < 1) {
                ai::detectEDState(DetectLevel::Screen);
                dist_t d = ai::compassInfo.nav_target_dist;
                if (d.valid() && d.get(dist_t::M) > 100) {
                    if (d.get(dist_t::M) > 6000 && d.get(dist_t::M) < 10000) {
                        st::autopilot.distanceToDock = d;
                        return true;
                    }
                    dist[di++] = d;
                }
            }
        }
        if (di >= 2) {
            for (int i=1; i < di; i++) {
                if (dist[i].get(dist_t::M) != dist[0].get(dist_t::M)) {
                    dist = {};
                    di = 0;
                    break;
                }
            }
        }
        if (di < 2)
            continue;
        st::autopilot.distanceToDock = dist[0];
        return true;
    }
    return false;
}

bool DockSpaceStation::flyTowardsTarget() {
    status = APPROACH;
    setSpeed(0);
    st::autopilot.distanceToDock = {};
    if (!getDockDistance())
        return false;
    if (st::autopilot.distanceToDock.get(dist_t::M) < dock_req_dist) {
        setSpeed(0);
        return true;
    }
    CourseLocker course(0);
    flyTowardsStep();
    setSpeed(0);
    return false;
}

bool DockSpaceStation::flyTowardsStep() {
    if (st::autopilot.distanceToDock.get(dist_t::M) < dock_req_dist) {
        setSpeed(0);
        return true;
    }
    // distance to fly for auto-docking
    double dist = st::autopilot.distanceToDock.get(dist_t::M) - safe_dist;
    auto ship = eddb::getShipStats();
    double fwdacc = ship->getForwardAccel();
    double revacc = ship->getReverseAccel();
    int max_speed_percent = 100;
    if (dist < 700.)
        max_speed_percent = 50;
    else if (dist < 3000.)
        max_speed_percent = 75;
    double topspd = ship->getThrustSpeed() * max_speed_percent / 100.;
    // distance will be fwdacc*time1^2/2 + speed*time2 + revacc*time3^2/2
    // speed = fwdacc*time1
    // speed = revacc*time3
    // time3 = fwdacc*time1/revacc
    // distance during accelertion + breaking will be
    // dist = fwdacc*time1^2/2 + revacc*time3^2/2
    // dist = fwdacc*time1^2/2 + revacc*(fwdacc*time1/revacc)^2/2
    // dist = fwdacc*(1 + fwdacc/revacc)*time1^2/2
    // time1^2 = 2*dist / fwdacc*(1 + fwdacc/revacc)
    // acceleration time = sqrt(2*dist / (fwdacc + fwdacc^2/revacc))
    // max acceleration time (to reach top speed) will be
    // max acceleration time = topspd / fwdacc
    double accel_time = std::sqrt( 2*dist / (fwdacc*(1 + fwdacc/revacc)) );
    double break_time = accel_time*fwdacc/revacc;
    if (accel_time < topspd/fwdacc) {
        // acceleration time is not long enough to reach top speed,
        // so just accelerate and break immediately
        setSpeed(max_speed_percent);
        sleep(accel_time*1000);
        setSpeed(max_speed_percent);
        sleep(break_time*1000 + 500);
        return true;
    }
    // we need to fly some space with top speed
    accel_time = topspd / fwdacc;
    break_time = topspd / revacc;
    double accel_dist = fwdacc*accel_time*accel_time/2;
    double break_dist = revacc*break_time*break_time/2;
    double fly_dist = dist - accel_dist - break_dist;
    double fly_time = fly_dist / topspd;
    if (fly_time > 10)
        fly_time = 10; // limit time for one step
    setSpeed(max_speed_percent);
    sleep(accel_time*1000);
    sleep(fly_time*1000);
    setSpeed(0);
    sleep(break_time*1000 + 500);
    return true;
}

bool DockPlanetPort::run() {
    if (st::ship.flags.cruise)
        throw_trouble("Docking not possible in super-cruise mode");
    if (st::ship.flags.docked) {
        notify_progress(MSG_INFO, "Docking - already docked");
        prevSubStep.reset();
        currSubStep.reset();
        status = DONE;
        return true;
    }
    if (!canDock())
        throw_failed("Destination is not dockable");
    ai::detectEDState(DetectLevel::Screen);
    if (ai::uiState.autopilot)
        throw_trouble("Docking request while autopilot is active");


    BlindLock blindLock(ROLL_BLIND_NONE);
    normalizeOrientation();

    // clear expired docking event
    auto de = Cfg.dockingEvent;
    if (de) {
        if ((de->timestamp - std::chrono::utc_clock::now()) > 15min) {
            Cfg.dockingEvent.reset();
            de.reset();
        }
    }
    // try to dock, retry if something goes wrong
    for (int cnt=0; cnt < 10; cnt++) {
        status = WAITING;
        de = Cfg.dockingEvent;
        // end loop if we granted to tock
        if (de && (de->event == "DockingGranted" || de->event == "Docked"))
            break;
        // if we are close enough (or don't know the distance) - request docking permit
        dist_t dist = getDockDistance(true);
        if (!dist.valid() || dist.get(dist_t::M) > 7000) {
            flyTowardsTarget();
            continue;
        }
        de = requestDockingPermit();
        LOG(INFO) << "Docking status: " << (de ? de->event : "null");
        if (de) {
            if (de->event == "DockingDenied")
                lastDockingStatus = de->event + ": " + de->data["Reason"].as_string();
            else
                lastDockingStatus = de->event;
        }
        if (de && (de->event == "DockingGranted" || de->event == "Docked"))
            break;
        if (!de || de->event == "DockingRequested") {
            // need to wait a bit
            sleep(2000);
            continue;
        }
        if (de->event == "DockingCancelled") {
            // oops, we canceled docking, try again
            sleep(2000);
            continue;
        }
        if (de->event == "DockingTimeout") {
            // have not completed docking in time, try docking again
            continue;
        }
        // NoSpace, TooLarge, Hostile, Offences, Distance, ActiveFighter, NoReason, etc.
        if (de->event == "DockingDenied") {
            auto reason = de->data["Reason"].as_string();
            if (reason == "NoSpace") {
                LOG(ERROR) << "DockingDenied reason: NoSpace, waiting...";
                sleep(5000);
                cnt = 0;
                continue;
            }
            if (reason == "Distance") {
                LOG(ERROR) << "DockingDenied reason: Distance, flying towards station...";
                // need to get close
                flyTowardsTarget();
                continue;
            }
        }
        // all others are fatal
        throw_failed("Unknown docking event: {}", de->event);
    }
    if (st::ship.flags.docked || (de && de->event == "Docked")) {
        LOG(ERROR) << "Docking - already docked";
        prevSubStep.reset();
        currSubStep.reset();
        status = DONE;
        return true;
    }
    if (!de || de->event != "DockingGranted") {
        LOG(ERROR) << "Docking not granted";
        return false;
    }

    if (autopilot()) {
        prevSubStep.reset();
        currSubStep.reset();
        status = DONE;
        return true;
    }
    return false;
}

dist_t DockPlanetPort::getDockDistance(bool force) {
    if (st::compass.hemisphere) {
        double angle = std::abs(90 + st::compass.targetPitch);
        if (angle < 60) {
            double altitude = st::shipAtBody.altitude;
            double d = altitude / std::cos(angle * M_PI / 180);
            st::autopilot.distanceToDock = dist_t(dist_t::M, d);
            return st::autopilot.distanceToDock;
        }
    }
    if (force) {
        dist_t d = task->nl.getFocusedDist(3);
        if (!d.valid())
            return d;
        st::autopilot.distanceToDock = d;
        return st::autopilot.distanceToDock;
    }
    return {};
}

bool DockPlanetPort::checkYaw() {
    if (st::guiFocus != GuiFocus::None)
        kbd::send("UI_Back", 0, 1500);
    task->orientPitchStep(-35, 5000);
    for (int i=0; i < 5; i++) {
        ai::detectEDState(DetectLevel::Screen);
        if (!ai::compassInfo.hemisphere) {
            LOG(WARNING) << "Compass not detected";
            continue;
        }
        float yaw = ai::compassInfo.targetYaw;
        if (std::abs(yaw) > 5)
            task->orientYawStep(yaw, 5000);
        break;
    }
    task->orientPitchStep(+30, 5000);
    for (int i=0; i < 5; i++) {
        ai::detectEDState(DetectLevel::Screen);
        if (!ai::compassInfo.hemisphere) {
            LOG(WARNING) << "Compass not detected";
            continue;
        }
        float roll = ai::compassInfo.targetRoll;
        if (std::abs(roll) < 165) {
            task->orientRollStep(roll-180, 5000);
            continue;
        }
        break;
    }
    return true;
}

bool DockPlanetPort::normalizeOrientation() {
    setSpeed(0, true);

    status = PREPARE;
    if (!run_sub_step(new NavBodySelect))
        return false;
    for (;;) {
        ai::detectEDState(DetectLevel::Screen);
        if (ai::uiState.guiFocus != GuiFocus::None) {
            kbd::send("UI_Back", 0, 1500);
            continue;
        }
        if (!ai::compassInfo.hemisphere) {
            LOG(WARNING) << "Compass not detected";
            continue;
        }
        float roll = ai::compassInfo.targetRoll;
        float pitch = ai::compassInfo.targetPitch;
        if (std::abs(roll) <= 165) {
            task->orientRollStep(roll-180, 5000);
            continue;
        }
        if (std::abs(pitch+90) > 5) {
            task->orientPitchStep(pitch+90, 5000);
            continue;
        }
        break;
    }
    if (!run_sub_step(new NavDockSelect))
        return false;
    return true;
}

bool DockPlanetPort::flyTowardsTarget() {
    status = APPROACH;
    for (int step=0;;step++) {
        if (st::guiFocus != GuiFocus::None) {
            kbd::send("UI_Back", 0, 1500);
            step = 0;
            continue;
        }
        if ((step % 10) == 0) {
            checkYaw();
            step = 0;
            continue;
        }
        if (st::shipAtBody.altitude < 2000) {
            kbd::send("UpThrustButton", 3000, 500);
            continue;
        }
        if (st::shipAtBody.altitude > 6500) {
            kbd::send("DownThrustButton", 1000, 500);
            continue;
        }
        ai::detectEDState(DetectLevel::Screen);
        dist_t dist = getDockDistance((step % 10) == 0);
        if (dist.valid() && dist.get(dist_t::M) < 7000)
            return true;
        if (!ai::compassInfo.hemisphere) {
            LOG(WARNING) << "Compass not detected";
            step -= 1;
            continue;
        }
        float yaw = ai::compassInfo.targetYaw;
        float pitch = ai::compassInfo.targetPitch;
        if ((pitch > -60 || pitch < -120) && std::abs(yaw) > 5) {
            task->orientYawStep(yaw, 5000);
            continue;
        }
        if (pitch > -70) {
            setSpeed(75);
            sleep(5000);
            setSpeed(0);
            continue;
        }
        if (pitch < -110) {
            setSpeed(-25);
            sleep(5000);
            setSpeed(0);
            continue;
        }
    }
    return true;
}


bool NavDockSelect::run() {
    setSpeed(0);
    if (!dock) {
        dock = st::autopilot.destDock;
        if (!dock) {
            status = FAILED;
            throw_failed("No destination dock");
            return false;
        }
    }

    status = SELECTING;
    for (int retry = 0; retry < 3; retry++) {
        if (!task->nl.focusDestDock()) {
            notify_progress(MSG_WARN, "Failed to find the dock in nav list");
            continue;
        }
        if (!task->nl.selectFocused())
            notify_progress(MSG_WARN, "Failed to select the dock in nav list");
        sleep(500);
        if (dock->nameEq(st::destination.name)) {
            status = DONE;
            return true;
        }
    }
    status = FAILED;
    return false;
}
std::string NavDockSelect::getTitle() {
    std::string name;
    if (dock) {
        if (!dock->nloc.empty())
            name = dock->nloc;
        else
            name = dock->name;
    }
    switch (status) {
    default:
        return lc_format("Selecting dock: {}", name);
    case FAILED:
        return lc_format("Cannot select dock: {}", name);
    case DONE:
        return lc_format("Selected dock: {}", name);
    }
}

bool NavBodySelect::run() {
    setSpeed(0);
    if (!body) {
        body = st::autopilot.destBody;
        if (!body) {
            status = FAILED;
            throw_failed("No destination body");
            return false;
        }
    }

    status = SELECTING;
    for (int retry=0; retry < 3; retry++) {
        if (!task->nl.focusDestBody()) {
            notify_progress(MSG_WARN, "Failed to find the body in nav list");
            continue;
        }
        if (!task->nl.selectFocused())
            notify_progress(MSG_WARN, "Failed to select the body in nav list");
        sleep(500);
        if (body->nameEq(st::destination.name)) {
            status = DONE;
            return true;
        }
    }
    status = FAILED;
    return false;
}
std::string NavBodySelect::getTitle() {
    std::string type;
    std::string name;
    if (body) {
        name = body->name;
        if (body->type == TypeNav::Star)
            type = "star";
        else if (body->type == TypeNav::Planet)
            type = "planet";
    }
    switch (status) {
    default:
        return lc_format("Selecting {0}: {1}", type, name);
    case FAILED:
        return lc_format("Cannot select {0}: {1}", type, name);
    case DONE:
        return lc_format("Selected {0}: {1}", type, name);
    }
}

bool BaseCruiseStep::gotDistance(dist_t dist) {
    if (dist.valid()) {
        failCount = 0;
        currentDist = dist;
        return true;
    }
    failCount += 1;
    if (useNavList && failCount >= 3)
        status = DIST_BAD;
    else if (!useNavList && failCount >= 5)
        status = DIST_BAD;
    return false;
}

bool CruiseToSignal::run() {
    // select destination dock or body
    if (st::destination.name.empty())
        return false;
    if (!requiredDist.valid())
        return false;

    destName = st::destination.name;
    int focusIdx;
    if (!task->nl.focusDestination(focusIdx))
        return false;

    failCount = 0;
    if (st::guiFocus == GuiFocus::None) {
        ai::detectEDState(DetectLevel::Screen);
        gotDistance(ai::compassInfo.nav_target_dist);
    } else {
        dist_t dist = task->nl.getFocusedDist(3);
        gotDistance(dist);
    }

    if (currentDist.valid() && currentDist <= requiredDist) {
        status = DIST_STOP;
        setSpeed(0, true);
        if (useNavList)
            kbd::send("UI_Back", 100);
        sleep(5000);
        prevSubStep.reset();
        currSubStep.reset();
        status = DONE;
        return true;
    }

    if (currentDist.valid() && currentDist.get(dist_t::LS) > 100)
        setSpeed(50, true);
    else
        setSpeed(0, true);

    notify_progress(MSG_INFO, "Fly towards");
    useNavList = false;
    task->orientTowardTarget(5);

    failCount = 0;

    CourseLocker course(0);
    // wait until we get to required distance
    for (;;) {
        if (!st::ship.flags.cruise) {
            setSpeed(0);
            throw_trouble("Unexpected cruise exit");
        }
        if (st::shipAtBody.approachBody || st::shipAtBody.nearBody) {
            setSpeed(0);
            throw_trouble("Unexpected close to body: {}", st::shipAtBody.bodyName);
        }
        if (useNavList) {
            dist_t focused_dist = task->nl.getFocusedDist(1);
            if (!gotDistance(focused_dist)) {
                if (failCount >= 3) {
                    notify_progress(MSG_WARN, "Bad dist in nav list");
                    useNavList = false;
                    failCount = 0;
                } else if (failCount & 1) {
                    rollBlindCompass();
                }
                continue;
            }
            useNavList = false;
            failCount = 0;
            kbd::send("UI_Back", 100, 1000);
        } else {
            ai::detectEDState(DetectLevel::Screen);
            if (ai::uiState.guiFocus != GuiFocus::None) {
                kbd::send("UI_Back", 100, 1000);
                continue;
            }
            dist_t compass_dist = ai::compassInfo.nav_target_dist;
            if (!gotDistance(compass_dist)) {
                if (ai::compassInfo.has_nav_target) {
                    if (failCount >= 15) {
                        notify_progress(MSG_WARN, "Bad dist in target mark");
                        useNavList = true;
                        failCount = 0;
                    } else if (failCount % 5 == 0) {
                        rollBlindCompass();
                    }
                }
                else {
                    if (failCount >= 3) {
                        notify_progress(MSG_WARN, "Cannot see target mark");
                        useNavList = true;
                        failCount = 0;
                        setSpeed(75);
                    }
                    else if (ai::compassInfo.hemisphere < 0 || std::abs(ai::compassInfo.targetAngle) > 10) {
                        setSpeed(0);
                        task->orientTowardTarget(5);
                    }
                }
                continue;
            }
        }
        if (currentDist >= requiredDist) {
            status = DIST_STOP;
            setSpeed(0);
            if (useNavList)
                kbd::send("UI_Back", 100);
            sleep(5000);
            prevSubStep.reset();
            currSubStep.reset();
            status = DONE;
            return true;
        } else {
            if (currentDist <= requiredDist * 1.5) {
                status = DIST_NEAR;
                setSpeed(25);
            }
            else if (currentDist <= requiredDist * 5) {
                status = DIST_FAR;
                setSpeed(50);
            }
            else if (currentDist <= 300_ls) {
                status = DIST_FAR;
                setSpeed(75);
            }
            else {
                status = DIST_FAR;
                setSpeed(100);
            }
        }
    }

    prevSubStep.reset();
    currSubStep.reset();
    status = DONE;
    return true;
}

bool CruiseToDistStep::run() {
    // select destination dock or body
    if (!st::autopilot.destDock && !st::autopilot.destBody)
        return false;
    if (!minDist.valid() || !maxDist.valid() || minDist >= maxDist)
        return false;
    bool destIsSignal = false;
    bool destIsDock = false;
    if (st::autopilot.destDock && st::autopilot.destDock->nameEq(st::destination.name)) {
        destIsDock = true;
        destName = st::destination.name;
        if (!task->nl.focusDestDock())
            return false;
    }
    else if (st::autopilot.destBody && st::autopilot.destBody->nameEq(st::destination.name)) {
        destIsDock = false;
        destName = st::destination.name;
        if (!task->nl.focusDestBody())
            return false;
    }
    else if (st::autopilot.destBody) {
        destIsDock = false;
        destName = st::autopilot.destBody->name;
        if (!run_sub_step(new NavBodySelect))
            return false;
    }
    else if (st::autopilot.destDock) {
        destIsDock = true;
        if (!st::autopilot.destDock->nloc.empty())
            destName = st::autopilot.destDock->nloc;
        else
            destName = st::autopilot.destDock->name;
        if (!run_sub_step(new NavDockSelect))
            return false;
    }
    else
        return false;

    if (!(st::autopilot.isDestDockFocused || st::autopilot.isDestBodyFocused))
        return false;

    failCount = 0;
    requiredDist = maxDist;
    if (st::guiFocus == GuiFocus::None) {
        ai::detectEDState(DetectLevel::Screen);
        gotDistance(ai::compassInfo.nav_target_dist);
    } else {
        dist_t dist = task->nl.getFocusedDist(3);
        gotDistance(dist);
    }

    if (currentDist.valid()) {
        if (currentDist >= minDist && currentDist <= maxDist) {
            status = DIST_STOP;
            setSpeed(0, true);
            if (useNavList)
                kbd::send("UI_Back", 100);
            sleep(5000);
            prevSubStep.reset();
            currSubStep.reset();
            status = DONE;
            return true;
        }
        flyAway = currentDist < minDist;
    }

    if (currentDist.valid() && currentDist.get(dist_t::LS) > 100)
        setSpeed(50, true);
    else
        setSpeed(0, true);

    if (flyAway) {
        requiredDist = minDist;
        notify_progress(MSG_INFO, "Fly away");
        useNavList = true;
        task->orientAwayFromTarget(5);
    } else {
        requiredDist = maxDist;
        notify_progress(MSG_INFO, "Fly towards");
        useNavList = false;
        task->orientTowardTarget(5);
    }
    failCount = 0;

    CourseLocker course(flyAway ? 180 : 0);
    // wait until we get to required distance
    for (;;) {
        if (!st::ship.flags.cruise) {
            setSpeed(0);
            throw_trouble("Unexpected cruise exit");
        }
        if (st::shipAtBody.approachBody || st::shipAtBody.nearBody) {
            setSpeed(0);
            throw_trouble("Unexpected close to body: {}", st::shipAtBody.bodyName);
        }
        if (flyAway)
            useNavList = true;
        if (useNavList) {
            dist_t focused_dist = task->nl.getFocusedDist(1);
            if (!gotDistance(focused_dist)) {
                if (!flyAway && failCount >= 3) {
                    notify_progress(MSG_WARN, "Bad dist in nav list");
                    useNavList = false;
                    failCount = 0;
                } else if (failCount & 1) {
                    rollBlindCompass();
                }
                if (flyAway || failCount >= 3)
                    setSpeed(75);
                continue;
            }
            if (!flyAway) {
                useNavList = false;
                failCount = 0;
                kbd::send("UI_Back", 100, 1000);
            }
        } else {
            ai::detectEDState(DetectLevel::Screen);
            if (ai::uiState.guiFocus != GuiFocus::None) {
                kbd::send("UI_Back", 100, 1000);
                continue;
            }
            dist_t compass_dist = ai::compassInfo.nav_target_dist;
            if (!gotDistance(compass_dist)) {
                if (ai::compassInfo.has_nav_target) {
                    if (failCount >= 15) {
                        notify_progress(MSG_WARN, "Bad dist in target mark");
                        useNavList = true;
                        failCount = 0;
                    } else if (failCount % 5 == 0) {
                        rollBlindCompass();
                    }
                    if (flyAway || failCount >= 3)
                        setSpeed(75);
                }
                else if (!flyAway) {
                    if (failCount >= 3) {
                        notify_progress(MSG_WARN, "Cannot see target mark");
                        useNavList = true;
                        failCount = 0;
                        setSpeed(75);
                    }
                    else if (ai::compassInfo.hemisphere < 0 || std::abs(ai::compassInfo.targetAngle) > 10) {
                        setSpeed(0);
                        task->orientTowardTarget(5);
                    }
                }
                else { // flyAway
                    useNavList = true;
                    failCount = 0;
                }
                continue;
            }
        }
        if (currentDist >= minDist && currentDist <= maxDist) {
            status = DIST_STOP;
            setSpeed(0);
            if (useNavList)
                kbd::send("UI_Back", 100);
            sleep(5000);
            prevSubStep.reset();
            currSubStep.reset();
            status = DONE;
            return true;
        }
        else if (currentDist < minDist) {
            if (!flyAway) {
                if (ai::uiState.guiFocus != GuiFocus::None) {
                    setSpeed(0);
                    kbd::send("UI_Back", 100, 1000);
                    continue;
                }
                flyAway = true;
                requiredDist = minDist;
                setSpeed(50);
                course.requestPitchRoll(180);
                task->orientAwayFromTarget(5);
                continue;
            }
            if (currentDist < minDist * 0.75) {
                status = DIST_NEAR;
                setSpeed(25);
            }
            else {
                status = DIST_FAR;
                setSpeed(100);
            }
        } else { // currentDist > minDist
            if (flyAway) {
                setSpeed(0);
                flyAway = false;
                requiredDist = maxDist;
                if (ai::uiState.guiFocus != GuiFocus::None)
                    kbd::send("UI_Back", 100, 1500);
                course.requestPitchRoll(0);
                task->orientTowardTarget(5);
                continue;
            }
            if (currentDist <= maxDist * 2) {
                status = DIST_NEAR;
                setSpeed(25);
            }
            if (currentDist <= 50_ls) {
                status = DIST_FAR;
                setSpeed(50);
            }
            else if (currentDist <= 300_ls) {
                status = DIST_FAR;
                setSpeed(75);
            }
            else {
                status = DIST_FAR;
                setSpeed(100);
            }
        }
    }

    prevSubStep.reset();
    currSubStep.reset();
    status = DONE;
    return true;
}

std::string BaseCruiseStep::getTitle() {
    if (status == DONE) {
        if (flyAway)
            return lc_format("Cruised from: {}", destName);
        else
            return lc_format("Cruised to: {}", destName);
    } else {
        if (flyAway)
            return lc_format("Cruising from: {}", destName);
        else
            return lc_format("Cruising to: {}", destName);
    }
}

std::string BaseCruiseStep::getStatus() {
    dist_t cur_mm = currentDist.convertTo(dist_t::MM);
    dist_t cur_ls = currentDist.convertTo(dist_t::LS);
    dist_t cur = (cur_ls.dist >= 0.1) ? cur_ls : cur_mm;

    dist_t req_mm = requiredDist.convertTo(dist_t::MM);
    dist_t req_ls = requiredDist.convertTo(dist_t::LS);
    dist_t req = (req_ls.dist >= 0.1) ? req_ls : req_mm;

    const char* st = "";
    switch (status) {
    case DONE:
    case READY:
        return {};
    case DIST_BAD: st=_gt("Dist bad"); break;
    case DIST_FAR: st=_gt("Dist far"); break;
    case DIST_NEAR: st=_gt("Dist near"); break;
    case DIST_STOP: st=_gt("Reached"); break;
    }
    return lc_format("{}: {} / {}\nspeeed: {}%", st,
                     cur.to_string(), req.to_string(),
                     st::autopilot.speed_set_to.has_value() ? std::to_string(st::autopilot.speed_set_to.value()) : "??");
}

bool DiveUnderPlanetStep::run() {
    bool ok = true;

    setSpeed(0);
    if (!st::autopilot.destBody || !st::autopilot.destDock)
        return false;

    bool targetIsDock = st::autopilot.destDock->nameEq(st::destination.name);
    bool targetIsBody = st::autopilot.destBody->nameEq(st::destination.name);

    if (!(targetIsBody || targetIsDock)) {
        if (run_sub_step(new NavDockSelect))
            targetIsDock = true;
        else if (run_sub_step(new NavBodySelect))
            targetIsBody = true;
        else
            return false;
    }

    toPort = (int(st::autopilot.destDock->type) & int(TypeNav::PlanetaryThing)) != 0;
    int maxCruisePitch = 8;
    if (st::shipInfo.shipType == "panthermkii")
        maxCruisePitch = 4;

    if (targetIsBody)
        st::autopilot.distanceToDock = {};
    if (targetIsDock)
        st::autopilot.distanceToBody = {};

    for (int fails=0; fails < 20; fails++) {
        if ((fails % 8) == 7) {
            status = FLY_DIVE;
            setSpeed(50);
            task->orientPitchStep(70);
            timer = utc_timer(20s);
            setSpeed(100);
            while (!timer.expired()) {
                if (!st::ship.flags.cruise) {
                    setSpeed(0);
                    throw_trouble("Unexpected cruise exit");
                }
                if (st::shipAtBody.approachBody || st::shipAtBody.nearBody) {
                    setSpeed(0);
                    return false;
                }
                sleep(250);
            }
            setSpeed(50);
            sleep(3000);
            task->orientPitchStep(-70);
            setSpeed(0);
            fails = 0;
            continue;
        }
        bool pointingToBody = false;
        bool pointingToDock = false;
        if (targetIsBody) {
            status = ORIENT_BODY;
            if (!st::autopilot.isDestBodyFocused && !task->nl.focusDestBody()) {
                rollBlindCompass();
                continue;
            }
            if (st::guiFocus != GuiFocus::None)
                kbd::send("UI_Back", 0, 1500);
            if (!task->orientTowardTarget(2))
                continue;
            pointingToBody = true;
            dist_t dist_body = st::autopilot.distanceToBody;
            if (!run_sub_step(new NavDockSelect))
                continue;
            kbd::send("UI_Back", 0, 1500);
            targetIsDock = true;
            // oriented towards body, but dock is selected target
            ai::detectEDState(DetectLevel::Screen);
            bool dockIsVisible = ai::compassInfo.has_nav_target;
            bool needRollAlign = true;
            float to_body_center_angle = ai::compassInfo.targetAngle;
            if (ai::compassInfo.hemisphere && dist_body.valid()) {
                double visible_body_angle = std::asin(st::autopilot.destBody->radius / dist_body.get(dist_t::KM)) * 180 / M_PI;
                if (to_body_center_angle < visible_body_angle * 0.85 && dockIsVisible)
                    needRollAlign = false;
                else if (to_body_center_angle > visible_body_angle * 1.5)
                    needRollAlign = false;
            }
            if (dockIsVisible || !needRollAlign) {
                if (needRollAlign) {
                    if (toPort) {
                        keepCruisePitch = 0;
                        task->orientRollByTarget(0, 5);
                        task->orientPitchStep(to_body_center_angle, 10000);
                    } else {
                        keepCruisePitch = maxCruisePitch;
                        task->orientRollByTarget(180, 5);
                        task->orientPitchStep(-to_body_center_angle, 10000);
                    }
                } else {
                    keepCruisePitch = 0;
                    task->orientTowardTarget(5);
                }
                if (toPort) {
                    keepCruisePitch = 0;
                    prevSubStep.reset();
                    currSubStep.reset();
                    status = DONE;
                    return true;
                }
                prevSubStep.reset();
                currSubStep.reset();
                status = DONE;
                return true;
            } else {
                task->orientRollByTarget(180, 20);
                if (!run_sub_step(new NavBodySelect))
                    continue;
                kbd::send("UI_Back", 0, 1500);
                targetIsBody = true;
            }
        }
        else if (targetIsDock) {
            status = ORIENT_DOCK;
            if (!st::autopilot.isDestDockFocused && !task->nl.focusDestDock()) {
                rollBlindCompass();
                continue;
            }
            if (st::guiFocus != GuiFocus::None)
                kbd::send("UI_Back", 0, 1500);
            if (!task->orientTowardTarget(2))
                continue;
            pointingToDock = true;
            bool dockIsVisible = ai::compassInfo.has_nav_target;
            if (dockIsVisible && toPort) {
                keepCruisePitch = 0;
                prevSubStep.reset();
                currSubStep.reset();
                status = DONE;
                return true;
            }
            if (!run_sub_step(new NavBodySelect))
                continue;
            kbd::send("UI_Back", 0, 1500);
            targetIsBody = true;
            bool needRollAlign = true;
            if (dockIsVisible) {
                ai::detectEDState(DetectLevel::Screen);
                if (st::compass.hemisphere && st::autopilot.distanceToBody.valid()) {
                    double to_body_center_angle = st::compass.targetAngle;
                    double dist_body = st::autopilot.distanceToBody.get(dist_t::KM);
                    double visible_body_angle =
                            std::asin(st::autopilot.destBody->radius / dist_body) * 180 / M_PI;
                    if (to_body_center_angle < visible_body_angle * 0.8)
                        needRollAlign = false;
                    else if (to_body_center_angle > visible_body_angle * 1.5)
                        needRollAlign = false;
                }
            }
            if (needRollAlign)
                task->orientRollByTarget(0, dockIsVisible ? 5 : 20);
            if (dockIsVisible) {
                if (!run_sub_step(new NavDockSelect))
                    continue;
                kbd::send("UI_Back", 0, 1500);
                targetIsDock = true;
                if (needRollAlign) {
                    keepCruisePitch = maxCruisePitch;
                } else {
                    keepCruisePitch = 0;
                }
                prevSubStep.reset();
                currSubStep.reset();
                status = DONE;
                return true;
            }
        }
        else {
            if (run_sub_step(new NavDockSelect))
                targetIsDock = true;
            else if (run_sub_step(new NavBodySelect))
                targetIsBody = true;
            else
                return false;
            continue;
        }
        assert (targetIsBody);
        if (!targetIsBody) {
            if (!run_sub_step(new NavBodySelect))
                throw_trouble("Cannot select body");
            kbd::send("UI_Back", 0, 1500);
        }
        // having distance to dock and body and angle between, calc nearest distance
        // between ship-dock line and body center, compare it with body radius
        if (pointingToDock)
            task->orientRollByTarget(0, 5);
        double dist_body = st::autopilot.distanceToBody.get(dist_t::KM);
        if (dist_body < 2.25*st::autopilot.destBody->radius)
            return false; // need to fly away
        int angle_to_dive = int(std::asin(2*st::autopilot.destBody->radius / dist_body) * 180 / M_PI);
        if (angle_to_dive < 20)
            angle_to_dive = 20;
        if (!orient_pitch(angle_to_dive))
            return false;
        if (!fly_dive(180-angle_to_dive-15))
            return false;
    }
    return false;
}

bool DiveUnderPlanetStep::orient_roll(float requiredRoll) {
    if (st::guiFocus != GuiFocus::None) {
        notify_progress(MSG_INFO, "Orientation: goto compass");
        kbd::send("UI_Back", 0, 1500);
    }
    const int rollPrecision = 5;

    for (int fails=0; fails < 5; fails++) {
        ai::detectEDState(DetectLevel::Screen);
        if (ai::uiState.guiFocus != GuiFocus::None) {
            notify_progress(MSG_WARN, std::format("Unexpected ui mode {}", ai::uiState.to_string()));
            kbd::send("UI_Back", 0, 1500);
            continue;
        }
        if (!ai::compassInfo.hemisphere) {
            notify_progress(MSG_WARN, std::format("Compass not detected, fails {}", fails));
            continue;
        }
        fails = 0;

        if (!ai::compassInfo.has_nav_target && ai::compassInfo.targetAngle < 5)
            return true;
        float delta = ai::compassInfo.targetRoll - requiredRoll;
        if (delta > 180) delta = 360-delta;
        if (delta < -180) delta = 360+delta;
        if (std::abs(delta) < rollPrecision)
            return true;
        task->orientRollStep(delta, 10000);
    }
    return false;
}

bool DiveUnderPlanetStep::orient_pitch(int pitchGoal) {
    status = ORIENT_DIVE;
    setSpeed(0);
    if (st::guiFocus != GuiFocus::None) {
        notify_progress(MSG_INFO, "Orientation: goto compass");
        kbd::send("UI_Back", 0, 1500);
    }
    const int rollPrecision = 5;
    const int pitchPrecision = 5;

    for (int fails=0; fails < 5; fails++) {
        ai::detectEDState(DetectLevel::Screen);
        if (ai::uiState.guiFocus != GuiFocus::None) {
            notify_progress(MSG_WARN, std::format("Unexpected ui mode {}", ai::uiState.to_string()));
            kbd::send("UI_Back", 0, 1500);
            continue;
        }
        if (!ai::compassInfo.hemisphere) {
            notify_progress(MSG_ERROR, std::format("Compass not detected, fails {}", fails));
            continue;
        }
        fails = 0;

        float delta = ai::compassInfo.targetPitch - pitchGoal;
        if (std::abs(delta) < pitchPrecision)
            break;
        task->orientPitchStep(delta);
    }
    if (std::abs(ai::compassInfo.targetPitch - pitchGoal) > rollPrecision)
        return false;

    return true;
}

bool DiveUnderPlanetStep::fly_dive(int pitchGoal) {
    status = FLY_DIVE;

    if (st::guiFocus != GuiFocus::None) {
        notify_progress(MSG_INFO, "Orientation: goto compass");
        kbd::send("UI_Back", 0, 1500);
    }
    setSpeed(75);
    for (;;) {
        if (!st::ship.flags.cruise) {
            setSpeed(0);
            throw_trouble("Unexpected cruise exit");
        }
        if (st::shipAtBody.approachBody || st::shipAtBody.nearBody) {
            setSpeed(0);
            throw_trouble("Unexpected close to body: {}", st::shipAtBody.bodyName);
        }
        ai::detectEDState(DetectLevel::Screen);
        if (ai::uiState.guiFocus != GuiFocus::None) {
            kbd::send("UI_Back", 100, 1000);
            continue;
        }
        if (ai::compassInfo.hemisphere && std::abs(ai::compassInfo.targetPitch) >= pitchGoal) {
            setSpeed(0);
            return true;
        }
        ai::sleep(1000);
    }
}

std::string DiveUnderPlanetStep::getTitle() {
    std::string name;
    if (st::autopilot.destDock) {
        if (!st::autopilot.destDock->nloc.empty())
            name = st::autopilot.destDock->nloc;
        else
            name = st::autopilot.destDock->name;
    }
    if (status == DONE)
        return lc_format("Aligned to: {}", name);
    return lc_format("Aligning to: {}", name);
}

std::string DiveUnderPlanetStep::getStatus() {
    switch (status) {
    case DONE:
    case READY:
        return {};
    case ORIENT_BODY:
        return _gt("Orient to body");
    case DIST_BODY:
        return _gt("Get distance to body");
    case ORIENT_DOCK:
        return _gt("Orient to dock");
    case DIST_DOCK:
        return _gt("Get distance to dock");
    case ORIENT_DIVE:
        return _gt("Orient to dive");
    case FLY_DIVE:
        return _gt("Dive fly");
    }
    return {};
}

bool ExitCruiseToSpace::run() {
    if (!st::ship.flags.cruise)
        throw_trouble("Unexpected cruise exit");

    dist_t dist_too_far(dist_t::LS, 5);
    status = ORIENT;
    setSpeed(0, true);
    if (!st::autopilot.destDock || !st::autopilot.destDock->nameEq(st::destination.name)) {
        if (!run_sub_step(new NavDockSelect))
            return false;
        kbd::send("UI_Back", 0, 1000);
    }
    if (!task->orientTowardTarget(5))
        return false;
    status = APPROACH;
    double dist_km = st::autopilot.distanceToDock.valid() ? st::autopilot.distanceToDock.get(dist_t::KM) : 15000;
    if (dist_km > dist_too_far.get(dist_t::KM))
        throw_trouble("Too far from dock");
    CourseLocker course(keepPitch);
    // wait until we get to 1mm
    for (;;) {
        if (!st::ship.flags.cruise)
            throw_trouble("Unexpected cruise exit");
        if (st::guiFocus != GuiFocus::None) {
            kbd::send("UI_Back", 0, 1000);
            continue;
        }
        ai::detectEDState(DetectLevel::Screen);
        if (!st::compass.has_nav_target || std::abs(st::compass.targetYaw) > 10 || std::abs(st::compass.targetPitch-keepPitch) > 10)
            st::autopilot.distanceToDock = {};

        if (!st::autopilot.distanceToDock.valid()) {
            dist_fails += 1;
            if (dist_km < 5000 && dist_fails >= 5) {
                setSpeed(0);
                task->orientTowardTarget(5);
            }
            else if ((dist_fails % 5) == 4 && keepPitch == 0)
                rollBlindCompass();
            if (dist_fails > 100)
                throw_trouble("Lost compass or cruise direction");
            continue;
        }
        dist_fails = 0;
        dist_km = st::autopilot.distanceToDock.get(dist_t::KM);
        if (dist_km > dist_too_far.get(dist_t::KM))
            throw_trouble("Too far from dock");
        if (dist_km < 1000)
            exit_confirm += 1;
        else
            exit_confirm = 0;
        if (exit_confirm >= 2)
            break;
        if (dist_km < 3000) {
            setSpeed(25);
            if (keepPitch) {
                keepPitch = 0;
                course.requestPitchRoll(0);
            }
        }
        else if (dist_km < 6000)
            setSpeed(50);
        else
            setSpeed(75);
    }

    // wait until we exit super-cruise
    timer = utc_timer(10s);
    status = EXITING;
    setSpeed(25);
    while (st::ship.flags.cruise && !timer.expired()) {
        kbd::send("HyperSuperCombination", 100, 1000);
        sleep(1000);
        if (st::guiFocus != GuiFocus::None)
            kbd::send("UI_Back", 0, 1000);
    }

    notify_progress(MSG_INFO, "Arrived, speed zero");
    setSpeed(0);
    sleep(500);

    for (dist_fails=0; dist_fails < 15; dist_fails++) {
        if (st::guiFocus != GuiFocus::None)
            kbd::send("UI_Back", 0, 1000);
        ai::detectEDState(DetectLevel::Screen);
        if (ai::compassInfo.nav_target_dist.valid()) {
            if (ai::compassInfo.nav_target_dist.get(dist_t::KM) > 25) {
                throw_trouble("Unexpected distance after cruise exit: {}", ai::compassInfo.nav_target_dist.to_string());
            }
            prevSubStep.reset();
            currSubStep.reset();
            status = DONE;
            return true;
        }
        if ((dist_fails % 3) == 2)
            rollBlindCompass();
    }

    notify_progress(MSG_WARN, "Cannot confirm distance after cruise exit");
    prevSubStep.reset();
    currSubStep.reset();
    status = DONE;
    return true;
}

std::string ExitCruiseToSpace::getTitle() {
    std::string name;
    if (st::autopilot.destDock) {
        if (!st::autopilot.destDock->nloc.empty())
            name = st::autopilot.destDock->nloc;
        else
            name = st::autopilot.destDock->name;
    }
    if (status == DONE)
        return lc_format("Exited cruise at: {}", name);
    return lc_format("Exiting cruise to: {}", name);
}

std::string ExitCruiseToSpace::getStatus() {
    if (status == ORIENT)
        return _gt("Orienting towards target");
    if (status == APPROACH) {
        if (dist_fails)
            return lc_format("Approaching:\ndist {} (fails {})\nspeeed {}%", st::autopilot.distanceToDock.to_string(), dist_fails,
                               st::autopilot.speed_set_to.has_value() ? std::to_string(st::autopilot.speed_set_to.value()) : "??");
        else if (exit_confirm)
            return lc_format("Approaching:\ndist {} (confirm {})\nspeeed {}%", st::autopilot.distanceToDock.to_string(), exit_confirm,
                             st::autopilot.speed_set_to.has_value() ? std::to_string(st::autopilot.speed_set_to.value()) : "??");
        else
            return lc_format("Approaching:\ndist {}\nspeeed {}%", st::autopilot.distanceToDock.to_string(),
                             st::autopilot.speed_set_to.has_value() ? std::to_string(st::autopilot.speed_set_to.value()) : "??");
    }
    if (status == EXITING)
        return lc_format("Exiting cruise {}", timer.left());
    if (status == CONFIRM)
        return lc_format("Checking distance\nfails {}", dist_fails);
    return {};
}

bool ExitCruiseToPlanet::run() {
    if (!st::ship.flags.cruise)
        throw_trouble("Unexpected cruise exit");

    setSpeed(0, true);
    status = ORIENT;
    if (!run_sub_step(new NavDockSelect))
        return false;
    if (!task->orientTowardTarget(2))
        return false;
    if (!(st::shipAtBody.approachBody || st::shipAtBody.nearBody)) {
        CourseLocker course(keepPitch);
        setSpeed(50, true);
        timer = utc_timer(2min);
        status = FLY_TO_BODY;
        while (!(st::shipAtBody.approachBody || st::shipAtBody.nearBody) && !timer.expired()) {
            if (!st::ship.flags.cruise)
                throw_trouble("Unexpected cruise exit");
            if (st::guiFocus != GuiFocus::None)
                kbd::send("UI_Back", 0, 1000);
            sleep(250);
        }
    }

    if (!(st::shipAtBody.approachBody || st::shipAtBody.nearBody))
        throw_trouble("Cannot get to body vicinity");

    bool angle_is_close_to_tangent = false;
    {
        status = ORIENT;
        setSpeed(0, true);
        if (!run_sub_step(new NavBodySelect))
            return false;
        task->orientRollByTarget(180, 7);
        double pitchToBody = st::compass.targetPitch;
        if (!run_sub_step(new NavDockSelect))
            return false;
        kbd::send("UI_Back", 0, 1000);
        for (int retry=0; retry < 5; retry++) {
            if (st::guiFocus != GuiFocus::None) {
                kbd::send("UI_Back", 0, 1000);
                continue;
            }
            ai::detectEDState(DetectLevel::Screen);
            if (st::compass.has_nav_target)
                break;
            sleep(500);
        }
        if (!st::compass.has_nav_target)
            throw_trouble("Cannot see destination site");
        double pitchToDock = st::compass.targetPitch;
        double R = st::autopilot.destBody->radius;
        double altitude = st::shipAtBody.altitude * 0.001;
        double dist_to_body_center = R + altitude;
        double tangent = std::asin(R / dist_to_body_center) * 180 / M_PI;
        if (tangent - std::abs(pitchToDock - pitchToBody) < 15)
            angle_is_close_to_tangent = true;

        setSpeed(angle_is_close_to_tangent ? 50 : 25);
    }
    if (!st::ship.flags.cruise)
        throw_trouble("Unexpected cruise exit");
    {
        timer = utc_timer(angle_is_close_to_tangent ? 4min : 3min);
        status = APPROACH;
        bool check_dist_pitch = angle_is_close_to_tangent;
        CourseLocker course(angle_is_close_to_tangent ? -7 : 0);
        while (st::ship.flags.cruise && !timer.expired()) {
            sleep(250);
            if (check_dist_pitch) {
                auto &d = st::autopilot.distanceToDock;
                if (d.valid() && d.get(dist_t::KM) < 300) {
                    check_dist_pitch = false;
                    course.requestPitchRoll(0);
                }
            }
        }
    }

    if (st::ship.flags.cruise)
        throw_trouble("Cannot reach planetary port");

    status = CONFIRM;
    notify_progress(MSG_INFO, "Arrived, speed zero");
    setSpeed(0, true);
    sleep(angle_is_close_to_tangent ? 4000 : 2000);

    bool distance_verified = false;
    double prev_dist_km = 0;
    for (int retry=0; retry < 15; retry++) {
        ai::detectEDState(DetectLevel::Screen);
        auto ai_dist = ai::compassInfo.nav_target_dist;
        if (ai_dist.valid()) {
            double dist_km = ai_dist.get(dist_t::KM);
            if (prev_dist_km > 0 && std::abs(prev_dist_km - dist_km) < 1) {
                if (dist_km > 50)
                    throw_trouble("Unexpected distance after cruise exit: {}", ai_dist.to_string());
                distance_verified = true;
                break;
            }
            prev_dist_km = dist_km;
        }
        sleep(500);
    }

    if (!distance_verified)
        notify_progress(MSG_WARN, "Cannot confirm distance after cruise exit");
    task->orientPitchStep(60);
    prevSubStep.reset();
    currSubStep.reset();
    status = DONE;
    return true;
}

std::string ExitCruiseToPlanet::getTitle() {
    std::string name;
    if (st::autopilot.destDock) {
        if (!st::autopilot.destDock->nloc.empty())
            name = st::autopilot.destDock->nloc;
        else
            name = st::autopilot.destDock->name;
    }
    if (status == DONE)
        return lc_format("Exited cruise at: {}", name);
    return lc_format("Exiting cruise to: {}", name);
}

std::string ExitCruiseToPlanet::getStatus() {
    if (status == ORIENT)
        return _gt("Orienting towards target");
    if (status == FLY_TO_BODY) {
        return lc_format("Fly to body: {}\ndist {}\nspeeed {}%", timer.passed(),
                         st::autopilot.distanceToDock.to_string(),
                         st::autopilot.speed_set_to.has_value() ? std::to_string(st::autopilot.speed_set_to.value()) : "??");
    }
    if (status == APPROACH) {
        return lc_format("Approaching {}:\ndist {}\nspeeed {}%", timer.passed(),
                        st::autopilot.distanceToDock.to_string(),
                        st::autopilot.speed_set_to.has_value() ? std::to_string(st::autopilot.speed_set_to.value()) : "??");
    }
    if (status == EXITING)
        return _gt("Exiting cruise");
    if (status == CONFIRM)
        return _gt("Checking distance");
    return {};
}

bool CompleteNavRoute::run() {
    int routeIdx = getNavRoutePosition();
    if (routeIdx < 0) {
        prevSubStep.reset();
        currSubStep.reset();
        status = DONE;
        return true;
    }
    targetNextNavRoute(routeIdx);

    bool try_fast_jump = false;
    if (st::ship.flags.docked) {
        if (!run_sub_step(new DepartureStep))
            throw_trouble("Cannot departure from dock");
        auto dep = std::dynamic_pointer_cast<DepartureStep>(prevSubStep);
        if (dep && !std::isnan(dep->pitchBeforeAutopilot)) {
            if (dep->pitchBeforeAutopilot > 10)
                try_fast_jump = true;
        }
    }
    if (try_fast_jump) {
        status = ORIENT;
        setSpeed(50);
        if (task->orientTowardTarget(6)) {
            if (ai::compassInfo.has_nav_target) {
                status = JUMP;
                run_sub_step(new HyperJumpStep);
            }
        }
    }

    if (st::shipAtBody.nearBody) {
        status = LEAVE_BODY;
        if (!run_sub_step(new LeaveBodyStep))
            return false;
    }

    int orientAvoid = 60;
    while ((routeIdx = getNavRoutePosition()) >= 0) {
        targetNextNavRoute(routeIdx);
        for (int retry=0; retry < 5; retry++) {
            status = ORIENT;
            setSpeed(50);
            if (!task->orientTowardTarget(6))
                return false;
            if (ai::compassInfo.has_nav_target)
                break;
            ai::detectEDState(DetectLevel::Screen);
            if (ai::compassInfo.has_nav_target)
                break;
            bool leave_body = st::shipAtBody.approachBody || st::shipAtBody.nearBody;
            if (!st::ship.flags.cruise) {
                leave_body = true;
                status = ENTER_CRUISE;
                if (!run_sub_step(new EnterCruiseStep))
                    return false;
                targetNextNavRoute(routeIdx);
            }
            if (leave_body) {
                status = LEAVE_BODY;
                if (!run_sub_step(new LeaveBodyStep))
                    return false;
                targetNextNavRoute(routeIdx);
            }
            status = ORIENT;
            setSpeed(50);
            task->orientPitchStep(orientAvoid, 10000);
            orientAvoid = -orientAvoid;
            timer = utc_timer(10s);
            setSpeed(100);
            status = FLY_AWAY;
            while (!timer.expired()) {
                if (!st::ship.flags.cruise) {
                    setSpeed(0);
                    throw_trouble("Unexpected cruise exit");
                }
                if (st::shipAtBody.approachBody || st::shipAtBody.nearBody) {
                    setSpeed(50);
                    break;
                }
                sleep(250);
            }
        }
        status = JUMP;
        if (!run_sub_step(new HyperJumpStep))
            return false;
    }
    prevSubStep.reset();
    currSubStep.reset();
    status = DONE;
    return true;
}

void CompleteNavRoute::targetNextNavRoute(int routeIdx) {
    if (st::destination.systemAddress != st::currentNavRoute->route[routeIdx].systemAddress)
        kbd::send("TargetNextRouteSystem", 0, 300);
}

std::string CompleteNavRoute::getTitle() {
    std::string name;
    int step = 0;
    int count = 0;
    auto nr = st::currentNavRoute;
    if (nr && !nr->route.empty()) {
        name = nr->route.back().starSystem;
        count = nr->route.size();
        step = getNavRoutePosition();
    }
    if (status == DONE)
        return lc_format("Routed to: {}", name);
    return lc_format("Routing {0}/{1} to: {2}", step, count, name);
}

std::string CompleteNavRoute::getStatus() {
    switch (status) {
    case DONE:
    case JUMP:
    case READY:
        return {};
    case ORIENT:
        return _gt("Orienting");
    case ENTER_CRUISE:
        return _gt("Entering cruise");
    case LEAVE_BODY:
        return _gt("Fly away from nearest body");
    case FLY_AWAY:
        return lc_format("Fly away {}", timer.passed());
    }
    return {};
}

bool CruiseAndDock::run() {
    if (!st::autopilot.destDock)
        throw_failed("No destination dock");
    bool toPort = (int(st::autopilot.destDock->type) & int(TypeNav::PlanetaryThing)) != 0;

    if (st::ship.flags.docked) {
        if (!st::dockedAt.stationName.empty() && st::autopilot.destDock->nameEq(st::dockedAt.stationName)) {
            prevSubStep.reset();
            currSubStep.reset();
            status = DONE;
            return true;
        }
        status = DEPARTURE;
        if (!run_sub_step(new DepartureStep))
            throw_trouble("Cannot departure from dock");
    }

    bool at_dock = false;
    bool at_body = st::shipAtBody.approachBody || st::shipAtBody.nearBody;
    if (!st::ship.flags.cruise) {
        if (!st::space.stationName.empty() && st::autopilot.destDock->nameEq(st::space.stationName))
            at_dock = true;
        else if (!st::space.bodyName.empty() && st::autopilot.destDock->nameEq(st::space.bodyName))
            at_dock = true;
    }
    if (!at_dock && !st::ship.flags.cruise && !at_body) {
        status = ENTER_CRUISE;
        if (!run_sub_step(new EnterCruiseStep))
            throw_trouble("Cannot enter cruise");
    }
    else if (at_body) {
        notify_progress(MSG_ERROR, "Unexpected close to body: {}", st::shipAtBody.bodyName);
        setSpeed(0);
        status = LEAVE_BODY;
        if (!run_sub_step(new LeaveBodyStep))
            throw_trouble("Cannot leave body");
    }

    if (st::autopilot.destBody && !st::autopilot.destBody->nameEq(st::destination.name)) {
        if (st::autopilot.destBody->type == TypeNav::Planet)
            run_sub_step(new NavBodySelect);
    }

    while (!at_dock) {
        if (!st::ship.flags.cruise) {
            status = ENTER_CRUISE;
            if (!run_sub_step(new EnterCruiseStep))
                throw_trouble("Cannot enter cruise");
        }

        // a few degrees visible angle to stop and avoid planet
        dist_t min_dist = 0.5_ls;
        dist_t max_dist = 2.0_ls;
        if (st::autopilot.destBody) {
            if (toPort) {
                min_dist = dist_t(dist_t::KM, st::autopilot.destBody->radius * 4);
                max_dist = dist_t(dist_t::KM, st::autopilot.destBody->radius * 10);
            } else if (st::autopilot.destBody->type == TypeNav::Planet) {
                min_dist = dist_t(dist_t::KM, st::autopilot.destBody->radius * 4);
                max_dist = dist_t(dist_t::KM, st::autopilot.destBody->radius * 15);
            } else if (st::autopilot.destBody->type == TypeNav::Star) {
                min_dist = dist_t(dist_t::KM, st::autopilot.destBody->radius * 5);
                max_dist = dist_t(dist_t::KM, st::autopilot.destBody->radius * 10);
            }
        }

        ai::detectEDState(DetectLevel::Screen);

        bool skip_dive = false;
        bool skip_cruise_to_dist = false;
        if (st::autopilot.destBody && st::autopilot.destBody->nameEq(st::destination.name)) {
            if (ai::compassInfo.has_nav_target) {
                if (st::autopilot.distanceToBody.valid()) {
                    if (st::autopilot.distanceToBody <= max_dist) {
                        skip_cruise_to_dist = true;
                    }
                    if (st::autopilot.distanceToBody <= 5_Mm) {
                        skip_cruise_to_dist = true;
                        skip_dive = true;
                    }
                }
            }
        }
        else if (st::autopilot.destDock && st::autopilot.destDock->nameEq(st::destination.name)) {
            if (ai::compassInfo.has_nav_target) {
                if (st::autopilot.distanceToDock.valid()) {
                    if (st::autopilot.distanceToDock <= max_dist) {
                        skip_cruise_to_dist = true;
                    }
                    if (st::autopilot.distanceToDock <= 5_Mm) {
                        skip_cruise_to_dist = true;
                        skip_dive = true;
                    }
                }
            }
        }

        if (!skip_cruise_to_dist) {
            status = APPROACH;
            if (!skip_cruise_to_dist && !run_sub_step(new CruiseToDistStep(min_dist, max_dist)))
                throw_trouble("Cannot cruise to dock/body");
        }

        int exitCruisePitch = 0;
        if (!skip_dive) {
            if (st::autopilot.destBody && st::autopilot.destDock) {
                setSpeed(0);
                check_interrupted();
                status = DIVE;
                if (!run_sub_step(new DiveUnderPlanetStep))
                    continue;
                auto* dive = dynamic_cast<DiveUnderPlanetStep*>(prevSubStep.get());
                if (dive)
                    exitCruisePitch = dive->keepCruisePitch;
            }
        }

        status = LEAVE_CRUISE;
        if (!toPort) {
            if (!run_sub_step(new ExitCruiseToSpace(exitCruisePitch)))
                throw_trouble("Failed to exit cruise");
        } else {
            if (!run_sub_step(new ExitCruiseToPlanet(exitCruisePitch)))
                throw_trouble("Failed to exit cruise");
        }

        if (!st::ship.flags.cruise) {
            if (!st::space.stationName.empty() && st::autopilot.destDock->nameEq(st::space.stationName))
                at_dock = true;
        }
    }

    status = DOCK;
    if (!toPort) {
        if (!run_sub_step(new DockSpaceStation))
            throw_trouble("Failed to dock at space station");
    } else {
        if (!run_sub_step(new DockPlanetPort))
            throw_trouble("Failed to dock at planet port");
    }

    prevSubStep.reset();
    currSubStep.reset();
    status = DONE;
    return true;
}

std::string CruiseAndDock::getTitle() {
    std::string name;
    auto dock = st::autopilot.destDock;
    if (dock) {
        if (!dock->nloc.empty())
            name = dock->nloc;
        else
            name = dock->name;
    }
    return lc_format("Cruise and dock: {}", name);
}

std::string CruiseAndDock::getStatus() {
    switch (status) {
    case DONE:
    case READY:
        return {};
    case DEPARTURE:
        return _gt("Departure");
    case ENTER_CRUISE:
        return _gt("Entering cruise");
    case LEAVE_BODY:
        return _gt("Fly away from nearest body");
    case APPROACH:
        return _gt("Get close to body");
    case DIVE:
        return _gt("Dive to space");
    case LEAVE_CRUISE:
        return _gt("Leaving cruise");
    case DOCK:
        return _gt("Docking");
    }
    return {};
}

TaskTravel::TaskTravel(const TaskTemplate &templ_)
    : BaseAutopilotTask(templ_)
{
    assert(templ.id == ED_TASK_TRAVEL);
    for (auto& p : templ.params) {
        if (p.id == "system")
            destSystemName = p.as_string();
        else if (p.id == "dock")
            destDockName = p.as_string();
    }
}

std::string TaskTravel::getTitle() {
    std::string dest = destDockName.empty() ? destSystemName : destDockName;
    if (templ.nm.empty())
        return lc_format("Travel to: {}", dest);
    return templ.name();
}

bool TaskTravel::run() {
    if (destSystemName.empty() || destDockName.empty())
        throw_failed("Destination system and dock required");

    if (gal::getCurrentStarSystem()->systemName != destSystemName) {
        bool change_route = false;
        auto navRoute = st::currentNavRoute;
        if (!navRoute || navRoute->route.empty())
            change_route = true;
        else if (navRoute->route.back().starSystem != destSystemName)
            change_route = true;
        if (change_route) {
            if (!selectOnGalaxyMap(destSystemName))
                throw_trouble("Cannot make route to destination system: {}", destSystemName);
        }
        nl.init(st::navFilters);
        if (!run_sub_step(new CompleteNavRoute))
            throw_trouble("Cannot reach destination system");
        if (gal::getCurrentStarSystem()->systemName != destSystemName)
            throw_trouble("Cannot reach destination system");
    }

    auto starSystem = gal::getCurrentStarSystem();
    st::autopilot.setDestDock(starSystem->getDock(destDockName));
    if (!st::autopilot.destDock)
        throw_failed("Cannot find destination dock: {0} in system {1}", destDockName, starSystem->systemName);

    if (st::autopilot.destDock->parentBodyId < 0) {
        initNavFilter();
        if (!run_sub_step(new NavDockSelect))
            throw_trouble("Cannot select destination dock");
        sleep(500);
    }

    int bodyId = st::autopilot.destDock->parentBodyId;
    if (bodyId > 0) {
        auto body = starSystem->getBodyById(bodyId);
        if (!body)
            throw_failed("Cannot find body id: {0} in system {1}", bodyId, starSystem->systemName);
        st::autopilot.setDestBody(body);
        if (!st::autopilot.destBody)
            throw_failed("Not a body: {}", body->name);
    }

    initNavFilter();

    if (!run_sub_step(new CruiseAndDock))
        throw_trouble("Cannot cruise and dock");

    return true;
}

Autopilot::Autopilot(const TaskTemplate &templ_)
        : BaseAutopilotTask(templ_)
{
}

std::string Autopilot::getTitle() {
    if (templ.nm.empty())
        return _gt("Autopilot");
    return templ.name();
}

bool Autopilot::run() {
    if (getNavRoutePosition() >= 0) {
        nl.init(st::navFilters);
        if (!run_sub_step(new CompleteNavRoute))
            throw_trouble("Cannot reach destination system");
    }

    if (!st::autopilot.destDock && !st::autopilot.destBody) {
        if (st::destination.name.empty())
            throw_failed("No destination dock selected");

        auto starSystem = gal::getCurrentStarSystem();
        gal::spEntity dest = starSystem->getEntity(st::destination.name);
        if (dest) {
            if (isBody(dest->type))
                st::autopilot.setDestBody(dest);
            else if (isSite(dest->type))
                st::autopilot.setDestDock(dest);
        } else {
            nl.discoverSelected();
            dest = starSystem->getEntity(st::destination.name);
            if (dest) {
                if (isBody(dest->type))
                    st::autopilot.setDestBody(dest);
                else if (isSite(dest->type))
                    st::autopilot.setDestDock(dest);
            }
        }

        int bodyId = st::destination.bodyId;
        if (dest && isBody(dest->type))
            assert(dest->bodyId == bodyId);
        else if (dest && isSpaceStation(dest->type))
            bodyId = dest->parentBodyId;
        if (bodyId > 0) {
            auto body = starSystem->getBodyById(bodyId);
            if (!body)
                throw_failed("Cannot find body id: {0} in system {1}" , bodyId, starSystem->systemName);
            st::autopilot.setDestBody(body);
            if (!st::autopilot.destBody)
                throw_failed("Not a body: {}", body->name);
        }
    }

    initNavFilter();

    if (!st::autopilot.destDock && !st::autopilot.destBody) {
        dist_t dist(dist_t::LS, 0.9);
        if (!run_sub_step(new CruiseToSignal(dist)))
            throw_trouble("Cannot cruise to signal");
        if (st::destination.name.empty())
            throw_failed("No destination dock selected");
        nl.discoverSelected();
        return false;
    }

    if (!run_sub_step(new CruiseAndDock))
        throw_trouble("Cannot cruise and dock");

    return true;
}

} // ai
