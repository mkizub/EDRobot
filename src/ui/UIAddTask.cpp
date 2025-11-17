//
// Created by mkizub on 27.06.2025.
//

#include "../pch.h"

#include "UIAddTask.h"

#include "../../ui/resource.h"

//static const wchar_t* gWindowClass = L"AddTaskWindowClass";
//static const wchar_t* gWindowName = L"EDRobot Add Task";

UIAddTask::UIAddTask() {
    setup.dialogId = IDD_ADD_TASK;

    on_message(WM_INITDIALOG, [this](wl::params p){return initialize(p);});

    on_command(IDC_COMBO_TEMPLATES, [this](wl::params p) {
        on_template_selected(p);
        return 0;
    });
    for (int i=0; i < 20; i++)
        on_command(ctrlId+i, [this](wl::params p) { on_ctrl_change(p); return 0; });

    on_command(IDCANCEL, [this](wl::params params) {
        templ_controls.clear();
        curr_templ = nullptr;
        EndDialog(hwnd(), IDCANCEL);
        return 0;
    });
    on_command(IDOK, [this](wl::params params) {
        if (curr_templ) {
            ai::new_task(*curr_templ);
            templ_controls.clear();
            curr_templ = nullptr;
        }
        EndDialog(hwnd(), IDOK);
        return 0;
    });
    on_message(WM_SIZE, [this](wl::params params) {
        dilogResizer.adjust(params);
        paramsResizer.adjust(params);
        RECT rect{};
        GetWindowRect(params_panel.hwnd(), &rect);
        mPanelWidth = rect.right - rect.left;
        mPanelHeight = rect.bottom - rect.top;
        reset_scroll();
        InvalidateRect(this->hwnd(), nullptr, true);
        UpdateWindow(this->hwnd());
        return 0;
    });
    on_message(WM_VSCROLL, [this](wl::params params) {
        on_scrollbar(params);
        return 0;
    });
}

int UIAddTask::initialize(wl::params &params) {
    float dpi = GetDpiForWindow(hwnd());
    mUiScale = dpi / USER_DEFAULT_SCREEN_DPI;
    //if (auto hMonitor = MonitorFromWindow(hwnd(), MONITOR_DEFAULTTONEAREST)) {
    //    DEVICE_SCALE_FACTOR mUiScaleFactor {SCALE_100_PERCENT};
    //    if (GetScaleFactorForMonitor(hMonitor, &mUiScaleFactor))
    //        mUiScale *= float(mUiScaleFactor) / int(SCALE_100_PERCENT);
    //}
    mUiScale *= Cfg.getUiScaleFactor();
    EnableNonClientDpiScaling(hwnd());

    font.create(L"Segoe UI", (int)std::round(12*mUiScale));
    for (auto& tt : ai::getUserTasks())
        templates.push_back(tt);
    for (auto& tt : ai::getTemplates())
        templates.push_back(tt);

    btn_ok.assign(hwnd(), IDOK);
    btn_cancel.assign(hwnd(), IDCANCEL);
    cb_tasks.assign(hwnd(), IDC_COMBO_TEMPLATES);
    params_panel.assign(hwnd(), IDC_TASK_PARAMETERS);
    params_scrollbar.assign(hwnd(), IDC_TASK_PARAMS_SCROLLBAR);

    params_scrollbar.style.set_style(false, WS_TABSTOP);
    params_panel.style.set_style_ex(true, WS_EX_CONTROLPARENT);

    RECT rect{};
    GetWindowRect(params_panel.hwnd(), &rect);
    mPanelWidth = rect.right - rect.left;
    mPanelHeight = rect.bottom - rect.top;


    dilogResizer.add(cb_tasks, wl::resizer::go::RESIZE, wl::resizer::go::NOTHING);
    dilogResizer.add(params_scrollbar, wl::resizer::go::REPOS, wl::resizer::go::RESIZE);
    dilogResizer.add(params_panel, wl::resizer::go::RESIZE, wl::resizer::go::RESIZE);
    dilogResizer.add(btn_ok, wl::resizer::go::NOTHING, wl::resizer::go::REPOS);
    dilogResizer.add(btn_cancel, wl::resizer::go::NOTHING, wl::resizer::go::REPOS);

    btn_ok.set_enabled(false);

    for (auto& t : templates) {
        cb_tasks.add({toUtf16(t.name()).c_str()});
    }

    return TRUE;
}

