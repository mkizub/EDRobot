//
// Created by mkizub on 09.02.2026.
//

#include "../pch.h"

#include "UIShowCargo.h"
#include "UILayout.h"
#include "../net/RavenColonial.h"
#include "../../ui/resource.h"

#include <winlamb/label.h>
#include <winlamb/textbox.h>
#include "wl_cargobox.h"
#include "wl_svg_button.h"

class BaseCargoCtrl {
public:
    BaseCargoCtrl(UIShowCargo* ui) : ui(ui) {}
    virtual ~BaseCargoCtrl() = default;
    virtual void create() = 0;
    virtual void layout(UILayout& lo) = 0;
    virtual void on_ctrl_edit(HWND changed, WORD msg) = 0;
    virtual bool validate(bool* changed) = 0;
    virtual bool save() = 0;
    virtual Commodity* updateCargo() = 0;
    UIShowCargo* ui;
};

class FullCargoCtrl : public BaseCargoCtrl {
public:
    FullCargoCtrl(UIShowCargo* ui, Commodity* commodity);
    ~FullCargoCtrl() override;
    void create() override;
    void layout(UILayout& lo) override;
    void on_ctrl_edit(HWND changed, WORD msg) override;
    bool validate(bool* changed) override;
    bool save() override;
    Commodity * updateCargo() override;
    std::wstring com_text;
    std::wstring sh_text;
    std::wstring fc_text;
    std::wstring dp_text;
    Commodity* commodity;

    wl::label lbl_cargo;
    wl::textbox txt_sh_count;
    wl::textbox txt_fc_count;
    wl::textbox txt_dp_count;
};

class NewCargoCtrl : public BaseCargoCtrl {
public:
    NewCargoCtrl(UIShowCargo* ui);
    ~NewCargoCtrl() override;
    void create() override;
    void layout(UILayout& lo) override;
    void on_ctrl_edit(HWND changed, WORD msg) override;
    bool validate(bool* changed) override;
    bool save() override;
    Commodity* updateCargo() override;
    std::wstring text;

    wl::cargobox dl;
    wl::svg_button btn_add;
    wl::svg_button btn_fc_save;
    wl::svg_button btn_dp_save;
};


UIShowCargo::UIShowCargo() : UIControl(true) {
}

UIShowCargo::~UIShowCargo() {
}


void UIShowCargo::initControls() {
    clear();
    beginControls();
    std::vector<Commodity*> allCommodities = Cfg.getAllKnownCommodities();
    for (auto* c : allCommodities) {
        if (c->ship.count <= 0 && c->fc.count <= 0)
            continue;
        controls.emplace_back(new FullCargoCtrl(this, c))->create();
    }
    new_control.reset(new NewCargoCtrl(this));
    new_control->create();
    endControls();
    relayout(true);
}

bool UIShowCargo::appendCargoControl(Commodity* commodity) {
    for (auto& ctrl : controls) {
        if (auto fcc = dynamic_cast<FullCargoCtrl*>(ctrl.get())) {
            if (fcc->commodity == commodity)
                return false;
        }
    }
    beginControls();
    controls.emplace_back(new FullCargoCtrl(this, commodity))->create();
    endControls();
    relayout(true);
    return true;
}

void UIShowCargo::clear() {
    controls.clear();
    new_control.reset();
    nextTryId = 0;
    usedIds = {};
}

bool UIShowCargo::updateCargo() {
    std::set<Commodity*> hasCommodities;
    for (auto& cc : controls) {
        Commodity* c = cc->updateCargo();
        if (c)
            hasCommodities.insert(c);
    }
    std::vector<Commodity*> addCommodities;
    for (auto* c : Cfg.getAllKnownCommodities()) {
        if ((c->ship.count > 0 || c->fc.count > 0) && !hasCommodities.contains(c))
            addCommodities.push_back(c);
    }
    if (!addCommodities.empty() && !controls.empty()) {
        beginControls();
        for (auto* c : addCommodities)
            controls.emplace_back(new FullCargoCtrl(this, c))->create();
        endControls();
        relayout(true);
    }
    return true;
}

void UIShowCargo::relayout(bool scroll_to_top) {
    if (scroll_to_top)
        reset_scroll(true);

    RECT rect{};
    GetClientRect(hwnd(), &rect);

    int uiPercent = Cfg.getUiScalePercents();
    int uiDpi = GetDpiForWindow(hwnd());
    UILayout lo(uiDpi, uiPercent, rect);
    if (uiDpi != scaled_to_dpi) {
        scaled_to_dpi = uiDpi;
        loCreateFont(font, uiDpi, uiPercent);
        lo.font = &font;
    }

    panel_width = lo.width;
    panel_height = lo.height;

    lo.wpi = BeginDeferWindowPos(100);
    lo.left = lo.vgap;
    lo.top = lo.vgap - scroll_pos;
    lo.width -= 2*lo.vgap;
    for (auto& cc : controls)
        cc->layout(lo);
    if (new_control)
        new_control->layout(lo);
    params_height = lo.top + scroll_pos;
    EndDeferWindowPos(lo.wpi);

    reset_scroll(false);
}

