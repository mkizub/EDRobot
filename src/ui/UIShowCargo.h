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
    void relayout(bool scroll_to_top) override;
    void initControls();
    bool appendCargoControl(Commodity* commodity);
    void clear();
    bool updateCargo();

    void on_update() override;
    void on_ctrl_edit(int id, WORD msg) override;
    bool validate() const override;
    void on_cargo_load();
    void on_cargo_save();

    std::deque<std::unique_ptr<FullCargoCtrl>> controls;
    std::unique_ptr<NewCargoCtrl> new_control;

private:
    mutable bool changed = false;
    mutable bool valid = true;
};


#endif //EDROBOT_UISHOWCARGO_H