void UIAddTask::reset_scroll() {
    mScrollPos = 0;

    scrollinfo.set_flags(wl::scrollinfo::info::POS);
    scrollinfo.get_scroll(params_panel.hwnd(), wl::scrollinfo::bar::VERT);
    if (scrollinfo.pos)
        ScrollWindow(params_panel.hwnd(), 0, -scrollinfo.pos, NULL, NULL);

    scrollinfo.set_flags(static_cast<wl::scrollinfo::info>(0x1F));
    scrollinfo.pos = 0;
    scrollinfo.minPos = 0;
    scrollinfo.maxPos = mParamsHeight;
    scrollinfo.trackPos = 0;
    scrollinfo.pageSz = mPanelHeight;
    scrollinfo.set_scroll(params_panel.hwnd(), wl::scrollinfo::bar::VERT);
    scrollinfo.set_scroll(params_scrollbar.hwnd(), wl::scrollinfo::bar::CTRL);

    InvalidateRect(this->hwnd(), nullptr, true);
    UpdateWindow(this->hwnd());
}

void UIAddTask::on_scrollbar(wl::params& params) {
    int event = LOWORD(params.wParam);
    bool fullWindowUpdate = false;
    int delta = 0;

    switch (event) {
    case SB_TOP:
        reset_scroll();
        return;
    case SB_LINEDOWN:
        delta = +22 * mUiScale;
        if (mScrollPos + delta + mPanelHeight > mParamsHeight)
            delta = mParamsHeight-mPanelHeight-mScrollPos;
        break;
    case SB_LINEUP:
        delta = -22 * mUiScale;
        if (mScrollPos + delta < 0)
            delta = -mScrollPos;
        break;
    case SB_THUMBTRACK:
        delta = HIWORD(params.wParam) - mScrollPos;
        break;
    case SB_THUMBPOSITION:
        fullWindowUpdate = true;
        delta = HIWORD(params.wParam) - mScrollPos;
        break;
    case SB_ENDSCROLL:
        scrollinfo.set_flags(wl::scrollinfo::info::POS);
        scrollinfo.get_scroll(params_scrollbar.hwnd(), wl::scrollinfo::bar::CTRL);
        delta = mScrollPos - scrollinfo.pos;
        fullWindowUpdate = true;
        break;
    }
    if (delta) {
        mScrollPos += delta;
        scrollinfo.pos = mScrollPos;
        scrollinfo.set_flags(wl::scrollinfo::info::POS);
        scrollinfo.set_scroll(params_scrollbar.hwnd(), wl::scrollinfo::bar::CTRL);
        ScrollWindow(params_panel.hwnd(), 0, -delta, NULL, NULL);
        UpdateWindow(params_panel.hwnd());
    }
    if (fullWindowUpdate) {
        scrollinfo.pos = mScrollPos;
        scrollinfo.set_flags(wl::scrollinfo::info::POS);
        scrollinfo.set_scroll(params_scrollbar.hwnd(), wl::scrollinfo::bar::CTRL);
        InvalidateRect(this->hwnd(), nullptr, true);
        UpdateWindow(this->hwnd());
    }
}

int UIAddTask::on_template_selected(wl::params &params) {
    if (HIWORD(params.wParam) != CBN_SELCHANGE)
        return TRUE;

    int idx = cb_tasks.get_selected_index();
    templ_controls.clear();
    curr_templ = nullptr;
    if (idx < 0 || idx >= templates.size())
        return TRUE;

    curr_templ = &templates[idx];

    int x = 0;
    int y = 0;
    int w = mPanelWidth;
    int h = 22 * mUiScale;
    int id = ctrlId;
    for (auto& p : curr_templ->params) {
        add_ctrl({*curr_templ, p}, id, x, y, w, h);
    }
    mParamsHeight = y;
    reset_scroll();

    bool ok = true;
    for (auto& ctrl : templ_controls) {
        if (!validate(ctrl)) {
            ok = false;
            break;
        }
    }
    btn_ok.set_enabled(ok);

    InvalidateRect(hwnd(), nullptr, true);
    UpdateWindow(hwnd());

    return TRUE;
}

