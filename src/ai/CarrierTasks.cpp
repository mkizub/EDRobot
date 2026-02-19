//
// Created by mkizub on 11.02.2026.
//

#include "../pch.h"

#include "AIManager.h"
#include "CarrierTasks.h"
#include "AIUtils.h"

#include "../Keyboard.h"

namespace ai {

inline void sendUiBack(int pause=1000) {
    if (pause >= 500 && Cfg.isHeadlookSmoothing())
        pause = 250;
    kbd::send("UI_Back", 0, pause);
}

int getShipPageIndex(const std::string &page_name) {
    int pageIndex = -1;
    if (page_name == "mod-main")
        pageIndex = 0;
    else if (page_name == "mod-modules")
        pageIndex = 1;
    else if (page_name == "mod-weapon")
        pageIndex = 2;
    else if (page_name == "mod-ship-info")
        pageIndex = 3;
    else if (page_name == "mod-eq-cargo")
        pageIndex = 4;
    else if (page_name == "mod-storage")
        pageIndex = 5;
    else if (page_name == "mod-state")
        pageIndex = 6;
    else if (page_name == "mod-playlist")
        pageIndex = 7;
    else
        LOG(ERROR) << "Ship page '" << page_name << "' not known";
    return pageIndex;
}

bool gotoShipPage(const std::string &page_name, bool required) {
    int targetPageIndex = getShipPageIndex(page_name);
    if (targetPageIndex < 0)
        ai::throw_failed("Bad ship panel page: {}", page_name);

    for (int i = 0; i < 6 && !ai::uiState.match("scr-right-panel:" + page_name); i++) {
        ai::detectEDState(DetectLevel::Buttons);
        LOG(DEBUG) << "Goto '" << page_name << "'...";

        if (ai::uiState.guiFocus == GuiFocus::None) {
            LOG(DEBUG) << "FocusRightPanel...";
            kbd::send("FocusRightPanel", 0, 1500);
            continue;
        }
        if (ai::uiState.guiFocus == GuiFocus::Right && !ai::uiState.screen) {
            if ((i & 1) == 0)
                rollBlindCompass();
            else
                sendUiBack();
            continue;
        }
        if (!ai::uiState.match("scr-right-panel:*")) {
            LOG(DEBUG) << "FocusRightPanel...";
            kbd::send("FocusRightPanel", 0, 1500);
            continue;
        }
        if (ai::uiState.match("scr-right-panel:mod-transfer")) {
            sendUiBack();
            continue;
        }
        std::vector<std::string> segments = ai::uiState.splitPath();
        if (segments.size() < 2)
            ai::throw_failed("Expecting 2 segments in {}", ai::uiState.to_string());
        int currentPageIndex = getShipPageIndex(segments[1]);
        if (currentPageIndex < 0)
            ai::throw_failed("Bad ship panel page: {}", segments[1]);
        int dist = targetPageIndex - currentPageIndex;
        if (dist >= 0) {
            for (int j = 0; j < dist; j++)
                kbd::send("CycleNextPanel", 0, 250);
        } else {
            for (int j = 0; j < -dist; j++)
                kbd::send("CyclePreviousPanel", 0, 250);
        }
    }
    if (!ai::uiState.match("scr-right-panel:" + page_name)) {
        if (required)
            ai::throw_trouble("Unexpected scr-right-panel: {}", ai::uiState.to_string());
        return false;
    }
    return true;
}


TaskMyCarrierUnload::TaskMyCarrierUnload(const TaskTemplate &templ)
    : Task(templ)
{
    assert (templ.id == ED_TASK_CARRIER_UNLOAD);
}

bool TaskMyCarrierUnload::run() {
    if (st::shipStats.cargo <= 0 && (!st::currentCargo || st::currentCargo->count <= 0)) {
        status = DONE_NOTHING;
        return true;
    }
    if (!st::cmdr.fleetCarrierId || st::dockedAt.marketId != st::cmdr.fleetCarrierId) {
        throw_failed("Not docked at own carrier");
        return false;
    }
    status = TO_TRANSFER;
    gotoShipPage("mod-eq-cargo", true);
    for (int i=0; i < 3; i++) {
        kbd::send("UI_Up", 0, 250);
        kbd::send("UI_Right", 0, 250);
    }
    kbd::send("UI_Select", 0, 500);

    ai::detectEDState(DetectLevel::Buttons);
    if (!ai::uiState.match("scr-right-panel:mod-transfer"))
        throw_trouble("Cannot enter transfer panel");

    status = UNLOAD;
    kbd::send("UI_Up", 1500, 500);
    kbd::send("UI_Up", 0, 500);
    kbd::send("UI_Left", 0, 250);
    kbd::send("UI_Select", 0, 500);
    kbd::send("UI_Select", 0, 1500);

    waitMarketEvent(4s);
    if (Cfg.marketEvent && Cfg.marketEvent->event == "CargoTransfer") {
        for (auto& item : Cfg.marketEvent->data["Transfers"].as_array()) {
            contributed += item.at("Count",0).as_integer();
        }
    }
    sendUiBack();
    sendUiBack();
    status = DONE;
    return true;
}


std::string TaskMyCarrierUnload::getStatus() {
    switch (status) {
    case READY:
        return {};
    case TO_TRANSFER:
        return _gt("Going to transfer page");
    case UNLOAD:
        return _gt("Unloading");
    case DONE:
        return lc_format("Unloaded {} items", contributed);
    case DONE_NOTHING:
        return _gt("Nothing to unload");
    }
    return {};
}

} // namespace ai
