//
// Created by mkizub on 28.06.2025.
//

#include "../pch.h"

#include "Task.h"
#include "AIManager.h"
#include "AutopilotTasks.h"
#include "../OCR.h"
#include "../FuzzyMatch.h"

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

int BaseAutopilotTask::getNavPageIndex(const std::string& page_name) {
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

bool BaseAutopilotTask::gotoNavPage(const std::string& page_name) {
    int targetPageIndex = getNavPageIndex(page_name);
    if (targetPageIndex < 0)
        task_return(Result::Failure);

    for (int i=0; i < 6 && !mgr.uiState.match("scr-left-panel:"+page_name); i++) {
        mgr.detectEDState(DetectLevel::ListRows);
        LOG(DEBUG) << "Goto '" << page_name << "'...";

        if (!mgr.uiState.match("scr-left-panel:*")) {
            LOG(DEBUG) << "FocusLeftPanel...";
            sendKey("FocusLeftPanel", 0, 1500);
            continue;
        }
        if (mgr.uiState.match("scr-left-panel:dlg-nav-select") || mgr.uiState.match("scr-left-panel:dlg-filters")) {
            sendKey("UI_Back", 0, 500);
            continue;
        }
        std::vector<std::string> segments = mgr.uiState.splitPath();
        if (segments.size() < 2) {
            LOG(ERROR) << "Expecting 2 segments in " << mgr.uiState;
            task_return(Result::Failure);
        }
        int currentPageIndex = getNavPageIndex(segments[1]);
        if (currentPageIndex < 0)
            task_return(Result::Failure);
        int dist = currentPageIndex - targetPageIndex;
        if (dist >= 0) {
            for (int i=0; i < dist; i++)
                sendKey("CycleNextPanel", 0, 250);
        } else {
            for (int i=0; i < -dist; i++)
                sendKey("CyclePreviousPanel", 0, 250);
        }
    }
    if (!mgr.uiState.match("scr-left-panel:"+page_name))
        task_return(Result::Trouble);
    return true;
}

bool BaseAutopilotTask::selectNavFilters(LocationPanelFilters requiredFilters) {
    for (int retry=0; retry < 3; retry++) {
        LocationPanelFilters currentFilters = mgr.cfg.configLocationPanelFilters;
        if (requiredFilters == currentFilters)
            return true;

        gotoNavPage("mod-navigation");

        int delay = 300;
        sendKey("UI_Left", 0, delay);
        for (int i = 0; i < 4; i++)
            sendKey("UI_Up", 0, delay);
        sendKey("UI_Select", 0, 1000);

        mgr.detectEDState(DetectLevel::Buttons);
        if (!mgr.uiState.match("scr-left-panel:dlg-filters")) {
            LOG(WARNING) << "TaskDock expecting 'scr-left-panel:dlg-filters' but got " << mgr.uiState;
            task_return(Result::Trouble);
        }
        // currently ED always opens filters at top position 'stars',
        // so just scroll down and select/deselect what we need
        if (currentFilters.bits.star)
            sendKey("UI_Select", 0, delay);
        sendKey("UI_Down", 0, delay);

        if (currentFilters.bits.asteroidCluster)
            sendKey("UI_Select", 0, delay);
        sendKey("UI_Down", 0, delay);

        if (currentFilters.bits.planetOrMoon)
            sendKey("UI_Select", 0, delay);
        sendKey("UI_Down", 0, delay);

        if (currentFilters.bits.landablePlanetOrMoon)
            sendKey("UI_Select", 0, delay);
        sendKey("UI_Down", 0, delay);

        if (!currentFilters.bits.settlement)
            sendKey("UI_Select", 0, delay);
        sendKey("UI_Down", 0, delay);

        if (!currentFilters.bits.station)
            sendKey("UI_Select", 0, delay);
        sendKey("UI_Down", 0, delay);

        if (!currentFilters.bits.fleetCarrier)
            sendKey("UI_Select", 0, delay);
        sendKey("UI_Down", 0, delay);

        if (!currentFilters.bits.pointOfInterest)
            sendKey("UI_Select", 0, delay);
        sendKey("UI_Down", 0, delay);

        if (currentFilters.bits.signalSource)
            sendKey("UI_Select", 0, delay);
        sendKey("UI_Down", 0, delay);

        if (currentFilters.bits.system)
            sendKey("UI_Select", 0, delay);
        sendKey("UI_Down", 0, delay);

        sendKey("UI_Select", 0, 1500); // wait for configuration changes
        sendKey("UI_Right", 0, delay);
    }
    return (requiredFilters == mgr.cfg.configLocationPanelFilters);
}

bool BaseAutopilotTask::parseNavNameDist(std::wstring text, std::wstring dist, StationRowInfo& rowInfo) {
    rowInfo = {};
    text = trim(text);
    if (text.empty())
        return false;
    wchar_t ch = text.front();
    if (text[0] >= 0x2000 && text[0] <= 0x2FFF) {
        rowInfo.type = text[0];
        text = trim(text.substr(1));
        if (text.empty())
            return false;
    }
    ch = text.back();
    wchar_t ch1 = text[text.size()-2];
    wchar_t ch2 = text[text.size()-3];
    if (ch == nav::LOCATION.charOCR ||
        (ch1 == nav::SHIELD1.charOCR || ch1 == nav::SHIELD2.charOCR || ch1 == nav::SHIELD3.charOCR) ||
        (ch1 == L' ' && (ch2 == nav::SHIELD1.charOCR || ch2 == nav::SHIELD2.charOCR || ch2 == nav::SHIELD3.charOCR))
        )
    {
        rowInfo.isLocation = true;
        text.pop_back();
        text = trim(text);
        if (text.empty())
            return false;
    }
    ch = text.back();
    if (ch == nav::SHIELD1.charOCR || ch == nav::SHIELD2.charOCR || ch == nav::SHIELD3.charOCR) {
        rowInfo.danger = ch;
        text.pop_back();
        text = trim(text);
        if (text.empty())
            return false;
    }
    ch = text.back();
    while (ch == L'+') {
        rowInfo.size += 1;
        text.pop_back();
        text = trim(text);
        if (text.empty())
            return false;
        ch = text.back();
    }
    if (text[0] == L'<' && ch == '>') {
        rowInfo.isTarget = true;
        text = text.substr(1,text.size()-2);
        text.pop_back();
        text = trim(text.substr(1));
        if (text.empty())
            return false;
    }
    rowInfo.name = text;

    rowInfo.dockable = false;
    if (rowInfo.type) {
        for (auto nt: nav::ALL_NAV_TYPES) {
            if (nt->charOCR == rowInfo.type) {
                rowInfo.navType = nt;
                break;
            }
        }
        if (rowInfo.navType) {
            switch (rowInfo.navType->poiType) {
            case ai::POIType::Station:
            case ai::POIType::Port:
            case ai::POIType::FleetCarrier:
                rowInfo.dockable = true;
                break;
            case ai::POIType::Place:
                if (rowInfo.type == nav::STATION_MEGASHIP.charOCR)
                    rowInfo.dockable = true;
                else if (rowInfo.type == nav::MEGASHIP.charOCR) {
                    FuzzyMatch fm;
                    rowInfo.dockable = fm.ratio(rowInfo.name, L"Trailblazer Dream") > 90;
                }
                else
                    rowInfo.dockable = false;
                break;
            default:
                rowInfo.dockable = false;
            }
        }
    }

    rowInfo.dist = parseDist(dist);

    return true;
}

bool BaseAutopilotTask::parseNavRow(const cv::Mat& grayImage, const ClassifiedRect& cr, StationRowInfo& rowInfo) {
    rowInfo = {};
    std::string text;
    std::string dist;
    if (ocr::ocrRowText(grayImage, mgr.rEnv, cr, 0, text) < 60)
        return false;
    if (ocr::ocrRowText(grayImage, mgr.rEnv, cr, 1, dist) < 60)
        return false;
    std::wstring wtext = toUtf16(text);
    std::wstring wdist = toUtf16(dist);
    return parseNavNameDist(wtext, wdist, rowInfo);
}

bool BaseAutopilotTask::parseFocusedNavRow(const cv::Mat& grayImage, StationRowInfo& rowInfo) {
    rowInfo = {};
    for (auto &cr: mgr.rEnv.classified) {
        if (cr.cdt != ClsDetType::ListRow)
            continue;
        if (cr.u.lrow.ws == WState::Focused)
            return parseNavRow(grayImage, cr, rowInfo);
    }
    return false;
}

bool BaseAutopilotTask::getFocusedNavRow(StationRowInfo& rowInfo) {
    if (!mgr.uiState.match("scr-left-panel:mod-navigation"))
        gotoNavPage("mod-navigation");

    cv::Mat grayImage;
    double dist_km = 10;
    for (int cnt=0; cnt < 3; cnt++) {
        mgr.detectEDState(DetectLevel::ListRows, nullptr, &grayImage);
        if (mgr.uiState.focused_name() != "lst-bodies") {
            sendKey("UI_Right", 0, 500);
            continue;
        }
        if (!parseFocusedNavRow(grayImage, rowInfo)) {
            LOG(INFO) << "Failed to parse nav row";
            continue;
        }
        return true;
    }
    return false;
}

bool BaseAutopilotTask::orientTowardTargetStep(int precision) {
    bool front = mgr.compassInfo.hemisphere > 0;
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
        int duration = std::abs(roll * 1000 / rollSpd) * 0.75;
        int pause = duration < 1000 ? duration : 1000;
        notifyProgress(std::format("Orientation: fix roll {}", roll));
        if (roll > 0)
            sendKey("RollRightButton", duration, pause);
        else
            sendKey("RollLeftButton", duration, pause);
        return false;
    }
    int pitch = mgr.compassInfo.targetPitch;
    if (std::abs(pitch) > precision) {
        int duration = std::abs(pitch * 1000 / pitchSpd) * 0.75;
        int pause = duration < 1000 ? duration : 1000;
        notifyProgress(std::format("Orientation: fix pitch {}", pitch));
        if (pitch > 0)
            sendKey("PitchUpButton", duration, pause);
        else
            sendKey("PitchDownButton", duration, pause);
        return false;
    }
    int yaw = mgr.compassInfo.targetYaw;
    if (std::abs(yaw) > precision) {
        int duration = std::abs(yaw * 1000 / yawSpd) * 0.75;
        int pause = duration < 1000 ? duration : 1000;
        notifyProgress(std::format("Orientation: fix yaw {}", yaw));
        if (yaw > 0)
            sendKey("YawRightButton", duration, pause);
        else
            sendKey("YawLeftButton", duration, pause);
        return false;
    }
    return true;
}

