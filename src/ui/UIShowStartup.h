//
// Created by mkizub on 22.03.2026.
//

#pragma once

#ifndef EDROBOT_UISHOWSTARTUP_H
#define EDROBOT_UISHOWSTARTUP_H

#include "UIMainDialog.h"
#include "UIControl.h"


class UIShowStartup : public UIControl {
public:

    UIShowStartup();
    UIShowStartup(const std::string &message, std::string latest_version, std::string latest_url);
    ~UIShowStartup() override;

    const wchar_t* title() const override { return L"EDRobit startup message"; }
    void initialize() override;
    void relayout(bool scroll_to_top=false) override;
    void on_ctrl_edit(int id, WORD msg) {};
    bool validate() const { return true; };

    std::string startup_message;
    std::string latest_version;
    std::string latest_url;

    wl::label lbl_message;
    wl::label lbl_version;
};


#endif //EDROBOT_UISHOWSTARTUP_H
