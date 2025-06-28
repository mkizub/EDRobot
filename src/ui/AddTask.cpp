//
// Created by mkizub on 27.06.2025.
//

#include "../pch.h"

#include "AddTask.h"

#include "../../ui/resource.h"

//static const wchar_t* gWindowClass = L"AddTaskWindowClass";
//static const wchar_t* gWindowName = L"EDRobot Add Task";

AddTask::AddTask() : aiManager(nullptr) {
    aiManager = Master::getInstance().getAIManager();
    setup.dialogId = IDD_ADD_TASK;

    on_message(WM_INITDIALOG, [this](wl::params p){return initialize(p);});

    on_command(IDC_COMBO_TEMPLATES, [this](wl::params p) { return on_template_selected(p); });
    for (int i=0; i < 20; i++)
        on_command(ctrlId+i, [this](wl::params p) { return on_ctrl_change(p); });

    on_command(IDCANCEL, [this](wl::params params) {
        templ_controls.clear();
        curr_templ = nullptr;
        EndDialog(hwnd(), params.message);
        return (INT_PTR) TRUE;
    });
    on_command(IDOK, [this](wl::params params) {
        if (curr_templ) {
            aiManager->new_task(*curr_templ);
            templ_controls.clear();
            curr_templ = nullptr;
        }
        EndDialog(hwnd(), params.message);
        return (INT_PTR) TRUE;
    });

}

int AddTask::initialize(wl::params &params) {
    templates = aiManager->getTaskTemplates();

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

int AddTask::on_template_selected(wl::params &params) {
    if (HIWORD(params.wParam) != CBN_SELCHANGE)
        return TRUE;
    int idx = cb_tasks.get_selected_index();
    templ_controls.clear();
    curr_templ = nullptr;
    if (idx < 0 || idx >= templates.size())
        return TRUE;

    ai::TaskTemplate* templ = templates[idx];
    templMap.try_emplace(templ->name, *templ);
    curr_templ = &templMap[templ->name];

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

int AddTask::on_ctrl_change(wl::params& p) {
    if (HIWORD(p.wParam) != EN_CHANGE)
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

void AddTask::on_ctrl_edit(ParamCtrl& ctrl) {
    try {
        if (ctrl.param.type == ai::Param::Bool) {
            curr_templ->set(ctrl.param.name, ctrl.cb.is_checked());
        } else {
            ctrl.text = ctrl.tb.get_text();
        }
    } catch (const std::exception& ex) {
        // ignore
    }
}

void AddTask::add_ctrl(ai::Param &p, int &id, int &left, int &top, int width) {
    if (p.type == ai::Param::Bool) {
        templ_controls.emplace_back(p, (int) id);
        wl::checkbox& cb = templ_controls.back().cb;
        cb.create(hwnd(), id, toUtf16(p.name).c_str(), {left,top}, {width, 21});
        cb.set_check(std::get<bool>(p.value));
    } else {
        templ_controls.emplace_back(p, (int) id);
        ParamCtrl& ctrl = templ_controls.back();
        wl::textbox& tb = ctrl.tb;
        int y = top;
        int cx = left + width / 2;
        if (p.type == ai::Param::Int) {
            int w = 50;
            tb.create(hwnd(), id, wl::textbox::type::NORMAL, {cx-w-2, y}, w, 21);
            ctrl.text = std::to_wstring(std::get<int64_t>(p.value));
        }
        else if (p.type == ai::Param::Real) {
            int w = 70;
            tb.create(hwnd(), id, wl::textbox::type::NORMAL, {cx-w-2, y}, w, 21);
            ctrl.text = std::to_wstring(std::get<double>(p.value));
        }
        else if (p.type == ai::Param::Commodity) {
            int w = width / 2 - 4;
            tb.create(hwnd(), id, wl::textbox::type::NORMAL, {cx-w-2, y}, w, 21);
            auto commodity = Master::getInstance().getConfiguration()->getCommodityByName(std::get<std::string>(p.value), false);
            if (commodity) {
                ctrl.text = toUtf16(commodity->name);
            }
        }
        else {
            int w = width / 2 - 4;
            tb.create(hwnd(), id, wl::textbox::type::NORMAL, {cx-w-2, y}, w, 21);
            ctrl.text = toUtf16(std::get<std::string>(p.value));
        }
        tb.set_text(ctrl.text);
        id += 1;
        wl::label& lbl = templ_controls.back().label;
        int w = width / 2 - 4;
        lbl.create(hwnd(), id, toUtf16(p.name).c_str(), {cx+2, y}, {w, 21});
    }

    id += 1;
    top += 25;
}

bool AddTask::validate(ParamCtrl& ctrl) {
    bool ok = true;
    try {
        if (ctrl.param.type == ai::Param::Int) {
            curr_templ->set(ctrl.param.name, std::stoll(ctrl.text));
            ok = std::get<int64_t>(ctrl.param.value) >= 0;
        }
        else if (ctrl.param.type == ai::Param::Real) {
            curr_templ->set(ctrl.param.name, std::stod(ctrl.text));
            ok = std::isfinite(std::get<double>(ctrl.param.value));
        }
        else if (ctrl.param.type == ai::Param::String ||
                ctrl.param.type == ai::Param::Star ||
                ctrl.param.type == ai::Param::POI ||
                ctrl.param.type == ai::Param::Dock)
        {
            curr_templ->set(ctrl.param.name, toUtf8(ctrl.text));
            ok = !ctrl.tb.get_text().empty();
        }
        else if (ctrl.param.type == ai::Param::Commodity) {
            auto commodity = Master::getInstance().getConfiguration()->getCommodityByName(ctrl.text, false);
            if (commodity) {
                curr_templ->set(ctrl.param.name, commodity->nameId);
            } else {
                curr_templ->set(ctrl.param.name, "");
                ok = false;
            }
        }
    } catch (const std::exception& ex) {
        ok = false;
    }
    return ok;
}

AddTask::ParamCtrl::ParamCtrl(ai::Param &p, int id)
    : param(p)
    , id(id)
{
}

AddTask::ParamCtrl::~ParamCtrl() {
    BOOL ok;
    if (cb.hwnd()) {
        ok = DestroyWindow(cb.hwnd());
        LOG_IF(!ok,ERROR) << "Checkbox not destroyed: " << getErrorMessage();
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
