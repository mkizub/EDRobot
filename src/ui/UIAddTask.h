//
// Created by mkizub on 27.06.2025.
//

#pragma once

#ifndef EDROBOT_UIADDTASK_H
#define EDROBOT_UIADDTASK_H

#include <shellapi.h>
#include <shellscalingapi.h>
#include <winlamb/dialog_modal.h>
#include <winlamb/combobox.h>
#include <winlamb/checkbox.h>
#include <winlamb/textbox.h>
#include <winlamb/label.h>
#include <winlamb/button.h>
#include <winlamb/font.h>
#include <winlamb/resizer.h>
#include <winlamb/scrollinfo.h>

#include "../ai/AIManager.h"

#include "UITaskEditor.h"

class UIAddTask : public wl::dialog_modal {
public:
    UIAddTask();

    void initialize();
    void relayout();
    void init_templ_list(std::string select={});
    void on_template_run();
    void on_template_save();
    void on_template_delete();
    void on_template_selected();
    void validate_callback(bool valid);

private:
    std::deque<ai::TaskTemplate> templates;

    wl::font font;
    wl::combobox cb_tasks;
    wl::button btn_run;
    wl::button btn_save;
    wl::button btn_del;
    UITaskEditor taskEditor;
};

#endif //EDROBOT_UIADDTASK_H
