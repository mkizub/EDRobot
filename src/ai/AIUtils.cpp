//
// Created by mkizub on 10.12.2025.
//

#include "../pch.h"
#include "Types.h"
#include "Task.h"
#include "AIManager.h"
#include "AIUtils.h"
#include "../Keyboard.h"

namespace ai {

bool clickWidget(const char* btn, int delay_ms, int pause_ms) {
    cv::Rect rect = Mgr.resolveWidgetReferenceRect(btn);
    if (rect.empty())
        return false;
    cv::Point pos = (rect.tl() + rect.br()) * 0.5;
    return kbd::sendMouseClick(pos, delay_ms, pause_ms);
}

bool clickButton(const char* btn) {
    cv::Rect rect = Mgr.resolveWidgetReferenceRect(btn);
    if (rect.empty())
        return false;
    cv::Point pos = (rect.tl() + rect.br()) * 0.5;
    return kbd::sendMouseClick(pos, 100, Cfg.getDefaultKeyAfterTime());
}

bool moveToWidget(const char* widget) {
    cv::Rect rect = Mgr.resolveWidgetReferenceRect(widget);
    if (rect.empty())
        return false;
    cv::Point pos = (rect.tl() + rect.br()) * 0.5;
    return kbd::sendMouseMove(pos, 300);
}

bool waitUiState(const std::string& state, std::chrono::seconds duration) {
    utc_timer timer(duration);
    do {
        ai::detectEDState(DetectLevel::Buttons);
        if (ai::uiState.match(state))
            return true;
        sleep(250);
    } while (!timer.expired());
    ai::detectEDState(DetectLevel::Buttons);
    return ai::uiState.match(state);
}

bool waitMarketEvent(std::chrono::seconds duration) {
    utc_timer timer(duration);
    while (!Cfg.marketEvent && !timer.expired()) {
        sleep(250);
    }
    return bool(Cfg.marketEvent);
}

bool leaveScrGalaxy() {
    if (ai::uiState.guiFocus != GuiFocus::GalaxyMap)
        return true;
    for (int i = 0; i < 10 && st::guiFocus == GuiFocus::GalaxyMap; i++) {
        kbd::send("UI_Back", 0, 1000);
        ai::detectEDState(DetectLevel::Buttons);
        if (ai::uiState.guiFocus != GuiFocus::GalaxyMap)
            return true;
        clickWidget("btn-exit", 500, 1000);
        ai::detectEDState(DetectLevel::Buttons);
        if (ai::uiState.guiFocus != GuiFocus::GalaxyMap)
            return true;
    }
    return false;
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
        pos.y = dd_rect.y + (entry+0.5)*28.65*ai::rEnv.getScale();
        kbd::sendMouseClick(pos, 300, 100);
        ai::sleep(3000);

        clickWidget("btn-tgt-copy",300,500);
        auto name = textFromClipboard();
        if (name == systemName) {
            clickWidget("btn-tgt-nav-to", 500, 1000);
            leaveScrGalaxy();
            return true;
        }

        cv::Rect rect = Mgr.resolveWidgetReferenceRect("lbl-search");
        pos = (rect.tl() + rect.br()) * 0.5;
        kbd::sendMouseMove(pos, 500);
    }
    return false;
}


void gotoMarketScreen(bool buy) {
    if (!st::ship.flags.docked)
        throw_trouble("Not docked");
    for (int step=0; step < 20; step++) {
        ai::detectEDState(DetectLevel::Buttons);
        if (st::guiFocus == GuiFocus::None) {
            for (int i = 0; i < 4; i++)
                kbd::send("UI_Up");
            kbd::send("UI_Down");
            kbd::send("UI_Select");
            if (st::dockedAt.stationType == "SpaceConstructionDepot" ||  st::dockedAt.stationType == "PlanetaryConstructionDepot")
                waitUiState("scr-constr", 6s);
            else
                waitUiState("scr-services", 6s);
            continue;
        }
        if (ai::uiState.match("scr-services")) {
            clickButton("til-market");
            waitUiState("scr-market:*", 5s);
            continue;
        }
        if (ai::uiState.match("scr-constr")) {
            return;
        }
        if (ai::uiState.match("scr-market:mod-buy")) {
            if (buy) {
                moveToWidget("lst-goods");
                return;
            }
            // go to sell mode
            clickButton("btn-to-sell");
            if (waitUiState("scr-market:mod-buy", 2s))
                kbd::send("UI_Right", 0, 300);
            continue;
        }
        if (ai::uiState.match("scr-market:mod-sell")) {
            if (!buy) {
                moveToWidget("lst-goods");
                return;
            }
            // go to sell mode
            clickButton("btn-to-buy");
            if (waitUiState("scr-market:mod-sell", 2s))
                kbd::send("UI_Right", 0, 300);
            continue;
        }
        if (ai::uiState.guiFocus == GuiFocus::GalaxyMap || ai::uiState.match("scr-galaxy")) {
            leaveScrGalaxy();
            continue;
        }
        kbd::send("UI_Back", 0, 1000);
    }
    throw_trouble("Cannot enter market");
}

void gotoLandingPad(bool refuel) {
    if (st::guiFocus != GuiFocus::None) {
        LOG(INFO) << "Going to landing pad.";
        for (int i = 0; i < 10 && st::guiFocus != GuiFocus::None; i++) {
            if (ai::uiState.guiFocus == GuiFocus::GalaxyMap || ai::uiState.match("scr-galaxy")) {
                leaveScrGalaxy();
                continue;
            }
            kbd::send("UI_Back", 0, 1000);
            ai::detectEDState(DetectLevel::Screen);
        }
    }
    ai::detectEDState(DetectLevel::Screen);
    if (st::guiFocus != GuiFocus::None)
        throw_trouble("Cannot get to landing pad");

    sleep(500);
    for (int i = 0; i < 4; i++)
        kbd::send("UI_Up");

    if (refuel || st::shipStats.fuelMain >= st::shipStats.fuelCapacityMain) {
        LOG(INFO) << "Refuel...";
        kbd::send("UI_Select", 0, 500); // refuel
        kbd::send("UI_Right");
        kbd::send("UI_Select", 0, 500); // repair
        kbd::send("UI_Right");
        kbd::send("UI_Select", 0, 500); // rearm
        kbd::send("UI_Left", 700);
    }
}


} // ai