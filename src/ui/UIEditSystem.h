//
// Created by mkizub on 06.04.2026.
//

#pragma once

#ifndef EDROBOT_UIEDITSYSTEM_H
#define EDROBOT_UIEDITSYSTEM_H

#include <shellapi.h>
#include <shellscalingapi.h>
#include <winlamb/combobox.h>
#include <winlamb/checkbox.h>
#include <winlamb/textbox.h>
#include <winlamb/label.h>
#include <winlamb/button.h>

#include "wl_svg_button.h"
#include "wl_svg_static.h"
#include "wl_starbox.h"

#include "UIControl.h"

class EntityCtrl;

class UIEditSystem : public UIControl {
public:
    UIEditSystem();
    ~UIEditSystem();

    const wchar_t* title() const override { return L"EDRobot Star System editor"; };
    void initialize() override;
    void relayout(bool scroll_to_top=false) override;
    void on_ctrl_edit(int id, WORD msg) override;
    bool validate() const override;
    void clear();

    void init_systems_list(std::string select={});
    void on_system_import();
    void on_system_save();
    void on_system_delete();
    void on_system_selected(gal::spStarSystem starSystem);

    std::unique_ptr<EntityCtrl> create_ctrl(gal::spEntity& entity);
    EntityCtrl* find_ctrl(int bodyId);
    void sort_controls();

    gal::spStarSystem currStarSystem;

private:
    std::vector<std::string> starSystems;

    wl::starbox cb_system;
    wl::svg_button btn_import;
    wl::svg_button btn_save;
    wl::svg_button btn_del;
    std::vector<std::unique_ptr<EntityCtrl>> controls;

    mutable bool changed = false;
    mutable bool valid = true;
};


#endif //EDROBOT_UIEDITSYSTEM_H