bool UIShowCargo::validate() const {
    changed = false;
    valid = true;
    for (auto& cc : controls) {
        if (!cc->validate(&changed))
            valid = false;
    }
    return valid;
}

void UIShowCargo::on_update() {
    if (new_control)
        new_control->btn_fc_save.set_enabled(valid && changed);
}

void UIShowCargo::on_ctrl_edit(int id, WORD msg) {
    HWND changed = GetDlgItem(hwnd(), id);
    for (auto& cc : controls)
        cc->on_ctrl_edit(changed, msg);
    if (new_control)
        new_control->on_ctrl_edit(changed, msg);
}

void UIShowCargo::on_cargo_load() {
    const auto jv = RavenColonial::carrierGetCargo(st::cmdr.fleetCarrierId);
    if (!jv.is_object() || jv.empty() || !jv["cargo"].is_object()) {
        LOG(ERROR) << "Bad response from RavenColonial: " << jv;
        return;
    }
    // {"marketId":3708647424,"name":"VFT-85B","displayName":"Daimonio tou Sokrati","owner":"mkzu","cargo":{"agronomictreatment":32,"bertrandite":234,"cobalt":403,"drones":11,"titanium":587,"tritium":1337}}
    if (jv["marketId"].as_int_or() != st::cmdr.fleetCarrierId || jv["owner"].as_string_or() != st::cmdr.name) {
        LOG_ERROR("Bad carrier id from RavenColonial: {}:{}, expected {}:{}",
                                  jv["owner"].as_string_or(), jv["marketId"].as_int(),
                                  st::cmdr.name, st::cmdr.fleetCarrierId);
        return;
    }
    for (auto [nameId,count] : jv["cargo"].key_value()) {
        Commodity* c = Cfg.getCommodityById(nameId);
        if (!c) {
            LOG(ERROR) << "Cargo not found: " << nameId;
            continue;
        }
        c->fc.count = count.as_int();
    }
    initControls();
    if (new_control)
        new_control->btn_fc_save.set_enabled(false);
    CM.saveCarrierCargo(Timestamp::clock::now(), {});
}

void UIShowCargo::on_cargo_save() {
    if (!validate())
        return;
    for (auto& cc : controls)
        cc->save();
    CM.saveCarrierCargo(Timestamp::clock::now(), {});
    if (new_control)
        new_control->btn_fc_save.set_enabled(false);

    // post new data to RavenColonial, if different
    const auto jv = RavenColonial::carrierGetCargo(st::cmdr.fleetCarrierId);
    if (!jv.is_object() || jv.empty() || !jv["cargo"].is_object()) {
        LOG(ERROR) << "Bad response from RavenColonial: " << jv;
        return;
    }
    if (jv["marketId"].as_int_or() != st::cmdr.fleetCarrierId || jv["owner"].as_string_or() != st::cmdr.name) {
        LOG_ERROR("Bad carrier id from RavenColonial: {}:{}, expected {}:{}",
                                  jv["owner"].as_string_or(), jv["marketId"].as_int(),
                                  st::cmdr.name, st::cmdr.fleetCarrierId);
        return;
    }

    auto cargo = jv["cargo"];
    js::value diff = js::object({});
    for (auto c : Cfg.getAllKnownCommodities()) {
        if (!cargo[c->nameId].empty()) {
            if (c->fc.count != cargo[c->nameId].as_int())
                diff[c->nameId] = c->fc.count;
        }
        else if (c->fc.count != 0)
            diff[c->nameId] = c->fc.count;
    }
    if (!diff.empty())
        RavenColonial::carrierPostCargo(st::cmdr.fleetCarrierId, diff);
}

FullCargoCtrl::FullCargoCtrl(UIShowCargo* ui, Commodity* commodity)
        : BaseCargoCtrl(ui)
        , commodity(commodity)
{
    com_text = commodity->wide;
}

FullCargoCtrl::~FullCargoCtrl()
{
    ui->freeCtrl(lbl_cargo);
    ui->freeCtrl(txt_sh_count);
    ui->freeCtrl(txt_fc_count);
}