int UIAddTask::on_ctrl_change(wl::params& p) {
    if (HIWORD(p.wParam) != EN_CHANGE && HIWORD(p.wParam) != BN_CLICKED && HIWORD(p.wParam) != CBN_SELCHANGE)
        return TRUE;
    if (!curr_templ)
        return TRUE;
    int id = LOWORD(p.wParam);
    if (id < ctrlId)
        return TRUE;
    for (auto& ctrl : templ_controls) {
        if (ctrl && ctrl->id == id) {
           ctrl->on_ctrl_edit();
           break;
        }
    }

    bool ok = true;
    for (auto& ctrl : templ_controls) {
        if (!validate(ctrl)) {
            ok = false;
            break;
        }
    }
    btn_ok.set_enabled(ok);

    return TRUE;
}

void UIAddTask::add_ctrl(PRef ref, int &id, int left, int &top, int width, int height) {
    const int VGAP = 16 * mUiScale;
    const int HGAP = 2 * mUiScale;
    const int lw = width / 3 - VGAP;
    const int rw = width * 2 / 3;
    const int cx = left + width / 3;
    const ai::Param::Type type = ref.type();
    if (type == ai::Param::Bool) {
        auto ctrl = std::make_shared<BoolCtrl>(ref, (int) id);
        templ_controls.emplace_back(ctrl);
        ctrl->cb.create(params_panel.hwnd(), id, ref.name().c_str(), {left,top}, {width, height});
        ctrl->cb.set_check(ref.as_boolean());
        font.set_on(ctrl->cb);
        id += 1;
        top += height+4;
    }
    else if (type == ai::Param::Enum) {
        auto ctrl = std::make_shared<EnumCtrl>(ref, (int) id);
        templ_controls.emplace_back(ctrl);
        wl::combobox& dl = ctrl->dl;
        auto values = ref.meta()["values"].as_array();
        int h = mUiScale * std::max(75, 25 * std::min(11,(int)values.size()));
        dl.assign(CreateWindowExW(0, WC_COMBOBOX, nullptr,
                                  WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL,
                                  cx, top, rw, h, params_panel.hwnd(),
                                  reinterpret_cast<HMENU>(static_cast<UINT_PTR>(id)),
                                  reinterpret_cast<HINSTANCE>(GetWindowLongPtrW(hwnd(), GWLP_HINSTANCE)),
                                  nullptr));
        int select_idx = -1;
        for (int idx=0; idx < values.size(); idx++) {
            auto& val  = values[idx];
            std::string enum_id;
            if (val.is_string()) {
                enum_id = val.as_string();
                dl.add({toUtf16(enum_id).c_str()});
            } else {
                enum_id = val["id"].as_string();
                if (val["name"].is_string())
                    dl.add({toUtf16(val["name"].as_string()).c_str()});
                else
                    dl.add({toUtf16(enum_id).c_str()});
            }
            const auto& pv = ref.value();
            if (pv.is_string() && pv.as_string() == enum_id) {
                ctrl->index = idx;
                select_idx = idx;
            }
        }
        if (select_idx >= 0)
            dl.select(select_idx);

        id += 1;
        wl::label& lbl = ctrl->label;
        lbl.create(params_panel.hwnd(), id, ref.name().c_str(), {left, top}, {lw, height});
        font.set_on(lbl);
        id += 1;
        top += height + HGAP;
    }
    else if (type == ai::Param::Array) {
        auto ctrl = std::make_shared<ParamCtrl>(ref, (int) id);
        templ_controls.emplace_back(ctrl);
        wl::label& lbl = ctrl->label;
        lbl.create(params_panel.hwnd(), id, ref.name().c_str(), {left, top}, {width, height});
        font.set_on(lbl);

        id += 1;
        top += height + HGAP;
        auto& val = ref.value();
        if (val.is_array()) {
            auto& meta = ref.meta()["elements"];
            auto size = val.as_array().size();
            for (int i=1; i <= size; i++) {
                add_ctrl({ref.templ, ref.param, i}, id, left+VGAP, top, width-VGAP, height);
            }
        }
        return;
    }
    else if (type == ai::Param::Task) {
        auto ctrl = std::make_shared<TaskCtrl>(ref, (int) id);
        templ_controls.emplace_back(ctrl);
        auto text = toUtf8(ref.name()) + ": " + ctrl->sub_templ.name();
        wl::label& lbl = ctrl->label;
        lbl.create(params_panel.hwnd(), id, toUtf16(text).c_str(), {left, top}, {width, height});
        font.set_on(lbl);
        lbl.style.set_style(true, SS_LEFTNOWORDWRAP);
        paramsResizer.add(lbl, wl::resizer::go::RESIZE, wl::resizer::go::NOTHING);
        id += 1;
        top += height + HGAP;
        for (auto& sub_param : ctrl->sub_templ.params) {
            add_ctrl({ctrl->sub_templ, sub_param}, id, left+VGAP, top, width-VGAP, height);
        }
        return;
    }
    else {
        auto ctrl = std::make_shared<TextCtrl>(ref, (int) id);
        templ_controls.emplace_back(ctrl);
        wl::textbox& tb = ctrl->tb;
        int w = rw;
        if (type == ai::Param::Int) {
            if (!ref.empty())
                ctrl->text = std::to_wstring(ref.value().as_int64());
            w = 100 * mUiScale;
        }
        else if (type == ai::Param::Real) {
            if (!ref.empty())
                ctrl->text = std::to_wstring(ref.value().as_number());
            w = 100 * mUiScale;
        }
        else if (type == ai::Param::Commodity) {
            Commodity* commodity = nullptr;
            if (!ref.empty())
                commodity = Cfg.getCommodityByName(ref.as_string(), false);
            if (commodity)
                ctrl->text = commodity->wide;
        }
        else {
            ctrl->text = toUtf16(ref.as_string());
        }
        tb.create(params_panel.hwnd(), id, wl::textbox::type::NORMAL, {cx, top}, w, height);
        font.set_on(tb);
        tb.style.set_style(TRUE, WS_TABSTOP);
        if (type == ai::Param::Int || type == ai::Param::Real)
            tb.style.set_style(TRUE, ES_RIGHT);
        if (auto ph = ref.placeholder(); !ph.empty())
            Edit_SetCueBannerText(tb.hwnd(), ph.c_str());
        tb.set_text(ctrl->text);
        if (w == rw)
            paramsResizer.add(tb, wl::resizer::go::RESIZE, wl::resizer::go::NOTHING);

        // label
        id += 1;
        wl::label& lbl = ctrl->label;
        lbl.create(params_panel.hwnd(), id, ref.name().c_str(), {left, top}, {lw, height});
        font.set_on(lbl);
        id += 1;
        top += height + HGAP;
    }
}

