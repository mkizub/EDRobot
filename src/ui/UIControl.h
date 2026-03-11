//
// Created by mkizub on 11.03.2026.
//

#pragma once

#ifndef EDROBOT_UICONTROL_H
#define EDROBOT_UICONTROL_H

#include <shellapi.h>
#include <winlamb/window_control.h>
#include <winlamb/font.h>
#include <winlamb/scrollinfo.h>

class UIControl : public wl::window_control {
public:
    UIControl(bool scrollable);
    virtual ~UIControl() = default;
    void reset_scroll(bool scroll_to_top);
    void on_scrollbar(wl::params& params);

    virtual const wchar_t* title() const = 0;
    virtual void initialize() = 0;
    virtual void relayout() = 0;
    virtual bool need_timer_update() const { return false; }
    virtual void on_timer_update() {};

    wl::font font;
    wl::scrollinfo scrollinfo;

    const int ctrlIdBase = 0x8100;
    int panel_width = 0;
    int panel_height = 0;
    int params_height = 0;
    int scroll_pos = 0;
    int scroll_line_delta = 24;

    int scaled_to_dpi {};
};



#endif //EDROBOT_UICONTROL_H
