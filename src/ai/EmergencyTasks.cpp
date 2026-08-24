//
// Created by mkizub on 15.08.2026.
//

#include "../pch.h"

#include "AIManager.h"
#include "EmergencyTasks.h"
#include "AIUtils.h"
#include "../widget/List.h"
#include "../Keyboard.h"

namespace ai {

TaskDebugEmergency::TaskDebugEmergency(const ai::TaskTemplate &templ)
        : Task(templ)
{
    assert (templ.id == ED_TASK_DEBUG_EMERGENCY);
    for (auto& p : templ.params) {
        if (p.id == "test")
            test = p.as_string();
    }
}

bool TaskDebugEmergency::run() {
    if (status == DONE)
        return true;
    status = DONE;
    if (test == "Relogin") {
        TaskTemplate tt {ED_TASK_RELOGIN, _lc("Relogin"), [](const TaskTemplate &templ) { return new TaskRelogin(templ); }};
        run_sub_step(new TaskRelogin(tt));
    }
    else if (test == "RebootRepair") {
        TaskTemplate tt {ED_TASK_REBOOT_REPAIR, _lc("Reboot and Repair"), [](const TaskTemplate &templ) { return new TaskRebootRepair(templ); }};
        run_sub_step(new TaskRebootRepair(tt));
    }
    else if (test == "Resurrect") {
        TaskTemplate tt {ED_TASK_RESURRECT, _lc("Resurrect"), [](const TaskTemplate &templ) { return new TaskResurrect(templ); }};
        run_sub_step(new TaskResurrect(tt));
    }
    return true;
}

ai::TaskRelogin::TaskRelogin(const TaskTemplate& templ)
        : BaseEmergencyTask(templ)
{
    assert (templ.id == ED_TASK_RELOGIN);
}

bool ai::TaskRelogin::run() {
    if (status == DONE)
        return true;
    ExpectSceeenLocker expectAutopilot("emergency");
    for (;;) {
        ai::detectEDState(DetectLevel::Buttons);
        if (ai::uiState.match("scr-black")) {
            status = BLACK;
            sleep(3000);
            if (st::isDead) {
                TaskTemplate tt {ED_TASK_RESURRECT, _lc("Resurrect"), [](const TaskTemplate &templ) { return new TaskResurrect(templ); }};
                run_sub_step(new TaskResurrect(tt));
            } else {
                kbd::send("UI_Select", 0, 1000);
                sleep(10000);
            }
            continue;
        }
        if (ai::uiState.match("scr-game-menu:mod-pause")) {
            status = LOGOUT;
            if (ai::uiState.focused_name() != "btn-exit") {
                kbd::send("UI_Up", 0, 250);
                continue;
            }
            notify_info("Logout");
            kbd::send("UI_Select", 0, 3000); // exit to choise: main menu or desktop
            continue;
        }
        if (ai::uiState.match("scr-menu-choise:mod-logout")) {
            status = LOGOUT;
            if (isTileDisabled()) {
                sleep(1000);
                continue;
            }
            if (ai::uiState.focused_name() == "btn-exit") {
                kbd::send("UI_Up", 0, 250);
                continue;
            }
            if (ai::uiState.focused_name() != "btn-tile-main") {
                kbd::send("UI_Left", 0, 250);
                continue;
            }
            notify_info("Logout to main menu");
            kbd::send("UI_Select", 0, 15000); // login, wait for menu init
            continue;
        }
        if (ai::uiState.match("scr-game-menu:mod-main")) {
            status = LOGIN;
            if (ai::uiState.focused_name() == "btn-exit") {
                kbd::send("UI_Down", 0, 250);
                continue;
            }
            if (ai::uiState.focused_name() != "btn-play") {
                kbd::send("UI_Up", 0, 250);
                continue;
            }
            notify_info("Login");
            kbd::send("UI_Select", 0, 3000); // login to select game mode
            continue;
        }
        if (ai::uiState.match("scr-menu-choise:mod-login")) {
            status = LOGOUT;
            if (isTileDisabled()) {
                sleep(1000);
                continue;
            }
            if (ai::uiState.focused_name() == "btn-exit") {
                kbd::send("UI_Up", 0, 250);
                continue;
            }
            if (ai::uiState.focused_name() != "btn-tile-solo") {
                kbd::send("UI_Right", 0, 250);
                continue;
            }
            notify_info("Login to Solo...");
            kbd::send("UI_Select", 0, 15000); // wait game init
            break;
        }
        // not in game menu or black screen, pause game
        kbd::send("Pause", 0, 1000);
    }

    status = DONE;
    if (st::isNeedRebootRepair) {
        TaskTemplate tt {ED_TASK_REBOOT_REPAIR, _lc("Reboot and Repair"), [](const TaskTemplate &templ) { return new TaskRebootRepair(templ); }};
        run_sub_step(new TaskRebootRepair(tt));
    }
    throw_trouble("Finished re-login");
}

bool TaskRelogin::isTileDisabled() {
    for (auto btn : ai::rEnv.classified) {
        if (btn.cdt != ClsDetType::Widget || btn.u.widg.widget->tp != widget::WidgetType::Button)
            continue;
        if (!btn.u.widg.widget->name.starts_with("btn-tile-"))
            continue;
        if (!(btn.u.widg.ws == WState::Normal || btn.u.widg.ws == WState::Focused))
            return true;
    }
    return false;
}

std::string ai::TaskRelogin::getStatus() {
    switch (status) {
    case DONE:
    case READY:
        return {};
    case BLACK:
        return _gt("Black screen");
    case LOGOUT:
        return _gt("Logout");
    case LOGIN:
        return _gt("Login");
    }
    return {};
}


ai::TaskRebootRepair::TaskRebootRepair(const TaskTemplate& templ)
    : BaseEmergencyTask(templ)
{
    assert (templ.id == ED_TASK_REBOOT_REPAIR);
}

bool ai::TaskRebootRepair::run() {
    if (status == DONE)
        return true;
    status = PREPARE;
    timer = utc_timer(10s);
    sleep(3000);
    gotoShipPage("mod-ship-info", true);
    kbd::send("UI_Left");
    kbd::send("UI_Left");
    kbd::send("UI_Left");
    kbd::send("UI_Up");
    kbd::send("UI_Up");
    kbd::send("UI_Up");
    kbd::send("UI_Up");
    kbd::send("UI_Up");
    kbd::send("UI_Right");
    kbd::send("UI_Right");
    kbd::send("UI_Right");
    kbd::send("UI_Up");
    kbd::send("UI_Up");
    kbd::send("UI_Up");
    kbd::send("UI_Up");
    kbd::send("UI_Up");
    kbd::send("UI_Select", 150);
    sleep(5000);
    kbd::send("UI_Down", 150, 500);
    status = REPAIR;
    kbd::send("UI_Select", 150);
    timer = utc_timer(25s);
    sleep(25000);
    status = DONE;
    st::isNeedRebootRepair = false;
    throw_trouble("Finished reboot repair");
}

std::string ai::TaskRebootRepair::getStatus() {
    switch (status) {
    case DONE:
    case READY:
        return {};
    case PREPARE:
        return lc_format("Prepare to reboot: {}", timer.left());
    case REPAIR:
        return lc_format("Reboot and repair: {}", timer.left());
    }
    return {};
}


ai::TaskResurrect::TaskResurrect(const TaskTemplate& templ)
        : BaseEmergencyTask(templ)
{
    assert (templ.id == ED_TASK_RESURRECT);
}

bool ai::TaskResurrect::run() {
    if (status == DONE)
        return true;
    if (!st::isDead)
        return true;
    sleep(3000);
    // detect
    for (int retry=0; retry < 5; retry++) {
        if (!st::isDead) {
            status = DONE;
            throw_trouble("Finished resurrect");
        }
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
        if (count == 0)
            kbd::send("UI_Select", 0, 1000);
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

} // namespace ai
