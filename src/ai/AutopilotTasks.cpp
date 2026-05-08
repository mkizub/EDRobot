//
// Created by mkizub on 28.06.2025.
//

#include "../pch.h"

#include "Task.h"
#include "AIManager.h"
#include "AutopilotTasks.h"
#include "AIUtils.h"
#include "../Galaxy.h"
#include "../Keyboard.h"
#include "../ShipStats.h"
#include "../widget/EDWidget.h"

namespace ai {

// TODO: departure from planet - orient away before mass-locked
// TODO: complete nav route - leave body first
// TODO: autopilot to planet port - dive, check direct visibility before going far from planet
// TODO: autopilot to planet port - orient roll to planet center before flying to port
// TODO: restart frame capturing if many fails

const dist_t kPlDockFar = 10_km;
const dist_t kPlDockTooFar = 50_km;

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
        notify_warn("Glare, but rolling disabled");
        return;
    }
    notify_info("Anti-glare rolling");
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

double orbitShowAltitude(double planet_radius) {
    return 500 + planet_radius * 0.415; // show orbit altitude and add it to Status.json
}
double orbitEnterAltitude(double planet_radius) {
    return 400 + planet_radius * 0.1; // orbit exit altitude (angle shown)
}
double orbitExitAltitude(double planet_radius) {
    return 25; // orbit exit, start gliding, always at 25 km
}

inline void sendUiBack(int pause=1000) {
    if (pause >= 500 && Cfg.isHeadlookSmoothing())
        pause = 250;
    kbd::send("UI_Back", 0, pause);
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
        LOG_ERROR("Nav page '{}' not known", page_name);
    return pageIndex;
}

