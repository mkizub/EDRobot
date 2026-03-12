//
// Created by mkizub on 11.03.2026.
//

#pragma once

#ifndef EDROBOT_UISHOWTASK_H
#define EDROBOT_UISHOWTASK_H

#include "UIMainDialog.h"
#include "UIControl.h"

class UIShowTask : public UIControl {
public:

    UIShowTask();
    UIShowTask(const std::string &message, std::string latest_version, std::string latest_url);
    ~UIShowTask() override;

    const wchar_t* title() const override { return L"EDRobit task status"; }
    void initialize() override;
    void relayout(bool scroll_to_top=false) override;
    bool need_timer_update() const override { return true; }
    void on_timer_update() override;
    void on_ctrl_edit(int id, WORD msg) {};
    bool validate() const { return true; };

    std::string startup_message;
    std::string latest_version;
    std::string latest_url;
    bool startup_shown {};

    wl::label lbl_task_status;
    wl::label lbl_status;
};


#endif //EDROBOT_UISHOWTASK_H
