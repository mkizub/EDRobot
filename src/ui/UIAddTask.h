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

namespace wl {

// Wrapper to native scrollbar control.
class scrollbar final :
        public wnd,
        public _wli::base_native_ctrl_pubm<scrollbar>
{
private:
    HWND                   _hWnd = nullptr;
    _wli::base_native_ctrl _baseNativeCtrl{_hWnd};

public:
    // Wraps window style changes done by Get/SetWindowLongPtr.
    _wli::styler<scrollbar> style{this};

    scrollbar() noexcept :
            wnd(_hWnd), base_native_ctrl_pubm(_baseNativeCtrl) { }

    scrollbar(scrollbar&&) = default;
    scrollbar& operator=(scrollbar&&) = default; // movable only
};

}//namespace wl



class UIAddTask : public wl::dialog_modal {
public:
    UIAddTask();

    int initialize(wl::params& params);
    int on_template_selected(wl::params& params);
    int on_ctrl_change(wl::params& params);
    void reset_scroll();
    void on_scrollbar(wl::params& params);

    struct PRef {
        ai::TaskTemplate& templ;
        ai::Param& param;
        int idx;

        ai::Param::Type type() const;
        bool empty() const;
        bool valid() const;
        std::wstring name() const;
        std::wstring placeholder() const;
        const json5pp::value& meta() const;
        const json5pp::value& value() const;
        bool set(const json5pp::value& value);
        bool as_boolean() const;
        const std::string as_string() const;
    };
    class ParamCtrl : public std::enable_shared_from_this<ParamCtrl> {
    public:
        ParamCtrl(PRef ref, int id);
        virtual ~ParamCtrl();
        virtual void on_ctrl_edit();
        PRef pref;
        const int id;
        std::wstring text;
        wl::label label;
    };
    class BoolCtrl : public ParamCtrl {
    public:
        BoolCtrl(PRef ref, int id) : ParamCtrl(ref, id) {}
        ~BoolCtrl() override;
        void on_ctrl_edit() override;
        wl::checkbox cb;
    };
    class EnumCtrl : public ParamCtrl {
    public:
        EnumCtrl(PRef ref, int id) : ParamCtrl(ref, id) {}
        ~EnumCtrl() override;
        void on_ctrl_edit() override;
        wl::combobox dl;
        int index {-1};
    };
    class TextCtrl : public ParamCtrl {
    public:
        TextCtrl(PRef ref, int id) : ParamCtrl(ref, id) {}
        ~TextCtrl() override;
        void on_ctrl_edit() override;
        wl::textbox tb;
    };
    class TaskCtrl : public ParamCtrl {
    public:
        TaskCtrl(PRef ref, int id);
        ~TaskCtrl() override;
        ai::TaskTemplate sub_templ;
    };
    typedef std::shared_ptr<ParamCtrl> spParamCtrl;
private:
    void add_ctrl(PRef ref, int& id, int left, int &top, int w, int h);
    bool validate(spParamCtrl ctrl);

    float mUiScale = 1.0f;
    int mPanelHeight = 0;
    int mPanelWidth = 0;
    int mParamsHeight = 0;
    int mScrollPos = 0;
    std::deque<ai::TaskTemplate> templates;

    wl::font font;
    wl::resizer dilogResizer;
    wl::resizer paramsResizer;
    wl::combobox cb_tasks;
    wl::label params_panel;
    wl::scrollbar params_scrollbar;
    wl::scrollinfo scrollinfo;
    wl::button btn_ok;
    wl::button btn_cancel;

    const int ctrlId = 0x8100;
    ai::TaskTemplate* curr_templ {nullptr};
    std::deque<spParamCtrl> templ_controls;
};


#endif //EDROBOT_UIADDTASK_H