bool gotoNavPage(const std::string &page_name, bool required, cv::Mat* grayImage) {
    int targetPageIndex = getNavPageIndex(page_name);
    if (targetPageIndex < 0)
        ai::throw_failed("Bad nav panel page: {}", page_name);

    for (int i = 0; i < 6 && !ai::uiState.match("scr-left-panel:" + page_name); i++) {
        if (grayImage)
            ai::detectEDStateGrayIm(DetectLevel::ListRows, *grayImage);
        else
            ai::detectEDState(DetectLevel::Buttons);
        LOG_DEBUG("Goto '{}'...", page_name);

        if (ai::uiState.guiFocus == GuiFocus::None) {
            LOG_DEBUG("FocusLeftPanel...");
            kbd::send("FocusLeftPanel", 0, 1500);
            continue;
        }
        if (st::ship.flags.fsd_charging) {
            notify_warn("Unexpected fsd charging");
            kbd::send("HyperSuperCombination", 100, 1000);
            continue;
        }
        if (ai::uiState.guiFocus == GuiFocus::Left && !ai::uiState.screen) {
            if ((i & 1) == 0)
                rollBlindCompass();
            else
                sendUiBack();
            continue;
        }
        if (!ai::uiState.match("scr-left-panel:*")) {
            LOG_DEBUG("FocusLeftPanel...");
            kbd::send("FocusLeftPanel", 0, 1500);
            continue;
        }
        if (ai::uiState.match("scr-left-panel:dlg-nav-select") ||
            ai::uiState.match("scr-left-panel:dlg-filters")) {
            sendUiBack();
            continue;
        }
        std::vector<std::string> segments = ai::uiState.splitPath();
        if (segments.size() < 2)
            ai::throw_failed("Expecting 2 segments in {}", ai::uiState.to_string());
        int currentPageIndex = getNavPageIndex(segments[1]);
        if (currentPageIndex < 0)
            ai::throw_failed("Bad nav panel page: {}", segments[1]);
        int dist = targetPageIndex - currentPageIndex;
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

int getNavRoutePosition(const std::shared_ptr<NavRoute>& navRoute) {
    if (!navRoute || navRoute->route.empty())
        return -1;

    auto starSystem = gal::getCurrentStarSystem();
    if (!starSystem)
        throw_failed("Current star system not known");

    for (int i = 0; i < navRoute->route.size(); i++) {
        if (navRoute->route[i].systemAddress == starSystem->systemAddress)
            return i;
    }
    return -1;
}

BaseAutopilotStep::BaseAutopilotStep()
    : task(nullptr)
{
    for (Step* s=parent; s && !task; s=s->parent) {
        task = dynamic_cast<BaseAutopilotTask*>(s);
    }
    if (!task) {
        LOG_ERROR("BaseAutopilotStep needs BaseAutopilotTask");
        throw std::runtime_error("BaseAutopilotStep needs BaseAutopilotTask");
    }
}


void BaseAutopilotTask::relogin() {
    // something is really wrong, logout and login again
    notify_warn("Something is wrong with departure, trying to re-login");
    kbd::send("Pause", 0, 1000);
    kbd::send("UI_Up", 0, 100); // go to Exit button
    notify_info("Logout");
    kbd::send("UI_Select", 0, 1000); // logout
    notify_info("Logout to main menu");
    kbd::send("UI_Select", 0, 8000); // logout to main menu
    notify_info("Login to Solo...");
    kbd::send("UI_Select", 0, 3000); // login, select mode screen
    kbd::send("UI_Right", 0, 100);
    kbd::send("UI_Right", 0, 500);  // choose Solo
    notify_info("Login");
    kbd::send("UI_Select", 0, 12000); // login
    throw_trouble("Finished re-login");
}

bool setSpeed(int percents, bool force, const char* reason) {
    percents = std::clamp(percents, -100, +100);
    if (st::autopilot.speed_set_to.has_value() && percents == st::autopilot.speed_set_to.value() && !force)
        return true;
    LOG_DEBUG("set speed {}%: {}{}", percents, force ? " (force)" : "", reason);
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

static void sendOrientAxis(double pitchDelta, double yawDelta, double rollDelta, int max_time_ms) {
    auto shipStats = eddb::getShipStats();
    if (!shipStats)
        throw_failed("Unsupported or unknown ship");
    int sst = st::autopilot.speed_set_to.value_or(0);
    double pitchSpeed = shipStats->getPitchSpeed(sst);
    double yawSpeed = shipStats->getYawSpeed(sst);
    double rollSpeed = shipStats->getRollSpeed(sst);
    double pitchSeconds = pitchDelta / pitchSpeed;
    double yawSeconds = yawDelta / yawSpeed;
    double rollSeconds = rollDelta / rollSpeed;
    double seconds = std::max({2.0, std::abs(pitchSeconds), std::abs(yawSeconds), std::abs(rollSeconds)});
    int millis = std::min(int(seconds * 1000), max_time_ms);
    pitchAxis.set(pitchSeconds / seconds, millis);
    yawAxis.set(yawSeconds / seconds, millis);
    rollAxis.set(rollSeconds / seconds, millis);
    sleep(millis, true);
    pitchAxis.reset();
    yawAxis.reset();
    rollAxis.reset();
    sleep(1000);
}

static void sendOrientKeys(const char* pos, const char* neg, double speed, double delta, int max_time_ms) {
    if (std::abs(delta) < 0.1)
        return;
    int duration = std::min(max_time_ms, getDuration(delta, speed));
    int pause = duration < max_time_ms ? duration : 1000;
    kbd::send(delta > 0 ? pos : neg, duration, pause);
}

inline double normalizeAngle(double angle) {
    if (angle > 180) angle =  360 - angle;
    if (angle < -180) angle = 360 + angle;
    return angle;
}

void BaseAutopilotTask::orientRollStep(double delta, int max_time_ms) {
    sendOrientAxis(0, 0, normalizeAngle(delta), max_time_ms);
}

void BaseAutopilotTask::orientPitchStep(double delta, int max_time_ms) {
    sendOrientAxis(normalizeAngle(delta), 0, 0, max_time_ms);
}

void BaseAutopilotTask::orientYawStep(double delta, int max_time_ms) {
    sendOrientAxis(0, normalizeAngle(delta), 0, max_time_ms);
}

bool BaseAutopilotTask::orientTowardTargetStep(double precision, int max_time_ms) {
    check_interrupted();
    if (ai::compassInfo.hemisphere < 0)
        precision = std::max(5.0, precision);
    else if (!ai::compassInfo.has_nav_target && ai::compassInfo.targetAngle < 6)
        precision = std::max(3.0, precision);
    bool front = ai::compassInfo.hemisphere > 0;
    double hemiYaw = ai::compassInfo.targetYaw;
    if (!front) {
        if (hemiYaw > 0)
            hemiYaw = 180 - hemiYaw;
        else
            hemiYaw = -180 - hemiYaw;
    }
    if (std::abs(hemiYaw) > 20) {
        double roll = ai::compassInfo.targetRoll;
        if (std::abs(roll) <= 90)
            orientRollStep(roll, max_time_ms);
        else
            orientRollStep(roll-180, max_time_ms);
        return false;
    }

    double pitchDelta = normalizeAngle(ai::compassInfo.targetPitch);
    double yawDelta = normalizeAngle(hemiYaw);
    if (std::abs(pitchDelta) > precision || std::abs(yawDelta) > precision) {
        sendOrientAxis(pitchDelta, yawDelta, 0, max_time_ms);
        return false;
    }
    return true;
}

bool BaseAutopilotTask::orientTowardTarget(double precision) {
    LOG_DEBUG("Orient toward target");
    CourseLockerPause lockPause;
    if (st::guiFocus != GuiFocus::None) {
        notify_info("Orientation: goto compass");
        sendUiBack();
    }
    int compass_fails = 0;
    for (int retry=0; retry < 10; retry++) {
        check_interrupted();
        if (retry > 5)
            setSpeed(0, false, "Orient toward target: too many retries");
        ai::detectEDState(DetectLevel::Screen);
        if (ai::uiState.guiFocus != GuiFocus::None) {
            notify_warn("Unexpected ui mode {}", ai::uiState.to_string());
            sendUiBack();
            continue;
        }
        if (!ai::compassInfo.hemisphere) {
            compass_fails += 1;
            if (compass_fails > 6)
                throw_trouble("Compass not detected");
            notify_warn("Compass not detected, fails {}", compass_fails);
            rollBlindCompass();
            continue;
        }
        LOG_DEBUG("Orient toward target step {}", retry);
        if (orientTowardTargetStep(precision))
            return true;
    }
    return false;
}

bool BaseAutopilotTask::orientAwayFromTargetStep(double precision, int max_time_ms) {
    check_interrupted();
    precision = std::max(5.0, precision);
    bool front = ai::compassInfo.hemisphere > 0;
    double hemiYaw = ai::compassInfo.targetYaw;
    if (!front) {
        if (hemiYaw > 0)
            hemiYaw = 180 - hemiYaw;
        else
            hemiYaw = -180 - hemiYaw;
    }
    if (std::abs(hemiYaw) > 20) {
        double roll = ai::compassInfo.targetRoll;
        if (std::abs(roll) <= 90)
            orientRollStep(roll, max_time_ms);
        else
            orientRollStep(roll-180, max_time_ms);
        return false;
    }

    double pitchDelta;
    if (ai::compassInfo.targetPitch < 0)
        pitchDelta = normalizeAngle(180-ai::compassInfo.targetPitch);
    else
        pitchDelta = normalizeAngle(-180+ai::compassInfo.targetPitch);
    double yawDelta = front ? hemiYaw : -hemiYaw;
    if (std::abs(pitchDelta) > precision || std::abs(yawDelta) > precision) {
        sendOrientAxis(pitchDelta, yawDelta, 0, max_time_ms);
        return false;
    }
    return true;
}

bool BaseAutopilotTask::orientAwayFromTarget(double precision) {
    LOG_DEBUG("Orient away from target");
    CourseLockerPause lockPause;
    if (st::guiFocus != GuiFocus::None) {
        notify_info("Orientation: goto compass");
        sendUiBack();
    }
    int compass_fails = 0;
    for (int retry=0; retry < 10; retry++) {
        check_interrupted();
        if (retry > 5) {
            setSpeed(0, false, "Orient away from target: too many retries");
            continue;
        }
        ai::detectEDState(DetectLevel::Screen);
        if (ai::uiState.guiFocus != GuiFocus::None) {
            notify_warn("Unexpected ui mode {}", ai::uiState.to_string());
            sendUiBack();
            continue;
        }
        if (!ai::compassInfo.hemisphere) {
            compass_fails += 1;
            if (compass_fails > 6)
                throw_trouble("Compass not detected");
            notify_warn("Compass not detected, fails {}", compass_fails);
            rollBlindCompass();
            continue;
        }
        LOG_DEBUG("Orient away from target step {}", retry);
        if (orientAwayFromTargetStep(precision))
            return true;
    }
    return false;
}

bool BaseAutopilotTask::orientRollByTarget(double reqRoll, double precision, int max_time_ms) {
    LOG_DEBUG("Orient roll by target: {}", int(reqRoll));
    CourseLockerPause lockPause;
    if (st::guiFocus != GuiFocus::None) {
        notify_info("Orientation: goto compass");
        sendUiBack();
    }
    int compass_fails = 0;
    for (int retry=0; retry < 10; retry++) {
        check_interrupted();
        if (retry > 5) {
            setSpeed(0, false, "Orient roll by target: too many retries");
            continue;
        }
        ai::detectEDState(DetectLevel::Screen);
        if (ai::uiState.guiFocus != GuiFocus::None) {
            notify_warn("Unexpected ui mode {}", ai::uiState.to_string());
            sendUiBack();
            continue;
        }
        if (!ai::compassInfo.hemisphere) {
            compass_fails += 1;
            if (compass_fails > 6)
                throw_trouble("Compass not detected");
            notify_warn("Compass not detected, fails {}", compass_fails);
            rollBlindCompass();
            continue;
        }
        if (ai::compassInfo.targetAngle < 3 || ai::compassInfo.targetAngle > 177)
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
        LOG_DEBUG("Orient roll by target step {}", retry);
        orientRollStep(delta, max_time_ms);
    }
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
    LOG_DEBUG("Init Nav Filter: {}", enum_name<TypeNav>(nt));
    switch (nt) {
    case TypeNav::Body:
    case TypeNav::Barycenter:
    case TypeNav::Star:
    case TypeNav::Planet:
    case TypeNav::NavBeacon:
    case TypeNav::SpaceStation:
    case TypeNav::Orbis:
    case TypeNav::Ocellus:
    case TypeNav::Dodec:
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
    case TypeNav::Megaship:
    case TypeNav::TouristBeacon:
        filters.pointOfInterest = true;
        break;
    case TypeNav::FleetCarrier:
        if (st::autopilot.destDock->marketId != st::cmdr.fleetCarrierId)
            filters.fleetCarrier = true;
        break;
    case TypeNav::SquadronCarrier:
        if (st::autopilot.destDock->marketId != st::cmdr.squadronCarrierId)
            filters.fleetCarrier = true;
        break;
    case TypeNav::PlanetaryThing:
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

    nl.init(st::navFilters);
    setSpeed(0, false, "TaskDebugAutopilot Init");
    sendUiBack();

    if (test == "Departure") {
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
    else if (test == "RecognizeNavList") {
        int focusIdx;
        cv::Mat grayImage;
        nl.recognizeWholePage(grayImage, focusIdx);
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
    return true;
}

TaskDebugShipStats::TaskDebugShipStats(const ai::TaskTemplate &templ)
        : BaseAutopilotTask(templ)
{
    assert (templ.id == ED_TASK_DEBUG_SHIP_STATS);
    for (auto& p : templ.params) {
        if (p.id == "test")
            test = p.as_string();
        if (p.id == "value")
            value = p.as_number();
        if (p.id == "duration")
            duration = p.as_number();
        if (p.id == "speed")
            speed = p.as_integer();
    }
}


bool TaskDebugShipStats::run() {
    auto ship = eddb::getShipStats();
    if (!ship)
        throw_failed("Unsupported or unknown ship: {}", st::shipInfo.shipType);

    if (test == "OrientTowards") {
        orientTowardTarget(value.value_or(1.0));
    }
    else if (test == "OrientAway") {
        orientAwayFromTarget(value.value_or(1.0));
    }
    else if (test == "KeepCourse") {
        CourseLocker course(0, true);
        for (;;) {
            sleep(1000);
            LOG_INFO("KeepCourse: pitch: {:.1f}, yaw: {:.1f}, roll: {:.1f}, angle: {:.1f}",
                                     st::compass.targetPitch, st::compass.targetYaw,
                                     st::compass.targetRoll, st::compass.targetAngle);
        }
    }
    else if (test == "ForwardAccelerate") {
        accelForward();
    }
    else if (test == "ReverseAccelerate") {
        accelReverse();
    }
    else if (test == "ForwardDist") {
        forwardDist();
    }
    else if (test == "Pitch") {
        rotateAxis(pitchAxis);
    }
    else if (test == "Yaw") {
        rotateAxis(yawAxis);
    }
    else if (test == "Roll") {
        rotateAxis(rollAxis);
    }
    else if (test == "PitchCurve") {
        rotateCurve(pitchAxis);
    }
    else if (test == "YawCurve") {
        rotateCurve(yawAxis);
    }
    else if (test == "RollCurve") {
        rotateCurve(rollAxis);
    }
    return true;
}

bool TaskDebugShipStats::accelForward() {
    auto ship = eddb::getShipStats();
    if (!ship)
        throw_failed("Unsupported or unknown ship");

    double seconds = duration.value_or(1.0);
    double fwdacc = ship->getForwardAccel() * speed.value_or(100) / 100;
    if (st::ship.flags.fa_off) {
        LOG_ERROR("FA-OFF mode");
        setSpeed(speed.value_or(100), true, "accelForward start");
        LOG_INFO("Start forward acceleration");
        ai::sleep(int(seconds * 1000), true);
        setSpeed(0, true, "accelForward stop");
        LOG_INFO("End acceleration");
        ai::sleep(1000);
        LOG_INFO("Expected speed: {}", int(fwdacc * seconds));
    } else {
        LOG_ERROR("FA-ON mode");
        setSpeed(speed.value_or(50), true, "accelForward start");
        LOG_INFO("Start forward acceleration");
        ai::sleep(int(seconds * 1000), true);
        kbd::send("Z"); //setSpeed(0, true, "accelForward stop");
        LOG_INFO("End acceleration (enter FA-OFF)");
        ai::sleep(1000);
        double max_speed = ship->getThrustSpeed();
        LOG_INFO("Expected speed (max {}): {}", int(max_speed), int(fwdacc * seconds));
    }
    return true;
}

bool TaskDebugShipStats::accelReverse() {
    auto ship = eddb::getShipStats();
    if (!ship)
        throw_failed("Unsupported or unknown ship");

    double seconds = duration.value_or(1.0);
    double revacc = ship->getReverseAccel() * speed.value_or(100) / 100;
    if (st::ship.flags.fa_off) {
        LOG_ERROR("FA-OFF mode");
        setSpeed(-speed.value_or(100), true, "accelReverse start");
        LOG_INFO("Start reverse acceleration");
        ai::sleep(int(seconds * 1000), true);
        setSpeed(0, true, "accelReverse stop");
        LOG_INFO("End acceleration");
        ai::sleep(1000);
        double max_speed = ship->getThrustSpeed();
        LOG_INFO("Expected speed (if dropped from max {}): {}", int(max_speed), int(max_speed - revacc * seconds));
    } else {
        LOG_ERROR("FA-ON mode");
        setSpeed(0, true, "accelReverse start"); //setSpeed(-speed.value_or(100), true, "accelReverse start");
        LOG_INFO("Start reverse acceleration (set speed 0)");
        ai::sleep(int(seconds * 1000), true);
        kbd::send("Z"); //setSpeed(0, true, "accelReverse stop");
        LOG_INFO("End acceleration (enter FA-OFF)");
        ai::sleep(1000);
        double max_speed = ship->getThrustSpeed();
        LOG_INFO("Expected speed (if dropped from max {}): {}", int(max_speed), int(max_speed - revacc * seconds));
    }
    return true;
}

bool TaskDebugShipStats::forwardDist() {
    auto ship = eddb::getShipStats();
    if (!ship)
        throw_failed("Unsupported or unknown ship");

    double seconds = duration.value_or(1.0);
    double fwdacc = ship->getForwardAccel();
    double revacc = ship->getReverseAccel();
    double topspd = ship->getThrustSpeed();
    LOG_INFO("Top speed: {:.1f}, acceleration forward: {:1f} and reverse: {:1f}",
                             topspd, fwdacc, revacc);
    double accel_time_to_topspd = topspd/fwdacc;
    if (accel_time_to_topspd > seconds) {
        LOG_INFO("Max acceleration time: {:.1f}, test time: {:1f}, no const speed time", accel_time_to_topspd, seconds);
        double fwd_time = seconds;
        double rev_time = seconds * fwdacc / revacc;
        kbd::send("SetSpeed100", fwd_time * 1000, 1, true);
        kbd::send("SetSpeedZero", rev_time * 1000, 500, true);
        int fwd_dist = fwdacc * fwd_time * fwd_time / 2;
        int rev_dist = revacc * rev_time * rev_time / 2;
        int total_dist = fwd_dist + rev_dist;
        LOG_INFO("Expected distance: {}", total_dist);
    } else {
        double accel_time = accel_time_to_topspd;
        double fwd_time = accel_time;
        double rev_time = accel_time * fwdacc / revacc;
        double top_time = seconds - accel_time;
        LOG_INFO("Max acceleration time: {:.1f}, test time: {:1f}, const speed time: {:1f}", fwd_time, seconds, top_time);
        kbd::send("SetSpeed100", fwd_time * 1000, 1, true);
        sleep(top_time*1000, true);
        kbd::send("SetSpeedZero", rev_time * 1000, 500, true);
        int fwd_dist = fwdacc * fwd_time * fwd_time / 2;
        int rev_dist = revacc * rev_time * rev_time / 2;
        int top_dist = topspd * top_time;
        int total_dist = fwd_dist + top_dist + rev_dist;
        LOG_INFO("Expected distance: {}", total_dist);
    }
    return true;
}

bool TaskDebugShipStats::rotateAxis(Axis& axis) {
    if (!st::autopilot.speed_set_to.has_value() || st::autopilot.speed_set_to.value() != speed.value_or(50)) {
        setSpeed(speed.value_or(50), true, "rotateAxis init");
        sleep(5000);
    }
    auto ship = eddb::getShipStats();
    double rot_speed = ship->getRotationSpeed(axis.type, st::autopilot.speed_set_to.value());
    float* compassVal;
    switch (axis.type) {
    case Axis::Pitch:
        compassVal = &ai::compassInfo.targetPitch;
        break;
    case Axis::Yaw:
        compassVal = &ai::compassInfo.targetYaw;
        break;
    case Axis::Roll:
        compassVal = &ai::compassInfo.targetRoll;
        break;
    default:
        throw_failed_(std::format("Unknown axis {}", axis.name()));
    }
    auto round = 360 / rot_speed;
    LOG_ERROR("{} speed: {:2f}, full turn time: {:1f}", axis.name(), rot_speed, round);

    ai::detectEDState(DetectLevel::Screen);
    double val1 = *compassVal;
    LOG_INFO("Pitch: {:.1f}, Yaw: {:.1f}, Roll: {:.1f}", ai::compassInfo.targetPitch,
                             ai::compassInfo.targetYaw, ai::compassInfo.targetRoll);
    axis.setRaw(value.value_or(1.0), duration.value_or(1.0));
    sleep(1000 * duration.value_or(1.0), true);
    axis.reset();
    sleep(2000);
    ai::detectEDState(DetectLevel::Screen);
    LOG_INFO("Pitch: {:.1f}, Yaw: {:.1f}, Roll: {:.1f}", ai::compassInfo.targetPitch,
                             ai::compassInfo.targetYaw, ai::compassInfo.targetRoll);
    double val2 = *compassVal;
    LOG_INFO("{} delta: {:.1f}, average speed: {:.1f} for value {:.2f} time {:.2f}",
                             axis.name(), std::abs(val2-val1), std::abs(val2-val1)/value.value_or(1.0),
                             value.value_or(1.0), duration.value_or(1.0));
    return true;
}

bool TaskDebugShipStats::rotateCurve(Axis& axis) {
    if (!st::autopilot.speed_set_to.has_value() || st::autopilot.speed_set_to.value() != speed.value_or(50)) {
        setSpeed(speed.value_or(50), true, "rotateCurve init");
        sleep(5000);
    }

    const std::vector<float> speedPercents {0.3, 0.5, 1, 2.5, 5, 10, 15, 20, 30, 40, 50, 60, 70, 80, 90, 100};
    std::vector<float> hardcodedScaleTable(speedPercents.size(), 0);
    for (int i=0; i < speedPercents.size(); i++) {
        hardcodedScaleTable[i] = axis.timeScaleFor(speedPercents[i]*0.01);
        double ts = axis.timeScaleFor(speedPercents[i]*0.01);
        double vs = axis.valueScaleFor(speedPercents[i]*0.01);
        LOG_INFO("For speed {:5.1f}% time scale {:9.6f} and value scale {:9.6f}",
                                 speedPercents[i], ts, vs);
        //double check = axis.timeScaleFor(vs);
        //assert (std::abs(check-1) < 0.01);
    }
    std::vector<float> rotScaleTable = hardcodedScaleTable;
    // calculate rough values
    rotateCurveTest(axis, speedPercents, rotScaleTable);
    // calculate norm values
    rotateCurveTest(axis, speedPercents, rotScaleTable);
    // fine-tune values
    rotateCurveTest(axis, speedPercents, rotScaleTable);

    return true;
}

bool TaskDebugShipStats::rotateCurveTest(Axis& axis, const std::vector<float>& speedPercents, std::vector<float>& rotScaleTable) {
    auto ship = eddb::getShipStats();
    double full_rot_speed = ship->getRotationSpeed(axis.type, st::autopilot.speed_set_to.value());
    float* compassVal;
    switch (axis.type) {
    case Axis::Pitch:
        compassVal = &ai::compassInfo.targetPitch;
        break;
    case Axis::Yaw:
        compassVal = &ai::compassInfo.targetYaw;
        break;
    case Axis::Roll:
        compassVal = &ai::compassInfo.targetRoll;
        break;
    default:
        throw_failed_(std::format("Unknown axis {}", axis.name()));
    }

    const int size = speedPercents.size();
    std::vector<float> speedTable(size, 0);
    std::vector<float> speedTableNormalized(size, 0);
    for (int i = size-1; i >= 0; i--) {
        orientTowardTarget(1);
        value = speedPercents[i] * 0.01;
        float rot_time_scale = rotScaleTable[i];
        if (rot_time_scale == 0) {
            if (i < size-1)
                rot_time_scale = rotScaleTable[i+1];
            if (rot_time_scale == 0)
                rot_time_scale = 1;
        }
        // slow rotation is below 3 degree per second, in this case we rotate 10 degreed only
        double rot_speed = full_rot_speed * value.value() / rot_time_scale;
        bool slow_rot = rot_speed < 3;
        if (!slow_rot) {
            duration = rot_time_scale * (360 / full_rot_speed) / value.value();
        } else {
            duration = std::min(10 / rot_speed, 120.0);
            value = - value.value();
        }
        float val1 = *compassVal;
        LOG_INFO("Testing {} at ship speed {}% rot speed {}%",
                                 axis.name(), speed.value(), speedPercents[i]);
        rotateAxis(axis);
        float val2 = *compassVal;
        if (!slow_rot)
            rot_speed = (360 - val2 + val1) / duration.value();
        else
            rot_speed = (val2 - val1) / duration.value();
        LOG_INFO("Fixed rotation speed: {:.1f} (for {} at ship speed {}% rot speed {}%)", rot_speed,
                                 axis.name(), speed.value(), speedPercents[i]);
        speedTable[i] = rot_speed;
        speedTableNormalized[i] = std::abs(rot_speed / value.value());
        rotScaleTable[i] = speedTableNormalized.back() / speedTableNormalized[i];
    }
    LOG_INFO("Fixed speed table for {} at ship speed {}%", axis.name(), speed.value());
    for (int i = 0; i < size; i++) {
        LOG_INFO("{:5.1f}% : rot speed {:10.6f}, relative to max: {:.6f}, rot scale need: {:.6f}",
                                 speedPercents[i],
                                 speedTable[i], speedTable[i]/speedTable.back(),
                                 rotScaleTable[i]);
    }
    return true;
}



bool DepartureStep::run() {
    fromDock = gal::getCurrentStarSystem()->getDock(st::dockedAt.marketId);
    if (!fromDock)
        fromDock = gal::getCurrentStarSystem()->getDock(st::dockedAt.stationName);
    if (fromDock)
        fromDockName = fromDock->name;
    else
        fromDockName = st::dockedAt.stationName;
    LOG_DEBUG("Departure from {}", fromDockName);

    bool fromCompletedConstruction = false; // autopilot is off after construction is complete
    bool fromSpaceConstruction = false; // need UpThrustButton
    bool fromStarPort = false; // need special break for panthermkii
    bool fromPlaneraryPort = false;
    bool fromSimpleMegaship = false;
    if (fromDock && isConstrDepot(fromDock->type)) {
        auto market = gal::getMarket(fromDock->marketId);
        if (market && market->raven && market->raven->status == "complete")
            fromCompletedConstruction = true;
    }
    if ((fromDock && isPlanetarySite(fromDock->type)) || st::shipAtBody.nearBody) {
        fromPlaneraryPort = true;
    }
    if (fromDock) {
        switch (fromDock->type) {
        case TypeNav::StationMegaShip:
        case TypeNav::FleetCarrier:
        case TypeNav::SquadronCarrier:
        case TypeNav::ColonisationShip:
            fromSimpleMegaship = true;
        }
    }
    if (!st::dockedAt.stationType.empty()) {
        if (gal::PLANETARY_CONSTR_DEPOT.match_type(st::dockedAt.stationType)) {
            auto market = gal::getMarket(st::dockedAt.marketId);
            if (market && market->raven && market->raven->status == "complete")
                fromCompletedConstruction = true;
            LOG_DEBUG("Departure from PlanetaryConstruction");
        }
        if (gal::SPACE_CONSTR_DEPOT.match_type(st::dockedAt.stationType)) {
            fromSpaceConstruction = true;
            auto market = gal::getMarket(st::dockedAt.marketId);
            if (market && market->raven && market->raven->status == "complete")
                fromCompletedConstruction = true;
            LOG_DEBUG("Departure from SpaceConstruction");
        }
        else if (st::dockedAt.stationType == "SurfaceStation" && fromDock) {
            // bug in ED for newly constructed space stations
            if (isSpaceStation(fromDock->type)) {
                fromStarPort = true;
                LOG_DEBUG("Departure from StarPort");
            }
        }
        else if (gal::ORBIS.match_type(st::dockedAt.stationType) ||
                gal::OCELLUS.match_type(st::dockedAt.stationType) ||
                gal::DODEC.match_type(st::dockedAt.stationType) ||
                gal::CORIOLIS.match_type(st::dockedAt.stationType) ||
                gal::MINER_BASE.match_type(st::dockedAt.stationType)) {
            fromStarPort = true;
            LOG_DEBUG("Departure from StarPort");
        }
    }

    {
        ExpectSceeenLocker expectAutopilot("scr-autopilot");
        if (st::ship.flags.docked) {
            status = REFUEL;
            gotoLandingPad(false);

            LOG_INFO("Takeoff...");
            // 20 seconds to leave landing pad
            timer = utc_timer(25s);
            status = TAKEOFF;
            kbd::send("UI_Down");
            kbd::send("UI_Down");
            kbd::send("UI_Select");

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
            LOG_INFO("Departure autopilot waiting...");
            // 15 seconds wait autopilot
            timer = utc_timer(15s);
            status = WAIT_AUTOPILOT;
            // wait at least 15 seconds for autopilot to departure
            while (!ai::uiState.autopilot && !timer.expired()) {
                sleep(250);
                ai::detectEDState(DetectLevel::Screen);
            }
        }
        if (fromCompletedConstruction && !ai::uiState.autopilot) {
            LOG_INFO("Departure from completed construction...");
            if (st::ship.flags.landing_gear_down) {
                LOG_DEBUG("EnterCruise: LandingGearToggle");
                kbd::send("LandingGearToggle");
            }
            kbd::send("UpThrustButton", 20000);
        }
        // 4 minutes for departure
        LOG_INFO("Departure: got autopilot");
        timer = utc_timer(4min);
        status = AUTOPILOT;
        setSpeed(0, true, "Departure: got autopilot");
        notAutoPilotCounter = 0;
        int waitCounter = 4;
        //if (fromStarPort && st::shipInfo.shipType == "panthermkii") {
        //    LOG_DEBUG("Departure: panthermkii from StarPort");
        //    waitCounter = 7;
        //}
        for (;;) {
            if (timer.expired()) {
                notify_error("Autopilot time expired");
                status = RELOGIN;
                task->relogin();
                return false;
            }
            if (timer.sec_passed() > 60) {
                pitchAxis.set(0.25, 60);
            }
            //unsigned kh = kbd::post("SetSpeedZero", 500);
            sleep(250);
            ai::detectEDState(DetectLevel::Screen);
            //kbd::clearInput(kh);
            if (ai::uiState.autopilot) {
                notAutoPilotCounter = 0;
                continue;
            }
            if (++notAutoPilotCounter > waitCounter) {
                notify_info("Departure complete (autopilot off)");
                break;
            } else {
                //notify_info("Auto-pilot off counter: {}", notAutoPilotCounter);
                //if ((notAutoPilotCounter%3)==0 && fromStarPort && st::shipInfo.shipType == "panthermkii") {
                //    setSpeed(-50, true, "panthermkii bug");
                //    sleep(1000);
                //    setSpeed(0, true, "panthermkii bug");
                //    kbd::send("SetSpeedZero", 2500);
                //}
            }
        }
        Axis::resetAll(true);
    }

    if (fromSpaceConstruction) {
        LOG_DEBUG("Departure: from SpaceConstruction, thrust up");
        timer = utc_timer(15s);
        status = LEAVE_DEPOT;
        setSpeed(0,true,"Departure: from SpaceConstruction, thrust up");
        kbd::send("UpThrustButton", 15000);
    }

    kbd::send("TargetNextRouteSystem", 0, 500);
    if (!st::shipAtBody.nearBody && (!st::currentNavRoute || st::currentNavRoute->route.empty())) {
        if ((fromSimpleMegaship || fromStarPort || fromSpaceConstruction) && st::autopilot.destDock && st::autopilot.destBody) {
            auto at_dock = gal::getCurrentStarSystem()->getDock(st::space.marketId);
            auto at_body = gal::getCurrentStarSystem()->getBodyById(at_dock ? at_dock->parentBodyId : -1);
            if (at_body && at_body->radius) {
                if (at_body == st::autopilot.destBody)
                    run_sub_step(new NavDockSelect);
                else
                    run_sub_step(new NavBodySelect);
                sendUiBack();
            }
        }
    }
    ai::detectEDState(DetectLevel::Screen);
    compassAfterAutopilot = ai::compassInfo;

    if (st::shipAtBody.nearBody) {
        LOG_DEBUG("Departure: shipAtBody nearBody {}", st::shipAtBody.bodyName);
        status = ORIENT_AWAY;
        task->orientPitchStep(90, 7000);
    }
    bool needFlyAway = true;
    if (compassAfterAutopilot.hemisphere) {
        if (fromStarPort && compassAfterAutopilot.targetAngle < 110)
            needFlyAway = false;
        if (fromSpaceConstruction && compassAfterAutopilot.targetAngle < 110)
            needFlyAway = false;
        if (fromSpaceConstruction && compassAfterAutopilot.targetPitch >= 0)
            needFlyAway = false;
        if (fromSimpleMegaship && compassAfterAutopilot.targetPitch >= 0)
            needFlyAway = false;
    }

    if (st::ship.flags.fsd_masslocked) {
        timer = utc_timer(1min);
        status = MASSLOCKED;
        notify_info("Mass-locked, flying away");
        setSpeed(100, true,"Departure: Mass-locked, flying away");
        if (!needFlyAway) {
            if (compassAfterAutopilot.targetAngle > 60) {
                if (std::abs(compassAfterAutopilot.targetRoll) <= 90)
                    task->orientRollStep(compassAfterAutopilot.targetRoll, 5000);
                else
                    task->orientRollStep(compassAfterAutopilot.targetRoll-180, 5000);
            }
            CourseLocker course(0);
            while (st::ship.flags.fsd_masslocked && !timer.expired()) {
                sleep(500);
            }
        } else {
            while (st::ship.flags.fsd_masslocked && !timer.expired()) {
                sleep(500);
            }
            notify_info("Ready to jump, flying away");
            timer = utc_timer(15s);
            status = FLYAWAY;
            while (!timer.expired()) {
                ai::detectEDState(DetectLevel::Screen);
                sleep(1000);
                if (fromPlaneraryPort && ai::compassInfo.hemisphere && ai::compassInfo.targetAngle > 10) {
                    float roll = ai::compassInfo.targetRoll;
                    double delta = normalizeAngle(roll - 180);
                    if (std::abs(delta) > 5)
                        task->orientRollStep(delta);
                }
            }
        }
    }
    setSpeed(50,false,"Departure: completed");
    prevSubStep.reset();
    currSubStep.reset();
    status = DONE;
    return true;
}

std::string DepartureStep::getTitle() {
    if (status == DONE)
        return lc_format("Departed from: {}", fromDockName);
    return lc_format("Departing from: {}", fromDockName);
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
    LOG_DEBUG("EnterCruise");
    if (st::ship.flags.cruise) {
        LOG_DEBUG("EnterCruise: already in cruise");
        prevSubStep.reset();
        currSubStep.reset();
        status = DONE;
        return true;
    }

    bool flyAwayFromNearest = false;
    if (!enterSimple) {
        status = LOCK_BODY;
        dist_t dist;
        bool wasDestDockFocused = st::autopilot.isDestDockFocused;
        bool wasDestBodyFocused = st::autopilot.isDestBodyFocused;
        bool needToSelectNearest = true;
        gal::spEntity body = task->nl.focusNearestBody(&dist);
        if (body && body->radius > 0 && dist) {
            LOG_DEBUG("EnterCruise: body type {}", enum_name<TypeNav>(body->type));
            if (body->type == TypeNav::Star) {
                if (dist > 20_ls || dist.get_km() / body->radius > 12) {
                    needToSelectNearest = false;
                    LOG_DEBUG("EnterCruise: far away from Star: {}", dist);
                }
            } else {
                if (dist.get_km() / body->radius > 5) {
                    needToSelectNearest = false;
                    LOG_DEBUG("EnterCruise: far away from Planet: {}", dist);
                }
            }
        }
        else if (st::space.bodyType == "Star") {
            LOG_DEBUG("EnterCruise: space.bodyType == Star");
            needToSelectNearest = true;
        }

        if (needToSelectNearest) {
            flyAwayFromNearest = true;
            task->nl.selectFocused(nullptr);
            status = ORIENT;
            sendUiBack();
            task->orientAwayFromTarget(10);
        } else if (st::autopilot.destBody && (wasDestBodyFocused || !wasDestDockFocused)) {
            run_sub_step(new NavBodySelect);
            task->orientTowardTarget(10);
        } else if (st::autopilot.destDock) {
            run_sub_step(new NavDockSelect);
            task->orientTowardTarget(10);
        }
    }
    sendUiBack();

    setSpeed(100, false, "EnterCruise: fly away");
    sleep(500);
    if (st::ship.flags.fsd_masslocked) {
        LOG_DEBUG("EnterCruise: mass-locked");
        timer = utc_timer(60s);
        status = MASSLOCKED;
        while (st::ship.flags.fsd_masslocked && !timer.expired()) {
            //notifyProgress("Mass-locked, flying away");
            sleep(1000);
        }
        if (st::ship.flags.fsd_masslocked)
            throw_trouble("Cannot enter cruise: mass-locked");
    }

    if (st::ship.flags.cargo_scoop_on || st::ship.flags.weapon_on || st::ship.flags.landing_gear_down) {
        status = PREPARE;
        if (st::ship.flags.cargo_scoop_on) {
            LOG_DEBUG("EnterCruise: ToggleCargoScoop");
            kbd::send("ToggleCargoScoop");
        }
        if (st::ship.flags.weapon_on) {
            LOG_DEBUG("EnterCruise: DeployHardpointToggle");
            kbd::send("DeployHardpointToggle");
        }
        if (st::ship.flags.landing_gear_down) {
            LOG_DEBUG("EnterCruise: LandingGearToggle");
            kbd::send("LandingGearToggle");
        }
        sleep(1000);
    }

    if (st::ship.flags.fsd_cooldown) {
        LOG_DEBUG("EnterCruise: FSD cooldown");
        timer = utc_timer(30s);
        status = FSD_COOLDOWN;
        while (st::ship.flags.fsd_cooldown && !timer.expired()) {
            sleep(1000);
        }
        if (st::ship.flags.fsd_cooldown)
            throw_trouble("Cannot enter cruise: FSD cooldown");
    }

    LOG_DEBUG("EnterCruise: enter Supercruise");
    timer = utc_timer(20s);
    status = ENTER_CRUISE;
    kbd::send("Supercruise", 100, 1000);
    if (!(st::ship.flags.fsd_charging || st::ship.flags.fsd_jump)) {
        notify_error("Entering supercruise failed");
        return false;
    }

    if (!st::ship.flags.cruise && (st::ship.flags.fsd_charging || st::ship.flags.fsd_jump)) {
        LOG_DEBUG("EnterCruise: waiting cruise");
        CourseLocker course(0);
        while (!st::ship.flags.cruise && (st::ship.flags.fsd_charging || st::ship.flags.fsd_jump) && !timer.expired()) {
            if (st::guiFocus != GuiFocus::None)
                sendUiBack();
            sleep(500);
        }
    }

    if (!st::ship.flags.cruise) {
        notify_error("Entering supercruise failed");
        return false;
    }

    if (flyAwayFromNearest) {
        setSpeed(100,false,"EnterCruise: flyAwayFromNearest");
        timer = utc_timer(15s);
        status = FLY_AWAY;
        while (!timer.expired()) {
            sleep(1000);
            if (!st::ship.flags.cruise) {
                setSpeed(0,false,"EnterCruise: !st::ship.flags.cruise");
                throw_trouble("Unexpected cruise exit");
            }
        }
    }

    LOG_DEBUG("EnterCruise: done");
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
    LOG_DEBUG("HyperJump to {}", destSystem);
    if (st::ship.flags.cargo_scoop_on || st::ship.flags.weapon_on || st::ship.flags.landing_gear_down) {
        if (st::ship.flags.cargo_scoop_on) {
            LOG_DEBUG("HyperJump: ToggleCargoScoop");
            kbd::send("ToggleCargoScoop");
        }
        if (st::ship.flags.weapon_on) {
            LOG_DEBUG("HyperJump: DeployHardpointToggle");
            kbd::send("DeployHardpointToggle");
        }
        if (st::ship.flags.landing_gear_down) {
            LOG_DEBUG("HyperJump: LandingGearToggle");
            kbd::send("LandingGearToggle");
        }
        sleep(1000);
    }

    if (st::ship.flags.fsd_masslocked) {
        timer = utc_timer(1min);
        status = MASSLOCKED;
        notify_info("Mass-locked, flying away");
        setSpeed(100, true, "HyperJump: Mass-locked, flying away");
        for (int cnt=0; st::ship.flags.fsd_masslocked; cnt++) {
            sleep(1000);
            if (cnt > 10) {
                setSpeed(100, true, "HyperJump: Mass-locked, flying away repeat");
                cnt = 0;
            }
        }
    }

    // select next jump system and jump
    LOG_DEBUG("HyperJump: charge");
    timer = utc_timer(15s);
    status = CHARGE;
    if (!(st::ship.flags.fsd_charging || st::ship.flags.fsd_jump)) {
        kbd::send("HyperSuperCombination", 100, 2000);
        if (!(st::ship.flags.fsd_charging || st::ship.flags.fsd_jump)) {
            if (!st::ship.flags.cruise)
                run_sub_step(new LeaveBodyStep);
            notify_error("Entering jump failed");
            return false;
        }
    }
    {
        setSpeed(50, false, "HyperJump: charging, orient");
        CourseLocker course(0);
        while (timer.sec_passed() < 9) {
            if (st::compass.has_nav_target && st::compass.targetAngle < 3)
                setSpeed(25, false, "HyperJump: charging, oriented");
            sleep(500);
        }
        setSpeed(100, false, "HyperJump: charging, accelerate");
        while (!st::ship.flags.fsd_jump) {
            if (!st::ship.flags.fsd_charging || timer.sec_passed() > 60) {
                notify_error("Jump failed");
                if (st::ship.flags.fsd_charging || st::ship.flags2.fsd_hyperdrive_charging)
                    kbd::send("HyperSuperCombination");
                setSpeed(0, false, "HyperJump: jump failed");
                return false;
            }
            sleep(500);
        }
    }

    LOG_DEBUG("HyperJump: hyperspace");
    status = HYPERSPACE;
    timer = utc_timer(60s);
    for (;;) {
        if (!st::ship.flags.fsd_jump) {
            LOG_DEBUG("HyperJump: fsd jump finished");
            break;
        }
        if (timer.expired()) {
            LOG_DEBUG("HyperJump: fsd jump timer expired");
            notify_error("Jump failed");
            return false;
        }
        sleep(250);
    }

    // avoid star
    LOG_DEBUG("HyperJump: avoid star");
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
            case 'K': // White Dwarfs, Neutron, Black Hole
                if (star->code == "K6") {
                    rotate_speed = 0;
                    fly_away_time = 15s;
                }
                break;
            }
        }
        LOG_DEBUG("HyperJump: avoid dwarf star");
    }
    setSpeed(rotate_speed, false, "HyperJump: avoid star");
    task->orientPitchStep(100, 20000);
    if (!st::ship.flags.cruise)
        run_sub_step(new LeaveBodyStep);

    LOG_DEBUG("HyperJump: fly away from star");
    status = FLY_AWAY;
    setSpeed(100, false, "HyperJump: fly away from star");
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
                    task->orientRollStep(delta, 2000);
                    continue;
                }
            }
            else if (!st::currentNavRoute || st::currentNavRoute->route.empty() && !(st::autopilot.isDestBodyTargeted||st::autopilot.isDestDockTargeted)) {
                if (st::autopilot.destBody && st::autopilot.destBody->type == TypeNav::Planet) {
                    run_sub_step((new NavBodySelect)->keepSpeed());
                    sendUiBack();
                    setSpeed(100, false, "HyperJump: fly away from star 2");
                    continue;
                }
                else if (st::autopilot.destDock) {
                    run_sub_step((new NavDockSelect)->keepSpeed());
                    sendUiBack();
                    setSpeed(100, false, "HyperJump: fly away from star 3");
                    continue;
                }
            }
        }
        sleep(1000);
    }
    if (!st::ship.flags.cruise)
        run_sub_step(new LeaveBodyStep);

    LOG_DEBUG("HyperJump: done");
    prevSubStep.reset();
    currSubStep.reset();
    status = DONE;
    return true;
}

