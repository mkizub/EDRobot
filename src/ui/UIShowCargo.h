//
// Created by mkizub on 09.02.2026.
//

#pragma once

#ifndef EDROBOT_UISHOWCARGO_H
#define EDROBOT_UISHOWCARGO_H

#include <shellapi.h>
#include <winlamb/dialog_modeless.h>
#include <winlamb/button.h>
#include "UICargoEditor.h"

class UIShowCargo : public wl::dialog_modeless {
public:
    UIShowCargo();

    void initialize();
    void relayout();
    void on_cargo_load();
    void on_cargo_save();
    void validate_callback(bool valid, bool changed);

    bool isDestroyed {};
private:
    wl::font font;
    wl::button btn_run;
    wl::button btn_save;
    wl::button btn_load;
    UICargoEditor cargoEditor;

    // for (re)layout
    int scaled_to_dpi {};
};


#endif //EDROBOT_UISHOWCARGO_H
