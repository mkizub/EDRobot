//
// Created by mkizub on 28.06.2025.
//

#pragma once

#ifndef EDROBOT_MAIN_H
#define EDROBOT_MAIN_H

#include <shellapi.h>
#include <winlamb/dialog_main.h>
#include <winlamb/label.h>
#include <winlamb/button.h>

#include "../ai/AIManager.h"

class Main : public wl::dialog_main {
public:
    Main();

    bool show();
    bool hide();
    int initialize(wl::params& params);
    int on_command_stop_new(wl::params& params);
    int on_command_pause_resume(wl::params& params);

    void update_curr_task();

    ai::AIManager* aiManager;

    NOTIFYICONDATA mNotifyIconData;
    wl::label lbl_curr_task;
    wl::label lbl_task_status;
    wl::button btn_stop_new;
    wl::button btn_pause_resume;
    wl::button btn_watch;

    UINT_PTR mUpdateTimerId {};
};


#endif //EDROBOT_MAIN_H
