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

class UIControl;
class UIControlDialog;

class UIMainDialog : public wl::window_main {
    friend class UIShowCargo;
public:
    UIMainDialog();

    bool show_startup(const std::string &message, const std::string& latest_version, const std::string& latest_url);
    bool show_task_status();
    bool show();
    bool hide(bool force_close);
    bool toggle();
    void updateCommander();
    void initialize();
    void savePrefs();
    void relayout();
    void on_command_task_new();
    void on_command_task_stop();
    void on_command_task_resume();
    void on_command_task_repeat();
    void on_command_task_pause();
    void on_command_show_detach();
    void on_command_show_task();
    void on_command_edit_task();
    void on_command_show_cargo();
    void on_command_show_bookmarks();
    void on_command_edit_star_system();

    void update_curr_task();

    NOTIFYICONDATA mNotifyIconData;
    wl::font font;
    wl::menu menu;
    wl::label lbl_task;
    wl::label lbl_curr_task;
    wl::svg_button btn_stop_new;
    wl::svg_button btn_pause_resume;

    std::unique_ptr<UIControl> control;
    std::vector<std::unique_ptr<UIControlDialog>> detached;

    UINT_PTR mUpdateTimerId {};

    int scaled_to_dpi;
    bool initializing {};
    bool keepOnTop {};
    bool minimizeToTray {};

};


#endif //EDROBOT_UIMAINDIALOG_H