void FullCargoCtrl::create() {
    lbl_cargo.create(ui->hwnd(), ui->nextID(), commodity->wide.c_str(), {0, 0}, {200, 24});
    lbl_cargo.style.set_style(TRUE, SS_WORDELLIPSIS|SS_NOTIFY);
    txt_sh_count.create(ui->hwnd(), ui->nextID(), wl::textbox::type::NORMAL, {0, 0}, 80, 24);
    txt_sh_count.style.set_style(TRUE, ES_RIGHT);
    txt_sh_count.set_enabled(false);
    txt_fc_count.create(ui->hwnd(), ui->nextID(), wl::textbox::type::NORMAL, {200, 0}, 80, 24);
    txt_fc_count.style.set_style(TRUE, ES_RIGHT);
    txt_fc_count.style.set_style(TRUE, WS_TABSTOP);
    txt_dp_count.create(ui->hwnd(), ui->nextID(), wl::textbox::type::NORMAL, {280, 0}, 80, 24);
    txt_dp_count.style.set_style(TRUE, ES_RIGHT);
    txt_dp_count.style.set_style(TRUE, WS_TABSTOP);
    if (commodity->ship.count) {
        sh_text = std::format(L"{:d}", commodity->ship.count);
        txt_sh_count.set_text(sh_text.c_str());
    }
    if (commodity->fc.count) {
        fc_text = std::format(L"{:d}", commodity->fc.count);
        txt_fc_count.set_text(fc_text.c_str());
    }
    ui->font.set_on(lbl_cargo);
    ui->font.set_on(txt_sh_count);
    ui->font.set_on(txt_fc_count);
    ui->font.set_on(txt_dp_count);
}

void FullCargoCtrl::layout(UILayout& lo) {
    if (lo.font) {
        lo.font->set_on(lbl_cargo);
        lo.font->set_on(txt_sh_count);
        lo.font->set_on(txt_fc_count);
        lo.font->set_on(txt_dp_count);
    }
    int w_txt = lo.txt6w;
    int w_lbl = lo.width - 3*lo.txt6w - 3*lo.hgap;
    if (w_lbl > lo.txt50w)
        w_lbl = lo.txt50w;
    else if (w_lbl < lo.txt20w)
        w_lbl = lo.txt20w;
    int x = lo.left;
    lo.wpi = DeferWindowPos(lo.wpi, lbl_cargo.hwnd(), nullptr, x, lo.top, w_lbl, lo.vrow, SWP_NOZORDER);
    x += w_lbl + lo.hgap;
    lo.wpi = DeferWindowPos(lo.wpi, txt_sh_count.hwnd(), nullptr, x, lo.top, w_txt, lo.vrow, SWP_NOZORDER);
    x += w_txt + lo.hgap;
    lo.wpi = DeferWindowPos(lo.wpi, txt_fc_count.hwnd(), nullptr, x, lo.top, w_txt, lo.vrow, SWP_NOZORDER);
    x += w_txt + lo.hgap;
    lo.wpi = DeferWindowPos(lo.wpi, txt_dp_count.hwnd(), nullptr, x, lo.top, w_txt, lo.vrow, SWP_NOZORDER);
    lo.top += lo.vrow + lo.vgap;
}

void FullCargoCtrl::on_ctrl_edit(HWND changed, WORD msg) {
    if (changed == txt_fc_count.hwnd()) {
        fc_text = txt_fc_count.get_text();
    }
    if (changed == txt_dp_count.hwnd()) {
        fc_text = txt_dp_count.get_text();
    }
}

bool FullCargoCtrl::validate(bool* changed) {
    std::string fc_s = trim(toUtf8(fc_text));
    int64_t result = 0;
    if (fc_s.empty() || parseInt(fc_s, result)) {
        if (changed)
            *changed = commodity->fc.count != result;
        return result >= 0;
    }

    std::string dp_s = trim(toUtf8(dp_text));
    if (dp_s.empty() || parseInt(dp_s, result)) {
        return result >= 0;
    }

    return false;
}

bool FullCargoCtrl::save() {
    std::string s = trim(toUtf8(fc_text));
    int64_t result = 0;
    if (s.empty() || parseInt(s, result)) {
        commodity->fc.count = std::max(0, int(result));
        return true;
    }
    return false;
}

Commodity * FullCargoCtrl::updateCargo() {
    if (commodity->ship.count) {
        auto text = std::format(L"{:d}", commodity->ship.count);
        if (text != sh_text) {
            sh_text = text;
            txt_sh_count.set_text(sh_text.c_str());
        }
    } else {
        if (!sh_text.empty()) {
            sh_text.clear();
            txt_sh_count.set_text(L"");
        }
    }

    if (commodity->fc.count) {
        auto text = std::format(L"{:d}", commodity->fc.count);
        if (text != fc_text) {
            fc_text = text;
            txt_fc_count.set_text(fc_text.c_str());
        }
    } else {
        if (!fc_text.empty()) {
            fc_text.clear();
            txt_fc_count.set_text(L"");
        }
    }

    return commodity;
}

