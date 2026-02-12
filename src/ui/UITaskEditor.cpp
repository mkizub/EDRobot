//
// Created by mkizub on 24.11.2025.
//

#include "../pch.h"

#include "windowsx.h"

#include "UITaskEditor.h"
#include "UILayout.h"


UITaskEditor::UITaskEditor() {
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

int UITaskEditor::nextID() {
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

void UITaskEditor::freeCtrl(wl::wnd& w) {
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

void UITaskEditor::beginControls() {
    initializing = true;
}
void UITaskEditor::endControls() {
    initializing = false;
}


void UITaskEditor::setTaskTemplate(ai::TaskTemplate& tt) {
    clear();
    beginControls();
    task_ctrl = std::make_unique<TaskCtrl>(this, tt);
    task_ctrl->create();
    endControls();
    relayout(true);
}

ai::TaskTemplate UITaskEditor::makeTemplate() {
    if (!task_ctrl)
        return {};
    task_ctrl->validate();
    return task_ctrl->templ;
}

void UITaskEditor::clear() {
    task_ctrl.reset();
    nextTryId = 0;
    usedIds = {};
}
void UITaskEditor::relayout(bool scroll_to_top) {
    if (!task_ctrl)
        return;

    if (scroll_to_top)
        reset_scroll(true);

    int uiDpi = GetDpiForWindow(hwnd());
    if (uiDpi != layout.scaled_to_dpi) {
        layout.scaled_to_dpi = uiDpi;
        layout.update_font = true;

        int uiPercent = Cfg.getUiScalePercents();
        int font_size = MulDiv(LO_FONT_SIZE, uiDpi * uiPercent, 100 * USER_DEFAULT_SCREEN_DPI);
        font.create(L"Segoe UI", font_size);

        layout.hgap = MulDiv(LO_H_GAP, uiDpi * uiPercent, 100 * USER_DEFAULT_SCREEN_DPI);
        layout.vgap = MulDiv(LO_V_GAP, uiDpi * uiPercent, 100 * USER_DEFAULT_SCREEN_DPI);
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
    task_ctrl->layout();
    params_height = layout.top + scroll_pos;
    EndDeferWindowPos(layout.hWinPosInfo);
    layout.hWinPosInfo = NULL;
    layout.update_font = false;
    reset_scroll(false);
}

void UITaskEditor::on_ctrl_change(wl::params& p) {
    if (initializing)
        return;
    auto hw = HIWORD(p.wParam);
    if (hw != EN_CHANGE && hw != BN_CLICKED && hw != CBN_SELENDOK && hw != CBN_EDITCHANGE)
        return;
    int id = LOWORD(p.wParam);
    if (id < ctrlIdBase)
        return;
    bool ok = on_ctrl_edit(id, hw);
    if (validate_callback)
        validate_callback(ok);
}

std::unique_ptr<ParamCtrl> UITaskEditor::create_ctrl(ai::Param& param) {
    switch (param.type) {
    case ai::Param::Void:
        break;
    case ai::Param::Bool:
        return std::make_unique<BoolCtrl>(this, param);
    case ai::Param::Enum:
        return std::make_unique<EnumCtrl>(this, param);
    case ai::Param::Int:
    case ai::Param::Real:
    case ai::Param::String:
    case ai::Param::System:
    case ai::Param::Dock:
        return std::make_unique<TextCtrl>(this, param);
    case ai::Param::Commodity:
        return std::make_unique<CargoCtrl>(this, param);
    case ai::Param::Task:
        return std::make_unique<TaskCtrl>(this, param);
    case ai::Param::Array:
        return std::make_unique<ArrayCtrl>(this, param);
    }
    return std::make_unique<ParamCtrl>(this);
}

bool UITaskEditor::validate() const {
    return task_ctrl && task_ctrl->validate();
}
bool UITaskEditor::on_ctrl_edit(int id, WORD msg) {
    if (!task_ctrl)
        return false;
    HWND changed = GetDlgItem(hwnd(), id);
    task_ctrl->on_ctrl_edit(changed, msg);
    return task_ctrl->validate();
}


ParamCtrl::ParamCtrl(UITaskEditor* ui)
        : ui(ui)
{}
ParamCtrl::ParamCtrl(UITaskEditor* ui, ai::Param& param)
    : ui(ui)
    , name(toUtf16(param.name()))
    , meta(param.meta)
    , optional(param.optional())
{}


ParamCtrl::~ParamCtrl() {
    ui->freeCtrl(label);
}
void ParamCtrl::create() {
}
void ParamCtrl::layout() {
}
void ParamCtrl::on_ctrl_edit(HWND changed, WORD msg) {
}
bool ParamCtrl::validate() {
    return false;
}
json5pp::value ParamCtrl::value() {
    return {};
}


BoolCtrl::BoolCtrl(UITaskEditor* ui, ai::Param& param)
    : ParamCtrl(ui, param)
    , checked(param.as_boolean())
{}
BoolCtrl::~BoolCtrl() {
    ui->freeCtrl(cb);
}
void BoolCtrl::create() {
    auto& l = ui->layout;
    cb.create(ui->hwnd(), ui->nextID(), name.c_str(), {l.left,l.top}, {l.width, l.vrow});
    cb.set_check(checked);
    ui->font.set_on(cb);
    l.top += l.vrow + l.vgap;
}
void BoolCtrl::layout() {
    auto& l = ui->layout;
    if (l.update_font) {
        ui->font.set_on(cb);
    }
    l.hWinPosInfo = DeferWindowPos(l.hWinPosInfo, cb.hwnd(), nullptr, l.left, l.top, l.width, l.vrow, SWP_NOZORDER);
    l.top += l.vrow + l.vgap;
}
void BoolCtrl::on_ctrl_edit(HWND changed, WORD msg) {
    if (changed == cb.hwnd())
        checked = cb.is_checked();
}
bool BoolCtrl::validate() {
    return true;
}
json5pp::value BoolCtrl::value() {
    return checked;
}


EnumCtrl::EnumCtrl(UITaskEditor* ui, ai::Param& param)
    : ParamCtrl(ui, param)
{
    selected_index= -1;
    if (param.value.is_integer())
        selected_index = param.value.as_integer();
    auto values = meta["values"].as_array();
    for (int idx=0; idx < values.size(); idx++) {
        auto& val  = values[idx];
        std::string enum_id;
        std::string enum_name;
        if (val.is_string()) {
            enum_id = val.as_string();
            enum_name = enum_id;
        } else {
            enum_id = val["id"].as_string();
            if (val["name"].is_string())
                enum_name = val["name"].as_string();
            else
                enum_name = enum_id;
        }
        entries.emplace_back(enum_id, toUtf16(_gt(enum_name.c_str())));
        if (selected_index < 0 && param.value.is_string() && param.value.as_string() == enum_id)
            selected_index = idx;
    }
    if (selected_index >= 0 && selected_index <= entries.size())
        text = toUtf16(entries[selected_index].enum_id);
}
EnumCtrl::~EnumCtrl() {
    ui->freeCtrl(dl);
}
void EnumCtrl::create() {
    auto& l = ui->layout;
    int drop_down_height = l.vrow * std::max(4, std::min(11,(int)entries.size()));
    const int lw = l.width / 3 - l.vgap;
    const int rw = l.width * 2 / 3;
    const int cx = l.left + l.width / 3;
    dl.assign(CreateWindowExW(0, WC_COMBOBOX, nullptr,
                              WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL,
                              cx, l.top, rw, drop_down_height, ui->hwnd(),
                              reinterpret_cast<HMENU>(static_cast<UINT_PTR>(ui->nextID())),
                              reinterpret_cast<HINSTANCE>(GetWindowLongPtrW(ui->hwnd(), GWLP_HINSTANCE)),
                              nullptr));
    ui->font.set_on(dl);
    for (auto& e : entries)
        dl.add(e.enum_name.c_str());
    if (selected_index >= 0)
        dl.select(selected_index);
    if (!name.empty()) {
        label.create(ui->hwnd(), ui->nextID(), name.c_str(), {l.left, l.top}, {lw, l.vrow});
        ui->font.set_on(label);
    }
    l.top += l.vrow + l.vgap;
}
void EnumCtrl::layout() {
    auto& l = ui->layout;
    if (l.update_font) {
        ui->font.set_on(label);
        ui->font.set_on(dl);
    }
    int drop_down_height = l.vrow * std::max(4, std::min(11,(int)entries.size()));
    const int lw = l.width / 3 - l.vgap;
    const int rw = l.width * 2 / 3;
    const int cx = l.left + l.width / 3;
    l.hWinPosInfo = DeferWindowPos(l.hWinPosInfo, dl.hwnd(), nullptr, cx, l.top, rw, drop_down_height, SWP_NOZORDER);
    if (!name.empty()) {
        l.hWinPosInfo = DeferWindowPos(l.hWinPosInfo, label.hwnd(), nullptr, l.left, l.top, lw, l.vrow, SWP_NOZORDER);
    }
    l.top += l.vrow + l.vgap;
}
void EnumCtrl::on_ctrl_edit(HWND changed, WORD msg) {
    if (changed == dl.hwnd()) {
        selected_index = dl.get_selected_index();
        if (selected_index < 0 || selected_index >= entries.size())
            text.clear();
        else
            text = toUtf16(entries[selected_index].enum_id);
    }
}
bool EnumCtrl::validate() {
    if (selected_index < 0 || selected_index >= entries.size())
        return optional;
    return true;
}
json5pp::value EnumCtrl::value() {
    if (selected_index < 0 || selected_index >= entries.size())
        return {};
    return entries[selected_index].enum_id;
}


TextCtrl::TextCtrl(UITaskEditor* ui, ai::Param& param)
    : ParamCtrl(ui, param)
    , type(param.type)
{
    if (!param.empty()) {
        if (param.type == ai::Param::Commodity) {
            auto* com = Cfg.getCommodityByName(param.as_string(), false);
            if (com)
                text = com->wide;
            else
                text = toUtf16(param.as_string());
        } else {
            text = toUtf16(param.as_string());
        }
    }
}
TextCtrl::~TextCtrl() {
    ui->freeCtrl(tb);
}
void TextCtrl::create() {
    auto& l = ui->layout;
    const int lw = l.width / 3 - l.vgap;
    const int rw = l.width * 2 / 3;
    const int cx = l.left + l.width / 3;

    tb.create(ui->hwnd(), ui->nextID(), wl::textbox::type::NORMAL, {cx, l.top}, rw, l.vrow);
    tb.style.set_style(TRUE, WS_TABSTOP);
    if (type == ai::Param::Int || type == ai::Param::Real)
        tb.style.set_style(TRUE, ES_RIGHT);
    if (meta["placeholder"].is_string()) {
        const char* ph = gettext(meta["placeholder"].as_string().c_str());
        if (ph && *ph)
            Edit_SetCueBannerText(tb.hwnd(), toUtf16(ph).c_str());
    }
    tb.set_text(text);

    if (!name.empty()) {
        label.create(ui->hwnd(), ui->nextID(), name.c_str(), {l.left, l.top}, {lw, l.vrow});
    }

    ui->font.set_on(tb);
    ui->font.set_on(label);
    l.top += l.vrow + l.vgap;
}
void TextCtrl::layout() {
    auto& l = ui->layout;
    if (l.update_font) {
        ui->font.set_on(tb);
        ui->font.set_on(label);
    }
    const int lw = l.width / 3 - l.vgap;
    const int rw = l.width * 2 / 3;
    const int cx = l.left + l.width / 3;
    int tw = rw;
    if (type == ai::Param::Int || type == ai::Param::Real)
        tw = MulDiv(90, l.scaled_to_dpi * Cfg.getUiScalePercents(), 100 * USER_DEFAULT_SCREEN_DPI);

    l.hWinPosInfo = DeferWindowPos(l.hWinPosInfo, tb.hwnd(), nullptr, cx, l.top, tw, l.vrow, SWP_NOZORDER);
    if (!name.empty()) {
        l.hWinPosInfo = DeferWindowPos(l.hWinPosInfo, label.hwnd(), nullptr, l.left, l.top, lw, l.vrow, SWP_NOZORDER);
    }
    l.top += l.vrow + l.vgap;
}
void TextCtrl::on_ctrl_edit(HWND changed, WORD msg) {
    if (changed == tb.hwnd())
        text = tb.get_text();
}
bool TextCtrl::validate() {
    ai::Param param{type, "", "", meta};
    param.set(trim(toUtf8(text)), true);
    return param.valid();
}
json5pp::value TextCtrl::value() {
    if (text.empty())
        return {};
    std::string s = trim(toUtf8(text));
    if (type == ai::Param::Int) {
        int64_t result = 0;
        if (parseInt(s, result))
            return result;
    }
    if (type == ai::Param::Real) {
        double result = 0;
        if (parseReal(s, result))
            return result;
    }
    if (type == ai::Param::Commodity) {
        auto* com = Cfg.getCommodityByName(s, false);
        if (com)
            return com->nameId;
    }
    return s;
}

CargoCtrl::CargoCtrl(UITaskEditor* ui, ai::Param& param)
        : ParamCtrl(ui, param)
{
    assert (param.type == ai::Param::Commodity);
    if (!param.empty()) {
        auto* com = Cfg.getCommodityByName(param.as_string(), false);
        if (com)
            text = com->wide;
        else
            text = toUtf16(param.as_string());
    }
}
CargoCtrl::~CargoCtrl() {
    ui->freeCtrl(dl);
}
void CargoCtrl::create() {
    auto& l = ui->layout;
    const int lw = l.width / 3 - l.vgap;
    const int rw = l.width * 2 / 3;
    const int cx = l.left + l.width / 3;
    dl.create(ui->hwnd(), ui->nextID(), {l.left, l.top}, {rw, l.vrow}, l.vrow * 8);
    if (!text.empty())
        dl.set_text(text);
    if (meta["placeholder"].is_string()) {
        const char* ph = gettext(meta["placeholder"].as_string().c_str());
        if (ph && *ph)
            Edit_SetCueBannerText(dl.hwnd(), toUtf16(ph).c_str());
    }
    ui->font.set_on(dl);
    if (!name.empty()) {
        label.create(ui->hwnd(), ui->nextID(), name.c_str(), {l.left, l.top}, {lw, l.vrow});
        ui->font.set_on(label);
    }
    l.top += l.vrow + l.vgap;

    ui->font.set_on(dl);
    l.top += l.vrow + l.vgap;
}
void CargoCtrl::layout() {
    auto& l = ui->layout;
    if (l.update_font) {
        ui->font.set_on(label);
        ui->font.set_on(dl);
    }
    const int lw = l.width / 3 - l.vgap;
    const int rw = l.width * 2 / 3;
    const int cx = l.left + l.width / 3;
    l.hWinPosInfo = DeferWindowPos(l.hWinPosInfo, dl.hwnd(), nullptr, cx, l.top, rw, l.vrow, SWP_NOZORDER);
    //SetWindowPos(dl.hwnd(), nullptr, cx, l.top, rw, l.vrow, SWP_NOZORDER);
    //SendMessage(dl.hwnd(), CB_SETITEMHEIGHT, 1, (LPARAM)l.vrow);
    if (!name.empty()) {
        l.hWinPosInfo = DeferWindowPos(l.hWinPosInfo, label.hwnd(), nullptr, l.left, l.top, lw, l.vrow, SWP_NOZORDER);
    }
    l.top += l.vrow + l.vgap;
}
void CargoCtrl::on_ctrl_edit(HWND changed, WORD msg) {
    if (changed != dl.hwnd())
        return;
    if (msg == CBN_SELENDOK)
        text = dl.get_selected_text();
    else
        text = dl.get_text();
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
bool CargoCtrl::validate() {
    ai::Param param{ai::Param::Commodity, "", "", meta};
    param.set(toUtf8(text), true);
    return param.valid();
}
json5pp::value CargoCtrl::value() {
    if (text.empty())
        return {};
    std::string s = toUtf8(text);
    auto* com = Cfg.getCommodityByName(s, false);
    if (com)
        return com->nameId;
    return s;
}

ElemCtrl::ElemCtrl(UITaskEditor* ui, ArrayCtrl* arr_ctrl, int idx)
    : arr_ctrl(arr_ctrl)
    , index(idx)
    , ParamCtrl(ui)
{}
ElemCtrl::~ElemCtrl() {
    el_ctrl.reset();
}
void ElemCtrl::create() {
    ai::Param param{arr_ctrl->el_type, "", "", arr_ctrl->el_meta};
    if (index < arr_ctrl->arr_value.size())
        param.value = arr_ctrl->arr_value[index];
    el_ctrl = ui->create_ctrl(param);
    el_ctrl->create();
}
void ElemCtrl::layout() {
    el_ctrl->layout();
}
void ElemCtrl::on_ctrl_edit(HWND changed, WORD msg) {
    el_ctrl->on_ctrl_edit(changed, msg);
}
bool ElemCtrl::validate() {
    if (!el_ctrl)
        return false;
    return el_ctrl->validate() || el_ctrl->value().empty();
}
json5pp::value ElemCtrl::value() {
    if (!el_ctrl)
        return {};
    return el_ctrl->value();
}


ArrayCtrl::ArrayCtrl(UITaskEditor* ui, ai::Param& param)
        : ParamCtrl(ui, param)
        , el_meta(meta["elements"])
        , el_type(enum_cast<ai::Param::Type>(el_meta["type"].as_string()).value())
{
    if (param.value.is_array())
        arr_value = param.value.as_array();
    arr_value.push_back({});
    if (!(el_type == ai::Param::Bool || el_type == ai::Param::Task || el_type == ai::Param::Array))
        simple = true;
}
ArrayCtrl::~ArrayCtrl() {
    controls.clear();
}
void ArrayCtrl::create() {
    auto& l = ui->layout;
    label.create(ui->hwnd(), ui->nextID(), name.c_str(), {l.left, l.top}, {l.width, l.vrow});
    ui->font.set_on(label);

    if (!simple) {
        l.top += l.vrow + l.vgap;
        l.left += l.hgap;
        l.width -= l.hgap;
    }
    for (int i=0; i < arr_value.size(); i++) {
        controls.push_back(std::make_unique<ElemCtrl>(ui, this, i));
        controls.back()->create();
    }
    if (!simple) {
        l.left -= l.hgap;
        l.width += l.hgap;
    }
}
void ArrayCtrl::layout() {
    auto& l = ui->layout;
    if (l.update_font) {
        ui->font.set_on(label);
    }
    l.hWinPosInfo = DeferWindowPos(l.hWinPosInfo, label.hwnd(), nullptr, l.left, l.top, l.width, l.vrow, SWP_NOZORDER);
    if (!simple) {
        l.top += l.vrow + l.vgap;
        l.left += l.hgap;
        l.width -= l.hgap;
    }
    for (auto& c : controls) {
        c->layout();
    }
    if (!simple) {
        l.left -= l.hgap;
        l.width += l.hgap;
    }
}
void ArrayCtrl::on_ctrl_edit(HWND changed, WORD msg) {
    for (auto& c : controls) {
        c->on_ctrl_edit(changed, msg);
    }
}
bool ArrayCtrl::validate() {
    assert (controls.size() == arr_value.size());
    bool ok = true;
    std::vector<json5pp::value> new_values;
    new_values.reserve(controls.size());
    for (int i=0; i < controls.size(); i++) {
        new_values.push_back(controls[i]->value());
        if (new_values.back().empty())
            continue;
        if (!controls[i]->validate())
            ok = false;
    }
    if (ok) {
        arr_value = new_values;
        if (!arr_value.back().empty() && controls.back()->validate()) {
            int pos = arr_value.size();
            arr_value.push_back({});
            ui->beginControls();
            controls.push_back(std::make_unique<ElemCtrl>(ui, this, pos));
            controls.back()->create();
            ui->endControls();
            ui->relayout(false);
        }
    }
    return ok;
}
json5pp::value ArrayCtrl::value() {
    assert (controls.size() == arr_value.size());
    json5pp::value arr = json5pp::array({});
    for (auto& c : controls) {
        auto v = c->value();
        if (!v.empty())
            arr.as_array().push_back(v);
    }
    if (arr.empty())
        return {};
    return arr;
}

TaskCtrl::TaskCtrl(UITaskEditor* ui, ai::TaskTemplate& templ)
    : ParamCtrl(ui)
    , toplevel(true)
    , templ(templ)
{
    name = toUtf16(templ.name());
}
TaskCtrl::TaskCtrl(UITaskEditor* ui, ai::Param& param)
    : ParamCtrl(ui, param)
    , toplevel(false)
    , templ(ai::TaskTemplate::loadTask(param.value))
{
    name = toUtf16(templ.name());
    std::vector<std::string> valid_templates;
    for (auto v : meta["values"].as_array()) {
        if (v.is_string())
            valid_templates.push_back(v.as_string());
    }
    for (auto& tt : ai::getUserTasks()) {
        if (std::ranges::contains(valid_templates, tt.id))
            templates.push_back(&tt);
    }
    for (auto& tt : ai::getTemplates()) {
        if (std::ranges::contains(valid_templates, tt.id))
            templates.push_back(&tt);
    }
}

TaskCtrl::~TaskCtrl() {
    ui->freeCtrl(tb);
    ui->freeCtrl(dl);
    controls.clear();
}
void TaskCtrl::create() {
    auto& l = ui->layout;
    const int lw = l.width / 4 - l.vgap;
    const int rw = l.width * 3 / 4;
    const int cx = l.left + l.width / 4;
    const char* lbl_name = toplevel ? _gt("Task name") : _gt("Task type");
    label.create(ui->hwnd(), ui->nextID(), toUtf16(lbl_name).c_str(), {l.left, l.top}, {lw, l.vrow});
    ui->font.set_on(label);

    if (toplevel) {
        tb.create(ui->hwnd(), ui->nextID(), wl::textbox::type::NORMAL, {cx, l.top}, rw, l.vrow);
        ui->font.set_on(tb);
        tb.style.set_style(TRUE, WS_TABSTOP);
        tb.set_text(name);
    } else {
        int drop_down_height = l.vrow * std::max(4, std::min(11,(int)templates.size()));
        dl.assign(CreateWindowExW(0, WC_COMBOBOX, nullptr,
                                  WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL,
                                  cx, l.top, rw, drop_down_height, ui->hwnd(),
                                  reinterpret_cast<HMENU>(static_cast<UINT_PTR>(ui->nextID())),
                                  reinterpret_cast<HINSTANCE>(GetWindowLongPtrW(ui->hwnd(), GWLP_HINSTANCE)),
                                  nullptr));

        int select_index = 0;
        dl.add(L"-------");
        for (auto* tt : templates) {
            dl.add(toUtf16(tt->name()).c_str());
        }
        for (int i=templates.size()-1; i >= 0; i--) {
            if (templates[i]->id == templ.id) {
                select_index = i+1;
                break;
            }
        }
        dl.select(select_index);
        ui->font.set_on(dl);
    }

    l.top += l.vrow + l.vgap;
    l.left += l.hgap;
    l.width -= l.hgap;
    for (auto& p : templ.params) {
        controls.push_back(ui->create_ctrl(p));
        controls.back()->create();
    }
    l.left -= l.hgap;
    l.width += l.hgap;
    assert(controls.size() == templ.params.size());
}
void TaskCtrl::layout() {
    auto& l = ui->layout;
    if (l.update_font) {
        ui->font.set_on(tb);
        ui->font.set_on(dl);
        ui->font.set_on(label);
    }
    const int lw = l.width / 4 - l.vgap;
    const int rw = l.width * 3 / 4;
    const int cx = l.left + l.width / 4;
    l.hWinPosInfo = DeferWindowPos(l.hWinPosInfo, label.hwnd(), nullptr, l.left, l.top, lw, l.vrow, SWP_NOZORDER);
    if (toplevel) {
        l.hWinPosInfo = DeferWindowPos(l.hWinPosInfo, tb.hwnd(), nullptr, cx, l.top, rw, l.vrow, SWP_NOZORDER);
    } else {
        l.hWinPosInfo = DeferWindowPos(l.hWinPosInfo, dl.hwnd(), nullptr, cx, l.top, rw, l.vrow, SWP_NOZORDER);
    }
    l.top += l.vrow + l.vgap;
    l.left += l.hgap;
    l.width -= l.hgap;
    for (auto& c : controls) {
        c->layout();
    }
    l.left -= l.hgap;
    l.width += l.hgap;
}
void TaskCtrl::on_ctrl_edit(HWND changed, WORD msg) {
    if (toplevel && changed == tb.hwnd()) {
        name = tb.get_text();
        return;
    }
    if (!toplevel && changed == dl.hwnd()) {
        templ = {};
        name.clear();
        text.clear();
        controls.clear();
        int select_index = dl.get_selected_index();
        if (select_index > 0) {
            templ = *templates[select_index-1];
            ui->beginControls();
            for (auto& p : templ.params) {
                controls.push_back(ui->create_ctrl(p));
                controls.back()->create();
            }
            ui->endControls();
        }
        ui->relayout(false);
        return;
    }
    for (auto& c : controls) {
        c->on_ctrl_edit(changed, msg);
    }
}
bool TaskCtrl::validate() {
    assert(controls.size() == templ.params.size());
    if (templ.id.empty())
        return false;
    if (templ.name() != toUtf8(name))
        const_cast<std::string&>(templ.nm) = toUtf8(name);
    bool ok = true;
    for (auto& c : controls) {
        if (!c->validate())
            ok = false;
    }
    if (ok) {
        for (int p=0; p < templ.params.size(); p++) {
            auto val = controls[p]->value();
            if (!templ.params[p].set(val, true) || !templ.params[p].valid())
                ok = false;
        }
    }
    return ok;
}
json5pp::value TaskCtrl::value() {
    assert(controls.size() == templ.params.size());
    if (templ.id.empty())
        return {};
    json5pp::value obj = json5pp::object({{"templ", templ.id}});
    if (!templ.nm.empty()) {
        auto& base_templ = ai::getTemplate(templ.id);
        if (!(templ.nm == templ.id || templ.nm == base_templ.nm || templ.name() == base_templ.name()))
            obj.as_object().emplace("name", templ.nm);
    }
    for (int p=0; p < templ.params.size(); p++) {
        auto val = controls[p]->value();
        templ.params[p].set(val);
        if (!val.empty())
            obj.as_object().emplace(templ.params[p].id, val);
    }
    return obj;
}