bool UIAddTask::validate(spParamCtrl ctrl) {
    if (!ctrl)
        return false;
    bool ok;
    try {
        switch (ctrl->pref.type()) {
        case ai::Param::Void:
        case ai::Param::Task:
        case ai::Param::Array:
        case ai::Param::Bool:
            break;
        case ai::Param::Enum:
            if (auto c = dynamic_cast<EnumCtrl*>(ctrl.get()))
                ctrl->pref.set(c->index);
            break;
        case ai::Param::Int:
        case ai::Param::Real:
        case ai::Param::String:
        case ai::Param::System:
        case ai::Param::Dock:
        case ai::Param::Commodity:
            ctrl->pref.set(toUtf8(ctrl->text));
            break;
        }
        ok = ctrl->pref.valid();
    } catch (const std::exception& ex) {
        ok = false;
    }
    return ok;
}

UIAddTask::ParamCtrl::ParamCtrl(PRef ref, int id)
    : pref(ref)
    , id(id)
{
}

UIAddTask::ParamCtrl::~ParamCtrl() {
    if (label.hwnd())
        DestroyWindow(label.hwnd());
}
void UIAddTask::ParamCtrl::on_ctrl_edit() {
}

UIAddTask::BoolCtrl::~BoolCtrl() {
    if (cb.hwnd())
        DestroyWindow(cb.hwnd());
}
void UIAddTask::BoolCtrl::on_ctrl_edit() {
    pref.set(cb.is_checked());
}

