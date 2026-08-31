//
// Created by mkizub on 27.06.2025.
//

#include "../pch.h"

#include "UIEditTask.h"
#include "UILayout.h"
#include "UIManager.h"
#include "UIMainDialog.h"

#include "wl_cargobox.h"
#include "wl_bookmark.h"

#include "../../ui/resource.h"

class ParamCtrl {
public:
    ParamCtrl(UIEditTask* ui);
    ParamCtrl(UIEditTask* ui, ai::Param& param);
    virtual ~ParamCtrl();
    virtual void create();
    virtual void layout(UILayout& lo);
    virtual void on_ctrl_edit(HWND changed, WORD msg);
    virtual bool validate();
    virtual js::value value();
    UIEditTask* ui;
    std::wstring name;
    const js::value meta;
    bool optional;
    std::wstring text;
    wl::label label;
};

class BoolCtrl : public ParamCtrl {
public:
    BoolCtrl(UIEditTask* ui, ai::Param& param);
    ~BoolCtrl() override;
    void create() override;
    void layout(UILayout& lo) override;
    void on_ctrl_edit(HWND changed, WORD msg) override;
    bool validate() override;
    js::value value() override;
    bool checked;
    wl::checkbox cb;
};

class EnumCtrl : public ParamCtrl {
public:
    EnumCtrl(UIEditTask* ui, ai::Param& param);
    ~EnumCtrl() override;
    void create() override;
    void layout(UILayout& lo) override;
    void on_ctrl_edit(HWND changed, WORD msg) override;
    bool validate() override;
    js::value value() override;

    int selected_index {-1};
    wl::combobox dl;

    struct IdName {
        std::string enum_id;
        std::wstring enum_name;
    };
    std::vector<IdName> entries;
};

class TextCtrl : public ParamCtrl {
public:
    TextCtrl(UIEditTask* ui, ai::Param& param);
    ~TextCtrl() override;
    void create() override;
    void layout(UILayout& lo) override;
    void on_ctrl_edit(HWND changed, WORD msg) override;
    bool validate() override;
    js::value value() override;

    ai::Param::Type type;
    wl::textbox tb;
};

class SiteCtrl : public ParamCtrl {
public:
    SiteCtrl(UIEditTask* ui, ai::Param& param);
    ~SiteCtrl() override;
    void create() override;
    void layout(UILayout& lo) override;
    void on_ctrl_edit(HWND changed, WORD msg) override;
    bool validate() override;
    js::value value() override;

    std::wstring system_text;
    std::wstring dock_text;

    wl::textbox tb_system;
    wl::textbox tb_dock;
    wl::bookmark btn_bookmark;
};

class CargoCtrl : public ParamCtrl {
public:
    CargoCtrl(UIEditTask* ui, ai::Param& param);
    ~CargoCtrl() override;
    void create() override;
    void layout(UILayout& lo) override;
    void on_ctrl_edit(HWND changed, WORD msg) override;
    bool validate() override;
    js::value value() override;

    wl::cargobox dl;
};

class ArrayCtrl;
class ElemCtrl : public ParamCtrl {
public:
    ElemCtrl(UIEditTask* ui, ArrayCtrl* arr_ctrl, int idx);
    ~ElemCtrl() override;
    void create() override;
    void layout(UILayout& lo) override;
    void on_ctrl_edit(HWND changed, WORD msg) override;
    bool validate() override;
    js::value value() override;

    const ArrayCtrl* arr_ctrl;
    int index;
    std::unique_ptr<ParamCtrl> el_ctrl;
};

class ArrayCtrl : public ParamCtrl {
public:
    ArrayCtrl(UIEditTask* ui, ai::Param& param);
    ~ArrayCtrl() override;
    void create() override;
    void layout(UILayout& lo) override;
    void on_ctrl_edit(HWND changed, WORD msg) override;
    bool validate() override;
    js::value value() override;

