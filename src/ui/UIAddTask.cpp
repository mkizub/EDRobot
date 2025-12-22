//
// Created by mkizub on 27.06.2025.
//

#include "../pch.h"

#include "UIAddTask.h"
#include "UILayout.h"

#include "../../ui/resource.h"

UIAddTask::UIAddTask() {
    setup.dialogId = IDD_ADD_TASK;

    on_message(WM_INITDIALOG, [this](wl::params p){
        initialize();
        return TRUE;
    });

    on_command(IDC_COMBO_TEMPLATES, [this](wl::params p) {
        if (HIWORD(p.wParam) == CBN_SELCHANGE)
            on_template_selected();
        return 0;
    });
    on_command(ID_RUN, [this](wl::params params) {
        on_template_run();
        EndDialog(hwnd(), IDOK);
        return 0;
    });
    on_command(IDCANCEL, [this](wl::params params) {
        EndDialog(hwnd(), IDCANCEL);
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
    on_message(WM_SIZE, [this](wl::params params) {
        relayout();
        return 0;
    });
}

void UIAddTask::initialize() {
    EnableNonClientDpiScaling(hwnd());

    int uiDpi = GetDpiForWindow(hwnd());
    int uiPercent = Cfg.getUiScalePercents();

    font.create(L"Segoe UI", MulDiv(LO_FONT_SIZE, uiDpi*uiPercent, 100*USER_DEFAULT_SCREEN_DPI));

    btn_run.assign(hwnd(), ID_RUN);
    btn_save.assign(hwnd(), ID_SAVE);
    btn_del.assign(hwnd(), ID_DELETE);
    cb_tasks.assign(hwnd(), IDC_COMBO_TEMPLATES);
    btn_run.set_enabled(false);
    btn_save.set_enabled(false);
    btn_del.set_enabled(false);
    btn_run.set_text(toUtf16(_gt("Run")));
    btn_save.set_text(toUtf16(_gt("Save")));
    btn_del.set_text(toUtf16(_gt("Delete")));

    taskEditor.create(hwnd(), IDC_TASK_PARAMETERS, {10,10}, {100, 100});
    taskEditor.validate_callback = [this](bool valid){ validate_callback(valid); };

    init_templ_list();
    relayout();
}

void UIAddTask::init_templ_list(std::string select) {
    cb_tasks.remove_all();
    templates.clear();
    for (auto& tt : ai::getUserTasks())
        templates.push_back(tt);
    for (auto& tt : ai::getTemplates())
        templates.push_back(tt);
    int select_index = -1;
    for (int i=0; i < templates.size(); i++) {
        auto& tt = templates[i];
        cb_tasks.add({toUtf16(tt.name()).c_str()});
        if (select_index < 0 && !select.empty() && select == tt.nm)
            select_index = i;
    }
    if (select_index >= 0)
        cb_tasks.select(select_index);
}

void UIAddTask::on_template_run() {
    if (taskEditor.validate()) {
        ai::new_task(taskEditor.makeTemplate());
        taskEditor.clear();
    }
}

void UIAddTask::on_template_save() {
    ai::TaskTemplate templ = taskEditor.makeTemplate();
    if (templ.id.empty())
        return;
    ai::saveUserTask(templ);
    init_templ_list(templ.nm);
}

void UIAddTask::on_template_delete() {
    auto selected_index = cb_tasks.get_selected_index();
    auto user_tasks_size = ai::getUserTasks().size();
    if (selected_index < user_tasks_size) {
        ai::delUserTask(selected_index);
        taskEditor.clear();
        init_templ_list();
    }
}

void UIAddTask::validate_callback(bool valid) {
    btn_run.set_enabled(valid);
    ai::TaskTemplate templ = taskEditor.makeTemplate();
    if (templ.id.empty() || templ.nm.empty()) {
        btn_save.set_enabled(false);
        btn_del.set_enabled(false);
        return;
    }

    auto selected_index = cb_tasks.get_selected_index();
    bool is_template = selected_index >= ai::getUserTasks().size();
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


#define S(N) MulDiv((N), uiDpi*uiPercent, 100*USER_DEFAULT_SCREEN_DPI)
void UIAddTask::relayout() {
    RECT rect{};
    //GetWindowRect(hwnd(), &rect);
    //ScreenToClient(hwnd(), reinterpret_cast<LPPOINT>(&rect.left));
    //ScreenToClient(hwnd(), reinterpret_cast<LPPOINT>(&rect.right));
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
        font.set_on(cb_tasks);
        font.set_on(btn_run);
        font.set_on(btn_save);
        font.set_on(btn_del);
    }

    auto wpi = BeginDeferWindowPos(10);
    int x = l + width*10/100;
    int y = t + S(LO_DLG_BORDER);
    int w = width*80/100;
    int h = height - t;
    wpi = DeferWindowPos(wpi, cb_tasks.hwnd(), nullptr, x, y, w, h, SWP_NOZORDER);

    int cx = (l + r) / 2;
    w = S(LO_BTN_W);
    h = S(LO_BTN_H);
    y = b - S(LO_DLG_BORDER+LO_BTN_H);

    x = cx - w/2 - S(LO_H_GAP+LO_BTN_W);
    wpi = DeferWindowPos(wpi, btn_run.hwnd(), nullptr, x, y, w, h, SWP_NOZORDER);
    x = cx - w/2;
    wpi = DeferWindowPos(wpi, btn_save.hwnd(), nullptr, x, y, w, h, SWP_NOZORDER);
    x = cx + w/2 + S(LO_H_GAP);
    wpi = DeferWindowPos(wpi, btn_del.hwnd(), nullptr, x, y, w, h, SWP_NOZORDER);

    x = l + S(LO_DLG_BORDER);
    y = t + S(LO_DLG_BORDER+LO_BTN_H+LO_V_GAP);
    w = width - S(2*LO_DLG_BORDER);
    h = height - S(2*LO_DLG_BORDER+2*LO_V_GAP+2*LO_BTN_H);
    wpi = DeferWindowPos(wpi, taskEditor.hwnd(), nullptr, x, y, w, h, SWP_NOZORDER);

    EndDeferWindowPos(wpi);

    RedrawWindow(this->hwnd(), 0, 0, RDW_INVALIDATE | RDW_ALLCHILDREN);
    InvalidateRect(this->hwnd(), nullptr, true);
    UpdateWindow(this->hwnd());
}
#undef S

void UIAddTask::on_template_selected() {
    int idx = cb_tasks.get_selected_index();
    taskEditor.clear();
    if (idx < 0 || idx >= templates.size())
        return;

    taskEditor.setTaskTemplate(templates[idx]);
    validate_callback(taskEditor.validate());

    RedrawWindow(this->hwnd(), 0, 0, RDW_INVALIDATE | RDW_ALLCHILDREN);
    InvalidateRect(hwnd(), nullptr, true);
    UpdateWindow(hwnd());
}