UIAddTask::EnumCtrl::~EnumCtrl() {
    if (dl.hwnd())
        DestroyWindow(dl.hwnd());
}
void UIAddTask::EnumCtrl::on_ctrl_edit() {
    index = dl.get_selected_index();
}

UIAddTask::TextCtrl::~TextCtrl() {
    if (tb.hwnd())
        DestroyWindow(tb.hwnd());
}
void UIAddTask::TextCtrl::on_ctrl_edit() {
    text = tb.get_text();
}

UIAddTask::TaskCtrl::~TaskCtrl() {
}


UIAddTask::TaskCtrl::TaskCtrl(UIAddTask::PRef ref, int id)
        : UIAddTask::ParamCtrl(ref, id)
        , sub_templ(ai::TaskTemplate::loadTemplate(ref.value()))
{
}

ai::Param::Type UIAddTask::PRef::type() const {
    if (!idx)
        return param.type;
    assert (param.type == ai::Param::Array);
    return enum_cast<ai::Param::Type>(param.meta["elements"]["type"].as_string()).value();
}
bool UIAddTask::PRef::empty() const {
    if (!idx)
        return param.empty();
    assert (param.type == ai::Param::Array);
    if (param.empty())
        return true;
    if (idx <= 0 || idx > param.value.as_array().size())
        return true;
    auto& v = param.value.as_array()[idx-1];
    if (v.is_null())
        return true;
    if (v.is_string() && v.as_string().empty())
        return true;
    if (v.is_object() && v.as_object().empty())
        return true;
    if (v.is_array() && v.as_array().empty())
        return true;
    return false;
}
bool UIAddTask::PRef::valid() const {
    if (!idx)
        return param.valid();
    assert (param.type == ai::Param::Array);
    if (idx <= 0 || idx > param.value.as_array().size())
        return false;
    return true;
    //auto& v = param.value.as_array()[idx-1];
    //ai::Param p {type(), "", "", meta(), v};
    //return p.valid();
}
std::wstring UIAddTask::PRef::name() const {
    if (!idx)
        return toUtf16(param.name());
    return std::to_wstring(idx);
}
std::wstring UIAddTask::PRef::placeholder() const {
    return toUtf16(param.placeholder());
}
const json5pp::value& UIAddTask::PRef::meta() const {
    if (!idx)
        return param.meta;
    assert (param.type == ai::Param::Array);
    return param.meta["elements"];
}
const json5pp::value& UIAddTask::PRef::value() const {
    if (!idx)
        return param.value;
    assert (param.type == ai::Param::Array);
    return param.value.as_array()[idx-1];

}
bool UIAddTask::PRef::set(const json5pp::value& value) {
    if (!idx)
        return param.set(value);
    assert (param.type == ai::Param::Array);
    if (idx <= 0 || idx > param.value.as_array().size())
        return false;
    param.value.as_array()[idx-1] = value;
    return true;
}
bool UIAddTask::PRef::as_boolean() const {
    if (!idx)
        return param.as_boolean();
    assert (param.type == ai::Param::Array);
    if (idx <= 0 || idx > param.value.as_array().size())
        return false;
    return bool(param.value.as_array()[idx-1]);
}
const std::string UIAddTask::PRef::as_string() const {
    if (!idx)
        return param.as_string();
    assert (param.type == ai::Param::Array);
    if (idx <= 0 || idx > param.value.as_array().size())
        return {};
    const auto& v = param.value.as_array()[idx-1];
    if (v.is_string())
        return v.as_string();
    if (v.is_null())
        return {};
    if (v.is_integer())
        return std::to_string(v.as_int64());
    if (v.is_number())
        return std::to_string(v.as_number());
    if (v.is_boolean())
        return v.as_boolean() ? "true" : "false";
    return {};
}
