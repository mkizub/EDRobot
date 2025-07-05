//
// Created by mkizub on 28.06.2025.
//

#include "../pch.h"

#include "Task.h"
#include "AIManager.h"
#include "AutopilotTasks.h"

using namespace std::chrono_literals;

namespace ai {

BaseAutopilotTask::BaseAutopilotTask(Task* parent, AIManager& mgr, const TaskTemplate& templ)
    : Task(parent, mgr, templ)
{}

void BaseAutopilotTask::relogin() {
    // something is really wrong, logout and login again
    notifyProgress("Something is wrong with departure, trying to re-login");
    sendKey("Pause", 0, 1000);
    sendKey("UI_Up", 0, 100); // go to Exit button
    sendKey("UI_Select", 0, 1000); // logout
    sendKey("UI_Select", 0, 8000); // logout to main menu
    notifyProgress("Login to Solo...");
    sendKey("UI_Select", 0, 3000); // login, select mode screen
    sendKey("UI_Right", 0, 100);
    sendKey("UI_Right", 0, 500);  // choose Solo
    sendKey("UI_Select", 0, 12000); // login
    notifyError("Finished re-login", Result::Trouble);
}

TaskDepart::TaskDepart(ai::Task *parent, ai::AIManager &mgr, const ai::TaskTemplate &templ)
    : BaseAutopilotTask(parent, mgr, templ)
{
}

Result TaskDepart::run() {
    auto& ss = mgr.cfg.getCurrentStatus();
    if (ss->flags.docked) {
        if (mgr.cfg.getGuiFocus() != GuiFocus::None) {
            for (int i = 0; i < 10 && mgr.cfg.getGuiFocus() != GuiFocus::None; i++) {
                sendKey("UI_Back", 0, 1000);
                mgr.detectEDState(DetectLevel::Screen);
            }
        }
        mgr.detectEDState(DetectLevel::Screen);
        if (mgr.cfg.getGuiFocus() != GuiFocus::None)
            return Result::Trouble;

        for (int i = 0; i < 4; i++)
            sendKey("UI_Up");
        sendKey("UI_Select", 0, 500); // refuel
        sendKey("UI_Right");
        sendKey("UI_Select", 0, 500); // repair
        sendKey("UI_Right");
        sendKey("UI_Select", 0, 500); // rearm
        sendKey("UI_Down");
        sendKey("UI_Down");
        sendKey("UI_Select");
        // 20 seconds to leave landing pad
        for (int i = 0; i < 20 && ss->flags.docked; i++) {
            sleep(1000);
            mgr.detectEDState(DetectLevel::Screen);
        }
        if (ss->flags.docked)
            return Result::Trouble;
    }
    // 4 minutes for departure
    auto autopilot_start = std::chrono::utc_clock::now();
    auto autopilot_limit = autopilot_start + 4min;
    // wait at least 15 seconds for autopilot to departure
    for (int i=0; i < 60; i++) {
        sleep(250);
        mgr.detectEDState(DetectLevel::Screen);
        if (mgr.uiState.autopilot)
            LOG(INFO) << "Departure autopilot waiting...";
    }
    int logCounter = 0;
    int notAutoPilotCounter = 0;
    for (;;) {
        auto now = std::chrono::utc_clock::now();
        if (now > autopilot_limit) {
            LOG(ERROR) << "Autopilot time expired";
            relogin();
        }
        logCounter = (logCounter + 1) % (10*4);
        LOG_IF(!logCounter,INFO) << "Departure autopilot, passed "
                                 << std::chrono::duration_cast<std::chrono::seconds>(now - autopilot_start).count() << " seconds, expire in "
                                 << std::chrono::duration_cast<std::chrono::seconds>(autopilot_limit - now).count() << " seconds";
        sleep(250);
        mgr.detectEDState(DetectLevel::Screen);
        if (mgr.uiState.autopilot) {
            notAutoPilotCounter = 0;
            continue;
        }
        if (++notAutoPilotCounter > 4) {
            LOG(INFO) << "Departure complete (autopilot off)";
            break;
        }
    }
    return Result::Success;
}

TaskDock::TaskDock(ai::Task *parent, ai::AIManager &mgr, const ai::TaskTemplate &templ)
        : BaseAutopilotTask(parent, mgr, templ)
{
}

Result TaskDock::run() {
    auto& ss = mgr.cfg.getCurrentStatus();
    if (ss->flags.cruise) {
        LOG(ERROR) << "Docking not possible in super-cruise mode: " << *ss;
        return Result::Failure;
    }
    if (ss->flags.docked) {
        LOG(ERROR) << "Docking - already docked:" << *ss;
        return Result::Success;
    }
    mgr.detectEDState(DetectLevel::Screen);
    if (mgr.uiState.autopilot) {
        LOG(ERROR) << "Docking request while autopilot is active";
        sleep(1000);
        return Result::Trouble;
    }
    //if (ss->bodyName.empty()) // not at any body at all
    //    return Result::Trouble;

    sendKey("SetSpeedZero");

    for (int cnt=0; cnt < 3; cnt++) {
        mgr.detectEDState(DetectLevel::Screen);
        if (mgr.cfg.getGuiFocus() == GuiFocus::None)
            break;
        sendKey("UI_Back", 0, 1500);
    }
    if (mgr.cfg.getGuiFocus() != GuiFocus::None)
        return Result::Trouble;

    if (mgr.compassInfo.hemisphere < 0) {
        if (!selectDockingFilters())
            return Result::Trouble;
        mgr.detectEDState(DetectLevel::Buttons);
        if (!lockDockingStation())
            return Result::Trouble;
        mgr.detectEDState(DetectLevel::Buttons);
    }

    auto de = mgr.cfg.dockingEvent;
    if (de) {
        if ((de->timestamp - std::chrono::utc_clock::now()) > 15min) {
            mgr.cfg.dockingEvent.reset();
            de.reset();
        }
    }
    for (int cnt=0; cnt < 10; cnt++) {
        de = mgr.cfg.dockingEvent;
        if (de && (de->event == "DockingGranted" || de->event == "Docked"))
            break;
        de = requestDockingPermit();
        LOG(INFO) << "Docking status: " << (de ? de->event : "null");
        if (de && (de->event == "DockingGranted" || de->event == "Docked"))
            break;
        if (!de || de->event == "DockingRequested") {
            // need to wait a bit
            sleep(1000);
            return Result::Trouble;
        }
        if (de->event == "DockingCancelled") {
            // oops, we canceled docking, try again
            sleep(2000);
            continue;
        }
        if (de->event == "DockingTimeout") {
            // have not completed docking in time, try docking again
            return Result::Trouble;
        }
        // NoSpace, TooLarge, Hostile, Offences, Distance, ActiveFighter, NoReason, etc.
        if (de->event == "DockingDenied") {
            auto reason = de->data["Reason"].as_string();
            if (reason == "NoSpace") {
                sleep(5000);
                cnt = 0;
                continue;
            }
            if (reason == "Distance") {
                // need to get close
                flyTowardsTarget();
                continue;
            }
        }
        // all others are fatal
        LOG(ERROR) << "Unknown docking event: " << de->data;
        return Result::Failure;
    }
    if (ss->flags.docked || (de && de->event == "Docked")) {
        LOG(ERROR) << "Docking - already docked:" << *ss;
        return Result::Success;
    }
    if (!de || de->event != "DockingGranted") {
        LOG(ERROR) << "Docking not granted";
        return Result::Trouble;
    }

    sendKey("SetSpeedZero", 50, 100); // set speed to 0 to start autopilot
    sendKey("UI_Back", 0, 1500);

    // 8 minutes for docking
    auto autopilot_start = std::chrono::utc_clock::now();
    auto autopilot_limit = autopilot_start + 8min;
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
    int logCounter = 0;
    int notAutoPilotCounter = 0;
    for (;;) {
        auto now = std::chrono::utc_clock::now();
        if (now > autopilot_limit) {
            LOG(ERROR) << "Autopilot time expired";
            relogin();
        }
        logCounter = (logCounter + 1) % (10*4);
        LOG_IF(!logCounter,INFO) << "Docking autopilot, passed "
                                 << std::chrono::duration_cast<std::chrono::seconds>(now - autopilot_start).count() << " seconds, expire in "
                                 << std::chrono::duration_cast<std::chrono::seconds>(autopilot_limit - now).count() << " seconds";

        sleep(250);
        mgr.detectEDState(DetectLevel::Screen);
        if (ss->flags.docked || (mgr.cfg.dockingEvent && mgr.cfg.dockingEvent->event == "Docked")) {
            LOG(INFO) << "Docking complete, status docked: " << ss->flags.docked
                      << ", docking event: " << (mgr.cfg.dockingEvent ? mgr.cfg.dockingEvent->event : "null");
            break;
        }
        if (!mgr.cfg.dockingEvent || mgr.cfg.dockingEvent->event != "DockingGranted") {
            LOG(ERROR) << "Docking permission revoked, docking event: " << (mgr.cfg.dockingEvent ? mgr.cfg.dockingEvent->event : "null");
            return Result::Trouble;
        }
    }

    return Result::Success;
}

spGameEvent TaskDock::requestDockingPermit() {
    sendKey("SetSpeedZero");
    if (!mgr.uiState.match("scr-left-panel:*")) {
        LOG(DEBUG) << "TaskDock FocusLeftPanel...";
        sendKey("FocusLeftPanel", 0, 1500);
        mgr.detectEDState(DetectLevel::Buttons);
        if (!mgr.uiState.match("scr-left-panel:*")) {
            LOG(ERROR) << "TaskDock expecting 'scr-left-panel:*' but got " << mgr.uiState;
            task_return(Result::Trouble);
        }
    }
    mgr.detectEDState(DetectLevel::Buttons);
    for (int i=0; i < 5 && !mgr.uiState.match("scr-left-panel:mod-contacts"); i++) {
        LOG(DEBUG) << "TaskDock goto mod-contacts...";
        if (mgr.uiState.match("scr-left-panel:mod-sysinfo")) {
            LOG(DEBUG) << "TaskDock mod-sysinfo -> mod-contacts";
            sendKey("CycleNextPanel", 0, 250);
            sendKey("CycleNextPanel", 0, 250);
            sendKey("CycleNextPanel", 0, 250);
        }
        else if (mgr.uiState.match("scr-left-panel:mod-navigation")) {
            LOG(DEBUG) << "TaskDock mod-navigation -> mod-contacts";
            sendKey("CycleNextPanel", 0, 250);
            sendKey("CycleNextPanel", 0, 250);
        }
        else if (mgr.uiState.match("scr-left-panel:mod-transact")) {
            LOG(DEBUG) << "TaskDock mod-transact -> mod-contacts";
            sendKey("CycleNextPanel", 0, 250);
        }
        else if (mgr.uiState.match("scr-left-panel:mod-target")) {
            LOG(DEBUG) << "TaskDock mod-target -> mod-contacts";
            sendKey("CyclePreviousPanel", 0, 250);
        }
        else {
            LOG(DEBUG) << "TaskDock {mod/dlg}-any -> UI_Back";
            sendKey("UI_Back", 0, 1500);
        }
        mgr.detectEDState(DetectLevel::Buttons);
    }

    if (!mgr.uiState.match("scr-left-panel:mod-contacts")) {
        LOG(ERROR) << "TaskDock expecting 'scr-left-panel:mod-contacts' but got " << mgr.uiState;
        task_return(Result::Trouble);
    }
    //mgr.detectEDState(DetectLevel::ListRows);

    // TODO: detect focused button
//    // there is a bug in ED, sometimes contacts have no focus
//    // also, it's possible we have problems with focused button detection
//    for (int i=0; i < 3 && !mgr.uiState.focused; i++) {
//        LOG(DEBUG) << "TaskDock no focused button...";
//        sendKey("UI_Left", 0, 250);
//        sendKey("CyclePreviousPanel", 0, 250);
//        sendKey("UI_Down", 0, 250);
//        sendKey("CycleNextPanel", 0, 500);
//        mgr.detectEDState(DetectLevel::ListRows);
//        if (!mgr.uiState.match("scr-left-panel:mod-contacts")) {
//            LOG(ERROR) << "TaskDock expecting 'scr-left-panel:mod-contacts' but got " << mgr.uiState;
//            task_return(Result::Trouble);
//        }
//    }
//    if (!mgr.uiState.focused) {
//        LOG(ERROR) << "TaskDock no focused widget!";
//        task_return(Result::Trouble);
//    }
//
//    for (int i=0; i < 3 && mgr.uiState.focused_name() != "btn-landing"; i++) {
//        if (mgr.uiState.focused_name() == "lst-contacts") {
//            LOG(DEBUG) << "TaskDock getting from 'lst-contacts' to 'btn-landing'";
//            sendKey("UI_Down", 0, 250);
//            sendKey("UI_Up", 1000, 0);
//            sendKey("UI_Right", 0, 500);
//            mgr.detectEDState(DetectLevel::ListRows);
//            if (!mgr.uiState.focused) {
//                LOG(ERROR) << "TaskDock no focused widget!";
//                task_return(Result::Trouble);
//            }
//        }
//    }
//    if (mgr.uiState.focused_name() != "btn-landing") {
//        LOG(ERROR) << "TaskDock 'btn-landing' is not focused!";
//        task_return(Result::Trouble);
//    }

    if (mgr.uiState.focused_name() != "btn-landing") {
        bool have_btn_landing = false;
        for (auto& cr : mgr.rEnv.classified) {
            if (cr.cdt == ClsDetType::Widget && cr.text == "btn-landing") {
                have_btn_landing = true;
                break;
            }
        }
        if (!have_btn_landing) {
            sendKey("UI_Down", 0, 250);
            sendKey("UI_Up", 1500, 0);
        }
        sendKey("UI_Right", 0, 500);
    }

    LOG(INFO) << "TaskDock requesting landing permission";
    mgr.cfg.dockingEvent.reset();
    // poll for docking event
    auto until = std::chrono::utc_clock::now() + 5000ms;
    sendKey("UI_Select", 0, 1000);
    sendKey("UI_Back");
    while (until > std::chrono::utc_clock::now()) {
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
    return {};
}

bool TaskDock::selectDockingFilters() {
    sendKey("SetSpeedZero");
    // we may dock at: settlement, station, fleetCarrier, pointOfInterest (trailblazer dream)
    LocationPanelFilters requiredFilters;
    requiredFilters.bits.settlement = true;
    requiredFilters.bits.station = true;
    requiredFilters.bits.fleetCarrier = true;
    requiredFilters.bits.pointOfInterest = true;

    LocationPanelFilters currentFilters = mgr.cfg.configLocationPanelFilters;
    if (requiredFilters == currentFilters)
        return true;

    if (!mgr.uiState.match("scr-left-panel:*")) {
        LOG(DEBUG) << "TaskDock FocusLeftPanel...";
        sendKey("FocusLeftPanel", 0, 1500);
        mgr.detectEDState(DetectLevel::Buttons);
        if (!mgr.uiState.match("scr-left-panel:*")) {
            LOG(ERROR) << "TaskDock expecting 'scr-left-panel:*' but got " << mgr.uiState;
            task_return(Result::Trouble);
        }
    }

    for (int i=0; i < 5 && !mgr.uiState.match("scr-left-panel:mod-navigation"); i++) {
        LOG(DEBUG) << "TaskDock goto mod-navigation...";
        mgr.detectEDState(DetectLevel::Buttons);
        if (mgr.uiState.match("scr-left-panel:mod-sysinfo")) {
            LOG(DEBUG) << "TaskDock mod-sysinfo -> mod-navigation";
            sendKey("CycleNextPanel", 0, 250);
        }
        else if (mgr.uiState.match("scr-left-panel:mod-transact")) {
            LOG(DEBUG) << "TaskDock mod-transact -> mod-navigation";
            sendKey("CyclePreviousPanel", 0, 250);
        }
        else if (mgr.uiState.match("scr-left-panel:mod-contacts")) {
            LOG(DEBUG) << "TaskDock mod-target -> mod-navigation";
            sendKey("CyclePreviousPanel", 0, 250);
            sendKey("CyclePreviousPanel", 0, 250);
        }
        else if (mgr.uiState.match("scr-left-panel:mod-target")) {
            LOG(DEBUG) << "TaskDock mod-target -> mod-navigation";
            sendKey("CyclePreviousPanel", 0, 250);
            sendKey("CyclePreviousPanel", 0, 250);
            sendKey("CyclePreviousPanel", 0, 250);
        }
        else {
            LOG(DEBUG) << "TaskDock {mod/dlg}-any -> UI_Back";
            sendKey("UI_Back", 0, 1500);
        }
    }
    if (!mgr.uiState.match("scr-left-panel:mod-navigation")) {
        LOG(ERROR) << "TaskDock expecting 'scr-left-panel:mod-navigation' but got " << mgr.uiState;
        task_return(Result::Trouble);
    }

    sendKey("UI_Left", 0, 500);
    sendKey("UI_Up", 0, 500);
    sendKey("UI_Up", 0, 500);
    sendKey("UI_Up", 0, 500);
    sendKey("UI_Up", 0, 500);
    sendKey("UI_Select", 0, 1000);
    mgr.detectEDState(DetectLevel::Buttons);
    if (!mgr.uiState.match("scr-left-panel:dlg-filters")) {
        LOG(WARNING) << "TaskDock expecting 'scr-left-panel:dlg-filters' but got " << mgr.uiState;
        //task_return(Result::Trouble);
    }
    // currently ED always opens filters at top position 'stars',
    // so just scroll down and select/deselect what we need
    if (currentFilters.bits.star)
        sendKey("UI_Select", 0, 500);
    sendKey("UI_Down", 0, 500);

    if (currentFilters.bits.asteroidCluster)
        sendKey("UI_Select", 0, 500);
    sendKey("UI_Down", 0, 500);

    if (currentFilters.bits.planetOrMoon)
        sendKey("UI_Select", 0, 500);
    sendKey("UI_Down", 0, 500);

    if (currentFilters.bits.landablePlanetOrMoon)
        sendKey("UI_Select", 0, 500);
    sendKey("UI_Down", 0, 500);

    if (!currentFilters.bits.settlement)
        sendKey("UI_Select", 0, 500);
    sendKey("UI_Down", 0, 500);

    if (!currentFilters.bits.station)
        sendKey("UI_Select", 0, 500);
    sendKey("UI_Down", 0, 500);

    if (!currentFilters.bits.fleetCarrier)
        sendKey("UI_Select", 0, 500);
    sendKey("UI_Down", 0, 500);

    if (!currentFilters.bits.pointOfInterest)
        sendKey("UI_Select", 0, 500);
    sendKey("UI_Down", 0, 500);

    if (currentFilters.bits.signalSource)
        sendKey("UI_Select", 0, 500);
    sendKey("UI_Down", 0, 500);

    if (currentFilters.bits.system)
        sendKey("UI_Select", 0, 500);
    sendKey("UI_Down", 0, 500);

    sendKey("UI_Select", 0, 1500); // wait for configuration changes
    sendKey("UI_Right", 0, 500);

    LocationPanelFilters newFilters = mgr.cfg.configLocationPanelFilters;
    return (requiredFilters == newFilters);
}

bool TaskDock::lockDockingStation() {
    sendKey("SetSpeedZero");
    if (!mgr.uiState.match("scr-left-panel:*")) {
        LOG(DEBUG) << "TaskDock FocusLeftPanel...";
        sendKey("FocusLeftPanel", 0, 1500);
        mgr.detectEDState(DetectLevel::Buttons);
        if (!mgr.uiState.match("scr-left-panel:*")) {
            LOG(ERROR) << "TaskDock expecting 'scr-left-panel:*' but got " << mgr.uiState;
            task_return(Result::Trouble);
        }
    }

    for (int i=0; i < 5 && !mgr.uiState.match("scr-left-panel:mod-navigation"); i++) {
        LOG(DEBUG) << "TaskDock goto mod-navigation...";
        mgr.detectEDState(DetectLevel::Buttons);
        if (mgr.uiState.match("scr-left-panel:mod-sysinfo")) {
            LOG(DEBUG) << "TaskDock mod-sysinfo -> mod-navigation";
            sendKey("CycleNextPanel", 0, 250);
        }
        else if (mgr.uiState.match("scr-left-panel:mod-transact")) {
            LOG(DEBUG) << "TaskDock mod-transact -> mod-navigation";
            sendKey("CyclePreviousPanel", 0, 250);
        }
        else if (mgr.uiState.match("scr-left-panel:mod-contacts")) {
            LOG(DEBUG) << "TaskDock mod-target -> mod-navigation";
            sendKey("CyclePreviousPanel", 0, 250);
            sendKey("CyclePreviousPanel", 0, 250);
        }
        else if (mgr.uiState.match("scr-left-panel:mod-target")) {
            LOG(DEBUG) << "TaskDock mod-target -> mod-navigation";
            sendKey("CyclePreviousPanel", 0, 250);
            sendKey("CyclePreviousPanel", 0, 250);
            sendKey("CyclePreviousPanel", 0, 250);
        }
        else {
            LOG(DEBUG) << "TaskDock {mod/dlg}-any -> UI_Back";
            sendKey("UI_Back", 0, 1500);
        }
    }
    if (!mgr.uiState.match("scr-left-panel:mod-navigation")) {
        LOG(ERROR) << "TaskDock expecting 'scr-left-panel:mod-navigation' but got " << mgr.uiState;
        task_return(Result::Trouble);
    }

    mgr.detectEDState(DetectLevel::ListRows);
    if (mgr.uiState.focused_name() != "lst-bodies") {
        sendKey("UI_Left", 0, 500);
        mgr.detectEDState(DetectLevel::ListRows);
    }
    sendKey("UI_Up", 2000, 500);
    sendKey("UI_Select", 0, 1000);
    sendKey("UI_Select", 0, 1000);

    return !mgr.cfg.getCurrentStatus()->destinationName.empty();
}

bool TaskDock::flyTowardsTarget() {
    mgr.detectEDState(DetectLevel::Screen);
    for (int cnt=0; cnt < 3 && mgr.uiState.guiFocus != GuiFocus::None; cnt++) {
        sendKey("UI_Back", 0, 1500);
        mgr.detectEDState(DetectLevel::Screen);
    }
    if (mgr.uiState.guiFocus != GuiFocus::None)
        return false;
    sendKey("SetSpeedZero");
    for (int cnt=0; cnt < 10; cnt++) {
        if (cnt)
            mgr.detectEDState(DetectLevel::Screen);
        if (mgr.compassInfo.hemisphere < 0)
            continue;
        bool front = mgr.compassInfo.hemisphere == 0;
        // TODO: calculate from database for each ship
        // test: roll 69, pitch: 24, yaw: 12  degree per second
        double rollSpd = 68.88;
        double pitchSpd = 24.49;
        double yawSpd = 12.24;
        int hemiYaw = mgr.compassInfo.targetYaw;
        if (!front) {
            if (hemiYaw > 0)
                hemiYaw = 180 - hemiYaw;
            else
                hemiYaw = -180 - hemiYaw;
        }
        if (std::abs(hemiYaw) > 20) {
            int roll = mgr.compassInfo.targetRoll;
            if (roll > 0) {
                sendKey("RollRightButton", roll * 1000 / rollSpd - 25, 50);
                continue;
            } else {
                sendKey("RollLeftButton", -roll * 1000 / rollSpd - 25, 50);
                continue;
            }
        }
        int pitch = mgr.compassInfo.targetPitch;
        if (std::abs(pitch) > 20) {
            if (pitch > 0) {
                sendKey("PitchUpButton", pitch * 1000 / pitchSpd - 25, 50);
                continue;
            } else {
                sendKey("PitchDownButton", -pitch * 1000/ pitchSpd - 25, 50);
                continue;
            }
        }
        break;
    }
    if (mgr.compassInfo.hemisphere < 0)
        return false;
    sendKey("SetSpeed50");
    sleep(8000);
    sendKey("SetSpeedZero");
    return true;
}


} // ai