std::string HyperJumpStep::getTitle() {
    if (status >= AVOID_STAR)
        return lc_format("Jumped to: {}", destSystem);
    return lc_format("Jumping to: {}", destSystem);
}

std::string HyperJumpStep::getStatus() {
    switch (status) {
    case DONE:
    case READY:
        return {};
    case MASSLOCKED:
        return lc_format("Mass-locked: {}", timer.passed());
    case CHARGE:
        return lc_format("Charging: {}", timer.passed());
    case HYPERSPACE:
        return lc_format("Hyperspace: {}", timer.passed());
    case AVOID_STAR:
        return _gt("Avoid star");
    case FLY_AWAY:
        return lc_format("Fly away: {}", timer.left());
    }
    return {};
}


bool LeaveBodyStep::run() {
    LOG_DEBUG("LeaveBody");
    if (st::ship.flags.cruise && !st::shipAtBody.approachBody && !st::shipAtBody.nearBody) {
        LOG_DEBUG("LeaveBody: not at body");
        prevSubStep.reset();
        currSubStep.reset();
        status = DONE;
        return true;
    }

    status = LOCK_BODY;
    if (!task->nl.focusNearestBody())
        throw_trouble("Cannot focus nearest body");
    if (!task->nl.selectFocused(nullptr))
        throw_trouble("Cannot select focused nearest body");
    fromBody = st::destination.name;
    LOG_DEBUG("LeaveBody: orient away from {}", fromBody);
    status = ORIENT;
    sendUiBack();
    task->orientAwayFromTarget(10);
    sendUiBack();

    setSpeed(100, false, "LeaveBody: leaving");
    sleep(500);
    if (!st::ship.flags.cruise) {
        if (st::ship.flags.fsd_masslocked) {
            timer = utc_timer(60s);
            status = MASSLOCKED;
            notify_info("Mass-locked, flying away");
            while (st::ship.flags.fsd_masslocked && !timer.expired()) {
                sleep(1000);
            }
            if (st::ship.flags.fsd_masslocked)
                throw_trouble("Cannot leave mass-locked area");
        }

        if (st::ship.flags.cargo_scoop_on || st::ship.flags.weapon_on || st::ship.flags.landing_gear_down) {
            status = PREPARE;
            if (st::ship.flags.cargo_scoop_on) {
                LOG_DEBUG("LeaveBody: ToggleCargoScoop");
                kbd::send("ToggleCargoScoop");
            }
            if (st::ship.flags.weapon_on) {
                LOG_DEBUG("LeaveBody: DeployHardpointToggle");
                kbd::send("DeployHardpointToggle");
            }
            if (st::ship.flags.landing_gear_down) {
                LOG_DEBUG("LeaveBody: LandingGearToggle");
                kbd::send("LandingGearToggle");
            }
            sleep(1000);
        }

        if (st::ship.flags.fsd_cooldown) {
            LOG_DEBUG("LeaveBody: FSD cooldown");
            timer = utc_timer(30s);
            status = FSD_COOLDOWN;
            while (st::ship.flags.fsd_cooldown && !timer.expired())
                sleep(1000);
        }

        LOG_DEBUG("LeaveBody: enter cruise");
        timer = utc_timer(20s);
        status = ENTER_CRUISE;
        kbd::send("Supercruise", 100, 1000);
        if (!(st::ship.flags.fsd_charging || st::ship.flags.fsd_jump)) {
            notify_error("Entering supercruise failed");
            return false;
        }
    }

    if (!st::ship.flags.cruise && (st::ship.flags.fsd_charging || st::ship.flags.fsd_jump)) {
        //CourseLocker course(0);
        while (!st::ship.flags.cruise && (st::ship.flags.fsd_charging || st::ship.flags.fsd_jump) && !timer.expired()) {
            if (st::guiFocus != GuiFocus::None)
                sendUiBack();
            ai::detectEDState(DetectLevel::Screen);
            if (ai::compassInfo.hemisphere > 1) {
                LOG_DEBUG("LeaveBody: align to exit cruise course");
                // need align to exit course
                task->orientTowardTargetStep(5, 1000);
            }
            sleep(500);
        }
    }

    if (!st::ship.flags.cruise) {
        notify_error("Entering supercruise failed");
        return false;
    }

    bool useFsdOvercharge = false;
    auto shipStats = eddb::getShipStats();
    if (st::shipAtBody.approachBody || st::shipAtBody.nearBody) {
        LOG_DEBUG("LeaveBody: leaving body in cruise");
        timer = utc_timer(60s);
        status = LEAVING_BODY;
        if (shipStats && shipStats->hasFsdSco()) {
            useFsdOvercharge = true;
            kbd::send("UseBoostJuice", 100);
        }
        while ((st::shipAtBody.approachBody || st::shipAtBody.nearBody) && !timer.expired()) {
            if (st::guiFocus != GuiFocus::None)
                sendUiBack();
            if (!st::ship.flags.cruise)
                throw_trouble("Unexpected cruise exit");
            sleep(useFsdOvercharge ? 100 : 500);
        }
    }
    if (useFsdOvercharge) {
        kbd::send("UseBoostJuice", 100, 1000);
        while (st::ship.flags2.supercruise_overcharge)
            kbd::send("UseBoostJuice", 100, 1000);
    } else {
        timer = utc_timer(15s);
        setSpeed(100, true, "LeaveBody: fly away");
        status = FLY_AWAY;
        while (!timer.expired() && st::ship.flags.cruise) {
            sleep(500);
        }
    }
    if (!st::ship.flags.cruise) {
        setSpeed(0, false, "LeaveBody: unexpected cruise exit");
        throw_trouble("Unexpected cruise exit");
    }

    LOG_DEBUG("LeaveBody: done");
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
    case TypeNav::Dodec:
    case TypeNav::Coriolis:
    case TypeNav::AsteroidBase:
    case TypeNav::SpaceOutpost:
    case TypeNav::SpaceConstrDepot:
    case TypeNav::StationMegaShip:
    case TypeNav::FleetCarrier:
    case TypeNav::SquadronCarrier:
    case TypeNav::StrongholdCarrier:
    case TypeNav::ColonisationShip:
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
    LOG_INFO("Docking, request docking permit start");
    lastDockingStatus.clear();
    status = REQUEST;
    for (int retry=0; retry < 4; retry++) {
        setSpeed(0, false, "requestDockingPermit");
        gotoNavPage("mod-contacts");

        if (ai::uiState.focused_name() != "btn-landing") {
            LOG_INFO("Docking, don't see btn-landing");
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
                LOG_INFO("Docking, steel don't see btn-landing");
                if (retry == 0)
                    continue;
            }
        }

        LOG_INFO("Docking requesting landing permission, {}", retry);
        Cfg.dockingEvent.reset();
        // poll for docking event
        timer = utc_timer(5s);
        kbd::send("UI_Right");
        kbd::send("UI_Select", 100, 700);
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
            LOG_DEBUG("Docking event {}", de->event);
            return de;
        }
        LOG_WARNING("Docking timer expired, {}", retry);
    }
    gotoNavPage("mod-nav-list");
    return {};
}

