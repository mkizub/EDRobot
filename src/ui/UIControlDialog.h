//
// Created by mkizub on 11.03.2026.
//

#pragma once

#ifndef EDROBOT_UICONTROLDIALOG_H
#define EDROBOT_UICONTROLDIALOG_H

#include <shellapi.h>
#include <winlamb/window_main.h>
#include <winlamb/font.h>

class UIControl;

class UIControlDialog : public wl::window_main {
public:
    UIControlDialog(std::unique_ptr<UIControl>& control);
    virtual ~UIControlDialog();

    void initialize();
    void relayout();
    void update_control();

    wl::font font;
    int scaled_to_dpi {};
    UINT_PTR mUpdateTimerId {};

    std::unique_ptr<UIControl> control;

};


#endif //EDROBOT_UICONTROLDIALOG_H