NewCargoCtrl::NewCargoCtrl(UIShowCargo *ui)
        : BaseCargoCtrl(ui)
{
}

NewCargoCtrl::~NewCargoCtrl() {
    ui->freeCtrl(dl);
}

void NewCargoCtrl::create() {
    dl.create(ui->hwnd(), ui->nextID(), {0, 0}, {200, LO_V_ROW}, LO_V_ROW * 8);
    btn_add.create(ui->hwnd(), ui->nextID(), "icon-add", LO_ICN_S, {200, 0}, {20,20});
    btn_add.set_enabled(false);
    btn_fc_save.create(ui->hwnd(), ui->nextID(), "icon-save", LO_ICN_S, {220, 0}, {20,20});
    btn_fc_save.set_enabled(false);
    btn_dp_save.create(ui->hwnd(), ui->nextID(), "icon-save", LO_ICN_S, {280, 0}, {20,20});
    btn_dp_save.set_enabled(false);

    ui->font.set_on(dl);
    ui->font.set_on(btn_add);
}

void NewCargoCtrl::layout(UILayout& lo) {
    if (lo.font) {
        lo.font->set_on(dl);
        lo.font->set_on(btn_add);
    }
    btn_add.set_icon_size(lo.icsz);

    int w_txt = lo.txt6w;
    int w_lbl = lo.width - 3*w_txt - 3*lo.hgap;
    if (w_lbl > lo.txt50w)
        w_lbl = lo.txt50w;
    else if (w_lbl < lo.txt20w)
        w_lbl = lo.txt20w;
    int x = lo.left;
    lo.wpi = DeferWindowPos(lo.wpi, dl.hwnd(), nullptr, x, lo.top, w_lbl, lo.vrow, SWP_NOZORDER);
    x += w_lbl + w_txt + 2*lo.hgap;
    lo.wpi = DeferWindowPos(lo.wpi, btn_add.hwnd(), nullptr, x, lo.top, lo.vrow, lo.vrow, SWP_NOZORDER);
    lo.wpi = DeferWindowPos(lo.wpi, btn_fc_save.hwnd(), nullptr, x+lo.vrow, lo.top, lo.vrow, lo.vrow, SWP_NOZORDER);
    x += w_txt + lo.hgap;
    lo.wpi = DeferWindowPos(lo.wpi, btn_dp_save.hwnd(), nullptr, x+lo.vrow, lo.top, lo.vrow, lo.vrow, SWP_NOZORDER);
    lo.top += lo.vrow + lo.xgap;
}

void NewCargoCtrl::on_ctrl_edit(HWND changed, WORD msg) {
    if (changed == btn_add.hwnd() && msg == BN_CLICKED) {
        text = dl.get_text();
        auto* c = Cfg.getCommodityByName(text, false);
        if (c && ui->appendCargoControl(c)) {
            text.clear();
            dl.remove_all();
            btn_add.set_enabled(false);
        }
    }
    if (changed == btn_fc_save.hwnd() && msg == BN_CLICKED) {
        ui->on_cargo_save();
    }
    if (changed != dl.hwnd())
        return;
    if (msg == CBN_SELENDOK)
        text = dl.get_text();
    else
        text = dl.get_text();
    bool can_add = false;
    auto* c = Cfg.getCommodityByName(text, false);
    if (c) {
        can_add = true;
        for (auto& ctrl : ui->controls) {
            if (auto fcc = dynamic_cast<FullCargoCtrl*>(ctrl.get())) {
                if (fcc->commodity == c)
                    can_add = false;
            }
        }
    }
    btn_add.set_enabled(can_add);
    if (text.empty()) {
        dl.remove_all();
        return;
    }
//    std::set<std::wstring> new_set;
//    std::wstring text_l = toLower(text);
//    for (auto* c : Cfg.getAllKnownCommodities()) {
//        if (toUtf16(c->nameId).starts_with(text_l))
//            new_set.insert(c->wide);
//        else if (!c->translation[int(Lang::EN)].empty() && toUtf16(toLower(c->translation[int(Lang::EN)])).starts_with(text_l))
//            new_set.insert(c->wide);
//        else if (st::lng != Lang::EN && !c->translation[int(st::lng)].empty() && toLower(toUtf16(c->translation[int(st::lng)])).starts_with(text_l))
//            new_set.insert(c->wide);
//    }
//    dl.set_list(new_set);
//    if (dl.count() <= 10)
//        SendMessage(dl.hwnd(), CB_SHOWDROPDOWN, TRUE, 0);
}

bool NewCargoCtrl::validate(bool *changed) {
    if (changed)
        *changed = false;
    return text.empty();
}

bool NewCargoCtrl::save() {
    return true;
}
Commodity* NewCargoCtrl::updateCargo() {
    return nullptr;
}