bool BaseDockStep::autopilot() {
    LOG_DEBUG("Docking, autopilot");
    ExpectSceeenLocker expectAutopilot("scr-autopilot");
    if (st::autopilot.destDock->type == TypeNav::SpaceConstrDepot)
        timer = utc_timer(4min); // speedup relogin if blocked
    else
        timer = utc_timer(8min);
    status = AUTOPILOT;
    setSpeed(0, true, "Docking, autopilot"); // set speed to 0 to start autopilot
    sendUiBack();

    // wait at least 5 seconds for autopilot to start docking
    LOG_INFO("Docking autopilot waiting...");
    for (int i=0; i < 40; i++) {
        sleep(250);
        ai::detectEDState(DetectLevel::Screen);
        if (ai::uiState.autopilot) {
            LOG_INFO("Docking autopilot started");
            break;
        }
        LOG_DEBUG("Docking autopilot waiting...");
    }
    for (;;) {
        if (timer.expired()) {
            LOG_ERROR("Autopilot time expired");
            task->relogin();
        }
        sleep(2000);
        ai::detectEDState(DetectLevel::Screen);
        if (st::ship.flags.docked) {
            LOG_INFO("Docking complete, status docked: {}, docking event: {}",
                     bool(st::ship.flags.docked), (Cfg.dockingEvent ? Cfg.dockingEvent->event : "null"));
            break;
        }
        auto de = Cfg.dockingEvent;
        if (!de || !(de->event == "DockingGranted" || de->event == "Docked")) {
            LOG_ERROR("Docking permission revoked, docking event: {}",
                      (Cfg.dockingEvent ? Cfg.dockingEvent->event : "null"));
            return false;
        }
    }

    if (st::ship.flags.docked) {
        LOG_DEBUG("Docking: docked");
        status = REFUEL;
        sleep(2000);
        gotoLandingPad(true);
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
            return lc_format("{}\nApproaching, passed {}, dist {}", lastDockingStatus, timer.passed(), st::autopilot.distanceToDock.to_string());
        return lc_format("Approaching, passed {}, dist {}", timer.passed(), st::autopilot.distanceToDock.to_string());
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
        LOG_DEBUG("DockSpaceStation: docking to statin type {}", st::autopilot.destDock->type);
        switch (st::autopilot.destDock->type) {
        case TypeNav::SpaceOutpost:
        case TypeNav::FleetCarrier:
        case TypeNav::SquadronCarrier:
        case TypeNav::ColonisationShip:
        case TypeNav::Megaship:
        case TypeNav::SpaceConstrDepot:
            LOG_DEBUG("DockSpaceStation: safe dist 6500");
            safe_dist = 6500_m;
            return;
        default:
            LOG_DEBUG("DockSpaceStation: safe dist 7300");
            safe_dist = 7300_m;
            break;
        }
    }
    if (st::space.stationType == "SpaceConstructionDepot" ||
        st::space.stationType == "FleetCarrier")
    {
        LOG_DEBUG("DockSpaceStation: safe dist 6500 for type ", st::space.stationType);
        safe_dist = 6500_m;
    }
}

