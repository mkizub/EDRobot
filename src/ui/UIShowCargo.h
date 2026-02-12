//
// Created by mkizub on 09.02.2026.
//

#pragma once

#ifndef EDROBOT_UISHOWCARGO_H
#define EDROBOT_UISHOWCARGO_H

#include <shellapi.h>
#include <winlamb/dialog_main.h>
#include <winlamb/button.h>
#include "UICargoEditor.h"

class UIShowCargo : public wl::dialog_main {
public:
    static std::shared_ptr<UIShowCargo> getInstance();
    static std::shared_ptr<UIShowCargo> makeInstance();

    void initialize();
    void relayout();
    void on_cargo_load();
    void on_cargo_save();
    void validate_callback(bool valid, bool changed);
    bool updateCargo();

    bool isInitialized {};
    bool isDestroyed {};
private:
    static std::shared_ptr<UIShowCargo> g_showCargo;
    static std::jthread uiThread;
    static void uiThreadLoop();

    UIShowCargo();

    wl::font font;
    wl::button btn_run;
    wl::button btn_save;
    wl::button btn_load;
    UICargoEditor cargoEditor;

    // for (re)layout
    int scaled_to_dpi {};
};


#endif //EDROBOT_UISHOWCARGO_H
