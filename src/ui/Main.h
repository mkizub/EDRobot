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

    int initialize(wl::params& params);
    int curr_task_command(wl::params& params);

    void update_curr_task();

    ai::AIManager* aiManager;

    NOTIFYICONDATA mNotifyIconData;
    wl::label lbl_curr_task;
    wl::button btn_curr_task;
};


#endif //EDROBOT_MAIN_H