    const js::value& el_meta;
    const ai::Param::Type el_type;
    bool simple;
    std::vector<js::value> arr_value;
    std::deque<std::unique_ptr<ElemCtrl>> controls;
};

class TaskCtrl : public ParamCtrl {
public:
    TaskCtrl(UIEditTask* ui, ai::TaskTemplate& templ);
    TaskCtrl(UIEditTask* ui, ai::Param& param);
    ~TaskCtrl() override;
    void create() override;
    void layout(UILayout& lo) override;
    void on_ctrl_edit(HWND changed, WORD msg) override;
    bool validate() override;
    js::value value() override;

    bool toplevel;
    std::vector<const ai::TaskTemplate*> templates;
    ai::TaskTemplate templ;
    wl::textbox tb;
    wl::combobox dl;
    std::deque<std::unique_ptr<ParamCtrl>> controls;
};

UIEditTask::UIEditTask() : UIControl(true) {
    on_command(IDC_COMBO_TEMPLATES, [this](wl::params p) {
        init_templ_list();
        menu_tasks.show_at_point(this->hwnd(), {0,20}, lbl_tasks.hwnd());
        return 0;
    });
    on_command(ID_RUN, [this](wl::params params) {
        on_template_run();
        return 0;
    });
    on_command(ID_SAVE, [this](wl::params params) {
        on_template_save();
        return 0;
    });
    on_command(ID_DELETE, [this](wl::params params) {
        on_template_delete();
        return 0;
    });
}

UIEditTask::~UIEditTask() {
}

void UIEditTask::initialize() {
    SetDialogDpiChangeBehavior(hwnd(), DDC_DISABLE_ALL, DDC_DISABLE_ALL);

    RECT rect{};
    GetClientRect(hwnd(), &rect);
    int uiDpi = GetDpiForWindow(hwnd());
    int uiPercent = Cfg.getUiScalePercents();

    UILayout lo(uiDpi, uiPercent, rect);
    loCreateFont(font, uiDpi, uiPercent);

    lo.left += lo.border;
    lo.top += lo.border;
    lo.width -= 2*lo.border;
    int x = lo.left;
    int y = lo.top;
    int cb_w = lo.width - 3*(lo.btnh+lo.xgap);
    lbl_tasks.create(hwnd(), IDC_COMBO_TEMPLATES, L"", {x,y}, {lo.btnh, cb_w})
            .style.set_style(true, WS_BORDER | SS_CENTER | SS_NOTIFY);
    x += cb_w + lo.xgap;
    btn_run.create(hwnd(), ID_RUN, "task-run", lo.icsz, {x,y}, {lo.btnh,lo.btnh}).set_enabled(false);
    x += cb_w + lo.xgap;
    btn_save.create(hwnd(), ID_SAVE, "icon-save", lo.icsz, {x,y}, {lo.btnh,lo.btnh}).set_enabled(false);
    x += cb_w + lo.xgap;
    btn_del.create(hwnd(), ID_DELETE, "icon-del", lo.icsz, {x,y}, {lo.btnh,lo.btnh}).set_enabled(false);

    init_templ_list();
    relayout();
}

void UIEditTask::clear() {
    task_ctrl.reset();
    nextTryId = 0;
    usedIds = {};
}

