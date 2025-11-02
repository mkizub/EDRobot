//
// Created by mkizub on 27.06.2025.
//

#include "../pch.h"

#include "UIAddTask.h"

#include "../../ui/resource.h"

//static const wchar_t* gWindowClass = L"AddTaskWindowClass";
//static const wchar_t* gWindowName = L"EDRobot Add Task";

UIAddTask::UIAddTask() : aiManager(nullptr) {
    aiManager = Master::getInstance().getAIManager();
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
            aiManager->new_task(*curr_templ);
            templ_controls.clear();
            curr_templ = nullptr;
        }
        EndDialog(hwnd(), IDOK);
        return 0;
    });

}

int UIAddTask::initialize(wl::params &params) {
    for (auto& tt : aiManager->getUserTasks())
        templates.push_back(&tt);
    for (auto& tt : aiManager->getTemplates())
        templates.push_back(&tt);

    btn_ok.assign(hwnd(), IDOK);
    btn_cancel.assign(hwnd(), IDCANCEL);
    cb_tasks.assign(hwnd(), IDC_COMBO_TEMPLATES);
    for (auto t : templates) {
        cb_tasks.add({toUtf16(t->name).c_str()});
    }

    layoutResizer.add(cb_tasks, wl::resizer::go::RESIZE, wl::resizer::go::REPOS);

    btn_ok.set_enabled(false);

    return TRUE;
}