bool DockSpaceStation::run() {
    LOG_DEBUG("DockSpaceStation: run");
    if (st::ship.flags.cruise)
        throw_trouble("Docking not possible in super-cruise mode");
    if (st::ship.flags.docked) {
        notify_info("Docking - already docked");
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

    setSpeed(0, false, "DockSpaceStation: init");
    updateSafeDist();

    status = PREPARE;
    // leave all UI panels
    if (st::guiFocus != GuiFocus::None) {
        for (int cnt = 0; cnt < 3; cnt++) {
            if (st::guiFocus == GuiFocus::None)
                break;
            sendUiBack();
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
        if (de && (de->event == "DockingGranted" || de->event == "Docked")) {
            LOG_DEBUG("DockSpaceStation: docking event {}", de->event);
            break;
        }
        // if we are close enough (or don't know the distance) - request docking permit
        if (!st::autopilot.distanceToDock || st::autopilot.distanceToDock >= dock_req_dist) {
            LOG_DEBUG("DockSpaceStation: fly towards, dist: {}", st::autopilot.distanceToDock);
            flyTowardsTarget();
            continue;
        }
        de = requestDockingPermit();
        LOG_INFO("Docking status: {}", (de ? de->event : "null"));
        if (de) {
            LOG_DEBUG("DockSpaceStation: docking event {}", de->event);
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
            auto reason = de->data["Reason"].as_string_or();
            if (reason == "NoSpace") {
                LOG_ERROR("DockingDenied reason: NoSpace, waiting...");
                sleep(5000);
                cnt = 0;
                continue;
            }
            if (reason == "Distance") {
                LOG_ERROR("DockingDenied reason: Distance, flying towards station...");
                // need to get close
                flyTowardsTarget();
                continue;
            }
            // if (reason == "TooLarge" || reason == "Hostile" || reason == "Offences")

            // all others are fatal
            LOG_ERROR("DockingDenied reason: {}, aborting...", reason);
            throw_failed("Docking impossible, reason: {}", reason);
        }
        // all others are fatal
        throw_failed("Unknown docking event: {}", de->event);
    }
    if (st::ship.flags.docked || (de && de->event == "Docked")) {
        LOG_ERROR("Docking - already docked");
        prevSubStep.reset();
        currSubStep.reset();
        status = DONE;
        return true;
    }
    if (!de || de->event != "DockingGranted") {
        LOG_ERROR("Docking not granted");
        return false;
    }

    if (autopilot()) {
        LOG_INFO("Docking: done");
        prevSubStep.reset();
        currSubStep.reset();
        status = DONE;
        return true;
    }
    LOG_WARNING("Docking: failed");
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
                sendUiBack();
            rollBlindCompass();
        }
        if (st::guiFocus == GuiFocus::None) {
            if (!dist[di]) {
                ai::detectEDState(DetectLevel::Screen);
                dist_t d = st::autopilot.distanceToDock;
                if (d && d > 100_m) {
                    if (d > 6000_m && d < 10000_m)
                        return true;
                    dist[di++] = d;
                }
            }
            if (!dist[di]) {
                dist_t d = task->nl.getFocusedDist(2);
                sendUiBack();
                if (d && d > 100_m) {
                    if (d > 6_km && d < 10_km)
                        return true;
                    dist[di++] = d;
                }
            }
        } else {
            if (!dist[di]) {
                dist_t d = task->nl.getFocusedDist(2);
                sendUiBack();
                if (d && d > 100_m) {
                    if (d > 6_km && d < 10_km)
                        return true;
                    dist[di++] = d;
                }
            }
            if (!dist[di]) {
                ai::detectEDState(DetectLevel::Screen);
                dist_t d = st::autopilot.distanceToDock;
                if (d && d > 100_m) {
                    if (d > 6_km && d < 10_km)
                        return true;
                    dist[di++] = d;
                }
            }
        }
        if (di >= 2) {
            for (int i=1; i < di; i++) {
                if (dist[i] != dist[0]) {
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

void DockSpaceStation::flyTowardsTarget() {
    setSpeed(0, false, "DockSpaceStation::flyTowardsTarget init");
    st::autopilot.distanceToDock = {};
    if (!getDockDistance())
        return;
    if (st::autopilot.distanceToDock >= dock_req_dist) {
        CourseLocker course(0);
        flyTowardsStep();
    }
    setSpeed(0, false, "DockSpaceStation::flyTowardsTarget exit");
}

void BaseDockStep::flyTowardsStep() {
    LOG_DEBUG("Docking: flyTowards");
    if (st::autopilot.distanceToDock < dock_req_dist)
        return;
    // distance to fly for auto-docking
    double dist = st::autopilot.distanceToDock.get(dist_t::M) - safe_dist.get(dist_t::M);
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
        timer = utc_timer(std::chrono::seconds((int)std::ceil(accel_time+break_time)));
        status = APPROACH;
        // acceleration time is not long enough to reach top speed,
        // so just accelerate and break immediately
        setSpeed(max_speed_percent, true, "Docking, fly towards accelerate");
        sleep_waiting_dist(accel_time*1000);
        setSpeed(0, true, "Docking, fly towards stop");
        sleep_waiting_dist(break_time*1000 + 500);
        return;
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
    timer = utc_timer(std::chrono::seconds((int)std::ceil(accel_time+fly_time+break_time)));
    status = APPROACH;
    setSpeed(max_speed_percent, true, "Docking, fly towards accelerate");
    sleep_waiting_dist(accel_time*1000);
    sleep_waiting_dist(fly_time*1000);
    setSpeed(0, true, "Docking, fly towards stop");
    sleep_waiting_dist(break_time*1000 + 500);
}

void BaseDockStep::sleep_waiting_dist(int milliseconds) {
    if (milliseconds <= 0)
        return;
    auto& dd = st::autopilot.distanceToDock;
    int prev_dist = 100000;
    if (dd <= 7400_m)
        prev_dist = (int)dd.get_m();
    //LOG(INFO) << "sleep_waiting_dist, safe dist " << safe_dist;
    auto now = std::chrono::high_resolution_clock::now();
    auto until = now + std::chrono::milliseconds(milliseconds);
    while (now < until) {
        check_interrupted();
        auto left = until - now;
        if (left < 5ms)
            break;
        if (left > 250ms) {
            auto until = now + 250ms;
            ai::detectEDState(DetectLevel::Screen);
            //LOG(INFO) << "sleep_waiting_dist, curr dist " << dd;
            std::this_thread::sleep_until(until);
        } else {
            std::this_thread::sleep_for(left);
        }
        now = std::chrono::high_resolution_clock::now();
        if (dd <= 7400_m) {
            //LOG(INFO) << "sleep_waiting_dist, maybe stop, dist " << dd << " and prev " << prev_dist;
            int dist = (int)dd.get_m();
            if (dist < prev_dist && prev_dist <= 7400)
                return;
            prev_dist = dist;
        }
    }
    check_interrupted();
}


bool DockPlanetPort::run() {
    LOG_DEBUG("DockPlanetPort: run");
    if (st::ship.flags.cruise)
        throw_trouble("Docking not possible in super-cruise mode");
    if (st::ship.flags.docked) {
        notify_info("Docking - already docked");
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
    if (st::autopilot.distanceToDock && st::autopilot.distanceToDock < kPlDockFar)
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
        if (de && (de->event == "DockingGranted" || de->event == "Docked")) {
            LOG_DEBUG("DockSpaceStation: docking event {}", de->event);
            break;
        }
        // if we are close enough (or don't know the distance) - request docking permit
        dist_t dist = getDockDistance(true);
        if (!dist || dist > 7_km) {
            LOG_DEBUG("DockPlanetPort: fly towards, dist: {}", dist);
            flyTowardsTarget(dist);
            continue;
        }
        de = requestDockingPermit();
        LOG_INFO("Docking status: {}", (de ? de->event : "null"));
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
                LOG_ERROR("DockingDenied reason: NoSpace, waiting...");
                sleep(5000);
                cnt = 0;
                continue;
            }
            if (reason == "Distance") {
                LOG_ERROR("DockingDenied reason: Distance, flying towards station...");
                sendUiBack();
                ai::detectEDState(DetectLevel::Screen);
                dist = getDockDistance(true);
                if (dist < 7500_m) {
                    setSpeed(50, true, "Docking denied - distance, fly towards");
                    sleep(5000);
                    setSpeed(0, true, "Docking denied - distance, try again");
                    sleep(5000);
                    throw_trouble("Incorrect docking distance");
                }
                continue;
            }
        }
        // all others are fatal
        throw_failed("Unknown docking event: {}", de->event);
    }
    if (st::ship.flags.docked || (de && de->event == "Docked")) {
        LOG_ERROR("Docking - already docked");
        prevSubStep.reset();
        currSubStep.reset();
        status = DONE;
        return true;
    }
    if (!de || de->event != "DockingGranted") {
        LOG_ERROR("Docking not granted");
        return false;
    }

    if (autopilot()) {
        LOG_INFO("Docking: done");
        prevSubStep.reset();
        currSubStep.reset();
        status = DONE;
        return true;
    }
    LOG_WARNING("Docking: failed");
    return false;
}

dist_t DockPlanetPort::getDockDistance(bool force) {
    if (st::compass.hemisphere) {
        double angle = std::abs(90 + st::compass.targetPitch);
        if (std::abs(angle) < 80) {
            double altitude = st::shipAtBody.altitude;
            double d = altitude / std::cos(angle * M_PI / 180);
            st::autopilot.distanceToDock = dist_t(dist_t::M, d);
            return st::autopilot.distanceToDock;
        }
    }
    if (force) {
        dist_t d = task->nl.getFocusedDist(3);
        if (!d)
            return d;
        st::autopilot.distanceToDock = d;
        return st::autopilot.distanceToDock;
    }
    return {};
}

bool DockPlanetPort::flyTowardsTarget(dist_t dist) {
    LOG_DEBUG("DockPlanetPort: flyTowardsTarget dist {}", dist);
    setSpeed(0, false, "DockPlanetPort::flyTowardsTarget init");
    while (dist && dist > kPlDockFar) {
        setSpeed(0, false, "DockPlanetPort::flyTowardsTarget dist too far");
        while (st::guiFocus != GuiFocus::None)
            sendUiBack();
        surface_aligned = false;
        safe_dist = kPlDockFar;
        CourseLocker course(0);
        flyTowardsStep();
        safe_dist = 7400_m;
        dist = st::autopilot.distanceToDock;
    }
    if (!surface_aligned) {
        setSpeed(0, false, "DockPlanetPort::flyTowardsTarget surface align");
        normalizeOrientation();
    }
    flyAlongSurface();
    setSpeed(0, false, "DockPlanetPort::flyTowardsTarget exit");
    return true;
}

bool DockPlanetPort::normalizeOrientation() {
    setSpeed(0, true, "DockPlanetPort::normalizeOrientation");

    status = PREPARE;
    if (!run_sub_step(new NavBodySelect))
        return false;
    for (;;) {
        ai::detectEDState(DetectLevel::Screen);
        if (ai::uiState.guiFocus != GuiFocus::None) {
            sendUiBack();
            continue;
        }
        if (!ai::compassInfo.hemisphere) {
            LOG_WARNING("Compass not detected");
            continue;
        }
        float roll = ai::compassInfo.targetRoll;
        float pitch = ai::compassInfo.targetPitch;
        if (ai::compassInfo.targetAngle > 10 && std::abs(roll) <= 165) {
            task->orientRollStep(roll-180, 5000);
            continue;
        }
        if (std::abs(pitch+90) > 15) {
            if (std::abs(pitch+90) > 35)
                task->orientPitchStep(pitch+90, 5000);
            else
                task->orientPitchStep((pitch+90)*0.7, 5000);
            continue;
        }
        surface_aligned = true;
        break;
    }
    if (!run_sub_step(new NavDockSelect))
        return false;
    return true;
}

bool DockPlanetPort::flyAlongSurface() {
    LOG_DEBUG("DockPlanetPort: flyAlongSurface");
    status = APPROACH;
    for (int step=0; step < 20; step++) {
        if (st::guiFocus != GuiFocus::None) {
            sendUiBack();
            continue;
        }
        if (st::shipAtBody.altitude < 2600) {
            LOG_DEBUG("DockPlanetPort: flyAlongSurface low altitude {}", st::shipAtBody.altitude);
            kbd::send("UpThrustButton", 5000, 500);
            continue;
        }
        ai::detectEDState(DetectLevel::Screen);
        dist_t dist = getDockDistance((step % 10) == 0);
        LOG_DEBUG("DockPlanetPort: dist {}", dist);
        if (dist && dist.get_m() < 7000) {
            LOG_DEBUG("DockPlanetPort: flyAlongSurface done");
            return true;
        }
        if (st::shipAtBody.altitude > 5000) {
            LOG_DEBUG("DockPlanetPort: flyAlongSurface high altitude {}", st::shipAtBody.altitude);
            kbd::send("DownThrustButton", 1000, 500);
            continue;
        }
        if (!ai::compassInfo.hemisphere) {
            LOG_WARNING("Compass not detected");
            step -= 1;
            continue;
        }
        float yaw = ai::compassInfo.targetYaw;
        if (ai::compassInfo.hemisphere < 0) {
            if (yaw > 0)
                yaw = 180 - yaw;
            else
                yaw = -180 - yaw;
        }
        float pitch = ai::compassInfo.targetPitch;
        if ((pitch > -60 || pitch < -120) && std::abs(yaw) > 10) {
            LOG_DEBUG("DockPlanetPort: flyAlongSurface fix yaw {}", int(yaw));
            task->orientYawStep(yaw, 5000);
            continue;
        }
        if (pitch > -70) {
            setSpeed(75, false, "DockPlanetPort: flyAlongSurface pitch > -70");
            sleep(5000);
            setSpeed(0, false, "DockPlanetPort: flyAlongSurface continue");
            continue;
        }
        if (pitch < -110) {
            setSpeed(-100, false, "DockPlanetPort: flyAlongSurface pitch < -110");
            sleep(5000);
            setSpeed(0, false, "DockPlanetPort: flyAlongSurface continue");
            continue;
        }
        LOG_DEBUG("DockPlanetPort: flyAlongSurface no action?");
    }
    return true;
}



bool NavDockSelect::run() {
    if (!mKeepSpeed)
        setSpeed(0, false, "NavDockSelect::run");
    if (!dock) {
        dock = st::autopilot.destDock;
        if (!dock) {
            status = FAILED;
            throw_failed("No destination dock");
            return false;
        }
    }
    LOG_DEBUG("NavDockSelect {}", dock->name);

    status = SELECTING;
    for (int retry = 0; retry < 3; retry++) {
        LOG_DEBUG("NavDockSelect try {}", retry);
        if (!task->nl.focusDestDock()) {
            notify_warn("Failed to find the dock in nav list");
            continue;
        }
        if (!task->nl.selectFocused(dock.get()))
            notify_warn("Failed to select the dock in nav list");
        for (int wait=0; wait < 4; wait++) {
            sleep(500);
            if (dock->nameEq(st::destination.name)) {
                LOG_DEBUG("NavDockSelect done");
                status = DONE;
                return true;
            }
        }
    }
    LOG_WARNING("NavDockSelect failed");
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
    if (!mKeepSpeed)
        setSpeed(0, false, "NavBodySelect::run");
    if (!body) {
        body = st::autopilot.destBody;
        if (!body) {
            status = FAILED;
            throw_failed("No destination body");
            return false;
        }
    }
    LOG_DEBUG("NavBodySelect {}", body->name);

    status = SELECTING;
    int missmatch = 0;
    for (int retry=0; retry < 3; retry++) {
        LOG_DEBUG("NavBodySelect try {}", retry);
        int body_conf = 0;
        if (mCheckDockBody && st::autopilot.destDock) {
            if (!task->nl.focusDockBody(&body_conf)) {
                notify_warn("Failed to find the body in nav list");
                continue;
            }
        } else {
            if (!task->nl.focusDestBody(&body_conf)) {
                notify_warn("Failed to find the body in nav list");
                continue;
            }
        }
        if (!task->nl.selectFocused(body.get()))
            notify_warn("Failed to select the body in nav list");
        for (int wait=0; wait < 4; wait++) {
            sleep(500);
            if (body->nameEq(st::destination.name)) {
                LOG_DEBUG("NavBodySelect done");
                status = DONE;
                return true;
            }
        }
        LOG_DEBUG("NavBodySelect body name missmatch {}", missmatch);
        if (missmatch >= 2 && (mCheckDockBody || body_conf >= 60)) {
            if (st::autopilot.destDock && st::autopilot.destDock->parentBodyId == body->bodyId) {
                LOG_WARNING("NavBodySelect clearing dock body");
                st::autopilot.destDock->parentBodyId = -1;
                st::autopilot.destBody.reset();
                throw_trouble("Missmatch dock and body");
            }
        }
        st::autopilot.isDestBodyFocused = false;
        st::autopilot.isDestDockFocused = false;
        missmatch += 1;
    }
    LOG_WARNING("NavBodySelect failed");
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
    if (dist) {
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

void BaseCruiseStep::checkExitSCO(bool exitSCO) {
    if (!st::ship.flags2.supercruise_overcharge)
        return;
    if (st::ship.flags.overheating)
        exitSCO = true;
    if (st::shipStats.fuelMain < 0.25f*st::shipStats.fuelCapacityMain)
        exitSCO = true;
    if (st::compass.hemisphere < 0 || std::abs(st::compass.targetAngle) > 20)
        exitSCO = true;

    if (!exitSCO)
        return;

    kbd::send("UseBoostJuice", 100, 100);
    while (st::ship.flags2.supercruise_overcharge) {
        utc_timer timer = 1s;
        while (!timer.expired()) {
            if (!st::ship.flags2.supercruise_overcharge)
                return;
            sleep(250);
        }
        // stop cruise overcharge
        kbd::send("UseBoostJuice", 100, 100);
    }
}

bool CruiseToSignal::run() {
    LOG_DEBUG("CruiseToSignal run");
    // select destination dock or body
    if (st::destination.name.empty())
        return false;
    if (!requiredDist)
        return false;

    destName = st::destination.name;
    int focusIdx;
    if (!task->nl.focusDestination(focusIdx))
        return false;

    failCount = 0;
    if (st::guiFocus == GuiFocus::None) {
        ai::detectEDState(DetectLevel::Screen);
        gotDistance(st::autopilot.distanceToTarget);
    } else {
        dist_t dist = task->nl.getFocusedDist(3);
        gotDistance(dist);
    }

    if (currentDist <= requiredDist) {
        LOG_DEBUG("CruiseToSignal stop");
        status = DIST_STOP;
        setSpeed(0, true, "CruiseToSignal currentDist <= requiredDist");
        if (useNavList)
            sendUiBack();
        sleep(5000);
        LOG_INFO("CruiseToSignal done");
        prevSubStep.reset();
        currSubStep.reset();
        status = DONE;
        return true;
    }

    if (currentDist > 50_ls)
        setSpeed(50, true, "CruiseToSignal currentDist > 50_ls");
    else
        setSpeed(25, true, "CruiseToSignal currentDist <= 50_ls");

    notify_info("Fly towards");
    useNavList = false;
    task->orientTowardTarget(5);

    failCount = 0;
    int noCompassCount = 0;

    CourseLocker course(0);
    // wait until we get to required distance
    for (;;) {
        if (!st::ship.flags.cruise) {
            setSpeed(0, false, "CruiseToSignal: unexpected cruise exit");
            throw_trouble("Unexpected cruise exit");
        }
        if (st::shipAtBody.approachBody || st::shipAtBody.nearBody) {
            setSpeed(0, false, "CruiseToSignal: unexpected landable body");
            throw_trouble("Unexpected close to body: {}", st::shipAtBody.bodyName);
        }
        if (useNavList) {
            dist_t focused_dist = task->nl.getFocusedDist(1);
            if (!gotDistance(focused_dist)) {
                if (failCount >= 3) {
                    notify_warn("Bad dist in nav list");
                    useNavList = false;
                    failCount = 0;
                } else if (failCount & 1) {
                    rollBlindCompass();
                }
                continue;
            }
            useNavList = false;
            failCount = 0;
            sendUiBack();
        } else {
            ai::detectEDState(DetectLevel::Screen);
            if (ai::uiState.guiFocus != GuiFocus::None) {
                sendUiBack();
                continue;
            }
            if (ai::compassInfo.hemisphere == 0) {
                noCompassCount += 1;
                if (noCompassCount > 20)
                    throw_trouble("Cannot see compass");
            } else {
                noCompassCount = 0;
            }
            dist_t compass_dist = st::autopilot.distanceToTarget;
            if (!gotDistance(compass_dist)) {
                if (ai::compassInfo.has_nav_target) {
                    if (failCount >= 15) {
                        notify_warn("Bad dist in target mark");
                        useNavList = true;
                        failCount = 0;
                    } else if (failCount % 5 == 0) {
                        rollBlindCompass();
                    }
                }
                else {
                    if (failCount >= 3) {
                        notify_warn("Cannot see target mark");
                        useNavList = true;
                        failCount = 0;
                        setSpeed(75, false, "CruiseToSignal: Cannot see target mark");
                    }
                    else if (ai::compassInfo.hemisphere < 0 || std::abs(ai::compassInfo.targetAngle) > 10) {
                        setSpeed(0, false, "CruiseToSignal: too big angle to course");
                        task->orientTowardTarget(5);
                    }
                }
                continue;
            }
        }
        if (currentDist >= requiredDist) {
            LOG_DEBUG("CruiseToSignal stop");
            status = DIST_STOP;
            setSpeed(0, false, "CruiseToSignal: stop");
            if (useNavList)
                sendUiBack();
            sleep(5000);
            LOG_INFO("CruiseToSignal done");
            prevSubStep.reset();
            currSubStep.reset();
            status = DONE;
            return true;
        } else {
            if (currentDist <= requiredDist * 1.5) {
                status = DIST_NEAR;
                setSpeed(25, false, "CruiseToSignal: currentDist <= requiredDist * 1.5");
            }
            else if (currentDist <= requiredDist * 5) {
                status = DIST_FAR;
                setSpeed(50, false, "CruiseToSignal: currentDist <= requiredDist * 5");
            }
            else if (currentDist <= 300_ls) {
                status = DIST_FAR;
                setSpeed(75, false, "CruiseToSignal: currentDist <= 300_ls");
            }
            else {
                status = DIST_FAR;
                setSpeed(100, false, "CruiseToSignal: currentDist > 300_ls");
            }
        }
    }

    LOG_INFO("CruiseToSignal done");
    prevSubStep.reset();
    currSubStep.reset();
    status = DONE;
    return true;
}

bool CruiseToDistStep::run() {
    LOG_INFO("CruiseToDist::run");
    // select destination dock or body
    if (!st::autopilot.destDock && !st::autopilot.destBody) {
        LOG_WARNING("CruiseToDist: no destination body and dock");
        return false;
    }
    if (!minDist || !maxDist || minDist >= maxDist) {
        LOG_WARNING("CruiseToDist: bad min/max dist");
        return false;
    }
    bool destIsSignal = false;
    bool destIsDock = false;
    if (st::autopilot.destDock && st::autopilot.destDock->nameEq(st::destination.name)) {
        destIsDock = true;
        destName = st::destination.name;
        LOG_INFO("CruiseToDist: destination is dock {}", destName);
        if (!task->nl.focusDestDock())
            return false;
    }
    else if (st::autopilot.destBody && st::autopilot.destBody->nameEq(st::destination.name)) {
        destIsDock = false;
        destName = st::destination.name;
        LOG_INFO("CruiseToDist: destination is body {}", destName);
        if (!task->nl.focusDestBody())
            return false;
    }
    else if (st::autopilot.destBody) {
        destIsDock = false;
        destName = st::autopilot.destBody->name;
        LOG_INFO("CruiseToDist: destination is body {}", destName);
        if (!run_sub_step(new NavBodySelect))
            return false;
    }
    else if (st::autopilot.destDock) {
        destIsDock = true;
        if (!st::autopilot.destDock->nloc.empty())
            destName = st::autopilot.destDock->nloc;
        else
            destName = st::autopilot.destDock->name;
        LOG_INFO("CruiseToDist: destination is dock {}", destName);
        if (!run_sub_step(new NavDockSelect))
            return false;
    }
    else {
        LOG_WARNING("CruiseToDist: destination is unknown");
        return false;
    }

    if (!(st::autopilot.isDestDockFocused || st::autopilot.isDestBodyFocused)) {
        LOG_WARNING("CruiseToDist: destination in nav list not focused");
        return false;
    }

    failCount = 0;
    requiredDist = maxDist;
    if (st::guiFocus == GuiFocus::None) {
        ai::detectEDState(DetectLevel::Screen);
        if (st::autopilot.distanceToTarget)
            gotDistance(st::autopilot.distanceToTarget);
        else if (st::autopilot.isDestBodyFocused)
            gotDistance(st::autopilot.distanceToBody);
        else if (st::autopilot.isDestDockFocused)
            gotDistance(st::autopilot.distanceToDock);
    } else {
        dist_t dist = task->nl.getFocusedDist(3);
        gotDistance(dist);
    }

    if (currentDist) {
        if (currentDist >= minDist && currentDist <= maxDist) {
            LOG_DEBUG("CruiseToDist stop");
            status = DIST_STOP;
            setSpeed(0, true, "CruiseToDist: stop");
            if (useNavList)
                sendUiBack();
            sleep(5000);
            LOG_INFO("CruiseToDist done");
            prevSubStep.reset();
            currSubStep.reset();
            status = DONE;
            return true;
        }
        flyAway = currentDist < minDist;
    }

    if (currentDist > 50_ls)
        setSpeed(50, true, "CruiseToDist: initial currentDist > 50_ls");
    else
        setSpeed(25, true, "CruiseToDist: initial currentDist <= 50_ls");

    if (flyAway) {
        requiredDist = minDist;
        notify_info("Fly away");
        useNavList = true;
        task->orientAwayFromTarget(5);
    } else {
        requiredDist = maxDist;
        notify_info("Fly towards");
        useNavList = false;
        task->orientTowardTarget(5);
    }
    failCount = 0;
    int noCompassCount = 0;

    bool useFsdOvercharge = false;
    auto shipStats = eddb::getShipStats();
    CourseLocker course(flyAway ? 180 : 0);
    // wait until we get to required distance
    for (;;) {
        if (!st::ship.flags.cruise) {
            setSpeed(0, false, "CruiseToDist: !st::ship.flags.cruise");
            throw_trouble("Unexpected cruise exit");
        }
        if (st::shipAtBody.approachBody || st::shipAtBody.nearBody) {
            setSpeed(0, false, "CruiseToDist: near body");
            throw_trouble("Unexpected close to body: {}", st::shipAtBody.bodyName);
        }
        if (flyAway && !useNavList) {
            LOG_DEBUG("CruiseToDist: useNavList because of flyAway");
            useNavList = true;
        }
        if (useNavList) {
            dist_t focused_dist = task->nl.getFocusedDist(1);
            if (!gotDistance(focused_dist)) {
                LOG_DEBUG("CruiseToDist: cannot get distance from nav list, failCount={}", failCount);
                if (!flyAway && failCount >= 3) {
                    notify_warn("Bad dist in nav list");
                    LOG_DEBUG("CruiseToDist: useNavList = false; failCount={}", failCount);
                    useNavList = false;
                    failCount = 0;
                } else if (failCount & 1) {
                    rollBlindCompass();
                }
                if (flyAway || failCount >= 3)
                    setSpeed(75, false, "CruiseToDist: flyAway || failCount >= 3");
                continue;
            }
            if (!flyAway) {
                LOG_DEBUG("CruiseToDist: useNavList = false; (have distance)");
                useNavList = false;
                failCount = 0;
                sendUiBack();
            }
        } else {
            ai::detectEDState(DetectLevel::Screen);
            if (ai::uiState.guiFocus != GuiFocus::None) {
                sendUiBack();
                continue;
            }
            if (ai::compassInfo.hemisphere == 0) {
                noCompassCount += 1;
                LOG_DEBUG("CruiseToDist: noCompassCount={}", noCompassCount);
                if (noCompassCount > 20)
                    throw_trouble("Cannot see compass");
            } else {
                noCompassCount = 0;
            }
            dist_t compass_dist = st::autopilot.distanceToTarget;
            if (!gotDistance(compass_dist)) {
                LOG_DEBUG("CruiseToDist: cannot get distance from target mark, failCount={}", failCount);
                if (ai::compassInfo.has_nav_target) {
                    if (failCount >= 15) {
                        notify_warn("Bad dist in target mark");
                        checkExitSCO(true);
                        useNavList = true;
                        failCount = 0;
                    } else if (failCount % 5 == 0) {
                        rollBlindCompass();
                    }
                    if (flyAway || failCount >= 3)
                        setSpeed(75, false, "CruiseToDist: flyAway || failCount >= 3");
                }
                else if (!flyAway) {
                    if (failCount >= 3) {
                        notify_warn("Cannot see target mark");
                        checkExitSCO(true);
                        useNavList = true;
                        failCount = 0;
                        setSpeed(25, false, "CruiseToDist: !flyAway && failCount >= 3");
                    }
                    else if (ai::compassInfo.hemisphere < 0 || std::abs(ai::compassInfo.targetAngle) > 20) {
                        checkExitSCO(true);
                        setSpeed(0, false, "CruiseToDist: too big angle to course");
                        task->orientTowardTarget(5);
                    }
                }
                else { // flyAway
                    LOG_DEBUG("CruiseToDist: useNavList because of flyAway");
                    checkExitSCO(true);
                    useNavList = true;
                    failCount = 0;
                }
                continue;
            }
        }
        if (currentDist >= minDist && currentDist <= maxDist) {
            LOG_DEBUG("CruiseToDist stop");
            status = DIST_STOP;
            checkExitSCO(true);
            setSpeed(0, true, "CruiseToDist: stop");
            if (useNavList)
                sendUiBack();
            sleep(5000);
            LOG_INFO("CruiseToDist done");
            prevSubStep.reset();
            currSubStep.reset();
            status = DONE;
            return true;
        }
        checkExitSCO(false);
        if (currentDist < minDist) {
            if (!flyAway) {
                if (ai::uiState.guiFocus != GuiFocus::None) {
                    setSpeed(0, false, "CruiseToDist: focus not cockpit");
                    sendUiBack();
                    continue;
                }
                LOG_DEBUG("CruiseToDist: flyAway = true; (currentDist < minDist)");
                flyAway = true;
                requiredDist = minDist;
                setSpeed(50, false, "CruiseToDist: !flyAway && currentDist < minDist");
                course.requestPitchRoll(180);
                task->orientAwayFromTarget(5);
                continue;
            } else {
                status = DIST_NEAR;
                setSpeed(75, false, "CruiseToDist: flyAway && currentDist < minDist");
            }
        } else { // currentDist > maxDist
            if (flyAway) {
                setSpeed(0, false, "CruiseToDist: flyAway && currentDist > maxDist");
                LOG_DEBUG("CruiseToDist: flyAway = false; (currentDist > maxDist)");
                flyAway = false;
                requiredDist = maxDist;
                if (ai::uiState.guiFocus != GuiFocus::None)
                    sendUiBack();
                course.requestPitchRoll(0);
                task->orientTowardTarget(5);
                continue;
            }
            if (currentDist <= maxDist * 1.5) {
                status = DIST_NEAR;
                setSpeed(50, false, "CruiseToDist: !flyAway && currentDist <= maxDist * 2");
            }
            else if (currentDist <= 50_ls) {
                status = DIST_CLOSE;
                setSpeed(50, false, "CruiseToDist: !flyAway && currentDist <= 100_ls");
                sleep(100);
            }
            else if (currentDist <= 500_ls) {
                checkExitSCO(true);
                status = DIST_FAR;
                setSpeed(75, false, "CruiseToDist: !flyAway && currentDist <= 1000_ls");
                sleep(500);
            }
            else {
                status = DIST_HUGE;
                if (currentDist <= 12000_ls)
                    checkExitSCO(true);
                setSpeed(100, false, "CruiseToDist: !flyAway && currentDist > 1000_ls");
                sleep(1000);
            }
        }
    }

    LOG_INFO("CruiseToDist done");
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
    case DIST_HUGE: st=_gt("Dist huge"); break;
    case DIST_FAR: st=_gt("Dist far"); break;
    case DIST_CLOSE: st=_gt("Dist close"); break;
    case DIST_NEAR: st=_gt("Dist near"); break;
    case DIST_STOP: st=_gt("Reached"); break;
    }
    return std::format("{}: {} / {}", st, cur.to_string(), req.to_string());
}

bool DiveUnderPlanetStep::run() {
    LOG_INFO("DiveUnderPlanet::run");

    setSpeed(0, false, "DiveUnderPlanet init");
    if (!st::autopilot.destBody || !st::autopilot.destDock) {
        LOG_WARNING("DiveUnderPlanet no destination dock and body");
        return false;
    }

    bool targetIsDock = st::autopilot.destDock->nameEq(st::destination.name);
    bool targetIsBody = st::autopilot.destBody->nameEq(st::destination.name);
    LOG_DEBUG("DiveUnderPlanet targetIsDock={}, targetIsBody={}", targetIsDock, targetIsBody);

    if (!(targetIsBody || targetIsDock)) {
        if (run_sub_step(new NavDockSelect)) {
            targetIsDock = true;
            LOG_DEBUG("DiveUnderPlanet targetIsDock = true;");
        }
        else if (run_sub_step(new NavBodySelect)) {
            LOG_DEBUG("DiveUnderPlanet targetIsBody = true;");
            targetIsBody = true;
        }
        else {
            LOG_WARNING("DiveUnderPlanet cannot select target dock or body");
            return false;
        }
    }

    toPort = (int(st::autopilot.destDock->type) & int(TypeNav::PlanetaryThing)) != 0;
    LOG_DEBUG("DiveUnderPlanet to planetary port={}", toPort);

    if (targetIsBody)
        st::autopilot.distanceToDock = {};
    if (targetIsDock)
        st::autopilot.distanceToBody = {};

    for (int fails=0; fails < 20; fails++) {
        if ((fails % 8) == 7) {
            status = FLY_DIVE;
            if (st::guiFocus != GuiFocus::None)
                sendUiBack();
            setSpeed(50, false, "DiveUnderPlanet, too many fails, orient");
            task->orientPitchStep(70);
            timer = utc_timer(20s);
            setSpeed(100, false, "DiveUnderPlanet, too many fails, fly away");
            while (!timer.expired()) {
                if (!st::ship.flags.cruise) {
                    setSpeed(0, false, "DiveUnderPlanet, too many fails, !st::ship.flags.cruise");
                    throw_trouble("Unexpected cruise exit");
                }
                if (st::shipAtBody.approachBody || st::shipAtBody.nearBody) {
                    setSpeed(0, false, "DiveUnderPlanet, too many fails, at landable body");
                    return false;
                }
                sleep(250);
            }
            setSpeed(50, false, "DiveUnderPlanet, too many fails, orient back");
            sleep(3000);
            task->orientPitchStep(-70);
            setSpeed(0, false, "DiveUnderPlanet, too many fails, orient stop");
            continue;
        }
        bool pointingToBody = false;
        bool pointingToDock = false;
        float disk_part = std::numeric_limits<float>::quiet_NaN();
        if (targetIsBody) {
            LOG_DEBUG("DiveUnderPlanet, oriented to body, {}", fails);
            status = ORIENT_BODY;
            if (!st::autopilot.isDestBodyFocused && !task->nl.focusDestBody()) {
                rollBlindCompass();
                continue;
            }
            if (st::guiFocus != GuiFocus::None)
                sendUiBack();
            if (!task->orientTowardTarget(2)) {
                LOG_WARNING("DiveUnderPlanet, failed to orient to body");
                continue;
            }
            pointingToBody = true;
            dist_t dist_body = st::autopilot.distanceToBody;
            if (!run_sub_step(new NavDockSelect)) {
                LOG_WARNING("DiveUnderPlanet, failed to select dock");
                continue;
            }
            sendUiBack();
            targetIsDock = true;
            targetIsBody = false;
        try_visible_again:
            // oriented towards body, but dock is selected target
            ai::detectEDState(DetectLevel::Screen);
            bool dockIsVisible = ai::compassInfo.has_nav_target;
            LOG_DEBUG("DiveUnderPlanet, dockIsVisible={}", dockIsVisible);
            float visible_body_angle = std::numeric_limits<float>::quiet_NaN();
            if (dist_body)
                visible_body_angle = std::asin(st::autopilot.destBody->radius / dist_body.get_km()) * 180 / M_PI;
            // nav target is not visible if obscured by body or is out of FOV
            if (!dockIsVisible) {
                if (ai::compassInfo.hemisphere < 0) {
                    LOG_DEBUG("DiveUnderPlanet, dockIsVisible because of back hemisphere");
                    dockIsVisible = true;
                } else if (!toPort && ai::compassInfo.hemisphere > 0 && ai::compassInfo.targetAngle >= visible_body_angle*2) {
                    LOG_DEBUG("DiveUnderPlanet, dockIsVisible because of targetAngle >= visible_body_angle*2");
                    dockIsVisible = true;
                } else if (ai::compassInfo.targetRoll < -100) {
                    LOG_DEBUG("DiveUnderPlanet, orient to see dock target mark");
                    task->orientRollByTarget(-60, 15, 10000);
                    goto try_visible_again;
                } else if (ai::compassInfo.targetRoll > 100) {
                    LOG_DEBUG("DiveUnderPlanet, orient to see dock target mark");
                    task->orientRollByTarget(60, 15, 10000);
                    goto try_visible_again;
                }
            }
            float to_body_center_angle = std::numeric_limits<float>::quiet_NaN();
            if (ai::compassInfo.hemisphere) {
                to_body_center_angle = ai::compassInfo.targetAngle;
                if (toPort && !std::isnan(visible_body_angle))
                    to_body_center_angle = std::min(to_body_center_angle, visible_body_angle);
                if (ai::compassInfo.hemisphere > 0)
                    disk_part = to_body_center_angle / visible_body_angle;
                else if (ai::compassInfo.hemisphere < 0)
                    disk_part = 100;
            }
            if (dockIsVisible) {
                if (toPort) {
                    LOG_DEBUG("DiveUnderPlanet, dockIsVisible && toPort");
                    float cruisePitch = 3;
                    float alphaO = std::numeric_limits<float>::quiet_NaN();
                    if (!std::isnan(to_body_center_angle) && dist_body && st::autopilot.destBody->radius > 0) {
                        const double angleEntry = 50;
                        const double oA = orbitShowAltitude(st::autopilot.destBody->radius);
                        double dP = dist_body.get_km();
                        double dO = st::autopilot.destBody->radius + oA;
                        double xP = sqrt(dP * dP + dO * dO - 2 * dP * dO * std::cos(angleEntry * M_PI / 180));
                        alphaO = std::asin(dO * std::sin(angleEntry * M_PI / 180) / xP) * 180 / M_PI;
                    }
                    LOG_DEBUG("DiveUnderPlanet, disk_part={}%", int(disk_part*100));
                    if (disk_part > 0.80) {
                        float T = (1.f - std::clamp(disk_part,0.80f,1.f)) / 0.20f;
                        cruisePitch = std::lerp(10.f, 0.f, T*T);
                        task->orientRollByTarget(180, 8);
                        task->orientPitchStep(-(to_body_center_angle+cruisePitch), 10000);
                        keepCruisePitch = cruisePitch;
                    //} else if (disk_part < 0.6) {
                    //    if (!std::isnan(alphaO)) {
                    //        task->orientRollByTarget(0, 3);
                    //        cruisePitch = std::clamp(alphaO + to_body_center_angle, 1.f, 25.f);
                    //        task->orientPitchStep(-cruisePitch, 10000);
                    //    }
                    //    keepCruisePitch = cruisePitch;
                    } else {
                        task->orientRollByTarget(0, 8);
                        task->orientPitchStep(to_body_center_angle, 10000);
                        keepCruisePitch = 0;
                    }
                } else {
                    LOG_DEBUG("DiveUnderPlanet, dockIsVisible && !toPort");
                    LOG_DEBUG("DiveUnderPlanet, disk_part={}%", int(disk_part*100));
                    if (disk_part < 0.85 || disk_part > 2.5 || st::autopilot.distanceToDock < dist_body) {
                        task->orientTowardTarget(5);
                        keepCruisePitch = 0;
                    } else {
                        task->orientRollByTarget(180, 8);
                        task->orientPitchStep(-(to_body_center_angle+10), 10000);
                        keepCruisePitch = 7;
                    }
                }
                LOG_INFO("DiveUnderPlanet, done");
                prevSubStep.reset();
                currSubStep.reset();
                status = DONE;
                return true;
            }
            task->orientRollByTarget(180, 20);
            if (!run_sub_step(new NavBodySelect(st::autopilot.destBody, true))) {
                LOG_WARNING("DiveUnderPlanet, failed to orient to body");
                continue;
            }
            sendUiBack();
            targetIsDock = false;
            targetIsBody = true;
            // fall to DIVE
            LOG_DEBUG("DiveUnderPlanet, to dive");
        }
        else if (targetIsDock) {
            LOG_DEBUG("DiveUnderPlanet, oriented to dock, {}", fails);
            status = ORIENT_DOCK;
            if (!st::autopilot.isDestDockFocused && !task->nl.focusDestDock()) {
                rollBlindCompass();
                continue;
            }
            if (st::guiFocus != GuiFocus::None)
                sendUiBack();
            if (!task->orientTowardTarget(2))
                continue;
            pointingToDock = true;
            bool dockIsVisible = ai::compassInfo.has_nav_target;
            LOG_DEBUG("DiveUnderPlanet, dockIsVisible={}", dockIsVisible);
            if (!run_sub_step(new NavBodySelect(st::autopilot.destBody, true))) {
                LOG_WARNING("DiveUnderPlanet, failed to select body");
                continue;
            }
            sendUiBack();
            targetIsBody = true;
            targetIsDock = false;
            ai::detectEDState(DetectLevel::Screen);
            dist_t dist_body = st::autopilot.distanceToBody;
            float visible_body_angle = std::numeric_limits<float>::quiet_NaN();
            if (dist_body)
                visible_body_angle = std::asin(st::autopilot.destBody->radius / dist_body.get_km()) * 180 / M_PI;
            float to_body_center_angle = std::numeric_limits<float>::quiet_NaN();
            if (ai::compassInfo.hemisphere != 0) {
                to_body_center_angle = ai::compassInfo.targetAngle;
                if (toPort && !std::isnan(visible_body_angle))
                    to_body_center_angle = std::min(to_body_center_angle, visible_body_angle);
            }
            if (ai::compassInfo.hemisphere > 0) {
                disk_part = to_body_center_angle / visible_body_angle;
            }
            // oriented towards dock, but body is selected target
            if (dockIsVisible) {
                if (toPort) {
                    LOG_DEBUG("DiveUnderPlanet, dockIsVisible && toPort");
                    // x^2 = dp^2 + do^2 - 2*dp*do*cos(60)
                    // dp = distance to planet (center)
                    // do = planet->radius + 1400 = distance to orbit at exit altitude
                    // x/sin(60) = dp/sin(alphaP) = do/sin(alphaO)
                    // sin(alphaO) = dO * sin(60) / x
                    // sin(alphaO) = dO * sin(60) / sqrt(dp^2 + do^2 - 2*dp*do*cos(60))
                    float cruisePitch = 3;
                    float alphaO = std::numeric_limits<float>::quiet_NaN();
                    if (!std::isnan(to_body_center_angle) && dist_body && st::autopilot.destBody->radius > 0) {
                        const double angleEntry = 50;
                        const double oA = orbitShowAltitude(st::autopilot.destBody->radius);
                        double dP = dist_body.get_km();
                        double dO = st::autopilot.destBody->radius + oA;
                        double xP = sqrt(dP * dP + dO * dO - 2 * dP * dO * std::cos(angleEntry * M_PI / 180));
                        alphaO = std::asin(dO * std::sin(angleEntry * M_PI / 180) / xP) * 180 / M_PI;
                    }
                    if (disk_part > 0.85) {
                        task->orientRollByTarget(0, 8);
                        task->orientPitchStep(-4, 10000);
                        if (!std::isnan(alphaO))
                            cruisePitch = std::max(1.f, alphaO - to_body_center_angle);
                        keepCruisePitch = cruisePitch;
                    //} else if (disk_part < 0.6) {
                    //    if (!std::isnan(alphaO)) {
                    //        task->orientRollByTarget(180, 3);
                    //        cruisePitch = std::clamp(alphaO + to_body_center_angle, 1.f, 25.f);
                    //        task->orientPitchStep(-cruisePitch, 10000);
                    //    }
                    //    keepCruisePitch = cruisePitch;
                    } else {
                        task->orientRollByTarget(0, 8);
                        keepCruisePitch = 0;
                    }
                } else {
                    LOG_DEBUG("DiveUnderPlanet, dockIsVisible && !toPort");
                    if (disk_part < 0.85 || disk_part > 2) {
                        keepCruisePitch = 0;
                    } else {
                        task->orientRollByTarget(0, 8);
                        task->orientPitchStep(-10, 10000);
                        keepCruisePitch = 7;
                    }
                }
                if (!run_sub_step(new NavDockSelect)) {
                    LOG_WARNING("DiveUnderPlanet, failed to select dock");
                    continue;
                }
                sendUiBack();
                targetIsDock = true;
                targetIsBody = false;
                LOG_INFO("DiveUnderPlanet, done");
                prevSubStep.reset();
                currSubStep.reset();
                status = DONE;
                return true;
            }
            // fall to DIVE
            LOG_DEBUG("DiveUnderPlanet, to dive");
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
            if (!run_sub_step(new NavBodySelect(st::autopilot.destBody, true)))
                throw_trouble("Cannot select body");
            sendUiBack();
            targetIsBody = true;
            targetIsDock = false;
        }
        // having distance to dock and body and angle between, calc nearest distance
        // between ship-dock line and body center, compare it with body radius
        if (pointingToDock)
            task->orientRollByTarget(0, 5);
        auto& dtb = st::autopilot.distanceToBody;
        if (dtb && dtb.get_km() < 2.25*st::autopilot.destBody->radius)
            return false; // need to fly away
        float angle_to_dive = 20;
        if (!std::isnan(disk_part) && disk_part > 0.6 && !toPort) {
            angle_to_dive = 50;
        } else if (dtb) {
            angle_to_dive = std::asin(2*st::autopilot.destBody->radius / dtb.get_km()) * 180 / M_PI;
        }
        if (angle_to_dive < 20)
            angle_to_dive = 20;
        if (!orient_pitch(angle_to_dive))
            continue;
        LOG_DEBUG("DiveUnderPlanet, fly the dive");
        if (!fly_dive(180-angle_to_dive-15))
            continue;
        LOG_DEBUG("DiveUnderPlanet, to orient");
    }
    return false;
}

bool DiveUnderPlanetStep::orient_pitch(float pitchGoal) {
    status = ORIENT_DIVE;
    setSpeed(0, false, "DiveUnderPlanet, orient pitch start");
    if (st::guiFocus != GuiFocus::None) {
        notify_info("Orientation: goto compass");
        sendUiBack();
    }
    const int rollPrecision = 5;
    const int pitchPrecision = 5;

    for (int retry=0; retry < 5; retry++) {
        LOG_DEBUG("DiveUnderPlanet, orient to dive, {}", retry);
        ai::detectEDState(DetectLevel::Screen);
        if (ai::uiState.guiFocus != GuiFocus::None) {
            notify_warn("Unexpected ui mode {}", ai::uiState.to_string());
            sendUiBack();
            continue;
        }
        if (!ai::compassInfo.hemisphere) {
            notify_warn("Compass not detected");
            continue;
        }

        float delta = ai::compassInfo.targetPitch - pitchGoal;
        if (std::abs(delta) < pitchPrecision)
            break;
        task->orientPitchStep(delta, 10000);
    }
    if (std::abs(ai::compassInfo.targetPitch - pitchGoal) > rollPrecision)
        return false;

    return true;
}

bool DiveUnderPlanetStep::fly_dive(float pitchGoal) {
    status = FLY_DIVE;

    if (st::guiFocus != GuiFocus::None) {
        notify_info("Orientation: goto compass");
        sendUiBack();
    }
    setSpeed(75, false, "DiveUnderPlanet, fly dive start");
    for (;;) {
        if (!st::ship.flags.cruise) {
            setSpeed(0, false, "DiveUnderPlanet, unexpected cruise exit");
            throw_trouble("Unexpected cruise exit");
        }
        if (st::shipAtBody.approachBody || st::shipAtBody.nearBody) {
            setSpeed(0, false, "DiveUnderPlanet, unexpected cruise exit");
            throw_trouble("Unexpected close to body: {}", st::shipAtBody.bodyName);
        }
        ai::detectEDState(DetectLevel::Screen);
        if (ai::uiState.guiFocus != GuiFocus::None) {
            sendUiBack();
            continue;
        }
        if (ai::compassInfo.hemisphere && std::abs(ai::compassInfo.targetPitch) >= pitchGoal) {
            setSpeed(0, false, "DiveUnderPlanet, near body");
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
    LOG_INFO("ExitCruise to space");

    st::autopilot.distanceToDock = {};
    dist_t dist_too_far;
    if (st::autopilot.destBody && st::autopilot.destBody->radius > 0) {
        if (st::autopilot.destBody->type == TypeNav::Star)
            dist_too_far = dist_t(dist_t::KM, st::autopilot.destBody->radius*15).convertTo(dist_t::LS);
        else
            dist_too_far = dist_t(dist_t::KM, st::autopilot.destBody->radius*25).convertTo(dist_t::LS);
    }
    if (!dist_too_far || dist_too_far < 11_ls)
        dist_too_far = 11_ls;
    status = ORIENT;
    setSpeed(0, true, "ExitCruiseToSpace, start");
    if (!st::autopilot.destDock || !st::autopilot.destDock->nameEq(st::destination.name)) {
        if (!run_sub_step(new NavDockSelect))
            return false;
        sendUiBack();
    }
    if (keepPitch == 0 && !task->orientTowardTarget(10)) {
        LOG_WARNING("Cannot orient towards target");
        return false;
    }
    double dist_km = st::autopilot.distanceToDock? st::autopilot.distanceToDock.get_km() : 15000;
    //if (dist_km > dist_too_far.get_km()) {
    //    if (st::autopilot.destBody)
    //        run_sub_step(new NavBodySelect(st::autopilot.destBody, true));
    //    throw_trouble("Too far from dock");
    //}
    LOG_DEBUG("ExitCruiseToSpace, approaching");
    status = APPROACH;
    int dist_too_far_counter = 0;
    CourseLocker course(keepPitch);
    // wait until we get to 1mm
    for (;;) {
        if (!st::ship.flags.cruise)
            throw_trouble("Unexpected cruise exit");
        if (st::guiFocus != GuiFocus::None) {
            sendUiBack();
            continue;
        }
        ai::detectEDState(DetectLevel::Screen);
        if (!st::compass.has_nav_target || std::abs(st::compass.targetYaw) > 10 || std::abs(st::compass.targetPitch-keepPitch) > 10)
            st::autopilot.distanceToDock = {};

        if (!st::autopilot.distanceToDock) {
            dist_fails += 1;
            if (dist_km < 5000 && dist_fails >= 5) {
                setSpeed(0, false, "ExitCruiseToSpace, dist_km < 5000 && dist_fails >= 5");
                task->orientTowardTarget(5);
            }
            else if ((dist_fails % 5) == 4 && keepPitch == 0)
                rollBlindCompass();
            if (dist_fails > 100)
                throw_trouble("Lost compass or cruise direction");
            continue;
        }
        dist_fails = 0;
        dist_km = st::autopilot.distanceToDock.get_km();
        if (dist_km > dist_too_far.get_km()) {
            if (++dist_too_far_counter > 10)
                throw_trouble("Too far from dock");
        } else {
            dist_too_far_counter = 0;
        }
        if (dist_km < 1000)
            exit_confirm += 1;
        else
            exit_confirm = 0;
        LOG_DEBUG("ExitCruiseToSpace, exit confirm {}", exit_confirm);
        if (exit_confirm >= 2)
            break;
        if (dist_km < 3000) {
            setSpeed(50, true, "ExitCruiseToSpace, dist_km < 3000km");
            if (keepPitch) {
                keepPitch = 0;
                course.requestPitchRoll(0);
            }
        }
        else if (dist_km <= 6000)
            setSpeed(50, false, "ExitCruiseToSpace, dist_km <= 6000km");
        else {
            setSpeed(75, false, "ExitCruiseToSpace, dist_km > 6000km");
            sleep(250);
        }
    }

    // wait until we exit super-cruise
    timer = utc_timer(10s);
    status = EXITING;
    setSpeed(25, false, "ExitCruiseToSpace, exiting");
    while (st::ship.flags.cruise && !timer.expired()) {
        kbd::send("HyperSuperCombination", 100, 1000);
        sleep(1000);
        if (st::guiFocus != GuiFocus::None)
            sendUiBack();
    }

    notify_info("Arrived, speed zero");
    setSpeed(0, false, "ExitCruiseToSpace, arrived");
    resetCompassDetects();
    sleep(500);

    for (dist_fails=0; dist_fails < 15; dist_fails++) {
        if (st::guiFocus != GuiFocus::None)
            sendUiBack();
        ai::detectEDState(DetectLevel::Screen);
        if (st::autopilot.distanceToTarget) {
            if (st::autopilot.distanceToTarget > 25_km) {
                throw_trouble("Unexpected distance after cruise exit: {}", st::autopilot.distanceToTarget.to_string());
            }
            LOG_DEBUG("ExitCruise to space, done");
            prevSubStep.reset();
            currSubStep.reset();
            status = DONE;
            return true;
        }
        if ((dist_fails % 3) == 2)
            rollBlindCompass();
    }

    notify_warn("Cannot confirm distance after cruise exit");
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
            return lc_format("Approaching, dist {} (fails {})", st::autopilot.distanceToDock.to_string(), dist_fails);
        else if (exit_confirm)
            return lc_format("Approaching, dist {} (confirm {})", st::autopilot.distanceToDock.to_string(), exit_confirm);
        else
            return lc_format("Approaching, dist {}", st::autopilot.distanceToDock.to_string());
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
    LOG_INFO("ExitCruise to planet");

    setSpeed(0, true, "ExitCruiseToPlanet start");
    status = ORIENT;
    if (!run_sub_step(new NavDockSelect))
        return false;
    if (keepPitch == 0 && !task->orientTowardTarget(10))
        return false;
    if (!(st::shipAtBody.approachBody || st::shipAtBody.nearBody)) {
        CourseLocker course(keepPitch);
        setSpeed(50, true, "ExitCruiseToPlanet, far from planet");
        timer = utc_timer(2min);
        status = FLY_TO_BODY;
        while (!(st::shipAtBody.approachBody || st::shipAtBody.nearBody) && !timer.expired()) {
            if (!st::ship.flags.cruise)
                throw_trouble("Unexpected cruise exit");
            if (st::guiFocus != GuiFocus::None)
                sendUiBack();
            sleep(250);
        }
        setSpeed(0, true, "ExitCruiseToPlanet, near planet");
        disableAutoTurn();
        if (keepPitch)
            task->orientPitchStep(keepPitch);
    }
    task->orientTowardTarget(4);

    if (!(st::shipAtBody.approachBody || st::shipAtBody.nearBody))
        throw_trouble("Cannot get to body vicinity");

    bool angle_is_close_to_tangent = false;
    double enteringDockToBodyAngle;
    {
        status = ORIENT;
        setSpeed(0, true, "ExitCruiseToPlanet, aligning to planet start");
        disableAutoTurn();
        if (!run_sub_step(new NavBodySelect))
            return false;
        task->orientRollByTarget(180, 7);
        double pitchToBody = st::compass.targetPitch;
        if (!run_sub_step(new NavDockSelect))
            return false;
        sendUiBack();
        for (int retry = 0; retry < 5; retry++) {
            if (st::guiFocus != GuiFocus::None) {
                sendUiBack();
                continue;
            }
            ai::detectEDState(DetectLevel::Screen);
            if (st::compass.has_nav_target || (st::compass.hemisphere > 0 && st::compass.targetAngle < 15))
                break;
            sleep(500);
        }
        if (!(st::compass.has_nav_target || (st::compass.hemisphere > 0 && st::compass.targetAngle < 15)))
            throw_trouble("Cannot see destination site");
        double pitchToDock = st::compass.targetPitch;
        enteringDockToBodyAngle = std::abs(pitchToDock - pitchToBody);
        double R = st::autopilot.destBody->radius;
        double altitude = st::shipAtBody.altitude * 0.001;
        double dist_to_body_center = R + altitude;
        double tangent = std::asin(R / dist_to_body_center) * 180 / M_PI;
        if (tangent - std::abs(pitchToDock - pitchToBody) < 15)
            angle_is_close_to_tangent = true;

        setSpeed(angle_is_close_to_tangent ? 50 : 25, true, "ExitCruiseToPlanet, aligning finished");
    }
    if (!st::ship.flags.cruise) {
        throw_trouble("Unexpected cruise exit");
    } else {
        timer = utc_timer(angle_is_close_to_tangent ? 4min : 3min);
        status = APPROACH;
        bool check_dist_pitch = angle_is_close_to_tangent;
        int course_pitch = 0;
        if (angle_is_close_to_tangent) {
            course_pitch = -7;
            if (st::shipInfo.shipType == "panthermkii")
                course_pitch = -4;
        }
        CourseLocker course(course_pitch);
        while (st::ship.flags.cruise && !timer.expired()) {
            sleep(250);
            if (check_dist_pitch) {
                if ((st::shipAtBody.nearBody && st::shipAtBody.altitude < 30) || st::autopilot.distanceToDock < 100_km) {
                    check_dist_pitch = false;
                    course.requestPitchRoll(0);
                }
            }
//            if (st::compass.has_nav_target && st::autopilot.distanceToTarget) {
//                double R = st::autopilot.destBody->radius;
//                double A = st::shipAtBody.altitude * 0.001;
//                double d = st::autopilot.distanceToTarget.get_km();
//                double angle = 90 - std::acos( (A*A + d*d + 2*A*R) / (2*d*(A+R)) ) * 180 / M_PI;
//                LOG_INFO("R {:5d}km;       Alt {:5d}km;     dist {:5d}km;     Angle: {:3d}",
//                                         int(R), int(A), int(d), int(angle));
//            }
        }
    }

    if (st::ship.flags.cruise)
        throw_trouble("Cannot reach planetary port");

    resetCompassDetects();
    status = CONFIRM;
    notify_info("Arrived, speed zero");
    setSpeed(0, true, "ExitCruiseToPlanet, cruise exited, gliding");
    sleep(2000);

    dist_t prev_dist;
    for (int retry=0; retry < 60; retry++) {
        ai::detectEDState(DetectLevel::Screen);
        auto& ai_dist = st::autopilot.distanceToTarget;
        if (ai_dist) {
            if (prev_dist && prev_dist != ai_dist && (prev_dist-ai_dist) < 0.5_km)
                break;
            prev_dist = ai_dist;
        }
        sleep(500);
    }

    if (!prev_dist)
        notify_warn("Cannot confirm distance after cruise exit");
    LOG_DEBUG("ExitCruiseToPlanet, align after gliding");
    if (!prev_dist || prev_dist < kPlDockFar)
        task->orientPitchStep(angle_is_close_to_tangent ? 40 : 60);
    if (prev_dist > kPlDockTooFar)
        throw_trouble("Unexpected distance after cruise exit: {}", prev_dist.to_string());
    LOG_DEBUG("ExitCruiseToPlanet, done");
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
        return lc_format("Fly to body, passed {}, dist {}", timer.passed(), st::autopilot.distanceToDock.to_string());
    }
    if (status == APPROACH) {
        return lc_format("Approaching, passed {}, dist {}", timer.passed(), st::autopilot.distanceToDock.to_string());
    }
    if (status == EXITING)
        return _gt("Exiting cruise");
    if (status == CONFIRM)
        return _gt("Checking distance");
    return {};
}

bool CompleteNavRoute::run() {
    lastNavRoute = st::currentNavRoute;
    int routeIdx = getNavRoutePosition(lastNavRoute);
    if (routeIdx < 0 || routeIdx+1 >= lastNavRoute->route.size()) {
        LOG_DEBUG("CompleteNavRoute, route empty, done");
        prevSubStep.reset();
        currSubStep.reset();
        status = DONE;
        return true;
    }
    routeIdx += 1;
    targetNextNavRoute(routeIdx);

    bool try_fast_jump = false;
    if (st::ship.flags.docked) {
        if (!run_sub_step(new DepartureStep))
            throw_trouble("Cannot departure from dock");
        auto dep = std::dynamic_pointer_cast<DepartureStep>(prevSubStep);
        if (dep && dep->compassAfterAutopilot.hemisphere) {
            if (dep->compassAfterAutopilot.targetPitch > 10 && dep->compassAfterAutopilot.targetPitch < 170)
                try_fast_jump = true;
        }
        if (!try_fast_jump) {
            targetNextNavRoute(routeIdx);
            ai::detectEDState(DetectLevel::Screen);
            if (ai::compassInfo.hemisphere > 0)
                try_fast_jump = true;
        }
    }
    if (try_fast_jump) {
        status = ORIENT;
        setSpeed(50, true, "CompleteNavRoute, try fast jump");
        if (st::shipAtBody.nearBody) {
            ai::detectEDState(DetectLevel::Screen);
            if (ai::compassInfo.hemisphere && ai::compassInfo.targetAngle > 15)
                task->orientRollByTarget(180, 5);
        }
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
    for (;;) {
        routeIdx = getNavRoutePosition(st::currentNavRoute);
        if (routeIdx < 0)
            break;
        routeIdx += 1;
        if (routeIdx >= lastNavRoute->route.size())
            break;
        lastNavRoute = st::currentNavRoute;
        targetNextNavRoute(routeIdx);
        for (int retry=0; retry < 5; retry++) {
            status = ORIENT;
            setSpeed(50, false, "CompleteNavRoute, orient 1");
            if (!task->orientTowardTarget(6))
                return false;
            if (ai::compassInfo.has_nav_target)
                break;
            ai::detectEDState(DetectLevel::Screen);
            if (ai::compassInfo.has_nav_target)
                break;
            if (st::shipAtBody.approachBody || st::shipAtBody.nearBody) {
                status = LEAVE_BODY;
                if (!run_sub_step(new LeaveBodyStep))
                    return false;
                targetNextNavRoute(routeIdx);
            }
            else if (!st::ship.flags.cruise) {
                status = ENTER_CRUISE;
                if (!run_sub_step(new EnterCruiseStep))
                    return false;
                targetNextNavRoute(routeIdx);
            }
            status = ORIENT;
            setSpeed(50, false, "CompleteNavRoute, orient 2");
            task->orientPitchStep(orientAvoid, 10000);
            orientAvoid = -orientAvoid;
            timer = utc_timer(10s);
            setSpeed(100, false, "CompleteNavRoute, fly away");
            status = FLY_AWAY;
            while (!timer.expired()) {
                if (!st::ship.flags.cruise) {
                    setSpeed(0, true, "CompleteNavRoute, unexpected cruise exit");
                    throw_trouble("Unexpected cruise exit");
                }
                if (st::shipAtBody.approachBody || st::shipAtBody.nearBody) {
                    setSpeed(50, true, "CompleteNavRoute, near body");
                    break;
                }
                sleep(250);
            }
        }
        if (lastNavRoute != st::currentNavRoute && st::currentNavRoute && st::currentNavRoute->route.size())
            lastNavRoute = st::currentNavRoute;
        status = JUMP;
        if (!run_sub_step(new HyperJumpStep))
            return false;
    }
    LOG_DEBUG("CompleteNavRoute, done");
    prevSubStep.reset();
    currSubStep.reset();
    status = DONE;
    return true;
}

void CompleteNavRoute::targetNextNavRoute(int routeIdx) {
    if (st::destination.systemAddress != st::currentNavRoute->route[routeIdx].systemAddress) {
        LOG_DEBUG("CompleteNavRoute, TargetNextRouteSystem");
        kbd::send("TargetNextRouteSystem", 0, 300);
    }
    ai::detectEDState(DetectLevel::Screen);
    if (ai::compassInfo.hemisphere == 0) {
        kbd::send("GalaxyMapOpen", 100);
        sleep(2000);
        sendUiBack(2000);
        if (st::destination.systemAddress != st::currentNavRoute->route[routeIdx].systemAddress) {
            LOG_DEBUG("CompleteNavRoute, TargetNextRouteSystem");
            kbd::send("TargetNextRouteSystem", 0, 300);
        }
    }
}

std::string CompleteNavRoute::getTitle() {
    std::string name;
    int step = 0;
    int count = 0;
    auto nr = lastNavRoute;
    if (nr && !nr->route.empty()) {
        name = nr->route.back().starSystem;
        count = nr->route.size()-1;
        step = getNavRoutePosition(nr);
        bool jumping = false;
        if (st::ship.flags.fsd_jump) {
            jumping = true;
        }
        else if (auto* sub=dynamic_cast<HyperJumpStep*>(currSubStep.get())) {
            if (sub->status == HyperJumpStep::HYPERSPACE || sub->status == HyperJumpStep::AVOID_STAR)
                jumping = true;
        }
        if (!jumping)
            step += 1;
    }
    if (status == DONE || count < 1 || step > count)
        return lc_format("Arrived {0}/{1} to: {2}", count, count, name);
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
    bool toPort = isPlanetarySite(st::autopilot.destDock->type);
    bool fromPort = st::shipAtBody.approachBody || st::shipAtBody.nearBody;
    LOG_DEBUG("CruiseAndDock, start, to planetary port={}", toPort);

    bool just_departured = false;
    if (st::ship.flags.docked) {
        if (!st::dockedAt.stationName.empty() && st::autopilot.destDock->nameEq(st::dockedAt.stationName)) {
            LOG_DEBUG("CruiseAndDock, done, already at the dock");
            prevSubStep.reset();
            currSubStep.reset();
            status = DONE;
            return true;
        }
        status = DEPARTURE;
        if (!run_sub_step(new DepartureStep))
            throw_trouble("Cannot departure from dock");
        just_departured = true;
    }

    bool at_dest_dock = !st::ship.flags.cruise && (
            st::autopilot.destDock->nameEq(st::space.stationName) ||
            st::autopilot.destDock->nameEq(st::space.bodyName)
            );
    if (at_dest_dock || (!just_departured && !st::ship.flags.cruise)) {
        run_sub_step(new NavDockSelect);
        at_dest_dock = st::autopilot.distanceToDock < 50_km;
    }
    LOG_DEBUG("CruiseAndDock, at dest dock={}", at_dest_dock);

    // check fast travel path (travel at local body)
    if (!at_dest_dock && !st::ship.flags.cruise && !toPort && !fromPort && st::space.marketId && st::autopilot.destDock && st::autopilot.destBody) {
        auto at_dock = gal::getCurrentStarSystem()->getDock(st::space.marketId);
        auto at_body = gal::getCurrentStarSystem()->getBodyById(at_dock ? at_dock->parentBodyId : -1);
        if (at_body && at_body->radius && at_body == st::autopilot.destBody) {
            if (!run_sub_step(new NavDockSelect))
                goto full_path;
            if (!task->orientTowardTarget(1))
                goto full_path;
            if (!ai::compassInfo.has_nav_target)
                goto full_path;
            if (!run_sub_step(new NavBodySelect))
                goto full_path;
            dist_t dist_body = st::autopilot.distanceToBody;
            sendUiBack();
            ai::detectEDState(DetectLevel::Screen);
            if (!dist_body || !ai::compassInfo.hemisphere) {
                sleep(1000);
                ai::detectEDState(DetectLevel::Screen);
                if (!dist_body || !ai::compassInfo.hemisphere)
                    goto full_path;
            }
            if (ai::compassInfo.hemisphere > 0 && ai::compassInfo.targetAngle < 70) {
                float visible_body_angle = std::asin(at_body->radius / dist_body.get_km()) * 180 / M_PI;
                float to_body_center_angle = ai::compassInfo.targetAngle;
                const double orbitAltitude = orbitShowAltitude(at_body->radius);
                const double bypassDistance = at_body->radius + orbitAltitude;
                float bypassAngle = std::asin(bypassDistance / dist_body.get_km()) * 180 / M_PI;
                if (to_body_center_angle < bypassAngle)
                    goto full_path;
            }
            if (!run_sub_step(new NavDockSelect))
                goto full_path;
            if (!task->orientTowardTarget(8))
                goto full_path;
            // now showt path: enter cruise and exit to space station
            status = ENTER_CRUISE;
            if (!run_sub_step(new EnterCruiseStep(true)))
                throw_trouble("Cannot enter cruise");
            if (!run_sub_step(new ExitCruiseToSpace(0)))
                throw_trouble("Failed to exit cruise");
            at_dest_dock = !st::ship.flags.cruise && (
                    st::autopilot.destDock->nameEq(st::space.stationName) ||
                    st::autopilot.destDock->nameEq(st::space.bodyName)
            );
        }
    }

full_path:
    int noCompassCount = 0;
    while (!at_dest_dock) {
        bool relaxed_min_dist = false;
        if (st::shipAtBody.approachBody || st::shipAtBody.nearBody) {
            notify_error("Unexpected close to body: {}", st::shipAtBody.bodyName);
            setSpeed(0, false, "CruiseAndDock, near landable planet");
            status = LEAVE_BODY;
            if (!run_sub_step(new LeaveBodyStep))
                throw_trouble("Cannot leave body");
            relaxed_min_dist = true;
        }
        else if (!st::ship.flags.cruise) {
            status = ENTER_CRUISE;
            if (!run_sub_step(new EnterCruiseStep))
                throw_trouble("Cannot enter cruise");
            relaxed_min_dist = true;
        }

        if (st::autopilot.destBody && st::autopilot.destBody->type == TypeNav::Planet) {
            if (!st::autopilot.destBody->nameEq(st::destination.name) || !st::autopilot.isDestBodyFocused || !st::autopilot.isDestBodyTargeted)
                run_sub_step(new NavBodySelect);
        }
        if (st::autopilot.destDock && !st::autopilot.isDestBodyTargeted) {
            if (!st::autopilot.destDock->nameEq(st::destination.name) || !st::autopilot.isDestDockFocused || !st::autopilot.isDestDockTargeted)
                run_sub_step(new NavDockSelect);
        }

        // a few degrees visible angle to stop and avoid planet
        dist_t min_dist = 0.5_ls;
        dist_t max_dist = 2.0_ls;
        if (st::autopilot.destBody) {
            if (toPort || st::autopilot.destBody->type == TypeNav::Planet) {
                auto radius = std::max(1000.0, st::autopilot.destBody->radius);
                min_dist = dist_t(dist_t::KM, radius * (relaxed_min_dist ? 2 : 4)).convertTo(dist_t::MM);
                max_dist = dist_t(dist_t::KM, radius * (toPort? 8 : 15)).convertTo(dist_t::MM);
            } else if (st::autopilot.destBody->type == TypeNav::Star) {
                auto radius = std::max((1_ls).get_km(), st::autopilot.destBody->radius);
                min_dist = dist_t(dist_t::KM, radius * (relaxed_min_dist ? 3 : 5)).convertTo(dist_t::LS);
                max_dist = dist_t(dist_t::KM, radius * 10).convertTo(dist_t::LS);
            }
        }

        ai::detectEDState(DetectLevel::Screen);
        if (ai::compassInfo.hemisphere == 0) {
            noCompassCount += 1;
            LOG_DEBUG("CruiseAndDock, no compass, {}", noCompassCount);
            if (noCompassCount > 20)
                throw_trouble("Cannot see compass");
            if (noCompassCount == 10) {
                st::destination.name.clear();
                st::autopilot.isDestDockFocused = false;
                st::autopilot.isDestBodyFocused = false;
                st::autopilot.isDestDockTargeted = false;
                st::autopilot.isDestBodyTargeted = false;
                continue;
            }
        } else {
            noCompassCount = 0;
        }

        bool skip_dive = false;
        bool skip_cruise_to_dist = false;
        if (st::autopilot.destBody && st::autopilot.destBody->nameEq(st::destination.name)) {
            if (ai::compassInfo.has_nav_target) {
                if (st::autopilot.distanceToBody) {
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
                if (st::autopilot.distanceToDock) {
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
            LOG_DEBUG("CruiseAndDock, cruising min/max dist: {} / {}", min_dist, max_dist);
            status = APPROACH;
            if (!skip_cruise_to_dist && !run_sub_step(new CruiseToDistStep(min_dist, max_dist)))
                throw_trouble("Cannot cruise to dock/body");
        }

        if (!st::autopilot.destBody && st::autopilot.destDock->parentBodyId < 0) {
            if (task->nl.focusDockBody()) {
                if (task->nl.selectFocused(st::autopilot.destDock.get())) {
                    sleep(1000); // wait for Status.json
                    int bodyId = st::destination.bodyId;
                    st::autopilot.destDock->parentBodyId = bodyId;
                    auto starSystem = gal::getCurrentStarSystem();
                    starSystem->saved = false;
                    starSystem->save();
                    st::autopilot.destBody = starSystem->getBodyById(bodyId);
                    if (st::autopilot.destBody)
                        continue; // run CruiseToDistStep again
                }
            }
        }

        if (!st::ship.flags.cruise)
            continue;

        int exitCruisePitch = 0;
        if (!skip_dive) {
            if (st::autopilot.destBody && st::autopilot.destDock) {
                setSpeed(0, false, "CruiseAndDock, dive");
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
                at_dest_dock = true;
        }
    }

    LOG_DEBUG("CruiseAndDock, docking");
    resetCompassDetects();
    status = DOCK;
    if (!toPort) {
        if (!run_sub_step(new DockSpaceStation))
            throw_trouble("Failed to dock at space station");
    } else {
        if (!run_sub_step(new DockPlanetPort))
            throw_trouble("Failed to dock at planet port");
    }

    LOG_DEBUG("CruiseAndDock, done");
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
        if (p.id == "dock") {
            destSystemName = p.value["system"].as_string_or();
            destDockName = p.value["dock"].as_string_or();
        }
    }
}

std::string TaskTravel::getTitle() {
    std::string dest = destDockName.empty() ? destSystemName : destDockName;
    if (templ.nm.empty())
        return lc_format("Travel to: {}", dest);
    return templ.name();
}

bool TaskTravel::setDestDockAndBody(bool required) {
    auto starSystem = gal::getStarSystem(destSystemName);
    if (!starSystem) {
        if (required)
            throw_trouble("Cannot select destination dock");
        return false;
    }
    st::autopilot.setDestDock(starSystem->getDock(destDockName));
    if (!st::autopilot.destDock) {
        if (required)
            throw_failed("Cannot find destination dock: {0} in system {1}", destDockName, starSystem->systemName);
        return false;
    }

    if (required && st::autopilot.destDock->parentBodyId < 0) {
        initNavFilter();
        if (!run_sub_step(new NavDockSelect)) {
            throw_trouble("Cannot select destination dock");
        }
        sendUiBack();
    }

    int bodyId = st::autopilot.destDock->parentBodyId;
    if (bodyId >= 0) {
        auto body = starSystem->getBodyById(bodyId);
        if (!body) {
            LOG_ERROR("Cannot find body id: {0} in system {1}" , bodyId, starSystem->systemName);
            st::autopilot.destDock->parentBodyId = -1;
        } else if (!isBody(body->type)) {
            LOG_ERROR("Not a star/planet body id: {0} in system {1}" , bodyId, starSystem->systemName);
            st::autopilot.destDock->parentBodyId = -1;
        } else {
            st::autopilot.setDestBody(body);
        }
    }

    initNavFilter();
    return true;
}

bool TaskTravel::run() {
    st::autopilot = {};
    resetCompassDetects();
    if (destSystemName.empty() || destDockName.empty())
        throw_failed("Destination system and dock required");

    LOG_INFO("TaskTravel, to system '{}' port '{}'", destSystemName, destDockName);
    if (st::ship.flags.docked) {
        gotoLandingPad(false);
    }

    if (gal::getCurrentStarSystem()->systemName != destSystemName) {
        setDestDockAndBody(false);

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

    setDestDockAndBody(true);

    if (!run_sub_step(new CruiseAndDock))
        throw_trouble("Cannot cruise and dock");

    LOG_INFO("TaskTravel, done");
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
    st::autopilot = {};
    resetCompassDetects();
    LOG_INFO("Autopilot, start");
    if (int ri=getNavRoutePosition(st::currentNavRoute); ri >= 0 && ri+1 < st::currentNavRoute->route.size()) {
        nl.init(st::navFilters);
        if (!run_sub_step(new CompleteNavRoute))
            throw_trouble("Cannot reach destination system");
    }

    if (!st::autopilot.destDock && !st::autopilot.destBody) {
        if (destName.empty() && st::destination.name.empty())
            throw_failed("No destination dock selected");
        if (destName.empty())
            destName = st::destination.name;

        auto starSystem = gal::getCurrentStarSystem();
        gal::spEntity dest = starSystem->getEntity(destName);
        if (dest) {
            if (isBody(dest->type))
                st::autopilot.setDestBody(dest);
            else if (isSite(dest->type))
                st::autopilot.setDestDock(dest);
        } else {
            nl.discoverSelected();
            dest = starSystem->getEntity(destName);
            if (dest) {
                if (isBody(dest->type))
                    st::autopilot.setDestBody(dest);
                else if (isSite(dest->type))
                    st::autopilot.setDestDock(dest);
            }
        }

        int destBodyId = st::destination.bodyId;
        if (dest && isBody(dest->type))
            assert(dest->bodyId == destBodyId);
        else if (dest && isSpaceStation(dest->type))
            destBodyId = dest->parentBodyId;
        if (destBodyId >= 0) {
            auto body = starSystem->getBodyById(destBodyId);
            if (!body) {
                LOG_ERROR("Cannot find body id: {0} in system {1}" , destBodyId, starSystem->systemName);
                if (dest && isSite(dest->type))
                    dest->parentBodyId = -1;
            } else if (!isBody(body->type)) {
                LOG_ERROR("Not a star/planet body id: {0} in system {1}" , destBodyId, starSystem->systemName);
                if (dest && isSite(dest->type))
                    dest->parentBodyId = -1;
            } else {
                st::autopilot.setDestBody(body);
            }
        }
    }

    nl.init(st::navFilters);

    if (!st::autopilot.destDock && !st::autopilot.destBody) {
        if (!run_sub_step(new CruiseToSignal(0.9_ls)))
            throw_trouble("Cannot cruise to signal");
        if (destName.empty())
            throw_failed("No destination dock selected");
        nl.discoverSelected();
        return false;
    }

    if (!run_sub_step(new CruiseAndDock))
        throw_trouble("Cannot cruise and dock");

    LOG_INFO("Autopilot, done");
    return true;
}

} // ai
