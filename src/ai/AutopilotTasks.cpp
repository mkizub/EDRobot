//
// Created by mkizub on 28.06.2025.
//

#include "../pch.h"

#include "Task.h"
#include "AIManager.h"
#include "AutopilotTasks.h"
#include "../State.h"
#include "../OCR.h"
#include "../FuzzyMatch.h"
#include "../Galaxy.h"
#include "../Keyboard.h"
#include "../ShipStats.h"
#include "../EDWidget.h"

using namespace std::chrono_literals;

namespace ai {

void rollBlindCompass() {
    const KeyBindings& bind = Cfg.getGameKeyBindings("RollAxisRaw");
    if ((bind.mode == KeyBindings::Axis || bind.mode == KeyBindings::AxisInv) && bind.primary.device == GameKey::vJoy) {
        kbd::axis(bind, 1);
        ai_sleep(800);
        kbd::axis(bind, 0);
        ai_sleep(500);
    } else {
        kbd::send("RollRightButton", 800, 500);
    }
}

int getNavPageIndex(const std::string &page_name) {
    int pageIndex = -1;
    if (page_name == "mod-sysinfo")
        pageIndex = 0;
    else if (page_name == "mod-navigation")
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

bool gotoNavPage(Step *task, const std::string &page_name) {
    int targetPageIndex = getNavPageIndex(page_name);
    if (targetPageIndex < 0)
        task->task_return(Result::Failure);

    for (int i = 0; i < 6 && !task->mgr.uiState.match("scr-left-panel:" + page_name); i++) {
        task->mgr.detectEDState(DetectLevel::Screen);
        LOG(DEBUG) << "Goto '" << page_name << "'...";

        if (task->mgr.uiState.guiFocus == GuiFocus::None) {
            LOG(DEBUG) << "FocusLeftPanel...";
            kbd::send("FocusLeftPanel", 0, 1500);
            continue;
        }
        if (task->mgr.uiState.guiFocus == GuiFocus::Left && !task->mgr.uiState.screen) {
            rollBlindCompass();
            continue;
        }
        if (!task->mgr.uiState.match("scr-left-panel:*")) {
            LOG(DEBUG) << "FocusLeftPanel...";
            kbd::send("FocusLeftPanel", 0, 1500);
            continue;
        }
        if (task->mgr.uiState.match("scr-left-panel:dlg-nav-select") ||
            task->mgr.uiState.match("scr-left-panel:dlg-filters")) {
            kbd::send("UI_Back", 0, 500);
            continue;
        }
        std::vector<std::string> segments = task->mgr.uiState.splitPath();
        if (segments.size() < 2) {
            LOG(ERROR) << "Expecting 2 segments in " << task->mgr.uiState;
            task->task_return(Result::Failure);
        }
        int currentPageIndex = getNavPageIndex(segments[1]);
        if (currentPageIndex < 0)
            task->task_return(Result::Failure);
        int dist = currentPageIndex - targetPageIndex;
        if (dist >= 0) {
            for (int j = 0; j < dist; j++)
                kbd::send("CycleNextPanel", 0, 250);
        } else {
            for (int j = 0; j < -dist; j++)
                kbd::send("CyclePreviousPanel", 0, 250);
        }
    }
    if (!task->mgr.uiState.match("scr-left-panel:" + page_name))
        task->task_return(Result::Trouble);
    return true;
}

BaseAutopilotTask::BaseAutopilotTask(Task* parent, AIManager& mgr, const TaskTemplate& templ)
    : Task(parent, mgr, templ)
{}

BaseAutopilotStep::BaseAutopilotStep(Step* parent)
    : Step(parent, parent->mgr)
{
    task = dynamic_cast<BaseAutopilotTask*>(getTask());
    if (!task) {
        LOG(ERROR) << "BaseAutopilotStep needs BaseAutopilotTask";
        throw std::runtime_error("BaseAutopilotStep needs BaseAutopilotTask");
    }
}


void BaseAutopilotTask::relogin() {
    // something is really wrong, logout and login again
    notifyProgress("Something is wrong with departure, trying to re-login");
    kbd::send("Pause", 0, 1000);
    kbd::send("UI_Up", 0, 100); // go to Exit button
    kbd::send("UI_Select", 0, 1000); // logout
    kbd::send("UI_Select", 0, 8000); // logout to main menu
    notifyProgress("Login to Solo...");
    kbd::send("UI_Select", 0, 3000); // login, select mode screen
    kbd::send("UI_Right", 0, 100);
    kbd::send("UI_Right", 0, 500);  // choose Solo
    kbd::send("UI_Select", 0, 12000); // login
    notifyError("Finished re-login", Result::Trouble);
}

bool BaseAutopilotTask::setSpeed(int percents) {
    percents = std::clamp(percents, -100, +100);
    switch (percents / 25) {
    case 4:
        kbd::send("SetSpeed100", 50);
        speed_set_to = 100;
        break;
    case 3:
        kbd::send("SetSpeed75", 50);
        speed_set_to = 75;
        break;
    case 2:
        kbd::send("SetSpeed50", 50);
        speed_set_to = 50;
        break;
    case 1:
        kbd::send("SetSpeed25", 50);
        speed_set_to = 25;
        break;
    case 0:
        kbd::send("SetSpeedZero", 50);
        speed_set_to = 0;
        break;
    case -1:
        kbd::send("SetSpeedMinus25", 50);
        speed_set_to = -25;
        break;
    case -2:
        kbd::send("SetSpeedMinus50", 50);
        speed_set_to = -50;
        break;
    case -3:
        kbd::send("SetSpeedMinus75", 50);
        speed_set_to = -75;
        break;
    case -4:
        kbd::send("SetSpeedMinus100", 50);
        speed_set_to = -100;
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

void BaseAutopilotTask::orientRollStep(double requiredRoll, int max_time_ms) {
    float delta = mgr.compassInfo.targetRoll - requiredRoll;
    if (delta > 180) delta = 360-delta;
    if (delta < -180) delta = 360+delta;
    auto shipStats = eddb::getShipStats();
    if (!shipStats)
        task_return(Result::Failure);
    double speed = shipStats->getRollSpeed(speed_set_to);
    const KeyBindings& bind = Cfg.getGameKeyBindings("RollAxisRaw");
    if ((bind.mode == KeyBindings::Axis || bind.mode == KeyBindings::AxisInv) && bind.primary.device == GameKey::vJoy) {
        double value = delta / speed;
        int duration, pause;
        if (std::abs(value) >= 1) {
            duration = std::min(max_time_ms, int(1000 * std::abs(value)));
            pause = 10*speed;
            value = value > 0 ? +1.0 : -1.0;
        } else {
            duration = 1000;
            pause = 1000 / (25+std::abs(value));
        }
        notifyProgress(std::format("Orientation: fix roll {} (joystick) hold {}ms", delta, duration));
        kbd::axis(bind, value);
        sleep(duration);
        kbd::axis(bind, 0);
        sleep(pause);
    } else {
        int duration = std::min(max_time_ms, getDuration(delta, speed));
        int pause = duration < 1000 ? duration : 1000;
        notifyProgress(std::format("Orientation: fix roll {} (button) hold {}ms", delta, duration));
        if (delta > 0)
            kbd::send("RollRightButton", duration, pause);
        else
            kbd::send("RollLeftButton", duration, pause);
    }
}

void BaseAutopilotTask::orientPitchStep(double requiredPitch, int max_time_ms) {
    float delta = mgr.compassInfo.targetPitch - requiredPitch;
    if (delta > 180) delta = 360-delta;
    if (delta < -180) delta = 360+delta;
    auto shipStats = eddb::getShipStats();
    if (!shipStats)
        task_return(Result::Failure);
    double speed = shipStats->getPitchSpeed(speed_set_to);
    const KeyBindings& bind = Cfg.getGameKeyBindings("PitchAxisRaw");
    if ((bind.mode == KeyBindings::Axis || bind.mode == KeyBindings::AxisInv) && bind.primary.device == GameKey::vJoy) {
        double value = -delta / speed;
        int duration, pause;
        if (std::abs(value) >= 1) {
            duration = std::min(max_time_ms, int(1000 * std::abs(value)));
            pause = 10*speed;
            value = value > 0 ? +1.0 : -1.0;
        } else {
            duration = 1000;
            pause = 1000 / (25+std::abs(value));
        }
        notifyProgress(std::format("Orientation: fix pitch {} (joystick) hold {}ms", delta, duration));
        kbd::axis(bind, value);
        sleep(duration);
        kbd::axis(bind, 0);
        sleep(pause);
    } else {
        int duration = std::min(max_time_ms, getDuration(delta, speed));
        int pause = duration < 1000 ? duration : 1000;
        notifyProgress(std::format("Orientation: fix pitch {} (button) hold {}ms", delta, duration));
        if (delta > 0)
            kbd::send("PitchUpButton", duration, pause);
        else
            kbd::send("PitchDownButton", duration, pause);
    }
}

void BaseAutopilotTask::orientYawStep(double requiredYaw, int max_time_ms) {
    float delta = mgr.compassInfo.targetYaw - requiredYaw;
    if (delta > 180) delta = 360-delta;
    if (delta < -180) delta = 360+delta;
    auto shipStats = eddb::getShipStats();
    if (!shipStats)
        task_return(Result::Failure);
    double speed = shipStats->getYawSpeed(speed_set_to);
    const KeyBindings& bind = Cfg.getGameKeyBindings("YawAxisRaw");
    if ((bind.mode == KeyBindings::Axis || bind.mode == KeyBindings::AxisInv) && bind.primary.device == GameKey::vJoy) {
        double value = delta / speed;
        int duration, pause;
        if (std::abs(value) >= 1) {
            duration = std::min(max_time_ms, int(1000 * std::abs(value)));
            pause = 10*speed;
            value = value > 0 ? +1.0 : -1.0;
        } else {
            duration = 1000;
            pause = 1000 / (25+std::abs(value));
        }
        notifyProgress(std::format("Orientation: fix yaw {} (joystick)", delta, duration));
        kbd::axis(bind, value);
        sleep(duration);
        kbd::axis(bind, 0);
        sleep(pause);
    } else {
        int duration = std::min(max_time_ms, getDuration(delta, speed));
        int pause = duration < 1000 ? duration : 1000;
        notifyProgress(std::format("Orientation: fix yaw {} (button) hold {}ms", delta, duration));
        if (delta > 0)
            kbd::send("YawRightButton", duration, pause);
        else
            kbd::send("YawLeftButton", duration, pause);
    }
}

bool BaseAutopilotTask::orientTowardTargetStep(double precision, int max_time_ms) {
    check_interrupted();
    if (!mgr.compassInfo.has_nav_target)
        precision = std::max(3.0, precision);
    bool front = mgr.compassInfo.hemisphere > 0;
    int hemiYaw = mgr.compassInfo.targetYaw;
    if (!front) {
        if (hemiYaw > 0)
            hemiYaw = 180 - hemiYaw;
        else
            hemiYaw = -180 - hemiYaw;
    }
    if (std::abs(hemiYaw) > 20) {
        float roll = mgr.compassInfo.targetRoll;
        if (std::abs(roll) <= 90)
            orientRollStep(0, max_time_ms);
        else
            orientRollStep(180, max_time_ms);
        return false;
    }

    float pitch = mgr.compassInfo.targetPitch;
    float yaw = mgr.compassInfo.targetYaw;

    if (std::abs(pitch) > precision) {
        orientPitchStep(0, max_time_ms);
        return false;
    }

    if (std::abs(yaw) > precision) {
        orientYawStep(0, max_time_ms);
        return false;
    }
    return true;
}

bool BaseAutopilotTask::orientTowardTarget(double precision) {
    if (mgr.uiState.guiFocus != GuiFocus::None) {
        notifyProgress("Orientation: goto compass");
        kbd::send("UI_Back", 0, 1500);
    }
    for (int fails=0; fails < 10; fails++) {
        check_interrupted();
        if (fails > 2) {
            setSpeed(0);
            continue;
        }
        mgr.detectEDState(DetectLevel::Screen);
        if (mgr.uiState.guiFocus != GuiFocus::None) {
            notifyProgress(std::format("Unexpected ui mode {}", mgr.uiState.to_string()));
            LOG(WARNING) << "Unexpected ui mode " << mgr.uiState;
            kbd::send("UI_Back", 0, 1500);
            continue;
        }
        if (!mgr.compassInfo.hemisphere) {
            notifyProgress(std::format("Compass not detected, fails {}", fails));
            LOG(WARNING) << "Compass not detected";
            rollBlindCompass();
            continue;
        }
        fails = 0;
        if (orientTowardTargetStep(precision))
            return true;
    }
    LOG(ERROR) << "Compass not detected";
    return false;
}

bool BaseAutopilotTask::orientAwayFromTargetStep(double precision, int max_time_ms) {
    check_interrupted();
    precision = std::max(5.0, precision);
    bool front = mgr.compassInfo.hemisphere > 0;
    int hemiYaw = mgr.compassInfo.targetYaw;
    if (!front) {
        if (hemiYaw > 0)
            hemiYaw = 180 - hemiYaw;
        else
            hemiYaw = -180 - hemiYaw;
    }
    if (std::abs(hemiYaw) > 20) {
        float roll = mgr.compassInfo.targetRoll;
        if (std::abs(roll) <= 90)
            orientRollStep(0, max_time_ms);
        else
            orientRollStep(180, max_time_ms);
        return false;
    }

    auto shipStats = eddb::getShipStats();
    if (!shipStats)
        task_return(Result::Failure);

    float pitch = mgr.compassInfo.targetPitch;
    float yaw = mgr.compassInfo.targetYaw;

    if (180-std::abs(pitch) > precision) {
        orientPitchStep(180);
        return false;
    }

    if (180-std::abs(yaw) > precision) {
        orientYawStep(180);
        return false;
    }
    return true;
}

bool BaseAutopilotTask::orientAwayFromTarget(double precision) {
    if (mgr.uiState.guiFocus != GuiFocus::None) {
        notifyProgress("Orientation: goto compass");
        kbd::send("UI_Back", 0, 1500);
    }
    int speedDropped = 0;
    for (int fails=0; fails < 10; fails++) {
        check_interrupted();
        if (fails > 2) {
            setSpeed(0);
            continue;
        }
        mgr.detectEDState(DetectLevel::Screen);
        if (mgr.uiState.guiFocus != GuiFocus::None) {
            notifyProgress(std::format("Unexpected ui mode {}", mgr.uiState.to_string()));
            LOG(WARNING) << "Unexpected ui mode " << mgr.uiState;
            kbd::send("UI_Back", 0, 1500);
            continue;
        }
        if (!mgr.compassInfo.hemisphere) {
            notifyProgress(std::format("Compass not detected, fails {}", fails));
            LOG(WARNING) << "Compass not detected";
            kbd::send("RollRightButton", 800, 500);
            continue;
        }
        fails = 0;
        if (orientAwayFromTargetStep(precision))
            return true;
    }
    LOG(ERROR) << "Compass not detected";
    return false;
}

TaskDebugAutopilot::TaskDebugAutopilot(ai::Task *parent, ai::AIManager &mgr, const ai::TaskTemplate &templ)
        : BaseAutopilotTask(parent, mgr, templ)
{
    assert (templ.name == ED_TASK_DEBUG_AUTOPILOT);
    for (auto& p : templ.params) {
        if (p.name == "test")
            test = std::get<std::string>(p.value);
        if (p.name == "target")
            target = std::get<std::string>(p.value);
        if (p.name == "precision")
            orient_precision = std::get<double>(p.value);
    }
}


Result TaskDebugAutopilot::run() {
    auto starSystem = gal::getCurrentStarSystem();
    if (target.empty())
        target = st::destination.name;
    destDock = starSystem->getDock(target);
    if (destDock) {
        auto body = starSystem->getBodyById(destDock->parentBodyId);
        destBody = std::dynamic_pointer_cast<gal::Body>(body);
    } else {
        destBody = starSystem->getBody(target);
    }

    st::NavPanelFilters filters = {};
    filters.star = true;
    filters.planetOrMoon = true;
    filters.landablePlanetOrMoon = true;
    filters.station = true;
    if (destDock) {
        if (destDock->typeNav == gal::TypeNav::SpacePort)
            filters.station = true;
        if (destDock->typeNav == gal::TypeNav::Carrier)
            filters.fleetCarrier = true;
        if (destDock->typeNav == gal::TypeNav::MegashipDock) {
            if (destDock->typeSite == gal::TypeSite::TrailblazerDream)
                filters.pointOfInterest = true;
            else
                filters.station = true;
        }
    }

    nl.init(this, destDock, destBody, filters);

    setSpeed(0);

    if (test == "OrientTowards") {
        setSpeed(0);
        orientTowardTarget(orient_precision);
        setSpeed(0);
    }
    else if (test == "OrientAway") {
        setSpeed(0);
        orientAwayFromTarget(orient_precision);
        setSpeed(0);
    }
    else if (test == "Departure") {
        run_sub_step(spStep(new DepartureStep(this)));
    }
    else if (test == "Dock") {
        run_sub_step(spStep(new DockStep(this)));
    }
    else if (test == "EnterCruise") {
        run_sub_step(spStep(new EnterCruiseStep(this)));
    }
    else if (test == "LeaveBody") {
        run_sub_step(spStep(new LeaveBodyStep(this)));
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
    else if (test == "NavDockSelect") {
        run_sub_step(spStep(new NavDockSelect(this, destDock)));
    }
    else if (test == "NavBodySelect") {
        run_sub_step(spStep(new NavBodySelect(this, destBody)));
    }
    else if (test == "CruiseToDist") {
        double dist_km = 15000;
        if (destBody && destBody->radius > 0)
            dist_km = destBody->radius * 50;
        run_sub_step(spStep(new CruiseToDistStep(this, dist_km)));
    }
    else if (test == "DiveUnderPlanet") {
        run_sub_step(spStep(new DiveUnderPlanetStep(this)));
    }
    else if (test == "ExitCruiseToStation") {
        run_sub_step(spStep(new ExitCruiseToStationStep(this)));
    }
    return Result::Success;
}


DepartureStep::DepartureStep(ai::Step *parent)
    : BaseAutopilotStep(parent)
{
}

bool DepartureStep::step() {
    bool fromSpaceConstruction = false; // need UpThrustButton
    if (st::dockedAt.stationType == "SpaceConstructionDepot") {
        fromSpaceConstruction = true;
    }

    if (st::ship.flags.docked) {
        if (st::guiFocus != GuiFocus::None) {
            LOG(INFO) << "Going to dock...";
            status = GOING_TO_DOCK;
            for (int i = 0; i < 10 && st::guiFocus != GuiFocus::None; i++) {
                kbd::send("UI_Back", 0, 1000);
                mgr.detectEDState(DetectLevel::Screen);
            }
        }
        mgr.detectEDState(DetectLevel::Screen);
        if (st::guiFocus != GuiFocus::None)
            return false;

        LOG(INFO) << "Refuel...";
        status = REFUEL;
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
        timer = utc_timer(20s);
        status = TAKEOFF;
        while (st::ship.flags.docked && !timer.expired()) {
            sleep(1000);
            mgr.detectEDState(DetectLevel::Screen);
            if (mgr.uiState.autopilot)
                break;
        }
        if (st::ship.flags.docked && !mgr.uiState.autopilot)
            return false;
    }
    if (!mgr.uiState.autopilot) {
        LOG(INFO) << "Departure autopilot waiting...";
        // 15 seconds wait autopilot
        timer = utc_timer(15s);
        status = WAIT_AUTOPILOT;
        // wait at least 15 seconds for autopilot to departure
        while (!mgr.uiState.autopilot && !timer.expired()) {
            sleep(250);
            mgr.detectEDState(DetectLevel::Screen);
        }
    }
    // 4 minutes for departure
    timer = utc_timer(4min);
    status = AUTOPILOT;
    notAutoPilotCounter = 0;
    int logCounter = 0;
    for (;;) {
        if (timer.expired()) {
            LOG(ERROR) << "Autopilot time expired";
            status = RELOGIN;
            task->relogin();
            return false;
        }
        sleep(250);
        mgr.detectEDState(DetectLevel::Screen);
        if (mgr.uiState.autopilot) {
            LOG(INFO) << "Still in auto-pilot...";
            notAutoPilotCounter = 0;
            continue;
        }
        if (++notAutoPilotCounter > 4) {
            LOG(INFO) << "Departure complete (autopilot off)";
            break;
        } else {
            LOG(INFO) << "Auto-pilot off counter: " << notAutoPilotCounter;
        }
    }

    timer = utc_timer(1min);
    status = MASSLOCKED;
    if (fromSpaceConstruction) {
        task->setSpeed(0);
        kbd::send("UpThrustButton", 15000);
    }
    task->setSpeed(100);
    while (st::ship.flags.fsd_masslocked) {
        LOG(INFO) << "Mass-locked, flying away";
        sleep(1000);
    }
    LOG(INFO) << "Ready to jump, flying away";
    timer = utc_timer(15s);
    status = FLYAWAY;
    sleep(10000);
    task->setSpeed(50);
    return true;
}

std::string DepartureStep::getStatus() {
    switch (status) {
    case READY:
        return "Ready";
    case GOING_TO_DOCK:
        return "Going to dock";
    case REFUEL:
        return "Refuel";
    case TAKEOFF:
        return std::format("Takeoff: {}s", timer.left());
    case WAIT_AUTOPILOT:
        return std::format("Wait for autopilot: {}s", timer.left());
    case AUTOPILOT:
        if (notAutoPilotCounter > 0)
            return std::format("Autopilot exiting: {}", notAutoPilotCounter);
        else
            return std::format("Autopilot: {}", timer.left());
    case MASSLOCKED:
        return std::format("Mass-locked: {}", timer.passed());
    case FLYAWAY:
        return std::format("Fly away: {}", timer.passed());
    case RELOGIN:
        return "Re-login";
    default:
        return "----";
    }
}


bool EnterCruiseStep::step() {
    if (st::ship.flags.cruise)
        return true;

    status = LOCK_BODY;
    if (task->nl.focusNearestBody()) {
        auto nle = task->nl.getFocusedEntry();
        gal::spBody body = std::dynamic_pointer_cast<gal::Body>(nle->item);
        if (body && body->radius > 0 && nle->dist.valid()) {
            if (nle->dist.get(dist_t::KM) / body->radius < 4) {
                task->nl.selectFocused();
                status = ORIENT;
                kbd::send("UI_Back", 0, 500);
                task->orientAwayFromTarget(10);
            }
        }
    }
    kbd::send("UI_Back");

    task->setSpeed(100);
    sleep(500);
    while (st::ship.flags.fsd_masslocked) {
        timer = utc_timer(60s);
        status = MASSLOCKED;
        //notifyProgress("Mass-locked, flying away");
        sleep(1000);
    }

    if (st::ship.flags.cargo_scoop_on || st::ship.flags.weapon_on) {
        status = PREPARE;
        if (st::ship.flags.cargo_scoop_on)
            kbd::send("ToggleCargoScoop");
        if (st::ship.flags.weapon_on)
            kbd::send("DeployHardpointToggle");
        sleep(1000);
    }

    while (st::ship.flags.fsd_cooldown) {
        timer = utc_timer(20s);
        status = FSD_COOLDOWN;
        sleep(1000);
    }

    timer = utc_timer(20s);
    status = ENTER_CRUISE;
    //notifyProgress("Entering supercruise");
    kbd::send("Supercruise", 100, 1000);
    if (!(st::ship.flags.fsd_charging || st::ship.flags.fsd_jump)) {
        parent->notifyProgress("Entering supercruise failed");
        return false;
    }

    while (!st::ship.flags.cruise && (st::ship.flags.fsd_charging || st::ship.flags.fsd_jump) && !timer.expired()) {
        mgr.detectEDState(DetectLevel::Screen);
        if (mgr.uiState.guiFocus != GuiFocus::None) {
            kbd::send("UI_Back", 0, 1500);
            continue;
        }
        if (mgr.compassInfo.hemisphere > 0) {
            if (task->orientTowardTargetStep(3))
                sleep(500);
        }
    }

    if (!st::ship.flags.cruise) {
        parent->notifyProgress("Entering supercruise failed");
        return false;
    }

    if (task->nl.focusDestDock() || task->nl.focusDestBody())
        task->nl.selectFocused();

    return true;
}

std::string EnterCruiseStep::getStatus() {
    switch (status) {
    case READY:
        return "Ready";
    case LOCK_BODY:
        return "Locking body";
    case LOCK_TARGET:
        return "Locking target";
    case ORIENT:
        return "Orienting";
    case MASSLOCKED:
        return std::format("Mass-locked: {}", timer.passed());
    case PREPARE:
        return std::format("Preparing");
    case FSD_COOLDOWN:
        return std::format("FSD Cooldown: {}", timer.passed());
    case ENTER_CRUISE:
        return std::format("Entering cruise: {}", timer.left());
    default:
        return "----";
    }
}

bool LeaveBodyStep::step() {
    if (st::ship.flags.cruise && !st::shipAtBody.approachBody && !st::shipAtBody.nearBody)
        return true;

    status = LOCK_BODY;
    if (!task->nl.focusNearestBody())
        task_return(Result::Trouble);
    if (!task->nl.selectFocused())
        task_return(Result::Trouble);
        status = ORIENT;
    kbd::send("UI_Back", 0, 500);
    task->orientAwayFromTarget(10);
    kbd::send("UI_Back");

    task->setSpeed(100);
    sleep(500);
    if (!st::ship.flags.cruise) {
        while (st::ship.flags.fsd_masslocked) {
            timer = utc_timer(60s);
            status = MASSLOCKED;
            //notifyProgress("Mass-locked, flying away");
            sleep(1000);
        }

        if (st::ship.flags.cargo_scoop_on || st::ship.flags.weapon_on) {
            status = PREPARE;
            if (st::ship.flags.cargo_scoop_on)
                kbd::send("ToggleCargoScoop");
            if (st::ship.flags.weapon_on)
                kbd::send("DeployHardpointToggle");
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
            parent->notifyProgress("Entering supercruise failed");
            return false;
        }
    }

    while (!st::ship.flags.cruise && (st::ship.flags.fsd_charging || st::ship.flags.fsd_jump) && !timer.expired()) {
        mgr.detectEDState(DetectLevel::Screen);
        if (mgr.uiState.guiFocus != GuiFocus::None) {
            kbd::send("UI_Back", 0, 1500);
            continue;
        }
        if (mgr.compassInfo.hemisphere > 0) {
            if (task->orientTowardTargetStep(3))
                sleep(500);
        }
    }

    if (!st::ship.flags.cruise) {
        parent->notifyProgress("Entering supercruise failed");
        return false;
    }

    timer = utc_timer(60s);
    while (st::shipAtBody.approachBody || st::shipAtBody.nearBody && !timer.expired()) {
        status = LEAVING_BODY;
        mgr.detectEDState(DetectLevel::Screen);
        if (mgr.uiState.guiFocus != GuiFocus::None) {
            kbd::send("UI_Back", 0, 1500);
            continue;
        }
        sleep(1000);
    }

    status = PREPARE;
    if (task->nl.focusDestDock() || task->nl.focusDestBody())
        task->nl.selectFocused();

    return true;
}

std::string LeaveBodyStep::getStatus() {
    switch (status) {
    case READY:
        return "Ready";
    case LOCK_BODY:
        return "Locking body";
    case ORIENT:
        return "Orienting";
    case MASSLOCKED:
        return std::format("Mass-locked: {}", timer.passed());
    case PREPARE:
        return std::format("Preparing");
    case FSD_COOLDOWN:
        return std::format("FSD Cooldown: {}", timer.passed());
    case ENTER_CRUISE:
        return std::format("Entering cruise: {}", timer.left());
    case LEAVING_BODY:
        return std::format("Leaving body: {}", timer.passed());
    default:
        return "----";
    }
}

bool DockStep::step() {
    if (st::ship.flags.cruise) {
        LOG(ERROR) << "Docking not possible in super-cruise mode";
        task_return(Result::Trouble);
    }
    if (st::ship.flags.docked) {
        LOG(INFO) << "Docking - already docked";
        return true;
    }
    mgr.detectEDState(DetectLevel::Screen);
    if (mgr.uiState.autopilot) {
        LOG(ERROR) << "Docking request while autopilot is active";
        return false;
    }

    task->setSpeed(0);

    status = PREPARE;
    // leave all UI panels
    if (st::guiFocus != GuiFocus::None) {
        for (int cnt = 0; cnt < 3; cnt++) {
            if (st::guiFocus == GuiFocus::None)
                break;
            kbd::send("UI_Back", 0, 1500);
            mgr.detectEDState(DetectLevel::Screen);
        }
        if (st::guiFocus != GuiFocus::None)
            task_return(Result::Trouble);
    }

    if (!task->nl.focusDestDock())
        task_return(Result::Trouble);
    else
        task->distanceToDock = task->nl.getFocusedEntry()->dist;
    if (!task->nl.selectFocused())
        task_return(Result::Trouble);
    if (!task->orientTowardTarget(5))
        task_return(Result::Trouble);

    // clear expired docking event
    auto de = mgr.cfg.dockingEvent;
    if (de) {
        if ((de->timestamp - std::chrono::utc_clock::now()) > 15min) {
            mgr.cfg.dockingEvent.reset();
            de.reset();
        }
    }
    // try to dock, retry if something goes wrong
    for (int cnt=0; cnt < 10; cnt++) {
        de = mgr.cfg.dockingEvent;
        // end loop if we granted to tock
        if (de && (de->event == "DockingGranted" || de->event == "Docked"))
            break;
        // if we are close enough (or don't know the distance) - request docking permit
        if (!task->distanceToDock.valid() || task->distanceToDock.get(dist_t::KM) >= 7.5) {
            flyTowardsTarget();
            continue;
        }
        de = requestDockingPermit();
        LOG(INFO) << "Docking status: " << (de ? de->event : "null");
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
        LOG(ERROR) << "Unknown docking event: " << de->data;
        task_return(Result::Failure);
    }
    if (st::ship.flags.docked || (de && de->event == "Docked")) {
        LOG(ERROR) << "Docking - already docked";
        return true;
    }
    if (!de || de->event != "DockingGranted") {
        LOG(ERROR) << "Docking not granted";
        return false;
    }

    status = AUTOPILOT;
    task->setSpeed(0); // set speed to 0 to start autopilot
    kbd::send("UI_Back", 0, 1500);

    // 8 minutes for docking
    timer = utc_timer(8min);
    // wait at least 5 seconds for autopilot to start docking
    for (int i=0; i < 40; i++) {
        sleep(250);
        mgr.detectEDState(DetectLevel::Screen);
        if (mgr.uiState.autopilot) {
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
        mgr.detectEDState(DetectLevel::Screen);
        if (st::ship.flags.docked || (mgr.cfg.dockingEvent && mgr.cfg.dockingEvent->event == "Docked")) {
            LOG(INFO) << "Docking complete, status docked: " << st::ship.flags.docked
                      << ", docking event: " << (mgr.cfg.dockingEvent ? mgr.cfg.dockingEvent->event : "null");
            break;
        }
        if (!mgr.cfg.dockingEvent || mgr.cfg.dockingEvent->event != "DockingGranted") {
            LOG(ERROR) << "Docking permission revoked, docking event: " << (mgr.cfg.dockingEvent ? mgr.cfg.dockingEvent->event : "null");
            return false;
        }
    }

    return true;
}

spGameEvent DockStep::requestDockingPermit() {
    status = REQUEST;
    for (int retry=0; retry < 3; retry++) {
        task->setSpeed(0);
        gotoNavPage(this, "mod-contacts");

        if (mgr.uiState.focused_name() != "btn-landing") {
            bool have_btn_landing = false;
            for (auto& cr : mgr.rEnv.classified) {
                if (cr.cdt == ClsDetType::Widget && cr.text == "btn-landing") {
                    have_btn_landing = true;
                    break;
                }
            }
            if (!have_btn_landing) {
                kbd::send("UI_Down");
                kbd::send("UI_Up", 1500);
            }
            kbd::send("UI_Right");
        }

        LOG(INFO) << "TaskDock requesting landing permission";
        mgr.cfg.dockingEvent.reset();
        // poll for docking event
        timer = utc_timer(5s);
        kbd::send("UI_Right");
        kbd::send("UI_Select");
        kbd::send("UI_Select");
        while (!timer.expired()) {
            auto de = mgr.cfg.dockingEvent;
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

bool DockStep::getDockDistance() {
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
                mgr.detectEDState(DetectLevel::Screen);
                dist_t d = mgr.compassInfo.nav_target_dist;
                if (d.valid() && d.get(dist_t::M) > 100)
                    dist[di++] = d;
            }
            if (!dist[di].valid() || dist[di].get(dist_t::M) < 1) {
                dist_t d = task->nl.getFocusedDist();
                kbd::send("UI_Back", 100, 1000);
                if (d.valid() && d.get(dist_t::M) > 100)
                    dist[di++] = d;
            }
        } else {
            if (!dist[di].valid() || dist[di].get(dist_t::M) < 1) {
                dist_t d = task->nl.getFocusedDist();
                kbd::send("UI_Back", 100, 1000);
                if (d.valid() && d.get(dist_t::M) > 100)
                    dist[di++] = d;
            }
            if (!dist[di].valid() || dist[di].get(dist_t::M) < 1) {
                mgr.detectEDState(DetectLevel::Screen);
                dist_t d = mgr.compassInfo.nav_target_dist;
                if (d.valid() && d.get(dist_t::M) > 100)
                    dist[di++] = d;
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
        task->distanceToDock = dist[0];
        return true;
    }
    return false;
}
bool DockStep::flyTowardsTarget() {
    status = APPROACH;
    task->setSpeed(0);
    for (int fails=0; fails < 10; fails++) {
        if (!getDockDistance())
            return false;
        if (task->distanceToDock.get(dist_t::KM) < 7.5) {
            task->setSpeed(0);
            return true;
        }

        flyTowardsStep();
        if (mgr.uiState.guiFocus == GuiFocus::None)
            task->orientTowardTargetStep(7, 1000);

        fails = 0;
    }
    task->setSpeed(0);
    return false;
}

bool DockStep::flyTowardsStep() {
    if (task->distanceToDock.get(dist_t::KM) < 7.5) {
        task->setSpeed(0);
        return true;
    }
    // distance to fly for auto-docking
    double dist = task->distanceToDock.get(dist_t::M) - 7300;
    auto ship = eddb::getShipStats();
    double fwdacc = ship->getForwardAccel();
    double revacc = ship->getReverseAccel();
    int max_speed_percent = 100;
    if (dist < (8500-7300))
        max_speed_percent = 50;
    else if (dist < (15000-7300))
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
        task->setSpeed(max_speed_percent);
        sleep(accel_time*1000);
        task->setSpeed(max_speed_percent);
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
    task->setSpeed(max_speed_percent);
    sleep(accel_time*1000);
    sleep(fly_time*1000);
    task->setSpeed(0);
    sleep(break_time*1000 + 500);
    return true;
}


std::string DockStep::getStatus() {
    switch (status) {
    default:
        return "----";
    case PREPARE:
        return "Prepare docking";
    case APPROACH:
        return std::format("Approach\n  dist {}", task->distanceToDock.to_string());
    case REQUEST:
        return "Requesting permit";
    case AUTOPILOT:
        return "Autopilot";
    }
}

bool NavDockSelect::step() {
    task->setSpeed(0);
    if (!dock)
        return false;

    for (int retry=0; retry < 3; retry++) {
        if (!task->nl.focusDestDock())
            continue;
        if (!task->nl.selectFocused())
            continue;
        return true;
    }
    return false;
}

bool NavBodySelect::step() {
    task->setSpeed(0);
    if (!body)
        return false;

    for (int retry=0; retry < 3; retry++) {
        if (!task->nl.focusDestBody())
            continue;
        if (!task->nl.selectFocused())
            continue;
        return true;
    }
    return false;
}

bool CruiseToDistStep::step() {
    // select destination dock or body
    if (!task->destDock || !task->destBody)
        return false;
    if (!(task->nl.isDestDockFocused || task->nl.isDestBodyFocused)) {
        if (task->destDock) {
            task->nl.focusDestDock();
        }
        else if (task->destBody) {
            task->nl.focusDestBody();
        }
    }
    if (!(task->nl.isDestDockFocused || task->nl.isDestBodyFocused))
        return false;
    // wait until we get to required distance
    int compassTry = 5;
    for (;;) {
        if (!st::ship.flags.cruise) {
            LOG(ERROR) << "Unexpected cruise exit";
            task->setSpeed(0);
            task_return(Result::Trouble);
        }
        if (st::shipAtBody.approachBody || st::shipAtBody.nearBody) {
            LOG(ERROR) << "Unexpected close to body: " << st::shipAtBody.bodyName;
            task->setSpeed(0);
            task_return(Result::Trouble);
        }
        if (compassTry <= 0) {
            compassTry += 1;
            dist_t focused_dist = task->nl.getFocusedDist();
            if (!focused_dist.valid()) {
                status = BAD_ROW;
                if (!currentDist_km.valid())
                    task->setSpeed(0);
                else if (task->speed_set_to > 25 && currentDist_km.get(dist_t::LS) < 10)
                    task->setSpeed(25);
                rollBlindCompass();
                continue;
            }
            currentDist_km = focused_dist.convertTo(dist_t::KM);
            kbd::send("UI_Back", 100, 1000);
            mgr.detectEDState(DetectLevel::Screen);
        } else {
            mgr.detectEDState(DetectLevel::Screen);
            if (mgr.uiState.guiFocus != GuiFocus::None) {
                kbd::send("UI_Back", 100, 1000);
                continue;
            }
            if (!mgr.compassInfo.nav_target_dist.valid()) {
                status = BAD_COMPASS;
                if (!currentDist_km.valid())
                    task->setSpeed(0);
                else if (task->speed_set_to > 25 && currentDist_km.get(dist_t::LS) < 10)
                    task->setSpeed(25);
                if (mgr.compassInfo.has_nav_target) {
                    rollBlindCompass();
                } else {
                    if (mgr.compassInfo.hemisphere < 0 || std::abs(mgr.compassInfo.targetYaw) > 7 || std::abs(mgr.compassInfo.targetPitch) > 7) {
                        task->setSpeed(0);
                        task->orientTowardTargetStep(7);
                    }
                }
                compassTry -= 1;
                if (compassTry <= 0)
                    compassTry = -2;
                continue;
            }
            currentDist_km = mgr.compassInfo.nav_target_dist.convertTo(dist_t::KM);
        }
        if (currentDist_km.dist <= requiredDist_km.dist) {
            status = DIST_STOP;
            task->setSpeed(0);
            return true;
        }
        else if (currentDist_km.dist <= requiredDist_km.dist * 1.5) {
            status = DIST_NEAR;
            task->setSpeed(25);
        } else {
            status = DIST_FAR;
            task->setSpeed(75);
        }

        compassTry = 5;

        if (!mgr.compassInfo.hemisphere) {
            rollBlindCompass();
            continue;
        }

        if (mgr.uiState.guiFocus == GuiFocus::None) {
            task->orientTowardTargetStep(0.5);
        }
    }

    return true;
}

std::string CruiseToDistStep::getStatus() {
    dist_t curr_mm = currentDist_km.convertTo(dist_t::MM);
    dist_t curr_ls = currentDist_km.convertTo(dist_t::LS);
    dist_t curr = (curr_ls.dist >= 0.1) ? curr_ls : curr_mm;

    dist_t reqr_mm = requiredDist_km.convertTo(dist_t::MM);
    dist_t reqr_ls = requiredDist_km.convertTo(dist_t::LS);
    dist_t reqr = (reqr_ls.dist >= 0.1) ? reqr_ls : curr_mm;

    const char* st = "----";
    switch (status) {
    case READY: st="----"; break;
    case BAD_ROW: st="Bad row"; break;
    case BAD_COMPASS: st="Bad compass"; break;
    case DIST_FAR: st="Dist far"; break;
    case DIST_NEAR: st="Dist near"; break;
    case DIST_STOP: st="Reached"; break;
    }
    return std::format("{}: {} / {}", st, curr.to_string(), reqr.to_string());
}

bool DiveUnderPlanetStep::step() {
    bool ok = true;

    task->setSpeed(0);
    if (!task->destBody || !task->destDock)
        return false;
    gal::spStarSystem ss = gal::getCurrentStarSystem();
    if (!ss) {
        LOG(ERROR) << "Current system not known";
        return false;
    }
    task->distanceToDock = {};
    task->distanceToBody = {};


    for (;;) {
        double angleDockToBody;
        if (task->nl.isDestBodyFocused) {
            status = ORIENT_BODY;
            if (st::destination.name != task->destBody->name) {
                if (!task->nl.selectFocused())
                    return false;
            }
            if (!task->orientTowardTarget(2))
                return false;
            if (!get_dist_body())
                return false;
            if (!task->nl.focusDestDock())
                return false;
            if (!task->nl.selectFocused())
                return false;
            kbd::send("UI_Back", 0, 1000);
            if (!orient_roll(180))
                return false;
            angleDockToBody = mgr.compassInfo.targetAngle;
            if (angleDockToBody >= 90)
                break;
            if (!get_dist_dock())
                return false;
        } else {
            status = ORIENT_DOCK;
            if (!task->nl.focusDestDock())
                return false;
            if (st::destination.name != task->destDock->name) {
                if (!task->nl.selectFocused())
                    return false;
            }
            if (!task->orientTowardTarget(2))
                return false;
            if (!get_dist_dock())
                return false;
            if (!task->nl.focusDestBody())
                return false;
            if (!task->nl.selectFocused())
                return false;
            kbd::send("UI_Back", 0, 1000);
            if (!orient_roll(0))
                return false;
            angleDockToBody = mgr.compassInfo.targetAngle;
            if (angleDockToBody >= 90)
                break;
            if (!get_dist_body())
                return false;
        }
        // having distance to dock and body and angle between, calc nearest distance
        // between ship-dock line and body center, compare it with body radius
        double dist_dock = task->distanceToDock.get(dist_t::KM);
        double dist_body = task->distanceToBody.get(dist_t::KM);
        double dist_min = dist_body * std::cos(angleDockToBody*M_PI/180);
        if (dist_min > dist_dock)
            break;
        double dist_surf = dist_body * std::sin(angleDockToBody*M_PI/180) - task->destBody->radius;
        if (dist_surf > task->destBody->radius * 0.2)
            break;
        // if we are close to body - dive targeting dock, otherwise it does not matter
        if (dist_body < 5*task->destBody->radius) {
            if (st::destination.name != task->destDock->name) {
                if (!task->nl.focusDestDock())
                    return false;
                if (!task->nl.selectFocused())
                    return false;
            }
        }

        // compass dot 50 degree above center
        if (!orient_pitch(50))
            return false;

        // fly till compass dot 90 degree above center
        if (!fly_dive(90))
            return false;
    }

    status = ORIENT_DOCK;
    if (st::destination.name != task->destDock->name) {
        if (!task->nl.focusDestDock())
            return false;
        if (!task->nl.selectFocused())
            return false;
    }
    task->orientTowardTarget(5);

    return true;
}

bool DiveUnderPlanetStep::get_dist_body() {
    status = DIST_BODY;
    task->distanceToBody = {};
    for (int i=0; i < 5 && !task->distanceToBody.valid(); i++) {
        if (!task->nl.focusDestBody())
            continue;
        nav::NavListEntry nle;
        if (task->nl.recognizeFocusedNavRow(nle)) {
            if (nle.dist.valid()) {
                task->distanceToBody = nle.dist;
                break;
            }
        }
        if (mgr.uiState.guiFocus != GuiFocus::None)
            kbd::send("UI_Back", 0, 1500);
        mgr.detectEDState(DetectLevel::Screen);
        if (!mgr.compassInfo.has_nav_target || mgr.compassInfo.nav_target_dist.valid())
            task->orientTowardTarget(5);
        if (mgr.compassInfo.has_nav_target && mgr.compassInfo.nav_target_dist.valid()) {
            task->distanceToBody = mgr.compassInfo.nav_target_dist;
            break;
        }
    }
    return task->distanceToBody.valid();
}

bool DiveUnderPlanetStep::get_dist_dock() {
    status = DIST_DOCK;
    task->distanceToDock = {};
    for (int i=0; i < 5 && !task->distanceToDock.valid(); i++) {
        status = DIST_DOCK;
        if (!task->nl.focusDestDock())
            continue;
        nav::NavListEntry nle;
        if (task->nl.recognizeFocusedNavRow(nle)) {
            if (nle.dist.valid()) {
                task->distanceToDock = nle.dist;
                break;
            }
        }
        if (mgr.uiState.guiFocus != GuiFocus::None)
            kbd::send("UI_Back", 0, 1500);
        mgr.detectEDState(DetectLevel::Screen);
        if (!mgr.compassInfo.has_nav_target || mgr.compassInfo.nav_target_dist.valid())
            task->orientTowardTarget(5);
        if (mgr.compassInfo.has_nav_target && mgr.compassInfo.nav_target_dist.valid()) {
            task->distanceToDock = mgr.compassInfo.nav_target_dist;
            break;
        }
    }
    return task->distanceToDock.valid();
}

bool DiveUnderPlanetStep::orient_roll(float requiredRoll) {
    if (mgr.uiState.guiFocus != GuiFocus::None) {
        notifyProgress("Orientation: goto compass");
        kbd::send("UI_Back", 0, 1500);
    }
    const int rollPrecision = 5;

    for (int fails=0; fails < 10; fails++) {
        mgr.detectEDState(DetectLevel::Screen);
        if (mgr.uiState.guiFocus != GuiFocus::None) {
            notifyProgress(std::format("Unexpected ui mode {}", mgr.uiState.to_string()));
            LOG(WARNING) << "Unexpected ui mode " << mgr.uiState;
            kbd::send("UI_Back", 0, 1500);
            continue;
        }
        if (!mgr.compassInfo.hemisphere) {
            notifyProgress(std::format("Compass not detected, fails {}", fails));
            LOG(WARNING) << "Compass not detected";
            continue;
        }
        fails = 0;

        float delta = mgr.compassInfo.targetRoll - requiredRoll;
        if (delta > 180) delta = 360-delta;
        if (delta < -180) delta = 360+delta;
        if (delta < rollPrecision)
            return true;
        task->orientRollStep(requiredRoll, 10000);
    }
    return false;
}

bool DiveUnderPlanetStep::orient_pitch(int pitchGoal) {
    status = ORIENT_DIVE;
    task->setSpeed(0);
    if (mgr.uiState.guiFocus != GuiFocus::None) {
        notifyProgress("Orientation: goto compass");
        kbd::send("UI_Back", 0, 1500);
    }
    const int rollPrecision = 5;
    const int pitchPrecision = 5;

    for (int fails=0; fails < 10; fails++) {
        mgr.detectEDState(DetectLevel::Screen);
        if (mgr.uiState.guiFocus != GuiFocus::None) {
            notifyProgress(std::format("Unexpected ui mode {}", mgr.uiState.to_string()));
            LOG(WARNING) << "Unexpected ui mode " << mgr.uiState;
            kbd::send("UI_Back", 0, 1500);
            continue;
        }
        if (!mgr.compassInfo.hemisphere) {
            notifyProgress(std::format("Compass not detected, fails {}", fails));
            LOG(WARNING) << "Compass not detected";
            continue;
        }
        fails = 0;

        if (std::abs(mgr.compassInfo.targetPitch - pitchGoal) < pitchPrecision)
            break;
        task->orientPitchStep(pitchGoal);
    }
    if (std::abs(mgr.compassInfo.targetPitch - pitchGoal) > rollPrecision)
        return false;

    return true;
}

bool DiveUnderPlanetStep::fly_dive(int pitchGoal) {
    status = FLY_DIVE;

    if (mgr.uiState.guiFocus != GuiFocus::None) {
        notifyProgress("Orientation: goto compass");
        kbd::send("UI_Back", 0, 1500);
    }
    task->setSpeed(75);
    for (;;) {
        if (!st::ship.flags.cruise) {
            LOG(ERROR) << "Unexpected cruise exit";
            task->setSpeed(0);
            task_return(Result::Trouble);
        }
        if (st::shipAtBody.approachBody || st::shipAtBody.nearBody) {
            LOG(ERROR) << "Unexpected close to body: " << st::shipAtBody.bodyName;
            task->setSpeed(0);
            task_return(Result::Trouble);
        }
        mgr.detectEDState(DetectLevel::Screen);
        if (mgr.uiState.guiFocus != GuiFocus::None) {
            kbd::send("UI_Back", 100, 1000);
            continue;
        }
        if (mgr.compassInfo.hemisphere && mgr.compassInfo.targetPitch >= pitchGoal) {
            task->setSpeed(0);
            return true;
        }
        task->sleep(1000);
    }
}

std::string DiveUnderPlanetStep::getStatus() {
    switch (status) {
    case READY:
        return "----";
    case ORIENT_BODY:
        return "Orient to body";
    case DIST_BODY:
        return "Get distance to body";
    case ORIENT_DOCK:
        return "Orient to dock";
    case DIST_DOCK:
        return "Get distance to dock";
    case ORIENT_DIVE:
        return "Orient to dive";
    case FLY_DIVE:
        return "Dive fly";
    }
    return "----";
}

bool ExitCruiseToStationStep::step() {
    if (!st::ship.flags.cruise) {
        LOG(ERROR) << "Unexpected cruise exit";
        task_return(Result::Trouble);
    }
    status = ORIENT;
    double dist_km = 15000;
    if (!task->nl.focusDestDock())
        return false;
    if (!task->nl.selectFocused())
        return false;
    if (!task->orientTowardTarget(5))
        return false;
    status = APPROACH;
    // wait until we get to 1mm
    for (;;) {
        if (!st::ship.flags.cruise) {
            LOG(ERROR) << "Unexpected cruise exit";
            task_return(Result::Trouble);
        }
        if (use_nav_panel) {
            if (dist_fails > 3) {
                use_nav_panel = false;
                kbd::send("UI_Back", 0, 1000);
                continue;
            }
            dist_t focused_dist = task->nl.getFocusedDist();
            if (focused_dist.valid())
                task->distanceToDock = focused_dist.convertTo(dist_t::KM);
        } else {
            mgr.detectEDState(DetectLevel::Screen);
            if (mgr.uiState.guiFocus != GuiFocus::None) {
                kbd::send("UI_Back", 0, 1000);
                continue;
            }
            if (mgr.compassInfo.nav_target_dist.valid())
                task->distanceToDock = mgr.compassInfo.nav_target_dist.convertTo(dist_t::KM);
        }

        if (!task->distanceToDock.valid()) {
            dist_fails += 1;
            if (dist_km < 5000 && (task->speed_set_to > 0 || dist_fails >= 5))
                task->setSpeed(0);
            if (!use_nav_panel) {
                if (dist_fails >= 15) {
                    use_nav_panel = true;
                    dist_fails = 0;
                }
                else if ((dist_fails % 5) == 4)
                    rollBlindCompass();
                continue;
            } else {
                use_nav_panel = false;
                dist_fails = 0;
                kbd::send("UI_Back", 0, 1000);
            }
            continue;
        }
        dist_km = task->distanceToDock.get(dist_t::KM);
        if (dist_km < 1000)
            exit_confirm += 1;
        else
            exit_confirm = 0;
        if (exit_confirm >= 2)
            break;
        if (dist_km < 3000)
            task->setSpeed(25);
        else if (dist_km < 5000)
            task->setSpeed(50);
        else
            task->setSpeed(75);
        if (!exit_confirm && !use_nav_panel && mgr.uiState.guiFocus == GuiFocus::None)
            task->orientTowardTargetStep(1, 1000);
    }

    // wait until we exit super-cruise
    timer = utc_timer(10s);
    status = EXITING;
    task->setSpeed(25);
    while (st::ship.flags.cruise && !timer.expired()) {
        kbd::send("HyperSuperCombination", 100, 1000);
        sleep(1000);
        if (mgr.uiState.guiFocus != GuiFocus::None)
            kbd::send("UI_Back", 0, 1000);
        task->orientTowardTargetStep(10, 1000);
    }

    notifyProgress("Arrived, speed zero");
    task->setSpeed(0);
    sleep(500);

    for (dist_fails=0; dist_fails < 15; dist_fails++) {
        mgr.detectEDState(DetectLevel::Screen);
        if (mgr.uiState.guiFocus != GuiFocus::None) {
            kbd::send("UI_Back", 0, 1000);
            mgr.detectEDState(DetectLevel::Screen);
        }
        if (mgr.compassInfo.nav_target_dist.valid()) {
            if (mgr.compassInfo.nav_target_dist.get(dist_t::KM) > 25) {
                LOG(ERROR) << "Unexpected distance after cruise exit: " << mgr.compassInfo.nav_target_dist;
                task_return(Result::Trouble);
            }
            return true;
        }
        if ((dist_fails % 3) == 2)
            rollBlindCompass();
    }

    LOG(WARNING) << "Cannot confirm distance after cruise exit";
    return true;
}

std::string ExitCruiseToStationStep::getStatus() {
    if (status == ORIENT) {
        return "Orienting towards target";
    }
    if (status == APPROACH) {
        if (dist_fails)
            return std::format("Approaching:\ndist {} (fails {})\nspeeed {}%", task->distanceToDock.to_string(), dist_fails, task->speed_set_to);
        else if (exit_confirm)
            return std::format("Approaching:\ndist {} (confirm {})\nspeeed {}%", task->distanceToDock.to_string(), exit_confirm, task->speed_set_to);
        else
            return std::format("Approaching:\ndist {}\nspeeed {}%", task->distanceToDock.to_string(), task->speed_set_to);
    }
    if (status == EXITING) {
        return std::format("Exiting cruise {}", timer.left());
    }
    if (status == CONFIRM) {
        return std::format("Checking distance\nfails {}", dist_fails);
    }
    return "----";
}


TaskTravel::TaskTravel(Task *parent, AIManager &mgr, const TaskTemplate &templ)
    : BaseAutopilotTask(parent, mgr, templ)
{
    for (auto& p : templ.params) {
        if (p.name == "system")
            destSystem = std::get<std::string>(p.value);
        else if (p.name == "dock")
            destDock = std::get<std::string>(p.value);
    }
}

void TaskTravel::plan() {
//    TaskTemplate taskDepart = mgr.getTaskTemplate(ED_TASK_DEPART);
//    sub_tasks.push_back(std::make_unique<TaskDepart>(this, mgr, taskDepart));
//
//    TaskTemplate taskJumpToSystem = mgr.getTaskTemplate(ED_TASK_JUMP_TO_SYSTEM);
//    taskJumpToSystem.set("system", destSystem);
//    sub_tasks.push_back(std::make_unique<TaskJumpToSystem>(this, mgr, taskJumpToSystem));
//
//    TaskTemplate taskCruiseToDock = mgr.getTaskTemplate(ED_TASK_CRUISE_TO_STATION);
//    taskCruiseToDock.set("dock", destDock);
//    sub_tasks.push_back(std::make_unique<TaskCruiseToDock>(this, mgr, taskCruiseToDock));
//
//    TaskTemplate taskDock = mgr.getTaskTemplate(ED_TASK_DOCK);
//    sub_tasks.push_back(std::make_unique<TaskDock>(this, mgr, taskDock));
}

Result TaskTravel::run() {
    return Result::Failure;
//    switch (result) {
//    case Result::Created:
//    case Result::Started:
//    case Result::Trouble:
//        plan();
//        break;
//    case Result::Failure:
//    case Result::Partly:
//    case Result::Success:
//        LOG(ERROR) << "Bad state on task run(): " << enum_name<Result>(result);
//        return result;
//    }
//
//    while (!sub_tasks.empty()) {
//        spTask& pTask = sub_tasks.front();
//        Result res = run_sub_task(pTask);
//        switch (res) {
//        case Result::Created:
//        case Result::Started:
//            LOG(ERROR) << "Bad state after task run(): " << enum_name<Result>(res);
//            plan();
//            continue;
//        case Result::Trouble:
//            if (pTask->missCount < pTask->maxMisses) {
//                plan();
//                pTask->result = Result::Started;
//                continue;
//            }
//            pTask->result = Result::Failure;
//            // fall through
//        case Result::Failure:
//        case Result::Partly:
//        case Result::Success:
//            sub_tasks.pop_front();
//            break;
//        }
//    }
//    notifyProgress(_("End of travel"));
//    result = Result::Success;
//    return result;
}

Autopilot::Autopilot(Task *parent, AIManager &mgr, const TaskTemplate &templ)
        : BaseAutopilotTask(parent, mgr, templ)
{
}

Result Autopilot::run() {
    auto starSystem = gal::getCurrentStarSystem();
    destDock = starSystem->getDock(st::destination.name);
    if (!destDock) {
        if (st::destination.name.empty())
            return Result::Failure;
        return Result::Failure;
    }
    int bodyId = st::destination.bodyId;
    if (destDock->typeNav == gal::TypeNav::SpacePort) {
        if (!destDock->bodyId.has_value()) {
            destDock->bodyId = bodyId;
            starSystem->saved = false;
            gal::saveStarSystem(starSystem.get());
        }
        bodyId = destDock->parentBodyId;
    }
    {
        auto body = starSystem->getBodyById(bodyId);
        if (!body)
            return Result::Failure;
        destBody = std::dynamic_pointer_cast<gal::Body>(body);
    }
    if (!destBody)
        return Result::Failure;

    st::NavPanelFilters filters = {};
    filters.star = true;
    filters.planetOrMoon = true;
    filters.landablePlanetOrMoon = true;
    filters.station = true;
    if (destDock) {
        if (destDock->typeNav == gal::TypeNav::SpacePort)
            filters.station = true;
        if (destDock->typeNav == gal::TypeNav::Carrier)
            filters.fleetCarrier = true;
        if (destDock->typeNav == gal::TypeNav::MegashipDock) {
            if (destDock->typeSite == gal::TypeSite::TrailblazerDream)
                filters.pointOfInterest = true;
            else
                filters.station = true;
        }
    }

    nl.init(this, destDock, destBody, filters);

    if (st::ship.flags.docked) {
        if (!st::dockedAt.stationName.empty() && destDock->nameEq(st::dockedAt.stationName))
            return Result::Success;
        if (!run_sub_step(spStep(new DepartureStep(this))))
            return Result::Trouble;
    }

    bool at_dock = false;
    if (!st::ship.flags.cruise) {
        if (!st::space.stationName.empty() && destDock->nameEq(st::space.stationName))
            at_dock = true;
        else if (!st::space.bodyName.empty() && destDock->nameEq(st::space.bodyName))
            at_dock = true;
    }
    if (!at_dock && !st::ship.flags.cruise) {
        if (!run_sub_step(spStep(new EnterCruiseStep(this))))
            return Result::Trouble;
    }
    else if (st::shipAtBody.approachBody || st::shipAtBody.nearBody) {
        LOG(ERROR) << "Unexpected close to body: " << st::shipAtBody.bodyName;
        setSpeed(0);
        if (!run_sub_step(spStep(new LeaveBodyStep(this))))
            return Result::Trouble;
    }


    if (!at_dock) {
        // a few degrees visible angle to stop and avoid planet
        double distToCorrect = destBody->radius * 70;
        if (distToCorrect < 15000)
            distToCorrect = 15000;
        if (!run_sub_step(spStep(new CruiseToDistStep(this, distToCorrect))))
            return Result::Trouble;

        if (destBody && destDock) {
            for (int i = 0; i < 5; i++) {
                setSpeed(0);
                check_interrupted();
                if (!run_sub_step(spStep(new DiveUnderPlanetStep(this))))
                    return Result::Trouble;
                break;
            }
        }

        if (!run_sub_step(spStep(new ExitCruiseToStationStep(this))))
            return Result::Trouble;
    }

    if (!run_sub_step(spStep(new DockStep(this))))
        return Result::Trouble;

    return Result::Success;
}

} // ai
