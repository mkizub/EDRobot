//
// Created by mkizub on 27.06.2025.
//

#pragma once

#ifndef EDROBOT_UIADDTASK_H
#define EDROBOT_UIADDTASK_H

#include <shellapi.h>
#include <winlamb/dialog_modal.h>
#include <winlamb/combobox.h>
#include <winlamb/checkbox.h>
#include <winlamb/textbox.h>
#include <winlamb/label.h>
#include <winlamb/button.h>
#include <winlamb/resizer.h>

#include "../ai/AIManager.h"

class UIAddTask : public wl::dialog_modal {
public:
    UIAddTask();

    int initialize(wl::params& params);
    int on_template_selected(wl::params& params);
    int on_ctrl_change(wl::params& params);

    struct ParamCtrl {
        ParamCtrl(ai::Param& param, int id);
        ParamCtrl(ParamCtrl&&) = default;
        ~ParamCtrl();
        ai::Param& param;
        const int id;
        std::wstring text;
        wl::checkbox cb;
        wl::combobox dl;
        wl::textbox tb;
        wl::label label;
    };
private:
    void on_ctrl_edit(ParamCtrl& ctrl);
    void add_ctrl(ai::Param& param, int& id, int &x, int &y, int w);
    bool validate(ParamCtrl& ctrl);

    ai::AIManager* aiManager;
    std::vector<const ai::TaskTemplate*> templates;

    wl::resizer  layoutResizer;
    wl::combobox cb_tasks;
    wl::button btn_ok;
    wl::button btn_cancel;

    const int ctrlId = 0x8100;
    ai::TaskTemplate* curr_templ {nullptr};
    std::map<std::string,ai::TaskTemplate> templMap;
    std::deque<ParamCtrl> templ_controls;
};


#endif //EDROBOT_UIADDTASK_H
