//
// Created by mkizub on 27.12.2025.
//

#include "../pch.h"

#include "AIManager.h"
#include "ContactsTasks.h"
#include "AIUtils.h"
#include "../widget/List.h"
#include "../Keyboard.h"

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
        clickButton("btn-acquire-res", 0.5);

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
                mouseMoveTo(pos, 0.5);
                kbd::sendMouseClick(pos, 100,500);
                sleep(1000);
                clickWidget("scr-contacts:mod-contact:dlg-power-play:dlg-acquire-res:spn-amount", 100, 500, 0.5);
                kbd::send("UI_Right", 3000);
                //kbd::send("UI_Right");
                clickWidget("scr-contacts:mod-contact:dlg-power-play:dlg-acquire-res:btn-commit", 100, 500, 0.5);
                if (waitMarketEvent(4s))
                    acquiredCount += Cfg.marketEvent->data.at("Count",0).as_integer();
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
        clickButton("btn-deliver-res", 0.5);

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
                mouseMoveTo(pos, 0.5);
                kbd::sendMouseClick(pos, 100,500);
                sleep(1000);
                //clickWidget("scr-contacts:mod-contact:dlg-power-play:dlg-deliver-res:spn-amount", 100, 500, 0.5);
                //kbd::send("UI_Right", 3000);
                clickWidget("scr-contacts:mod-contact:dlg-power-play:dlg-deliver-res:btn-commit", 100, 500, 0.5);
                if (waitMarketEvent(4s))
                    deliveredCount += Cfg.marketEvent->data.at("Count",0).as_integer();
            }
        }
    }
    return deliveredCount > 0;
}
