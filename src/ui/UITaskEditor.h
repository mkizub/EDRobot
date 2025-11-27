//
// Created by mkizub on 24.11.2025.
//

#pragma once

#ifndef EDROBOT_UITASKEDITOR_H
#define EDROBOT_UITASKEDITOR_H

#include <shellapi.h>
#include <winlamb/combobox.h>
#include <winlamb/checkbox.h>
#include <winlamb/textbox.h>
#include <winlamb/label.h>
#include <winlamb/button.h>
#include <winlamb/font.h>
#include <winlamb/resizer.h>
#include <winlamb/scrollinfo.h>
#include <winlamb/window_control.h>

#include "../ai/AIManager.h"

namespace wl {

// Wrapper to native scrollbar control.
//class scrollbar final :
//        public wnd,
//        public _wli::base_native_ctrl_pubm<scrollbar>
//{
//private:
//    HWND                   _hWnd = nullptr;
//    _wli::base_native_ctrl _baseNativeCtrl{_hWnd};
//
//public:
//    // Wraps window style changes done by Get/SetWindowLongPtr.
//    _wli::styler<scrollbar> style{this};
//
//    scrollbar() noexcept :
//            wnd(_hWnd), base_native_ctrl_pubm(_baseNativeCtrl) { }
//
//    scrollbar(scrollbar&&) = default;
//    scrollbar& operator=(scrollbar&&) = default; // movable only
//};

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

        //on_message(WM_PAINT, [this](wl::params p)->LRESULT
        //{
        //    PAINTSTRUCT ps{};
        //    /*HDC hdc =*/ BeginPaint(hwnd(), &ps);
        //    EndPaint(hwnd(), &ps);
        //    return 0;
        //});

        //on_message(WM_ERASEBKGND, [](wl::params p)->LRESULT
        //{
        //    return 0;
        //});

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
        int hgap, vgap, vrow, width;
        int left, top;
        int scaled_to_dpi;
        bool update_font;
        HDWP hWinPosInfo;
    } layout;
};

}//namespace wl

class UITaskEditor;

class ParamCtrl {
public:
    ParamCtrl(UITaskEditor* ui);
    ParamCtrl(UITaskEditor* ui, ai::Param& param);
    virtual ~ParamCtrl();
    virtual void create();
    virtual void layout();
    virtual void on_ctrl_edit(HWND changed);
    virtual bool validate();
    virtual json5pp::value value();
    UITaskEditor* ui;
    std::wstring name;
    const json5pp::value meta;
    bool optional;
    std::wstring text;
    wl::label label;
};

class BoolCtrl : public ParamCtrl {
public:
    BoolCtrl(UITaskEditor* ui, ai::Param& param);
    ~BoolCtrl() override;
    void create() override;
    void layout() override;
    void on_ctrl_edit(HWND changed) override;
    bool validate() override;
    json5pp::value value() override;
    bool checked;
    wl::checkbox cb;
};

class EnumCtrl : public ParamCtrl {
public:
    EnumCtrl(UITaskEditor* ui, ai::Param& param);
    ~EnumCtrl() override;
    void create() override;
    void layout() override;
    void on_ctrl_edit(HWND changed) override;
    bool validate() override;
    json5pp::value value() override;

    int selected_index {-1};
    wl::combobox dl;

    struct IdName {
        std::string enum_id;
        std::wstring enum_name;
    };
    std::vector<IdName> entries;
};

class TextCtrl : public ParamCtrl {
public:
    TextCtrl(UITaskEditor* ui, ai::Param& param);
    ~TextCtrl() override;
    void create() override;
    void layout() override;
    void on_ctrl_edit(HWND changed) override;
    bool validate() override;
    json5pp::value value() override;

    ai::Param::Type type;
    wl::textbox tb;
};

class ArrayCtrl;
class ElemCtrl : public ParamCtrl {
public:
    ElemCtrl(UITaskEditor* ui, ArrayCtrl* arr_ctrl, int idx);
    ~ElemCtrl() override;
    void create() override;
    void layout() override;
    void on_ctrl_edit(HWND changed) override;
    bool validate() override;
    json5pp::value value() override;

    const ArrayCtrl* arr_ctrl;
    int index;
    std::unique_ptr<ParamCtrl> el_ctrl;
};

class ArrayCtrl : public ParamCtrl {
public:
    ArrayCtrl(UITaskEditor* ui, ai::Param& param);
    ~ArrayCtrl() override;
    void create() override;
    void layout() override;
    void on_ctrl_edit(HWND changed) override;
    bool validate() override;
    json5pp::value value() override;

    const json5pp::value& el_meta;
    const ai::Param::Type el_type;
    bool simple;
    std::vector<json5pp::value> arr_value;
    std::deque<std::unique_ptr<ElemCtrl>> controls;
};

class TaskCtrl : public ParamCtrl {
public:
    TaskCtrl(UITaskEditor* ui, ai::TaskTemplate& templ);
    TaskCtrl(UITaskEditor* ui, ai::Param& param);
    ~TaskCtrl() override;
    void create() override;
    void layout() override;
    void on_ctrl_edit(HWND changed) override;
    bool validate() override;
    json5pp::value value() override;

    bool toplevel;
    std::vector<const ai::TaskTemplate*> templates;
    ai::TaskTemplate templ;
    wl::textbox tb;
    wl::combobox dl;
    std::deque<std::unique_ptr<ParamCtrl>> controls;
};

class UITaskEditor : public wl::params_panel {
public:

    UITaskEditor();
    int nextID();
    void freeCtrl(wl::wnd& wnd);
    void beginControls();
    void endControls();
    void setTaskTemplate(ai::TaskTemplate& tt);
    ai::TaskTemplate makeTemplate();
    void clear();
    void relayout(bool scroll_to_top);

    std::unique_ptr<ParamCtrl> create_ctrl(ai::Param& param);
    void on_ctrl_change(wl::params& params);
    bool validate() const;
    bool on_ctrl_edit(int id);

    std::function<void(bool)> validate_callback;
    std::unique_ptr<TaskCtrl> task_ctrl;

private:
    bool initializing {};
    int nextTryId;
    std::array<bool, 100> usedIds;
};


#endif //EDROBOT_UITASKEDITOR_H