void UIEditTask::init_templ_list() {
    menu_tasks.destroy();
    templates.clear();
    menu_tasks = CreatePopupMenu();
    MENUINFO mi {sizeof(MENUINFO), MIM_STYLE|MIM_HELPID|MIM_MENUDATA, MNS_AUTODISMISS|MNS_NOCHECK|MNS_NOTIFYBYPOS};
    mi.dwContextHelpID = UIControl::popup_menu_id;
    mi.dwMenuData = (ULONG_PTR)this;
    SetMenuInfo(menu_tasks.hmenu(), &mi);
    std::map<std::string,wl::menu> groups;
    WORD idx = 0;
    for (auto& tt : ai::getUserTasks()) {
        templates.push_back(tt);
        auto name = toUtf16(tt.nm.c_str());
        menu_tasks.append_item(idx++, name);
    }
    if (idx > 0)
        menu_tasks.append_separator();
    for (auto& tt : ai::getTemplates()) {
        templates.push_back(tt);
        auto name = toUtf16(gettext(tt.nm.c_str()));
        auto group = tt.meta["group"].as_string_or();
        if (group.empty()) {
            menu_tasks.append_item(idx++, name);
        } else {
            if (!groups.contains(group)) {
                auto wname = toUtf16(gettext(group.c_str()));
                groups[group] = menu_tasks.append_submenu(wname);
                SetMenuInfo(groups[group].hmenu(), &mi);
            }
            groups[group].append_item(idx++, name);
        }
    }
}

void UIEditTask::on_template_run() {
    if (!task_ctrl)
        return;
    if (validate()) {
        ai::new_task(makeTemplate());
        clear();
        UIManager& mgr = UIManager::getInstance();
        if (detached) {
            PostMessage(GetParent(hwnd()), WM_CLOSE, 0, 0);
        } else {
            mgr.uiMain.show_task_status();
        }
        mgr.uiMain.hide(false);
    }
}

void UIEditTask::on_template_save() {
    if (!task_ctrl)
        return;
    task_ctrl->validate();
    ai::TaskTemplate templ = makeTemplate();
    if (templ.id.empty())
        return;
    ai::saveUserTask(templ);
    lbl_tasks.set_text(toUtf16(templ.name()));
    btn_save.set_enabled(false);
}

void UIEditTask::on_template_delete() {
    lbl_tasks.set_text(L"");
    btn_run.set_enabled(false);
    btn_save.set_enabled(false);
    btn_del.set_enabled(false);
    auto user_tasks_size = ai::getUserTasks().size();
    if (selected_template_index < user_tasks_size) {
        ai::delUserTask(selected_template_index);
    }
    selected_template_index = -1;
    clear();
}

ai::TaskTemplate UIEditTask::makeTemplate() {
    if (!task_ctrl)
        return {};
    task_ctrl->validate();
    return task_ctrl->templ;
}

std::unique_ptr<ParamCtrl> UIEditTask::create_ctrl(ai::Param& param) {
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
        return std::make_unique<TextCtrl>(this, param);
    case ai::Param::Site:
        return std::make_unique<SiteCtrl>(this, param);
    case ai::Param::Commodity:
        return std::make_unique<CargoCtrl>(this, param);
    case ai::Param::Task:
        return std::make_unique<TaskCtrl>(this, param);
    case ai::Param::Array:
        return std::make_unique<ArrayCtrl>(this, param);
    }
    return std::make_unique<ParamCtrl>(this);
}

void UIEditTask::on_ctrl_edit(int id, WORD msg) {
    if (task_ctrl) {
        HWND changed = GetDlgItem(hwnd(), id);
        task_ctrl->on_ctrl_edit(changed, msg);
    }
    validate_callback(validate());
}

bool UIEditTask::validate() const {
    return task_ctrl && task_ctrl->validate();
}

void UIEditTask::validate_callback(bool valid) {
    btn_run.set_enabled(valid && task_ctrl);
    ai::TaskTemplate templ = makeTemplate();
    if (templ.id.empty() || templ.nm.empty()) {
        btn_save.set_enabled(false);
        btn_del.set_enabled(false);
        return;
    }

    bool is_template = selected_template_index >= ai::getUserTasks().size();
    btn_del.set_enabled(!is_template);

    bool is_modified = false;
    int name_eq_counter = 0;
    for (auto& tt : ai::getUserTasks()) {
        if (tt.id == templ.id && tt.nm == templ.nm) {
            name_eq_counter += 1;
            if (!templ.params.empty() && templ != tt)
                is_modified = true;
            break;
        }
    }
    for (auto& tt : ai::getTemplates()) {
        if (tt.nm == templ.nm || tt.name() == templ.nm)
            name_eq_counter += 1;
    }
    if (!is_template) {
        btn_save.set_enabled((is_modified && name_eq_counter == 1) || name_eq_counter == 0);
    } else {
        btn_save.set_enabled(valid && name_eq_counter == 0);
    }
}


