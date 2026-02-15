//
// Created by mkizub on 09.02.2026.
//

#include "UICargoEditor.h"
#include "UILayout.h"

UICargoEditor::UICargoEditor() {
    for (int i=0; i < usedIds.size(); i++) {
        on_command(ctrlIdBase + i, [this](wl::params p) {
            on_ctrl_change(p);
            return 0;
        });
    }
    font.create_ui();

    on_message(WM_SIZE, [this](wl::params params) {
        if (params.wParam == SIZE_RESTORED) {
            panel_width = LOWORD(params.lParam);
            panel_height = HIWORD(params.lParam);
            layout.width = panel_width;
            relayout(false);
        }
        return 0;
    });
}

int UICargoEditor::nextID() {
    assert (initializing);
    for (int i=nextTryId; i < usedIds.size(); i++) {
        if (!usedIds[i]) {
            usedIds[i] = true;
            nextTryId = i+1;
            return ctrlIdBase + i;
        }
    }
    nextTryId = 0;
    for (int i=0; i < usedIds.size(); i++) {
        if (!usedIds[i]) {
            usedIds[i] = true;
            nextTryId = i+1;
            return ctrlIdBase + i;
        }
    }
    return ctrlIdBase;
}

void UICargoEditor::freeCtrl(wl::wnd& w) {
    if (!w.hwnd())
        return;
    int id = GetDlgCtrlID(w.hwnd());
    DestroyWindow(w.hwnd());
    if (id < ctrlIdBase || id - ctrlIdBase >= usedIds.size())
        return;
    id -= ctrlIdBase;
    usedIds[id] = false;
    nextTryId = 0;
}

void UICargoEditor::beginControls() {
    initializing = true;
}
void UICargoEditor::endControls() {
    initializing = false;
}

void UICargoEditor::initControls() {
    clear();
    beginControls();
    std::vector<Commodity*> allCommodities = Cfg.getAllKnownCommodities();
    for (auto* c : allCommodities) {
        if (c->ship.count <= 0 && c->fc.count <= 0)
            continue;
        controls.emplace_back(new FullCargoCtrl(this, c))->create();
    }
    controls.emplace_back(new NewCargoCtrl(this))->create();
    endControls();
    relayout(true);
}

bool UICargoEditor::appendCargoControl(Commodity* commodity) {
    for (auto& ctrl : controls) {
        if (auto fcc = dynamic_cast<FullCargoCtrl*>(ctrl.get())) {
            if (fcc->commodity == commodity)
                return false;
        }
    }
    beginControls();
    auto back = std::move(controls.back());
    controls.pop_back();
    controls.emplace_back(new FullCargoCtrl(this, commodity))->create();
    controls.push_back(std::move(back));
    endControls();
    relayout(true);
    return true;
}

void UICargoEditor::save() {
    for (auto& cc : controls)
        cc->save();
}

void UICargoEditor::clear() {
    controls.clear();
    nextTryId = 0;
    usedIds = {};
}

bool UICargoEditor::updateCargo() {
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
        auto back = std::move(controls.back());
        controls.pop_back();
        for (auto* c : addCommodities) {
            controls.emplace_back(new FullCargoCtrl(this, c))->create();
        }
        controls.push_back(std::move(back));
        endControls();
        relayout(true);
    }
    return true;
}

void UICargoEditor::relayout(bool scroll_to_top) {
    if (scroll_to_top)
        reset_scroll(true);

    int uiDpi = GetDpiForWindow(hwnd());
    if (uiDpi != layout.scaled_to_dpi) {
        layout.scaled_to_dpi = uiDpi;
        layout.update_font = true;

        int uiPercent = Cfg.getUiScalePercents();
        loCreateFont(font, uiDpi, uiPercent);

        layout.hgap = MulDiv(LO_H_GAP, uiDpi * uiPercent, 100 * USER_DEFAULT_SCREEN_DPI);
        layout.vgap = MulDiv(LO_V_GAP, uiDpi * uiPercent, 100 * USER_DEFAULT_SCREEN_DPI);
        layout.xgap = MulDiv(LO_X_GAP, uiDpi * uiPercent, 100 * USER_DEFAULT_SCREEN_DPI);
        layout.vrow = MulDiv(LO_V_ROW, uiDpi * uiPercent, 100 * USER_DEFAULT_SCREEN_DPI);
    }

    RECT rect{};
    GetClientRect(hwnd(), &rect);
    panel_width = rect.right - rect.left;
    panel_height = rect.bottom - rect.top;
    layout.width = panel_width;

    layout.hWinPosInfo = BeginDeferWindowPos(100);
    layout.left = layout.vgap;
    layout.top = layout.vgap - scroll_pos;
    layout.width -= 2*layout.vgap;
    for (auto& cc : controls)
        cc->layout();
    params_height = layout.top + scroll_pos;
    EndDeferWindowPos(layout.hWinPosInfo);
    layout.hWinPosInfo = NULL;
    layout.update_font = false;
    reset_scroll(false);
}

