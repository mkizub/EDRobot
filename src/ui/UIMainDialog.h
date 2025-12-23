//
// Created by mkizub on 28.06.2025.
//

#pragma once

#ifndef EDROBOT_UIMAINDIALOG_H
#define EDROBOT_UIMAINDIALOG_H

#include <shellapi.h>
#include <winlamb/dialog_main.h>
#include <winlamb/label.h>
#include <winlamb/button.h>
#include <winlamb/checkbox.h>

#include "../ai/AIManager.h"

class UIMainDialog : public wl::dialog_main {
public:
    UIMainDialog();

    bool show();
    bool hide(bool force);
    void initialize();
    void relayout();
    void createFont(int dpi, int scale_percent);
    void on_command_stop_new();
    void on_command_pause_resume();

    void update_curr_task();

    NOTIFYICONDATA mNotifyIconData;
    wl::font font;
    wl::label lbl_task;
    wl::label lbl_curr_task;
    wl::label lbl_task_status;
    wl::label lbl_status;
    wl::checkbox cb_keep_on_top;
    wl::button btn_stop_new;
    wl::button btn_pause_resume;
    wl::button btn_ok;
    wl::button btn_watch;
    wl::button btn_exit;

    UINT_PTR mUpdateTimerId {};

    // for (re)layout
    int scaled_to_dpi {};
};


#endif //EDROBOT_UIMAINDIALOG_H