void UIEditTask::relayout(bool scroll_to_top) {
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
        font.set_on(lbl_tasks);
    }
    btn_run.set_icon_size(lo.icsz);
    btn_save.set_icon_size(lo.icsz);
    btn_del.set_icon_size(lo.icsz);

    panel_width = lo.width;
    panel_height = lo.height;
    lo.left += lo.xgap;
    lo.width -= 2*lo.xgap;
    lo.top += lo.border - scroll_pos;

    lo.wpi = BeginDeferWindowPos(100);

    int x = lo.left;
    int y = lo.top;
    int cb_w = lo.width - 3*(lo.btnh+lo.xgap);
    lo.wpi = DeferWindowPos(lo.wpi, lbl_tasks.hwnd(), nullptr, x, y, cb_w, lo.btnh, SWP_NOZORDER);
    x += cb_w + lo.xgap;
    lo.wpi = DeferWindowPos(lo.wpi, btn_run.hwnd(), nullptr, x, y, lo.btnh, lo.btnh, SWP_NOZORDER);
    x += lo.btnh + lo.xgap;
    lo.wpi = DeferWindowPos(lo.wpi, btn_save.hwnd(), nullptr, x, y, lo.btnh, lo.btnh, SWP_NOZORDER);
    x += lo.btnh + lo.xgap;
    lo.wpi = DeferWindowPos(lo.wpi, btn_del.hwnd(), nullptr, x, y, lo.btnh, lo.btnh, SWP_NOZORDER);

    lo.top += lo.btnh + lo.vgap;

    if (task_ctrl)
        task_ctrl->layout(lo);

    params_height = lo.top + scroll_pos + 5*lo.vrow;

    EndDeferWindowPos(lo.wpi);

    reset_scroll(false);
}

void UIEditTask::on_popup_menu(int idx) {
    clear();
    menu_tasks.destroy();
    if (idx < 0 || idx >= templates.size()) {
        selected_template_index = -1;
        lbl_tasks.set_text(L"");
        return;
    }
    selected_template_index = idx;
    ai::TaskTemplate& templ = templates[idx];
    lbl_tasks.set_text(toUtf16(templ.name()));

    beginControls();
    task_ctrl = std::make_unique<TaskCtrl>(this, templ);
    task_ctrl->create();
    endControls();
    relayout(true);

    validate_callback(validate());
}



ParamCtrl::ParamCtrl(UIEditTask* ui)
        : ui(ui)
{}
ParamCtrl::ParamCtrl(UIEditTask* ui, ai::Param& param)
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
void ParamCtrl::layout(UILayout& lo) {
}
void ParamCtrl::on_ctrl_edit(HWND changed, WORD msg) {
}
bool ParamCtrl::validate() {
    return false;
}
js::value ParamCtrl::value() {
    return {};
}


BoolCtrl::BoolCtrl(UIEditTask* ui, ai::Param& param)
        : ParamCtrl(ui, param)
        , checked(param.as_boolean())
{}
BoolCtrl::~BoolCtrl() {
    ui->freeCtrl(cb);
}
void BoolCtrl::create() {
    cb.create(ui->hwnd(), ui->nextID(), name.c_str(), {0,0}, {100, 20});
    cb.set_check(checked);
    ui->font.set_on(cb);
}
void BoolCtrl::layout(UILayout& lo) {
    if (lo.font) {
        lo.font->set_on(cb);
    }
    lo.wpi = DeferWindowPos(lo.wpi, cb.hwnd(), nullptr, lo.left, lo.top, lo.width, lo.vrow, SWP_NOZORDER);
    lo.top += lo.vrow + lo.vgap;
}
void BoolCtrl::on_ctrl_edit(HWND changed, WORD msg) {
    if (changed == cb.hwnd())
        checked = cb.is_checked();
}
bool BoolCtrl::validate() {
    return true;
}
js::value BoolCtrl::value() {
    return checked;
}


