//
// Created by mkizub on 28.06.2025.
//

#pragma once

#ifndef EDROBOT_UIMAINDIALOG_H
#define EDROBOT_UIMAINDIALOG_H

#include <shellapi.h>
#include <winlamb/dialog_main.h>
#include <winlamb/window_main.h>
#include <winlamb/label.h>
#include <winlamb/button.h>
#include <winlamb/checkbox.h>
#include <winlamb/menu.h>
#include <winlamb/icon.h>
#include "wl_svg_button.h"

#include "../ai/AIManager.h"
#include "UIShowCargo.h"

class UIMainDialog : public wl::window_main {
    friend class UIShowCargo;
public:
    UIMainDialog();

    bool show();
    bool hide(bool force);
    void initialize();
    void savePrefs();
    void relayout();
    void on_command_task_new();
    void on_command_task_stop();
    void on_command_task_resume();
    void on_command_task_repeat();
    void on_command_task_pause();
    void on_command_show_cargo();

    void update_curr_task();

    std::string startup_message;
    std::string latest_version;
    std::string latest_url;

    NOTIFYICONDATA mNotifyIconData;
    wl::font font;
    wl::menu menu;
    wl::label lbl_task;
    wl::label lbl_curr_task;
    wl::label lbl_task_status;
    wl::label lbl_status;
    wl::svg_button btn_stop_new;
    wl::svg_button btn_pause_resume;

    UINT_PTR mUpdateTimerId {};

    // for (re)layout
    int scaled_to_dpi {};
    bool initializing {};
    bool keepOnTop {};
};


#endif //EDROBOT_UIMAINDIALOG_H