bool BaseAutopilotTask::orientTowardTarget(int precision, const char* dropSpeedButton) {
    if (mgr.uiState.guiFocus != GuiFocus::None) {
        notifyProgress("Orientation: goto compass");
        sendKey("UI_Back", 0, 1500);
    }
    int speedDropped = 0;
    for (int fails=0; fails < 10; fails++) {
        if (fails > 2 && dropSpeedButton) {
            if (speedDropped <= 0) {
                notifyProgress(std::format("Orientation fails {}: drop speed {}", fails, dropSpeedButton));
                sendKey(dropSpeedButton, 100, 500);
                speedDropped = 3;
                continue;
            }
            speedDropped -= 1;
        }
        mgr.detectEDState(DetectLevel::Screen);
        if (mgr.uiState.guiFocus != GuiFocus::None) {
            notifyProgress(std::format("Unexpected ui mode {}", mgr.uiState.to_string()));
            LOG(WARNING) << "Unexpected ui mode " << mgr.uiState;
            sendKey("UI_Back", 0, 1500);
            continue;
        }
        if (!mgr.compassInfo.hemisphere) {
            notifyProgress(std::format("Compass not detected, fails {}", fails));
            LOG(WARNING) << "Compass not detected";
            sendKey("RollRightButton", 800, 500);
            continue;
        }
        fails = 0;
        if (dropSpeedButton && speedDropped <= 0) {
            if (mgr.compassInfo.hemisphere < 0 || std::abs(mgr.compassInfo.targetPitch) > 20 || std::abs(mgr.compassInfo.targetPitch) > 20) {
                notifyProgress(std::format("Orientation angle pitsh {}, yaw {}: drop speed {}", mgr.compassInfo.targetPitch, mgr.compassInfo.targetYaw, dropSpeedButton));
                sendKey(dropSpeedButton, 100, 100);
                speedDropped = 3;
                continue;
            }
            speedDropped -= 1;
        }
        if (orientTowardTargetStep(precision))
            return true;
    }
    LOG(ERROR) << "Compass not detected";
    return false;
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
    int notAutoPilotCounter = 0;
    // wait at least 15 seconds for autopilot to departure
    for (int i=0; i < 60 && notAutoPilotCounter < 4; i++) {
        sleep(250);
        mgr.detectEDState(DetectLevel::Screen);
        if (mgr.uiState.autopilot) {
            notAutoPilotCounter = 0;
            LOG(INFO) << "Departure autopilot waiting...";
        } else {
            notAutoPilotCounter += 1;
        }
    }
    notAutoPilotCounter = 0;
    int logCounter = 0;
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

    sendKey("SetSpeed100", 100, 500);
    while (mgr.cfg.getCurrentStatus()->flags.fsd_masslocked) {
        LOG(INFO) << "Mass-locked, flying away";
    }
    LOG(INFO) << "Ready to jump, flying away";
    sleep(3000);
    sendKey("SetSpeed75", 100, 500);
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

    // leave all UI panels
    for (int cnt=0; cnt < 3; cnt++) {
        mgr.detectEDState(DetectLevel::Screen);
        if (mgr.cfg.getGuiFocus() == GuiFocus::None)
            break;
        sendKey("UI_Back", 0, 1500);
    }
    if (mgr.cfg.getGuiFocus() != GuiFocus::None)
        return Result::Trouble;

    // lock nav target to nearest dock
    {
        // select nav filters to dockable entries (station, port, fleet carriers and interesting places for trailblazer ships)
        if (!selectDockingFilters())
            return Result::Trouble;
        // select station to dock (topmost entry in nav list)
        if (!lockDockingStation())
            return Result::Trouble;
        mgr.detectEDState(DetectLevel::Buttons);
    }

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
        if (distanceToDock.unit == dist_t::KM && distanceToDock.dist > 7.4) {
            LOG(INFO) << "Distance to dock is " << distanceToDock << ", fly towards the dock";
            flyTowardsTarget();
            continue;
        }
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
    for (int retry=0; retry < 3; retry++) {
        sendKey("SetSpeedZero");
        gotoNavPage("mod-contacts");

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
        sendKey("UI_Right", 0, 500);
        sendKey("UI_Select", 100, 1000);
        //sendKey("UI_Back");
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
    return selectNavFilters(requiredFilters);
}

bool TaskDock::lockDockingStation() {
    sendKey("SetSpeedZero");
    gotoNavPage("mod-navigation");

    StationRowInfo rowInfo;
    std::wstring dock_name;
    cv::Mat grayImage;
    for (int retry=0; retry < 3; retry++) {
        mgr.detectEDState(DetectLevel::ListRows, nullptr, &grayImage);
        if (mgr.uiState.focused_name() != "lst-bodies") {
            sendKey("UI_Right", 0, 500);
            continue;
        }
        if (!parseFocusedNavRow(grayImage, rowInfo))
            continue;
        dock_name = rowInfo.name;
        break;
    }
    if (mgr.uiState.focused_name() != "lst-bodies" || dock_name.empty())
        return false;

    for (int retry=0; retry < 3; retry++) {
        sendKey("UI_Down", 0, 200);
        sendKey("UI_Up", 1000, 500);

        mgr.detectEDState(DetectLevel::ListRows, nullptr, &grayImage);
        if (!parseFocusedNavRow(grayImage, rowInfo))
            continue;
        if (rowInfo.name != dock_name) {
            dock_name = rowInfo.name;
            retry = 0;
            continue;
        }
        if (!rowInfo.dockable) {
            retry = 0;
            continue;
        }
        if (rowInfo.dist && rowInfo.dist.convertTo(dist_t::KM).dist > 100)
            continue;
        if (rowInfo.isTarget)
            return true;
        sendKey("UI_Select", 0, 1000);
        sendKey("UI_Select", 0, 1000);
    }

    return false;
}

bool TaskDock::flyTowardsTarget() {
    sendKey("SetSpeedZero");
    int compassTry = 2;
    for (int fails=0; fails < 10; fails++) {
        if (compassTry <= 0) {
            compassTry += 1;
            StationRowInfo rowInfo;
            if (!getFocusedNavRow(rowInfo) || rowInfo.dist.unit == dist_t::X) {
                LOG(DEBUG) << "Failed to get distance from nav panel: " << rowInfo.dist;
                sendKey("SetSpeedZero");
                continue;
            }
            distanceToDock = rowInfo.dist.convertTo(dist_t::KM);
            LOG(DEBUG) << "DistanceToDock from nav panel: " << distanceToDock;
        } else {
            mgr.detectEDState(DetectLevel::ListRows);
            if (mgr.uiState.guiFocus != GuiFocus::None) {
                sendKey("UI_Back", 100, 1000);
                continue;
            }
            if (mgr.compassInfo.nav_target_dist.unit == dist_t::X) {
                LOG(DEBUG) << "Failed to get distance from compass: " << mgr.compassInfo.nav_target_dist;
                sendKey("SetSpeedZero");
                if (!mgr.compassInfo.hemisphere) {
                    sendKey("RollRightButton", 800, 1000);
                } else {
                    if (std::abs(mgr.compassInfo.targetYaw) > 7 || std::abs(mgr.compassInfo.targetPitch) > 7)
                        orientTowardTarget(7);
                }
                compassTry -= 1;
                if (compassTry <= 0)
                    compassTry = -2;
                continue;
            }
            compassTry = 2;
            distanceToDock = mgr.compassInfo.nav_target_dist.convertTo(dist_t::KM);
            LOG(DEBUG) << "DistanceToDock from compass: " << distanceToDock;
        }

        if (distanceToDock.dist <= 7.4) {
            LOG(INFO) << "Distance is " << distanceToDock << ", we are close enough";
            sendKey("SetSpeedZero");
            return true;
        }

        LOG(INFO) << "Distance is " << distanceToDock << ", orient towards the dock";
        if (!orientTowardTarget(7, "SetSpeedZero")) {
            sendKey("SetSpeedZero");
            compassTry = -2;
            continue;
        }

        if (distanceToDock.dist < 8.5) {
            LOG(INFO) << "Distance is " << distanceToDock << ", fly slowly";
            sendKey("SetSpeed25", 100, 1000);
        }
        else if (distanceToDock.dist > 10.5) {
            LOG(INFO) << "Distance is " << distanceToDock << ", fly fast";
            sendKey("SetSpeed100", 100, 1000);
        }
        else {
            LOG(INFO) << "Distance is " << distanceToDock << ", fly normal";
            sendKey("SetSpeed50", 100, 1000);
        }
        fails = 0;
    }
    sendKey("SetSpeedZero");
    return false;
}


TaskJumpToSystem::TaskJumpToSystem(Task* parent, AIManager& mgr, const TaskTemplate& templ)
    : BaseAutopilotTask(parent, mgr, templ)
{
    for (auto& p : templ.params) {
        if (p.name == "system")
            destSystem = std::get<std::string>(p.value);
    }
}

Result TaskJumpToSystem::run() {
    return Result::Success;
}

bool TaskJumpToSystem::selectDestSystem() {
    sendKey("GalaxyMapOpen", 100, 1500);
    mgr.detectEDState(DetectLevel::Buttons);
    if (mgr.uiState.guiFocus != GuiFocus::GalaxyMap) {
        return false;
    }

    return true;
}

TaskCruiseToDock::TaskCruiseToDock(Task* parent, AIManager& mgr, const TaskTemplate& templ)
    : BaseAutopilotTask(parent, mgr, templ)
{
    for (auto& p : templ.params) {
        if (p.name == "dock")
            destDock = std::get<std::string>(p.value);
    }
}

Result TaskCruiseToDock::run() {
    sendKey("SetSpeedZero", 100, 500);

    if (!selectDestDock())
        return Result::Trouble;

    if (!orientTowardTarget(5)) {
        notifyProgress("Cannot fix orientation");
        return Result::Trouble;
    }

    const auto& ss = mgr.cfg.getCurrentStatus();
    if (!ss->flags.cruise) {
        sendKey("SetSpeed100", 100, 500);
        while (ss->flags.fsd_masslocked) {
            notifyProgress("Mass-locked, flying away");
            sleep(1000);
        }

        if (ss->flags.cargo_scoop_on) {
            notifyProgress("Hide cargo scoop");
            sendKey("ToggleCargoScoop", 0, 1000);
        }
        if (ss->flags.weapon_on) {
            notifyProgress("Hide weapon");
            sendKey("DeployHardpointToggle", 0, 1000);
        }
        notifyProgress("Entering supercruise");
        sendKey("Supercruise", 100, 1000);
        if (!(ss->flags.fsd_charging || ss->flags.fsd_jump)) {
            notifyProgress("Entering supercruise failed");
            return Result::Trouble;
        }

        while (!ss->flags.cruise && (ss->flags.fsd_charging || ss->flags.fsd_jump)) {
            notifyProgress("Waiting for super-cruise mode");
            sleep(1000);
        }

        if (!ss->flags.cruise) {
            notifyProgress("Entering supercruise failed");
            return Result::Trouble;
        }
    }
    notifyProgress("Сruising speed (75%)");
    sendKey("SetSpeed75", 100, 500);

    // wait until we get to 1mm
    int compassTry = 2;
    for (int fails=0; ; fails++) {
        if (compassTry <= 0) {
            compassTry += 1;
            StationRowInfo rowInfo;
            if (!getFocusedNavRow(rowInfo) || rowInfo.dist.unit == dist_t::X) {
                notifyProgress(std::format("Failed to get distance from nav panel: {}, speed 25%", rowInfo.dist.to_string()));
                LOG(DEBUG) << "Failed to get distance from nav panel: " << rowInfo.dist;
                sendKey("SetSpeed25");
                continue;
            }
            distanceToDock = rowInfo.dist.convertTo(dist_t::KM);
            notifyProgress(std::format("Distance: {}", distanceToDock.to_string()));
            LOG(DEBUG) << "DistanceToDock from nav panel: " << distanceToDock;
            if (distanceToDock.dist < 3000) {
                if (distanceToDock.dist < 1000) {
                    notifyProgress(std::format("Distance: {}, speed zero", distanceToDock.to_string()));
                    sendKey("SetSpeedZero");
                } else {
                    notifyProgress(std::format("Distance: {}, speed slow (25%)", distanceToDock.to_string()));
                    sendKey("SetSpeed25");
                }
            }
            sendKey("UI_Back", 100, 1000);
            mgr.detectEDState(DetectLevel::ListRows);
        } else {
            mgr.detectEDState(DetectLevel::ListRows);
            if (mgr.uiState.guiFocus != GuiFocus::None) {
                sendKey("UI_Back", 100, 1000);
                continue;
            }
            if (mgr.compassInfo.nav_target_dist.unit == dist_t::X) {
                notifyProgress(std::format("Failed to get distance from compass: {}, speed 25%", mgr.compassInfo.nav_target_dist.to_string()));
                LOG(DEBUG) << "Failed to get distance from compass: " << mgr.compassInfo.nav_target_dist;
                sendKey("SetSpeed25");
                if (!mgr.compassInfo.hemisphere) {
                    sendKey("RollRightButton", 800, 1000);
                } else {
                    if (mgr.compassInfo.hemisphere < 0 || std::abs(mgr.compassInfo.targetYaw) > 7 || std::abs(mgr.compassInfo.targetPitch) > 7) {
                        orientTowardTargetStep(7);
                    }
                }
                compassTry -= 1;
                if (compassTry <= 0)
                    compassTry = -2;
                continue;
            }
            distanceToDock = mgr.compassInfo.nav_target_dist.convertTo(dist_t::KM);
            notifyProgress(std::format("Distance from compass: {}", distanceToDock.to_string()));
            LOG(DEBUG) << "DistanceToDock from compass: " << distanceToDock;
        }

        compassTry = 2;

        if (!mgr.compassInfo.hemisphere) {
            sendKey("RollRightButton", 800, 1000);
            continue;
        } else {
            if (mgr.compassInfo.hemisphere < 0 || std::abs(mgr.compassInfo.targetYaw) > 2 || std::abs(mgr.compassInfo.targetPitch) > 2) {
                orientTowardTargetStep(2);
                continue;
            }
        }

        if (distanceToDock.dist < 1000) {
            notifyProgress(std::format("Distance: {}, speed zero", distanceToDock.to_string()));
            sendKey("SetSpeedZero", 100, 500);
            if (mgr.compassInfo.hemisphere && std::abs(mgr.compassInfo.targetYaw) <= 20 && std::abs(mgr.compassInfo.targetPitch) <= 20)
                break;
        }

        orientTowardTargetStep(2);

        if (distanceToDock.dist < 3000) {
            notifyProgress(std::format("Distance: {}, speed slow (25%)", distanceToDock.to_string()));
            sendKey("SetSpeed25", 100, 500);
        } else {
            notifyProgress(std::format("Distance: {}, speed cruise (75%)", distanceToDock.to_string()));
            sendKey("SetSpeed75", 100, 500);
        }
    }

    // wait until we exit super-cruise
    while (ss->flags.cruise) {
        notifyProgress("Waiting cruise exit...");
        sendKey("HyperSuperCombination", 100, 1000);
        sleep(1000);
        orientTowardTargetStep(10);
    }

    notifyProgress("Arrived, speed zero");
    sendKey("SetSpeedZero", 100, 500);

    return Result::Success;
}

bool TaskCruiseToDock::selectDestDock() {
    return true;
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
    TaskTemplate taskDepart = mgr.getTaskTemplate(ED_TASK_DEPART);
    sub_tasks.push_back(std::make_unique<TaskDepart>(this, mgr, taskDepart));

    TaskTemplate taskJumpToSystem = mgr.getTaskTemplate(ED_TASK_JUMP_TO_SYSTEM);
    taskJumpToSystem.set("system", destSystem);
    sub_tasks.push_back(std::make_unique<TaskJumpToSystem>(this, mgr, taskJumpToSystem));

    TaskTemplate taskCruiseToDock = mgr.getTaskTemplate(ED_TASK_CRUISE_TO_STATION);
    taskCruiseToDock.set("dock", destDock);
    sub_tasks.push_back(std::make_unique<TaskCruiseToDock>(this, mgr, taskCruiseToDock));

    TaskTemplate taskDock = mgr.getTaskTemplate(ED_TASK_DOCK);
    sub_tasks.push_back(std::make_unique<TaskDock>(this, mgr, taskDock));
}

Result TaskTravel::run() {
    switch (result) {
    case Result::Created:
    case Result::Started:
    case Result::Trouble:
        plan();
        break;
    case Result::Failure:
    case Result::Partly:
    case Result::Success:
        LOG(ERROR) << "Bad state on task run(): " << enum_name<Result>(result);
        return result;
    }

    while (!sub_tasks.empty()) {
        spTask& pTask = sub_tasks.front();
        Result res = run_sub_task(pTask);
        switch (res) {
        case Result::Created:
        case Result::Started:
            LOG(ERROR) << "Bad state after task run(): " << enum_name<Result>(res);
            plan();
            continue;
        case Result::Trouble:
            if (pTask->missCount < pTask->maxMisses) {
                plan();
                pTask->result = Result::Started;
                continue;
            }
            pTask->result = Result::Failure;
            // fall through
        case Result::Failure:
        case Result::Partly:
        case Result::Success:
            sub_tasks.pop_front();
            break;
        }
    }
    notifyProgress(_("End of travel"));
    result = Result::Success;
    return result;
}


} // ai