void UICargoEditor::on_ctrl_change(wl::params& p) {
    if (initializing)
        return;
    auto hw = HIWORD(p.wParam);
    if (hw != EN_CHANGE && hw != BN_CLICKED && hw != CBN_SELENDOK && hw != CBN_EDITCHANGE)
        return;
    int id = LOWORD(p.wParam);
    if (id < ctrlIdBase)
        return;
    on_ctrl_edit(id, hw);
    if (validate_callback) {
        bool changed = false;
        bool valid = validate(&changed);
        validate_callback(valid, changed);
    }
}

bool UICargoEditor::validate(bool* changed) const {
    bool valid = true;
    for (auto& cc : controls) {
        bool cc_changed = false;
        if (!cc->validate(&cc_changed))
            valid = false;
        if (changed && cc_changed)
            *changed = true;
    }
    return valid;
}
void UICargoEditor::on_ctrl_edit(int id, WORD msg) {
    HWND changed = GetDlgItem(hwnd(), id);
    for (auto& cc : controls)
        cc->on_ctrl_edit(changed, msg);
}

FullCargoCtrl::FullCargoCtrl(UICargoEditor* ui, Commodity* commodity)
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

#define S(N) MulDiv((N), l.scaled_to_dpi * Cfg.getUiScalePercents(), 100*USER_DEFAULT_SCREEN_DPI)
void FullCargoCtrl::create() {
    auto& l = ui->layout;
    int w_txt = S(LO_TXT_6_W);
    int w_lbl = l.width - 2*w_txt - 2*l.hgap;
    if (w_lbl > S(LO_TXT_50_W))
        w_lbl = S(LO_TXT_50_W);
    else if (w_lbl < S(LO_TXT_20_W))
        w_lbl = S(LO_TXT_20_W);
    int x = l.left;
    lbl_cargo.create(ui->hwnd(), ui->nextID(), commodity->wide.c_str(), {x, l.top}, {w_lbl, l.vrow});
    lbl_cargo.style.set_style(TRUE, SS_WORDELLIPSIS|SS_NOTIFY);
    x += w_lbl + l.hgap;
    txt_sh_count.create(ui->hwnd(), ui->nextID(), wl::textbox::type::NORMAL, {x, l.top}, w_txt, l.vrow);
    txt_sh_count.style.set_style(TRUE, ES_RIGHT);
    txt_sh_count.set_enabled(false);
    x += w_txt + l.hgap;
    txt_fc_count.create(ui->hwnd(), ui->nextID(), wl::textbox::type::NORMAL, {x, l.top}, w_txt, l.vrow);
    txt_fc_count.style.set_style(TRUE, ES_RIGHT);
    txt_fc_count.style.set_style(TRUE, WS_TABSTOP);
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

    l.top += l.vrow + l.vgap;
}

void FullCargoCtrl::layout() {
    auto& l = ui->layout;
    if (l.update_font) {
        ui->font.set_on(lbl_cargo);
        ui->font.set_on(txt_sh_count);
        ui->font.set_on(txt_fc_count);
    }
    int w_txt = S(LO_TXT_6_W);
    int w_lbl = l.width - 2*w_txt - 2*l.hgap;
    if (w_lbl > S(LO_TXT_50_W))
        w_lbl = S(LO_TXT_50_W);
    else if (w_lbl < S(LO_TXT_20_W))
        w_lbl = S(LO_TXT_20_W);
    int x = l.left;
    l.hWinPosInfo = DeferWindowPos(l.hWinPosInfo, lbl_cargo.hwnd(), nullptr, x, l.top, w_lbl, l.vrow, SWP_NOZORDER);
    x += w_lbl + l.hgap;
    l.hWinPosInfo = DeferWindowPos(l.hWinPosInfo, txt_sh_count.hwnd(), nullptr, x, l.top, w_txt, l.vrow, SWP_NOZORDER);
    x += w_txt + l.hgap;
    l.hWinPosInfo = DeferWindowPos(l.hWinPosInfo, txt_fc_count.hwnd(), nullptr, x, l.top, w_txt, l.vrow, SWP_NOZORDER);
    l.top += l.vrow + l.vgap;
}
#undef S

