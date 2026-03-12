//
// Created by mkizub on 24.11.2025.
//

#pragma once

#ifndef EDROBOT_UITASKEDITOR_H
#define EDROBOT_UITASKEDITOR_H

#include "UIControl.h"

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

#include "wl_panel.h"
#include "wl_cargobox.h"
#include "../ai/AIManager.h"

class UILayout;
class UITaskEditor;

class ParamCtrl {
public:
    ParamCtrl(UITaskEditor* ui);
    ParamCtrl(UITaskEditor* ui, ai::Param& param);
    virtual ~ParamCtrl();
    virtual void create();
    virtual void layout(UILayout& lo);
    virtual void on_ctrl_edit(HWND changed, WORD msg);
    virtual bool validate();
    virtual js::value value();
    UITaskEditor* ui;
    std::wstring name;
    const js::value meta;
    bool optional;
    std::wstring text;
    wl::label label;
};

class BoolCtrl : public ParamCtrl {
public:
    BoolCtrl(UITaskEditor* ui, ai::Param& param);
    ~BoolCtrl() override;
    void create() override;
    void layout(UILayout& lo) override;
    void on_ctrl_edit(HWND changed, WORD msg) override;
    bool validate() override;
    js::value value() override;
    bool checked;
    wl::checkbox cb;
};

class EnumCtrl : public ParamCtrl {
public:
    EnumCtrl(UITaskEditor* ui, ai::Param& param);
    ~EnumCtrl() override;
    void create() override;
    void layout(UILayout& lo) override;
    void on_ctrl_edit(HWND changed, WORD msg) override;
    bool validate() override;
    js::value value() override;

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
    void layout(UILayout& lo) override;
    void on_ctrl_edit(HWND changed, WORD msg) override;
    bool validate() override;
    js::value value() override;

    ai::Param::Type type;
    wl::textbox tb;
};

class SiteCtrl : public ParamCtrl {
public:
    SiteCtrl(UITaskEditor* ui, ai::Param& param);
    ~SiteCtrl() override;
    void create() override;
    void layout(UILayout& lo) override;
    void on_ctrl_edit(HWND changed, WORD msg) override;
    bool validate() override;
    js::value value() override;

    std::wstring system_text;
    std::wstring dock_text;

    wl::textbox tb_system;
    wl::textbox tb_dock;
};

class CargoCtrl : public ParamCtrl {
public:
    CargoCtrl(UITaskEditor* ui, ai::Param& param);
    ~CargoCtrl() override;
    void create() override;
    void layout(UILayout& lo) override;
    void on_ctrl_edit(HWND changed, WORD msg) override;
    bool validate() override;
    js::value value() override;

    wl::cargobox dl;
};

class ArrayCtrl;
class ElemCtrl : public ParamCtrl {
public:
    ElemCtrl(UITaskEditor* ui, ArrayCtrl* arr_ctrl, int idx);
    ~ElemCtrl() override;
    void create() override;
    void layout(UILayout& lo) override;
    void on_ctrl_edit(HWND changed, WORD msg) override;
    bool validate() override;
    js::value value() override;

    const ArrayCtrl* arr_ctrl;
    int index;
    std::unique_ptr<ParamCtrl> el_ctrl;
};

class ArrayCtrl : public ParamCtrl {
public:
    ArrayCtrl(UITaskEditor* ui, ai::Param& param);
    ~ArrayCtrl() override;
    void create() override;
    void layout(UILayout& lo) override;
    void on_ctrl_edit(HWND changed, WORD msg) override;
    bool validate() override;
    js::value value() override;

    const js::value& el_meta;
    const ai::Param::Type el_type;
    bool simple;
    std::vector<js::value> arr_value;
    std::deque<std::unique_ptr<ElemCtrl>> controls;
};

class TaskCtrl : public ParamCtrl {
public:
    TaskCtrl(UITaskEditor* ui, ai::TaskTemplate& templ);
    TaskCtrl(UITaskEditor* ui, ai::Param& param);
    ~TaskCtrl() override;
    void create() override;
    void layout(UILayout& lo) override;
    void on_ctrl_edit(HWND changed, WORD msg) override;
    bool validate() override;
    js::value value() override;

    bool toplevel;
    std::vector<const ai::TaskTemplate*> templates;
    ai::TaskTemplate templ;
    wl::textbox tb;
    wl::combobox dl;
    std::deque<std::unique_ptr<ParamCtrl>> controls;
};

class UITaskEditor : public UIControl {
public:

    UITaskEditor();
    const wchar_t* title() const override { return L"EDRobot task editor"; }

    void setTaskTemplate(ai::TaskTemplate& tt);
    ai::TaskTemplate makeTemplate();
    void clear();
    void relayout(bool scroll_to_top=false) override;

    std::unique_ptr<ParamCtrl> create_ctrl(ai::Param& param);
    bool validate() const override;
    void on_ctrl_edit(int id, WORD msg) override;

    std::function<void(bool)> validate_callback;
    std::unique_ptr<TaskCtrl> task_ctrl;

private:
    bool initializing {};
    int nextTryId;
    std::array<bool, 100> usedIds;
};


#endif //EDROBOT_UITASKEDITOR_H