EnumCtrl::EnumCtrl(UIEditTask* ui, ai::Param& param)
        : ParamCtrl(ui, param)
{
    selected_index= -1;
    if (param.value.is_int())
        selected_index = param.value.as_int();
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
    dl.assign(CreateWindowExW(0, WC_COMBOBOX, nullptr,
                              WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL,
                              100, 0, 200, 400, ui->hwnd(),
                              reinterpret_cast<HMENU>(static_cast<UINT_PTR>(ui->nextID())),
                              reinterpret_cast<HINSTANCE>(GetWindowLongPtrW(ui->hwnd(), GWLP_HINSTANCE)),
                              nullptr));
    ui->font.set_on(dl);
    for (auto& e : entries)
        dl.add(e.enum_name.c_str());
    if (selected_index >= 0)
        dl.select(selected_index);
    if (!name.empty()) {
        label.create(ui->hwnd(), ui->nextID(), name.c_str(), {0, 0}, {100, 20});
        ui->font.set_on(label);
    }
}
void EnumCtrl::layout(UILayout& lo) {
    if (lo.font) {
        lo.font->set_on(label);
        lo.font->set_on(dl);
    }
    int drop_down_height = lo.vrow * std::max(4, std::min(11,(int)entries.size()));
    const int lw = lo.width / 3 - lo.vgap;
    const int rw = lo.width * 2 / 3;
    const int cx = lo.left + lo.width / 3;
    lo.wpi = DeferWindowPos(lo.wpi, dl.hwnd(), nullptr, cx, lo.top, rw, drop_down_height, SWP_NOZORDER);
    if (!name.empty()) {
        lo.wpi = DeferWindowPos(lo.wpi, label.hwnd(), nullptr, lo.left, lo.top, lw, lo.vrow, SWP_NOZORDER);
    }
    lo.top += lo.vrow + lo.xgap;
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
js::value EnumCtrl::value() {
    if (selected_index < 0 || selected_index >= entries.size())
        return {};
    return entries[selected_index].enum_id;
}


TextCtrl::TextCtrl(UIEditTask* ui, ai::Param& param)
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
    tb.create(ui->hwnd(), ui->nextID(), wl::textbox::type::NORMAL, {0,0}, 200, 20);
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
        label.create(ui->hwnd(), ui->nextID(), name.c_str(), {0,0}, {100,20});
    }

    ui->font.set_on(tb);
    ui->font.set_on(label);
}
void TextCtrl::layout(UILayout& lo) {
    if (lo.font) {
        lo.font->set_on(tb);
        lo.font->set_on(label);
    }
    const int lw = lo.width / 3 - lo.vgap;
    const int rw = lo.width * 2 / 3;
    const int cx = lo.left + lo.width / 3;
    int tw = rw;
    if (type == ai::Param::Int || type == ai::Param::Real)
        tw = lo.txt20w;

    lo.wpi = DeferWindowPos(lo.wpi, tb.hwnd(), nullptr, cx, lo.top, tw, lo.vrow, SWP_NOZORDER);
    if (!name.empty()) {
        lo.wpi = DeferWindowPos(lo.wpi, label.hwnd(), nullptr, lo.left, lo.top, lw, lo.vrow, SWP_NOZORDER);
    }
    lo.top += lo.vrow + lo.vgap;
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
js::value TextCtrl::value() {
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

SiteCtrl::SiteCtrl(UIEditTask* ui, ai::Param& param)
        : ParamCtrl(ui, param)
{
    if (!param.empty() && param.value.is_object()) {
        system_text = toUtf16(param.value["system"].as_string_or());
        dock_text = toUtf16(param.value["dock"].as_string_or());
    }
}
SiteCtrl::~SiteCtrl() {
    ui->freeCtrl(tb_system);
    ui->freeCtrl(tb_dock);
    ui->freeCtrl(btn_bookmark);
}
void SiteCtrl::create() {
    if (!name.empty())
        label.create(ui->hwnd(), ui->nextID(), name.c_str(), {0,0}, {100, 40});

    tb_system.create(ui->hwnd(), ui->nextID(), wl::textbox::type::NORMAL, {100,0}, 20, 20);
    tb_system.style.set_style(TRUE, WS_TABSTOP);
    Edit_SetCueBannerText(tb_system.hwnd(), toUtf16(_gt("Star system")).c_str());
    tb_system.set_text(system_text);

    tb_dock.create(ui->hwnd(), ui->nextID(), wl::textbox::type::NORMAL, {100,20}, 20, 20);
    tb_dock.style.set_style(TRUE, WS_TABSTOP);
    Edit_SetCueBannerText(tb_dock.hwnd(), toUtf16(_gt("Dock")).c_str());
    tb_dock.set_text(dock_text);

    btn_bookmark.create(ui->hwnd(), ui->nextID(), "icon-bookmark", LO_ICN_S, {280, 20}, {20,20});
    btn_bookmark.style.set_style(TRUE, WS_TABSTOP | SS_NOTIFY);

    ui->font.set_on(label);
    ui->font.set_on(tb_system);
    ui->font.set_on(tb_dock);
}
void SiteCtrl::layout(UILayout& lo) {
    if (lo.font) {
        lo.font->set_on(label);
        lo.font->set_on(tb_system);
        lo.font->set_on(tb_dock);
    }
    btn_bookmark.set_icon_size(lo.icsz);

    const int lw = lo.width / 3 - lo.vgap;
    const int rw = lo.width * 2 / 3;
    const int cx = lo.left + lo.width / 3;

    if (!name.empty())
        lo.wpi = DeferWindowPos(lo.wpi, label.hwnd(), nullptr, lo.left, lo.top, lw, 2*lo.vrow, SWP_NOZORDER);
    lo.wpi = DeferWindowPos(lo.wpi, tb_system.hwnd(), nullptr, cx, lo.top, rw, lo.vrow, SWP_NOZORDER);
    lo.top += lo.vrow;
    const int bw = lo.icsz + lo.hgap;
    int x = cx;
    lo.wpi = DeferWindowPos(lo.wpi, tb_dock.hwnd(), nullptr, x, lo.top, rw-bw, lo.vrow, SWP_NOZORDER);
    x += rw - bw + lo.hgap;
    lo.wpi = DeferWindowPos(lo.wpi, btn_bookmark.hwnd(), nullptr, x, lo.top, lo.icsz, lo.vrow, SWP_NOZORDER);
    lo.top += lo.vrow + lo.vgap;
}
void SiteCtrl::on_ctrl_edit(HWND changed, WORD msg) {
    if (changed == tb_system.hwnd())
        system_text = tb_system.get_text();
    if (changed == tb_dock.hwnd())
        dock_text = tb_dock.get_text();
    if (changed == btn_bookmark.hwnd())
        btn_bookmark.show_drop(ui, tb_system, tb_dock);
}
bool SiteCtrl::validate() {
    ai::Param param{ai::Param::Site, "", "", meta};
    std::string system = toUtf8(trimTextLine(system_text));
    std::string dock = toUtf8(trimTextLine(dock_text));
    js::value v = js::object({{"system", system},{"dock", dock}});
    btn_bookmark.set_icon(Cfg.isBookmarked(system, dock) ? "icon-bookmark-fill" : "icon-bookmark");
    param.set(v, true);
    return param.valid();
}
js::value SiteCtrl::value() {
    if (system_text.empty() && dock_text.empty())
        return {};
    js::value v = js::object({
        {"system", toUtf8(trimTextLine(system_text))},
        {"dock", toUtf8(trimTextLine(dock_text))}
    });
    return v;
}

CargoCtrl::CargoCtrl(UIEditTask* ui, ai::Param& param)
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
    dl.create(ui->hwnd(), ui->nextID(), {100,0}, {200,20}, 100);
    if (!text.empty())
        dl.set_text(text);
    if (meta["placeholder"].is_string()) {
        const char* ph = gettext(meta["placeholder"].as_string().c_str());
        if (ph && *ph)
            Edit_SetCueBannerText(dl.hwnd(), toUtf16(ph).c_str());
    }
    ui->font.set_on(dl);
    if (!name.empty()) {
        label.create(ui->hwnd(), ui->nextID(), name.c_str(), {0,0}, {100,20});
        ui->font.set_on(label);
    }
    ui->font.set_on(dl);
}
void CargoCtrl::layout(UILayout& lo) {
    if (lo.font) {
        lo.font->set_on(label);
        lo.font->set_on(dl);
        SendMessage(dl.hwnd(), CB_SETITEMHEIGHT, 1, (LPARAM)lo.vrow);
    }
    const int lw = lo.width / 3 - lo.vgap;
    const int rw = lo.width * 2 / 3;
    const int cx = lo.left + lo.width / 3;
    lo.wpi = DeferWindowPos(lo.wpi, dl.hwnd(), nullptr, cx, lo.top, rw, lo.vrow, SWP_NOZORDER);
    if (!name.empty()) {
        lo.wpi = DeferWindowPos(lo.wpi, label.hwnd(), nullptr, lo.left, lo.top, lw, lo.vrow, SWP_NOZORDER);
    }
    lo.top += lo.vrow + lo.xgap;
}
void CargoCtrl::on_ctrl_edit(HWND changed, WORD msg) {
    if (changed != dl.hwnd())
        return;
    if (msg == EN_SETFOCUS) {
        PostMessage(dl.hwnd(), EM_SETSEL, (WPARAM)0, (LPARAM)-1);
        return;
    }
    text = dl.get_text();
    dl.auto_drop(ui->hwnd());
}
bool CargoCtrl::validate() {
    ai::Param param{ai::Param::Commodity, "", "", meta};
    param.set(toUtf8(text), true);
    return param.valid();
}
js::value CargoCtrl::value() {
    if (text.empty())
        return {};
    std::string s = toUtf8(text);
    auto* com = Cfg.getCommodityByName(s, false);
    if (com)
        return com->nameId;
    return s;
}

ElemCtrl::ElemCtrl(UIEditTask* ui, ArrayCtrl* arr_ctrl, int idx)
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
void ElemCtrl::layout(UILayout& lo) {
    el_ctrl->layout(lo);
}
void ElemCtrl::on_ctrl_edit(HWND changed, WORD msg) {
    el_ctrl->on_ctrl_edit(changed, msg);
}
bool ElemCtrl::validate() {
    if (!el_ctrl)
        return false;
    return el_ctrl->validate() || el_ctrl->value().empty();
}
js::value ElemCtrl::value() {
    if (!el_ctrl)
        return {};
    return el_ctrl->value();
}


ArrayCtrl::ArrayCtrl(UIEditTask* ui, ai::Param& param)
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
    label.create(ui->hwnd(), ui->nextID(), name.c_str(), {0,0}, {300,20});
    ui->font.set_on(label);

    for (int i=0; i < arr_value.size(); i++) {
        controls.push_back(std::make_unique<ElemCtrl>(ui, this, i));
        controls.back()->create();
    }
}
void ArrayCtrl::layout(UILayout& lo) {
    if (lo.font) {
        lo.font->set_on(label);
    }
    lo.wpi = DeferWindowPos(lo.wpi, label.hwnd(), nullptr, lo.left, lo.top, lo.width, lo.vrow, SWP_NOZORDER);
    if (!simple) {
        lo.top += lo.vrow + lo.vgap;
        lo.left += lo.hgap;
        lo.width -= lo.hgap;
    }
    for (auto& c : controls) {
        c->layout(lo);
    }
    if (!simple) {
        lo.left -= lo.hgap;
        lo.width += lo.hgap;
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
    std::vector<js::value> new_values;
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
js::value ArrayCtrl::value() {
    assert (controls.size() == arr_value.size());
    js::value arr = js::array({});
    for (auto& c : controls) {
        auto v = c->value();
        if (!v.empty())
            arr.as_array().push_back(v);
    }
    if (arr.empty())
        return {};
    return arr;
}

TaskCtrl::TaskCtrl(UIEditTask* ui, ai::TaskTemplate& templ)
        : ParamCtrl(ui)
        , toplevel(true)
        , templ(templ)
{
    name = toUtf16(templ.name());
}
TaskCtrl::TaskCtrl(UIEditTask* ui, ai::Param& param)
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
    const char* lbl_name = toplevel ? _gt("Task name") : _gt("Task type");
    label.create(ui->hwnd(), ui->nextID(), toUtf16(lbl_name).c_str(), {0,0}, {100,20});
    ui->font.set_on(label);

    if (toplevel) {
        tb.create(ui->hwnd(), ui->nextID(), wl::textbox::type::NORMAL, {100,0}, 200,20);
        ui->font.set_on(tb);
        tb.style.set_style(TRUE, WS_TABSTOP);
        tb.set_text(name);
    } else {
        dl.assign(CreateWindowExW(0, WC_COMBOBOX, nullptr,
                                  WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL,
                                  100, 0, 200, 400, ui->hwnd(),
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

    for (auto& p : templ.params) {
        controls.push_back(ui->create_ctrl(p));
        controls.back()->create();
    }
    assert(controls.size() == templ.params.size());
}
void TaskCtrl::layout(UILayout& lo) {
    if (lo.font) {
        lo.font->set_on(tb);
        lo.font->set_on(dl);
        lo.font->set_on(label);
    }
    const int lw = lo.width / 4 - lo.vgap;
    const int rw = lo.width * 3 / 4;
    const int cx = lo.left + lo.width / 4;
    lo.wpi = DeferWindowPos(lo.wpi, label.hwnd(), nullptr, lo.left, lo.top, lw, lo.vrow, SWP_NOZORDER);
    if (toplevel) {
        lo.wpi = DeferWindowPos(lo.wpi, tb.hwnd(), nullptr, cx, lo.top, rw, lo.vrow, SWP_NOZORDER);
    } else {
        lo.wpi = DeferWindowPos(lo.wpi, dl.hwnd(), nullptr, cx, lo.top, rw, lo.vrow, SWP_NOZORDER);
    }
    lo.top += lo.vrow + lo.xgap;
    lo.left += lo.hgap;
    lo.width -= lo.hgap;
    for (auto& c : controls) {
        c->layout(lo);
    }
    lo.left -= lo.hgap;
    lo.width += lo.hgap;
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
js::value TaskCtrl::value() {
    assert(controls.size() == templ.params.size());
    if (templ.id.empty())
        return {};
    js::value obj = js::object({{"templ", templ.id}});
    if (!templ.nm.empty()) {
        auto& base_templ = ai::getTemplate(templ.id);
        if (!(templ.nm == templ.id || templ.nm == base_templ.nm || templ.name() == base_templ.name()))
            obj["name"] = templ.nm;
    }
    for (int p=0; p < templ.params.size(); p++) {
        auto val = controls[p]->value();
        templ.params[p].set(val, true);
        if (!val.empty())
            obj[templ.params[p].id] = val;
    }
    return obj;
}