void FullCargoCtrl::on_ctrl_edit(HWND changed, WORD msg) {
    if (changed == txt_fc_count.hwnd()) {
        fc_text = txt_fc_count.get_text();
    }
}

bool FullCargoCtrl::validate(bool* changed) {
    std::string s = trim(toUtf8(fc_text));
    int64_t result = 0;
    if (s.empty() || parseInt(s, result)) {
        if (changed)
            *changed = commodity->fc.count != result;
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

NewCargoCtrl::NewCargoCtrl(UICargoEditor *ui)
    : BaseCargoCtrl(ui)
{
}

NewCargoCtrl::~NewCargoCtrl() {
    ui->freeCtrl(dl);
}

#define S(N) MulDiv((N), l.scaled_to_dpi * Cfg.getUiScalePercents(), 100*USER_DEFAULT_SCREEN_DPI)
void NewCargoCtrl::create() {
    auto& l = ui->layout;
    int w_txt = S(LO_TXT_6_W);
    int w_lbl = l.width - 2*w_txt - 2*l.hgap;
    if (w_lbl > S(LO_TXT_50_W))
        w_lbl = S(LO_TXT_50_W);
    else if (w_lbl < S(LO_TXT_20_W))
        w_lbl = S(LO_TXT_20_W);
    int x = l.left;
    dl.create(ui->hwnd(), ui->nextID(), {x, l.top}, {w_lbl, l.vrow}, l.vrow * 8);
    x += w_lbl + l.hgap;
    btn_add.create(ui->hwnd(), ui->nextID(), L"+", {x, l.top}, {w_txt,l.vrow});
    btn_add.set_enabled(false);
    ui->font.set_on(dl);
    ui->font.set_on(btn_add);
    l.top += l.vrow + l.xgap;
}

void NewCargoCtrl::layout() {
    auto& l = ui->layout;
    if (l.update_font) {
        ui->font.set_on(dl);
        ui->font.set_on(btn_add);
    }
    int w_txt = S(LO_TXT_6_W);
    int w_lbl = l.width - 2*w_txt - 2*l.hgap;
    if (w_lbl > S(LO_TXT_50_W))
        w_lbl = S(LO_TXT_50_W);
    else if (w_lbl < S(LO_TXT_20_W))
        w_lbl = S(LO_TXT_20_W);
    int x = l.left;
    l.hWinPosInfo = DeferWindowPos(l.hWinPosInfo, dl.hwnd(), nullptr, l.left, l.top, w_lbl, l.vrow, SWP_NOZORDER);
    x += w_lbl + l.hgap;
    l.hWinPosInfo = DeferWindowPos(l.hWinPosInfo, btn_add.hwnd(), nullptr, x, l.top, w_txt, l.vrow, SWP_NOZORDER);
    l.top += l.vrow + l.xgap;
}
#undef S

void NewCargoCtrl::on_ctrl_edit(HWND changed, WORD msg) {
    if (changed == btn_add.hwnd() && msg == BN_CLICKED) {
        text = dl.get_selected_text();
        auto* c = Cfg.getCommodityByName(text, false);
        if (c && ui->appendCargoControl(c)) {
            text.clear();
            dl.remove_all();
            btn_add.set_enabled(false);
        }
    }
    if (changed != dl.hwnd())
        return;
    if (msg == CBN_SELENDOK)
        text = dl.get_selected_text();
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
    std::set<std::wstring> new_set;
    std::wstring text_l = toLower(text);
    for (auto* c : Cfg.getAllKnownCommodities()) {
        if (toUtf16(c->nameId).starts_with(text_l))
            new_set.insert(c->wide);
        else if (!c->translation[int(Lang::EN)].empty() && toUtf16(toLower(c->translation[int(Lang::EN)])).starts_with(text_l))
            new_set.insert(c->wide);
        else if (st::lng != Lang::EN && !c->translation[int(st::lng)].empty() && toLower(toUtf16(c->translation[int(st::lng)])).starts_with(text_l))
            new_set.insert(c->wide);
    }
    dl.set_list(new_set);
    if (dl.count() <= 10)
        SendMessage(dl.hwnd(), CB_SHOWDROPDOWN, TRUE, 0);
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
