//
// Created by mkizub on 09.02.2026.
//

#pragma once

#ifndef EDROBOT_UISHOWCARGO_H
#define EDROBOT_UISHOWCARGO_H

#include "UIControl.h"
#include <winlamb/button.h>

class BaseCargoCtrl;
class FullCargoCtrl;
class NewCargoCtrl;

class UIShowCargo : public UIControl {
public:
    UIShowCargo();
    ~UIShowCargo() override;
    const wchar_t* title() const override { return L"EDRobot cargo"; }
    void initialize() override {};
    void relayout() override { relayout(false); };
    int nextID();
    void freeCtrl(wl::wnd& wnd);
    void beginControls();
    void endControls();
    void initControls();
    bool appendCargoControl(Commodity* commodity);
    void clear();
    void relayout(bool scroll_to_top);
    bool updateCargo();

    void on_ctrl_change(wl::params& params);
    bool validate(bool* changed) const;
    void on_ctrl_edit(int id, WORD msg);
    void on_cargo_load();
    void on_cargo_save();

    std::deque<std::unique_ptr<FullCargoCtrl>> controls;
    std::unique_ptr<NewCargoCtrl> new_control;

private:
    bool initializing {};
    int nextTryId;
    std::array<bool, 100> usedIds;
};


#endif //EDROBOT_UISHOWCARGO_H
