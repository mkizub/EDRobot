//
// Created by mkizub on 09.02.2026.
//

#pragma once

#ifndef EDROBOT_WL_PANEL_H
#define EDROBOT_WL_PANEL_H

#include <shellapi.h>
#include <winlamb/font.h>
#include <winlamb/scrollinfo.h>
#include <winlamb/window_control.h>

namespace wl {

class params_panel :
        public window_control
{
public:
    params_panel() {
        setup.wndClassEx.lpszClassName = L"ParamsPanel";
        setup.wndClassEx.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
        setup.exStyle |= WS_EX_CLIENTEDGE | WS_EX_CONTROLPARENT;
        setup.style |= (WS_TABSTOP | WS_GROUP | WS_VSCROLL | WS_CHILD);

        layout = { 16, 4, 22, 100 };

        on_message(WM_VSCROLL, [this](wl::params params) {
            on_scrollbar(params);
            return 0;
        });
    }

    void reset_scroll(bool scroll_to_top) {
        if (scroll_to_top) {
            scrollinfo.set_flags(wl::scrollinfo::info::POS);
            scrollinfo.get_scroll(hwnd(), wl::scrollinfo::bar::VERT);
            if (scrollinfo.pos) {
                ScrollWindow(hwnd(), 0, -scrollinfo.pos, NULL, NULL);
                scroll_pos = 0;
            }
        }

        scrollinfo.set_flags(static_cast<wl::scrollinfo::info>(0x1F));
        scrollinfo.pos = scroll_pos;
        scrollinfo.minPos = 0;
        scrollinfo.maxPos = params_height;
        scrollinfo.trackPos = 0;
        scrollinfo.pageSz = panel_height;
        scrollinfo.set_scroll(hwnd(), wl::scrollinfo::bar::VERT);

        RedrawWindow(hwnd(), 0, 0, RDW_ERASE | RDW_INVALIDATE | RDW_ALLCHILDREN);
        InvalidateRect(hwnd(), nullptr, true);
        UpdateWindow(hwnd());
    }

    void on_scrollbar(wl::params& params) {
        int event = LOWORD(params.wParam);
        bool fullWindowUpdate = false;
        int delta = 0;

        switch (event) {
        case SB_TOP:
            reset_scroll(true);
            return;
        case SB_LINEDOWN:
            delta = layout.vrow + layout.vgap;
            if (scroll_pos + delta + panel_height > params_height)
                delta = panel_height - params_height - scroll_pos;
            break;
        case SB_LINEUP:
            delta = -layout.vrow - layout.vgap;
            if (scroll_pos + delta < 0)
                delta = -scroll_pos;
            break;
        case SB_THUMBTRACK:
            delta = HIWORD(params.wParam) - scroll_pos;
            break;
        case SB_THUMBPOSITION:
            fullWindowUpdate = true;
            delta = HIWORD(params.wParam) - scroll_pos;
            break;
        case SB_ENDSCROLL:
            scrollinfo.set_flags(wl::scrollinfo::info::POS);
            scrollinfo.get_scroll(hwnd(), wl::scrollinfo::bar::VERT);
            delta = scroll_pos - scrollinfo.pos;
            fullWindowUpdate = true;
            break;
        }
        if (delta) {
            scroll_pos += delta;
            scrollinfo.pos = scroll_pos;
            scrollinfo.set_flags(wl::scrollinfo::info::POS);
            scrollinfo.set_scroll(hwnd(), wl::scrollinfo::bar::VERT);
            ScrollWindow(hwnd(), 0, -delta, NULL, NULL);
            UpdateWindow(hwnd());
        }
        if (fullWindowUpdate) {
            scrollinfo.pos = scroll_pos;
            scrollinfo.set_flags(wl::scrollinfo::info::POS);
            scrollinfo.set_scroll(hwnd(), wl::scrollinfo::bar::VERT);
            RedrawWindow(hwnd(), 0, 0, RDW_ERASE | RDW_INVALIDATE | RDW_ALLCHILDREN);
            InvalidateRect(hwnd(), nullptr, true);
            UpdateWindow(hwnd());
        }
    }

    const int ctrlIdBase = 0x8100;

    wl::font font;
    wl::scrollinfo scrollinfo;
    int panel_width = 0;
    int panel_height = 0;
    int params_height = 0;
    int scroll_pos = 0;

    // for (re)layout
    struct {
        int hgap, vgap, xgap, vrow, width;
        int left, top;
        int scaled_to_dpi;
        bool update_font;
        HDWP hWinPosInfo;
    } layout;
};

}//namespace wl

#endif //EDROBOT_WL_PANEL_H
