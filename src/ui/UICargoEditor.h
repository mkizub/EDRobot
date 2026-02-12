//
// Created by mkizub on 09.02.2026.
//

#pragma once

#ifndef EDROBOT_UICARGOEDITOR_H
#define EDROBOT_UICARGOEDITOR_H

#include "wl_panel.h"
#include "wl_cargobox.h"
#include <winlamb/textbox.h>
#include <winlamb/label.h>
#include <winlamb/button.h>

class UICargoEditor;

class BaseCargoCtrl {
public:
    BaseCargoCtrl(UICargoEditor* ui) : ui(ui) {}
    virtual ~BaseCargoCtrl() = default;
    virtual void create() = 0;
    virtual void layout() = 0;
    virtual void on_ctrl_edit(HWND changed, WORD msg) = 0;
    virtual bool validate(bool* changed) = 0;
    virtual bool save() = 0;
    virtual Commodity* updateCargo() = 0;
    UICargoEditor* ui;
};

class FullCargoCtrl : public BaseCargoCtrl {
public:
    FullCargoCtrl(UICargoEditor* ui, Commodity* commodity);
    ~FullCargoCtrl() override;
    void create() override;
    void layout() override;
    void on_ctrl_edit(HWND changed, WORD msg) override;
    bool validate(bool* changed) override;
    bool save() override;
    Commodity * updateCargo() override;
    std::wstring com_text;
    std::wstring sh_text;
    std::wstring fc_text;
    Commodity* commodity;

    wl::label lbl_cargo;
    wl::textbox txt_sh_count;
    wl::textbox txt_fc_count;
};

class NewCargoCtrl : public BaseCargoCtrl {
public:
    NewCargoCtrl(UICargoEditor* ui);
    ~NewCargoCtrl() override;
    void create() override;
    void layout() override;
    void on_ctrl_edit(HWND changed, WORD msg) override;
    bool validate(bool* changed) override;
    bool save() override;
    Commodity* updateCargo() override;
    std::wstring text;

    wl::cargobox dl;
    wl::button btn_add;
};

class UICargoEditor : public wl::params_panel {
public:

    UICargoEditor();
    int nextID();
    void freeCtrl(wl::wnd& wnd);
    void beginControls();
    void endControls();
    void initControls();
    bool appendCargoControl(Commodity* commodity);
    void save();
    void clear();
    void relayout(bool scroll_to_top);
    bool updateCargo();

    void on_ctrl_change(wl::params& params);
    bool validate(bool* changed) const;
    void on_ctrl_edit(int id, WORD msg);

    std::function<void(bool,bool)> validate_callback;
    std::deque<std::unique_ptr<BaseCargoCtrl>> controls;

private:
    bool initializing {};
    int nextTryId;
    std::array<bool, 100> usedIds;
};

#endif //EDROBOT_UICARGOEDITOR_H
