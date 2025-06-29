//
// Created by mkizub on 28.06.2025.
//

#include "../pch.h"

#include "Task.h"
#include "AIManager.h"
#include "AutopilotTasks.h"

namespace ai {

TaskDepart::TaskDepart(ai::Task *parent, ai::AIManager &mgr, const ai::TaskTemplate &templ)
    : Task(parent, mgr, templ)
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
    // wait at least 15 seconds for autopilot to departure
    for (int i=0; i < 60; i++) {
        sleep(250);
        mgr.detectEDState(DetectLevel::Screen);
        LOG(INFO) << "Departure autopilot forced, autopilot: " << mgr.uiState.autopilot << ", docked: " << ss->flags.docked;
    }
    int notAutoPilotCounter = 0;
    for (int i=0; i < 1200; i++) { // 5 minutes for departure = 5*60 seconds, 4 times per cosend
        sleep(250);
        mgr.detectEDState(DetectLevel::Screen);
        LOG(INFO) << "Departure autopilot check: " << mgr.uiState.autopilot;
        if (mgr.uiState.autopilot) {
            notAutoPilotCounter = 0;
            continue;
        }
        if (++notAutoPilotCounter > 4) {
            LOG(INFO) << "Departure complete";
            break;
        }
    }
    return Result::Success;
}

}
