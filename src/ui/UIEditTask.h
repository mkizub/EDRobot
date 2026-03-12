//
// Created by mkizub on 27.06.2025.
//

#pragma once

#ifndef EDROBOT_UIEDITTASK_H
#define EDROBOT_UIEDITTASK_H

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

#include "wl_svg_button.h"

#include "UIControl.h"
#include "../ai/AIManager.h"

class ParamCtrl;
class TaskCtrl;

class UIEditTask : public UIControl {
public:
    UIEditTask();
    ~UIEditTask();

    const wchar_t* title() const override { return L"EDRobot task editor"; };
    void initialize() override;
    void relayout(bool scroll_to_top=false) override;
    void on_ctrl_edit(int id, WORD msg) override;
    bool validate() const override;
    void clear();

    void init_templ_list(std::string select={});
    void on_template_run();
    void on_template_save();
    void on_template_delete();
    void on_template_selected();
    void validate_callback(bool valid);

    ai::TaskTemplate makeTemplate();
    std::unique_ptr<ParamCtrl> create_ctrl(ai::Param& param);

private:
    std::deque<ai::TaskTemplate> templates;

    wl::combobox cb_tasks;
    wl::svg_button btn_run;
    wl::svg_button btn_save;
    wl::svg_button btn_del;
    std::unique_ptr<TaskCtrl> task_ctrl;
};

#endif //EDROBOT_UIEDITTASK_H
