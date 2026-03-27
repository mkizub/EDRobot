//
// Created by mkizub on 11.03.2026.
//

#include "../pch.h"

#include "UIControl.h"
#include "UILayout.h"

void loCreateFont(wl::font& font, UINT uiDpi, UINT uiPercent, LONG weight) {
    NONCLIENTMETRICS ncm{};
    ncm.cbSize = sizeof(ncm);
    SystemParametersInfoForDpi(SPI_GETNONCLIENTMETRICS, ncm.cbSize, &ncm, 0, MulDiv(uiDpi, uiPercent, 100));
    LOGFONT lf = ncm.lfMessageFont;
    if (weight >= FW_THIN && weight <= FW_HEAVY)
        lf.lfWeight = weight;
    font.create(lf);
}

UILayout::UILayout(int uiDpi, int uiPercent, RECT& rect) {
    hgap = MulDiv(LO_H_GAP, uiDpi * uiPercent, 100 * USER_DEFAULT_SCREEN_DPI);
    vgap = MulDiv(LO_V_GAP, uiDpi * uiPercent, 100 * USER_DEFAULT_SCREEN_DPI);
    xgap = MulDiv(LO_X_GAP, uiDpi * uiPercent, 100 * USER_DEFAULT_SCREEN_DPI);
    vrow = MulDiv(LO_V_ROW, uiDpi * uiPercent, 100 * USER_DEFAULT_SCREEN_DPI);
    icsz = MulDiv(LO_ICN_S, uiDpi * uiPercent, 100 * USER_DEFAULT_SCREEN_DPI);
    btnh = MulDiv(LO_BTN_H, uiDpi * uiPercent, 100 * USER_DEFAULT_SCREEN_DPI);
    btnw = MulDiv(LO_BTN_W, uiDpi * uiPercent, 100 * USER_DEFAULT_SCREEN_DPI);
    txt6w = MulDiv(LO_TXT_6_W, uiDpi * uiPercent, 100 * USER_DEFAULT_SCREEN_DPI);
    txt20w = MulDiv(LO_TXT_20_W, uiDpi * uiPercent, 100 * USER_DEFAULT_SCREEN_DPI);
    txt50w = MulDiv(LO_TXT_50_W, uiDpi * uiPercent, 100 * USER_DEFAULT_SCREEN_DPI);
    border = MulDiv(LO_DLG_BORDER, uiDpi * uiPercent, 100 * USER_DEFAULT_SCREEN_DPI);
    width = rect.right - rect.left;
    height = rect.bottom - rect.top;
    left = rect.left;
    top = rect.top;
    font = nullptr;
    wpi = nullptr;
}

UIControl::UIControl(bool scrollable) {
    setup.wndClassEx.lpszClassName = L"EDRobotControl";
    setup.wndClassEx.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
    setup.exStyle |= WS_EX_CONTROLPARENT; // | WS_EX_CLIENTEDGE;
    setup.style |= WS_TABSTOP | WS_GROUP; //  | WS_BORDER

    if (scrollable) {
        setup.style |= WS_VSCROLL;
        on_message(WM_VSCROLL, [this](wl::params params) {
            on_scrollbar(params);
            return 0;
        });
    }

    on_message(WM_CREATE, [this](wl::params p) -> INT_PTR {
        initialize();
        return 0;
    });
    on_message(WM_DPICHANGED, [this](wl::params params) {
        relayout();
        return 0;
    });
    on_message(WM_SIZE, [this](wl::params params) {
        relayout();
        return 0;
    });

    on_message(WM_MENUCOMMAND, [](wl::params params) {
        int idx = params.wParam;
        HMENU hMenu = (HMENU)params.lParam;
        MENUINFO mi {sizeof(MENUINFO), MIM_MENUDATA|MIM_HELPID|MIM_STYLE};
        BOOL ok = GetMenuInfo(hMenu, &mi);
        if (!ok || mi.dwContextHelpID != wl::popup_menu_id || !mi.dwMenuData)
            return 0;
        auto* pm = reinterpret_cast<wl::popup_menu*>(mi.dwMenuData);
        pm->apply(idx);
        return 0;
    });
    on_message(WM_MENUSELECT, [this](wl::params params) {
        UINT idx = LOWORD(params.wParam);
        UINT flags = HIWORD(params.wParam);
        HMENU hMenu = (HMENU)params.lParam;
        if ((flags == 0xFFFF && hMenu == NULL) || flags & MF_SEPARATOR) {
            // No menu item selected (menu closed).
            tooltip.hide();
            return 0;
        }
        MENUINFO mi {sizeof(MENUINFO), MIM_MENUDATA|MIM_HELPID|MIM_STYLE};
        BOOL ok = GetMenuInfo(hMenu, &mi);
        if (!ok || mi.dwContextHelpID != wl::popup_menu_id || !mi.dwMenuData) {
            tooltip.hide();
            return 0;
        }
        auto* pm = reinterpret_cast<wl::popup_menu*>(mi.dwMenuData);
        auto tt = pm->tooltip(idx);
        if (tt.empty()) {
            tooltip.hide();
            return 0;
        }
        tooltip.create(this->hwnd()).show(tt);
        return 0;
    });
    for (int i=0; i < usedIds.size(); i++) {
        on_command(ctrlIdBase + i, [this](wl::params p) {
            on_ctrl_change(p);
            return 0;
        });
    }
}

void UIControl::reset_scroll(bool scroll_to_top) {
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

void UIControl::on_scrollbar(wl::params& params) {
    int event = LOWORD(params.wParam);
    bool fullWindowUpdate = false;
    int delta = 0;

    switch (event) {
    case SB_TOP:
        reset_scroll(true);
        return;
    case SB_LINEDOWN:
        delta = scroll_line_delta;
        if (scroll_pos + delta + panel_height > params_height)
            delta = panel_height - params_height - scroll_pos;
        break;
    case SB_LINEUP:
        delta = -scroll_line_delta;
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

int UIControl::nextID() {
    assert (initializing);
    for (int i=nextTryId; i < usedIds.size(); i++) {
        if (!usedIds[i]) {
            usedIds[i] = true;
            nextTryId = i+1;
            return ctrlIdBase + i;
        }
    }
    nextTryId = 0;
    for (int i=0; i < usedIds.size(); i++) {
        if (!usedIds[i]) {
            usedIds[i] = true;
            nextTryId = i+1;
            return ctrlIdBase + i;
        }
    }
    return ctrlIdBase;
}

void UIControl::freeCtrl(wl::wnd& w) {
    if (!w.hwnd())
        return;
    int id = GetDlgCtrlID(w.hwnd());
    DestroyWindow(w.hwnd());
    if (id < ctrlIdBase || id - ctrlIdBase >= usedIds.size())
        return;
    id -= ctrlIdBase;
    usedIds[id] = false;
    nextTryId = 0;
}

void UIControl::beginControls() {
    SendMessage(hwnd(), WM_SETREDRAW, FALSE, 0);
    initializing = true;
}
void UIControl::endControls() {
    initializing = false;
    SendMessage(hwnd(), WM_SETREDRAW, TRUE, 0);
}

void UIControl::on_ctrl_change(wl::params& p) {
    if (initializing)
        return;
    auto hw = HIWORD(p.wParam);
    if (hw != EN_CHANGE && hw != BN_CLICKED && hw != CBN_SELENDOK && hw != CBN_EDITCHANGE && hw != EN_SETFOCUS)
        return;
    int id = LOWORD(p.wParam);
    if (id < ctrlIdBase)
        return;
    on_ctrl_edit(id, hw);
    validate();
    on_update();
}