int UIAddTask::on_template_selected(wl::params &params) {
    if (HIWORD(params.wParam) != CBN_SELCHANGE)
        return TRUE;
    int idx = cb_tasks.get_selected_index();
    templ_controls.clear();
    curr_templ = nullptr;
    if (idx < 0 || idx >= templates.size())
        return TRUE;

    auto templ = templates[idx];
    templMap.try_emplace(templ->id, *templ);
    curr_templ = &templMap[templ->id];

    RECT rcCtrl{};
    GetWindowRect(cb_tasks.hwnd(), &rcCtrl);
    ScreenToClient(hwnd(), (LPPOINT)&rcCtrl.left);
    ScreenToClient(hwnd(), (LPPOINT)&rcCtrl.right);

    int x = rcCtrl.left;
    int y = rcCtrl.bottom + 10;
    int w = rcCtrl.right - rcCtrl.left;
    int id = ctrlId;
    for (auto& p : curr_templ->params) {
        add_ctrl(p, id, x, y, w);
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

int UIAddTask::on_ctrl_change(wl::params& p) {
    if (HIWORD(p.wParam) != EN_CHANGE && HIWORD(p.wParam) != BN_CLICKED && HIWORD(p.wParam) != CBN_SELCHANGE)
        return TRUE;
    if (!curr_templ)
        return TRUE;
    int id = LOWORD(p.wParam);
    if (id < ctrlId)
        return TRUE;
    for (auto& ctrl : templ_controls) {
        if (ctrl.id == id) {
           on_ctrl_edit(ctrl);
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

void UIAddTask::on_ctrl_edit(ParamCtrl& ctrl) {
    try {
        if (ctrl.param.type == ai::Param::Bool) {
            curr_templ->set(ctrl.param.name, ctrl.cb.is_checked());
        }
        else if (ctrl.param.type == ai::Param::Enum) {
            ctrl.text = ctrl.dl.get_selected_text();
        }
        else {
            ctrl.text = ctrl.tb.get_text();
        }
    } catch (const std::exception& ex) {
        // ignore
    }
}

void UIAddTask::add_ctrl(ai::Param &p, int &id, int &left, int &top, int width) {
    if (p.type == ai::Param::Bool) {
        ParamCtrl& ctrl = templ_controls.emplace_back(p, (int) id);
        wl::checkbox& cb = ctrl.cb;
        cb.create(hwnd(), id, toUtf16(p.name).c_str(), {left,top}, {width, 21});
        cb.set_check(std::get<bool>(p.value));
    }
    else if (p.type == ai::Param::Enum) {
        ParamCtrl& ctrl = templ_controls.emplace_back(p, (int) id);
        wl::combobox& dl = ctrl.dl;
        auto values = split(p.meta,'|');
        int y = top;
        int cx = left + width / 2;
        int w = width / 2 - 4;
        int h = std::max(75, 25 * std::min(11,(int)values.size()));
        //dl.create(hwnd(), id, {cx-w-2, y}, w, wl::combobox::sort::UNSORTED);
        dl.assign(CreateWindowExW(0, WC_COMBOBOX, nullptr,
                                  WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL,
                                  cx-w-2, y, w, h, hwnd(),
                                  reinterpret_cast<HMENU>(static_cast<UINT_PTR>(id)),
                                  reinterpret_cast<HINSTANCE>(GetWindowLongPtrW(hwnd(), GWLP_HINSTANCE)),
                                  nullptr));
        int select_idx = -1;
        for (int idx=0; idx < values.size(); idx++) {
            auto& val  = values[idx];
            dl.add({toUtf16(val).c_str()});
            if (val == std::get<std::string>(p.value)) {
                ctrl.text = toUtf16(val);
                select_idx = idx;
            }
        }
        if (select_idx >= 0)
            dl.select(select_idx);

        id += 1;
        wl::label& lbl = templ_controls.back().label;
        w = width / 2 - 4;
        lbl.create(hwnd(), id, toUtf16(p.name).c_str(), {cx+2, y}, {w, 21});
    }
    else {
        ParamCtrl& ctrl = templ_controls.emplace_back(p, (int) id);
        wl::textbox& tb = ctrl.tb;
        int w;
        int y = top;
        int cx = left + width / 2;

        if (p.type == ai::Param::Int) {
            int64_t val = std::get<int64_t>(p.value);
            if (val)
                ctrl.text = std::to_wstring(val);
            w = 50;
        }
        else if (p.type == ai::Param::Real) {
            double val = std::get<double>(p.value);
            if (!std::isnan(val) && val != 0)
                ctrl.text = std::to_wstring(std::get<double>(p.value));
            w = 70;
        }
        else if (p.type == ai::Param::Commodity) {
            auto commodity = Cfg.getCommodityByName(std::get<std::string>(p.value), false);
            if (commodity)
                ctrl.text = toUtf16(commodity->name);
            w = width / 2 - 4;
        }
        else {
            ctrl.text = toUtf16(std::get<std::string>(p.value));
            w = width / 2 - 4;
        }
        tb.create(hwnd(), id, wl::textbox::type::NORMAL, {cx-w-2, y}, w, 21);
        {
            LONG_PTR style = GetWindowLongPtr(tb.hwnd(), GWL_STYLE);
            style |= WS_TABSTOP;
            SetWindowLongPtr(tb.hwnd(), GWL_STYLE, style);
        }
        tb.set_text(ctrl.text);

        // label
        id += 1;
        wl::label& lbl = templ_controls.back().label;
        w = width / 2 - 4;
        lbl.create(hwnd(), id, toUtf16(p.name).c_str(), {cx+2, y}, {w, 21});
    }

    id += 1;
    top += 25;
}

bool UIAddTask::validate(ParamCtrl& ctrl) {
    bool ok = true;
    try {
        if (ctrl.param.type == ai::Param::Bool)
            ok = true;
        else if (ctrl.param.type == ai::Param::Enum) {
            std::string text = toUtf8(ctrl.text);
            curr_templ->set(ctrl.param.name, text);
            ok = std::ranges::contains(split(ctrl.param.meta, '|'), text) || ctrl.param.optional;
        }
        else if (ctrl.param.type == ai::Param::Int) {
            try {
                curr_templ->set(ctrl.param.name, std::stoll(ctrl.text));
                ok = std::get<int64_t>(ctrl.param.value) >= 0 || ctrl.param.optional;
            } catch (...) {}
        }
        else if (ctrl.param.type == ai::Param::Real) {
            try {
                curr_templ->set(ctrl.param.name, std::stod(ctrl.text));
                ok = std::isfinite(std::get<double>(ctrl.param.value)) || ctrl.param.optional;
            } catch (...) {}
        }
        else if (ctrl.param.type == ai::Param::String ||
                ctrl.param.type == ai::Param::System ||
                ctrl.param.type == ai::Param::POI ||
                ctrl.param.type == ai::Param::Dock)
        {
            curr_templ->set(ctrl.param.name, toUtf8(ctrl.text));
            ok = !ctrl.tb.get_text().empty() || ctrl.param.optional;
        }
        else if (ctrl.param.type == ai::Param::Commodity) {
            auto commodity = Cfg.getCommodityByName(ctrl.text, false);
            if (commodity) {
                curr_templ->set(ctrl.param.name, commodity->nameId);
            } else {
                std::string empty;
                curr_templ->set(ctrl.param.name, empty);
                ok = ctrl.param.optional;
            }
        }
    } catch (const std::exception& ex) {
        ok = false;
    }
    return ok;
}

UIAddTask::ParamCtrl::ParamCtrl(ai::Param &p, int id)
    : param(p)
    , id(id)
{
}

UIAddTask::ParamCtrl::~ParamCtrl() {
    BOOL ok;
    if (cb.hwnd()) {
        ok = DestroyWindow(cb.hwnd());
        LOG_IF(!ok,ERROR) << "Checkbox not destroyed: " << getErrorMessage();
    }
    if (dl.hwnd()) {
        ok = DestroyWindow(dl.hwnd());
        LOG_IF(!ok,ERROR) << "Combobox not destroyed: " << getErrorMessage();
    }
    if (tb.hwnd()) {
        ok = DestroyWindow(tb.hwnd());
        LOG_IF(!ok,ERROR) << "Textbox not destroyed: " << getErrorMessage();
    }
    if (label.hwnd()) {
        ok = DestroyWindow(label.hwnd());
        LOG_IF(!ok,ERROR) << "Label not destroyed: " << getErrorMessage();
    }
}
