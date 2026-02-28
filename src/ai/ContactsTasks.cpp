//
// Created by mkizub on 27.12.2025.
//

#include "../pch.h"

#include "AIManager.h"
#include "ContactsTasks.h"
#include "AIUtils.h"
#include "../widget/List.h"
#include "../Keyboard.h"

ai::TaskResurrect::TaskResurrect(const TaskTemplate& templ)
    : Task(templ)
{
    assert (templ.id == ED_TASK_RESURRECT);
}

bool ai::TaskResurrect::run() {
    if (!st::isDead)
        return true;
    sleep(3000);
    for (int retry=0; retry < 5; retry++) {
        if (!st::isDead)
            return true;
        cv::Mat grayImage;
        ai::detectEDStateGrayIm(DetectLevel::Buttons, grayImage);
        if (ai::uiState.match("scr-death:mod-report")) {
            status = REPORT;
            if (ai::uiState.focused_name() == "btn-next")
                kbd::send("UI_Select", 100, 3000);
            else
                kbd::send("UI_Right", 100, 1000);
            continue;
        }
        if (ai::uiState.match("scr-death:mod-deploy")) {
            status = DEPLOY;
            kbd::send("UI_Right");
            kbd::send("UI_Right");
            kbd::send("UI_Right");
            kbd::send("UI_Right");
            kbd::send("UI_Select", 100, 250);
            kbd::send("UI_Up", 0, 250);
            kbd::send("UI_Select", 100, 250);
            sleep(15000);
            continue;
        }
        // check black screen
        cv::Rect rect {1110, 480, 150, 120};
        rect = ai::rEnv.cvtReferenceToCaptured(rect);
        cv::Mat blackImage;
        cv::threshold(grayImage(rect), blackImage, 10, 255, cv::THRESH_BINARY);
        int count = cv::countNonZero(blackImage);
        if (count == 0) {
            kbd::send("UI_Select", 0, 1000);
            sleep(10000);
            continue;
        }
        sleep(10000);
    }
    throw_failed_("Unknown death screen");
}

std::string ai::TaskResurrect::getStatus() {
    switch (status) {
    case DONE:
    case READY:
        return {};
    case REPORT:
        return _gt("Death report");
    case DEPLOY:
        return _gt("Deploing ship");
    }
    return {};
}


ai::TaskAcquirePPC::TaskAcquirePPC(const ai::TaskTemplate &templ)
    : Task(templ)
{
    assert (templ.id == ED_TASK_ACQUIRE_PPC);
    for (auto& p : templ.params) {
        if (p.id == "commodity")
            mCommodity = Cfg.getCommodityById(p.as_string());
    }
}

bool ai::TaskAcquirePPC::run() {
    int acquiredCount = 0;
    for (int retry=0; retry < 3; retry++) {
        gotoContactsScreen("dlg-power-play");
        clickButton("btn-acquire-res", 0.3);

        ai::detectEDState(DetectLevel::ListRows);
        for (auto &cr: ai::rEnv.classified) {
            if (cr.cdt != ClsDetType::ListRow)
                continue;
            if (cr.u.lrow.commodity == mCommodity) {
                auto* list = cr.u.lrow.list;
                auto& tab = list->getTab("acquire");
                cv::Rect button = cr.detectedRect;
                button.x += tab.tab_left;
                button.width = tab.tab_right - tab.tab_left;
                cv::Point pos = (button.tl() + button.br()) / 2;
                mouseMoveTo(pos, 0.3);
                kbd::sendMouseClick(pos, 100,500);
                sleep(1000);
                ai::detectEDState(DetectLevel::Buttons);
                clickWidget("scr-contacts:mod-contact:dlg-power-play:dlg-acquire-res:spn-amount", 100, 500, 0.2);
                kbd::send("UI_Right", 3000);
                //kbd::send("UI_Right");
                ai::detectEDState(DetectLevel::Buttons);
                clickWidget("scr-contacts:mod-contact:dlg-power-play:dlg-acquire-res:btn-commit", 100, 500, 0.2);
                if (waitMarketEvent(4s))
                    acquiredCount += Cfg.marketEvent->data["Count"].as_int_or();
            }
        }
    }
    return acquiredCount > 0;
}

ai::TaskDeliverPPC::TaskDeliverPPC(const ai::TaskTemplate &templ)
    : Task(templ)
{
    assert (templ.id == ED_TASK_DELIVER_PPC);
    for (auto& p : templ.params) {
        if (p.id == "commodity")
            mCommodity = Cfg.getCommodityById(p.as_string());
    }
}

bool ai::TaskDeliverPPC::run() {
    int haveCount = 0;
    spShipCargo shipCargo = st::currentCargo;
    for (Commodity* commodity: Cfg.getAllKnownCommodities()) {
        if (commodity->category->intId != 16)
            continue;
        haveCount += commodity->ship.count;
    }
    if (!haveCount)
        return true;

    int deliveredCount = 0;
    for (int retry=0; retry < 3; retry++) {
        gotoContactsScreen("dlg-power-play");
        clickButton("btn-deliver-res", 0.3);

        ai::detectEDState(DetectLevel::ListRows);
        for (auto &cr: ai::rEnv.classified) {
            if (cr.cdt != ClsDetType::ListRow)
                continue;
            if (cr.u.lrow.commodity == mCommodity) {
                auto* list = cr.u.lrow.list;
                auto& tab = list->getTab("deliver");
                cv::Rect button = cr.detectedRect;
                button.x += tab.tab_left;
                button.width = tab.tab_right - tab.tab_left;
                cv::Point pos = (button.tl() + button.br()) / 2;
                mouseMoveTo(pos, 0.3);
                kbd::sendMouseClick(pos, 100,500);
                sleep(1000);
                //ai::detectEDState(DetectLevel::Buttons);
                //clickWidget("scr-contacts:mod-contact:dlg-power-play:dlg-deliver-res:spn-amount", 100, 500, 0.2);
                //kbd::send("UI_Right", 3000);
                ai::detectEDState(DetectLevel::Buttons);
                clickWidget("scr-contacts:mod-contact:dlg-power-play:dlg-deliver-res:btn-commit", 100, 500, 0.2);
                if (waitMarketEvent(4s))
                    deliveredCount += Cfg.marketEvent->data["Count"].as_int_or();
            }
        }
    }
    return deliveredCount > 0;
}
