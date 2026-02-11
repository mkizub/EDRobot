//
// Created by mkizub on 09.02.2026.
//

#include "UIShowCargo.h"
#include "UILayout.h"
#include "UIManager.h"
#include "UIMainDialog.h"
#include "../net/NetUtils.h"

#include "../../ui/resource.h"

std::unique_ptr<UIShowCargo> UIShowCargo::g_showCargo;

UIShowCargo* UIShowCargo::getInstance() {
    if (g_showCargo && !g_showCargo->isDestroyed)
        return g_showCargo.get();
    return nullptr;
}
UIShowCargo* UIShowCargo::makeInstance() {
    if (g_showCargo && !g_showCargo->isDestroyed)
        return g_showCargo.get();
    g_showCargo.reset(new UIShowCargo());
    g_showCargo->create(&UIManager::getInstance().uiMain);
    return g_showCargo.get();
}

UIShowCargo::UIShowCargo() {
    setup.dialogId = IDD_SHOW_CARGO;

    on_message(WM_INITDIALOG, [this](wl::params p){
        initialize();
        return TRUE;
    });
    on_message(WM_DESTROY, [this](wl::params params) {
        cargoEditor.clear();
        isDestroyed = true;
        return 0;
    });
    on_command(ID_SAVE, [this](wl::params params) {
        on_cargo_save();
        return 0;
    });
    on_command(ID_LOAD, [this](wl::params params) {
        on_cargo_load();
        return 0;
    });
    on_message(WM_DPICHANGED, [this](wl::params params) {
        cv::Rect r = fromRECT(*(PRECT)params.lParam);
        SetWindowPos(this->hwnd(), HWND_TOPMOST, 0, 0, r.width, r.height, SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);
        relayout();
        return 0;
    });
    on_message(WM_SIZE, [this](wl::params params) {
        relayout();
        return 0;
    });
}

void UIShowCargo::initialize() {
    SetDialogDpiChangeBehavior(hwnd(), DDC_DISABLE_ALL, DDC_DISABLE_ALL);

    int uiDpi = GetDpiForWindow(hwnd());
    int uiPercent = Cfg.getUiScalePercents();

    font.create(L"Segoe UI", MulDiv(LO_FONT_SIZE, uiDpi*uiPercent, 100*USER_DEFAULT_SCREEN_DPI));

    btn_run.assign(hwnd(), ID_RUN);
    btn_save.assign(hwnd(), ID_SAVE);
    btn_load.assign(hwnd(), ID_LOAD);
    btn_run.set_enabled(false);
    btn_save.set_enabled(st::cmdr.fleetCarrierId != 0);
    btn_load.set_enabled(st::cmdr.fleetCarrierId != 0);
    btn_run.set_text(toUtf16(_gt("Run")));
    btn_save.set_text(toUtf16(_gt("Save")));
    btn_load.set_text(toUtf16(_gt("Load")));

    cargoEditor.create(hwnd(), IDC_TASK_PARAMETERS, {10,10}, {100, 100});
    cargoEditor.validate_callback = [this](bool valid, bool changed){ validate_callback(valid, changed); };

    {
        RECT rect;
        GetWindowRect(UIManager::getInstance().uiMain.hwnd(), &rect);
        SetWindowPos(this->hwnd(), HWND_TOPMOST,
                     rect.left, rect.top+50, rect.right-rect.left, rect.bottom-rect.top,
                     SWP_NOOWNERZORDER);
    }
    cargoEditor.initControls();
    validate_callback(cargoEditor.validate(nullptr), false);
    relayout();
}

#define S(N) MulDiv((N), uiDpi*uiPercent, 100*USER_DEFAULT_SCREEN_DPI)
void UIShowCargo::relayout() {
    RECT rect{};
    GetClientRect(hwnd(), &rect);
    int l = rect.left;
    int t = rect.top;
    int r = rect.right;
    int b = rect.bottom;
    int width = r - l;
    int height = b - t;

    int uiDpi = GetDpiForWindow(hwnd());
    int uiPercent = Cfg.getUiScalePercents();
    if (uiDpi != scaled_to_dpi) {
        scaled_to_dpi = uiDpi;
        int font_size = MulDiv(LO_FONT_SIZE, uiDpi * uiPercent, 100 * USER_DEFAULT_SCREEN_DPI);
        font.create(L"Segoe UI", font_size);
        font.set_on(btn_run);
        font.set_on(btn_save);
        font.set_on(btn_load);
    }

    auto wpi = BeginDeferWindowPos(10);
    int x = l + width*10/100;
    int y = t + S(LO_DLG_BORDER);
    int w = width*80/100;
    int h = height - t;

    int cx = (l + r) / 2;
    w = S(LO_BTN_W);
    h = S(LO_BTN_H);
    y = b - S(LO_DLG_BORDER+LO_BTN_H);

    x = cx - w/2 - S(LO_H_GAP+LO_BTN_W);
    wpi = DeferWindowPos(wpi, btn_run.hwnd(), nullptr, x, y, w, h, SWP_NOZORDER);
    x = cx - w/2;
    wpi = DeferWindowPos(wpi, btn_save.hwnd(), nullptr, x, y, w, h, SWP_NOZORDER);
    x = cx + w/2 + S(LO_H_GAP);
    wpi = DeferWindowPos(wpi, btn_load.hwnd(), nullptr, x, y, w, h, SWP_NOZORDER);

    x = l + S(LO_DLG_BORDER);
    y = t + S(LO_DLG_BORDER+LO_BTN_H+LO_V_GAP);
    w = width - S(2*LO_DLG_BORDER);
    h = height - S(2*LO_DLG_BORDER+2*LO_V_GAP+2*LO_BTN_H);
    wpi = DeferWindowPos(wpi, cargoEditor.hwnd(), nullptr, x, y, w, h, SWP_NOZORDER);

    EndDeferWindowPos(wpi);

    RedrawWindow(this->hwnd(), 0, 0, RDW_INVALIDATE | RDW_ALLCHILDREN);
    InvalidateRect(this->hwnd(), nullptr, true);
    UpdateWindow(this->hwnd());
}
#undef S

void UIShowCargo::validate_callback(bool valid, bool changed) {
    btn_save.set_enabled(valid && changed);
}

bool UIShowCargo::updateCargo() {
    if (isDestroyed)
        return false;
    return cargoEditor.updateCargo();
}

void UIShowCargo::on_cargo_load() {
    auto jv = curlRequestRavenFC(st::cmdr.fleetCarrierId);
    if (!jv.is_object() || jv.empty() || !jv["cargo"].is_object()) {
        LOG(ERROR) << "Bad responce from RavenColonial: " << jv;
        return;
    }
    // {"marketId":3708647424,"name":"VFT-85B","displayName":"Daimonio tou Sokrati","owner":"mkzu","cargo":{"agronomictreatment":32,"bertrandite":234,"cobalt":403,"drones":11,"titanium":587,"tritium":1337}}
    if (jv["marketId"].as_int64() != st::cmdr.fleetCarrierId || jv["owner"].asif_string() != st::cmdr.name) {
        LOG(ERROR) << std::format("Bad carrier id from RavenColonial: {}:{}, expected {}:{}",
                                  jv["owner"].asif_string().c_str(), jv["marketId"].as_int64(),
                                  st::cmdr.name, st::cmdr.fleetCarrierId);
        return;
    }
    for (auto jc : jv["cargo"].as_object()) {
        Commodity* c = Cfg.getCommodityById(jc.first);
        if (!c) {
            LOG(ERROR) << "Cargo not found: " << jc.first;
            continue;
        }
        c->fc.count = jc.second.as_int32();
    }
    cargoEditor.initControls();
    validate_callback(cargoEditor.validate(nullptr), false);
    if (!st::carrierCargo)
        Cfg.saveCarrierCargo(Timestamp::clock::now());
}

void UIShowCargo::on_cargo_save() {
    if (cargoEditor.validate(nullptr)) {
        cargoEditor.save();
        Cfg.saveCarrierCargo(Timestamp::clock::now());
        btn_save.set_enabled(false);
    }
}

