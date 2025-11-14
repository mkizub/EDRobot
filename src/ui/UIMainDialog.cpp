//
// Created by mkizub on 28.06.2025.
//

#include "../pch.h"

#include "UIMainDialog.h"
#include "UIManager.h"
#include "UIAddTask.h"
#include "../../ui/resource.h"

#define TRAY_ICONUID 100
#define WM_TRAY_NOTIFY WM_APP + 100

UIMainDialog::UIMainDialog()
    : mNotifyIconData{}
{
    setup.dialogId = IDD_ED_ROBOT;
    setup.iconId = IDI_DIALOGS;

    this->base_msg_pubm::on_message(WM_INITDIALOG, [this](wl::params p){return initialize(p);});

    this->base_msg_pubm::on_message(WM_CLOSE, [this](wl::params) noexcept -> INT_PTR {
        hide();
        return TRUE;
    });
    this->base_msg_pubm::on_message(WM_DESTROY, [this](wl::params) noexcept -> INT_PTR {
        Shell_NotifyIcon(NIM_DELETE, &mNotifyIconData);
        return FALSE;
    });
    this->base_msg_pubm::on_message(WM_TRAY_NOTIFY, [this](wl::params params) noexcept -> INT_PTR {
        switch (LOWORD(params.lParam)) // Check the mouse event
        {
        case WM_LBUTTONDOWN:
            show();
            break;
        case WM_RBUTTONDOWN: {
            const int IDM_EXIT = 100;
            POINT pt;
            GetCursorPos(&pt);
            HMENU hmenu = CreatePopupMenu();
            InsertMenu(hmenu, 0, MF_BYPOSITION | MF_STRING, IDM_EXIT, L"Exit");
            SetForegroundWindow(this->hwnd());
            int cmd = TrackPopupMenu(hmenu,
                                     TPM_LEFTALIGN | TPM_LEFTBUTTON | TPM_BOTTOMALIGN | TPM_NONOTIFY | TPM_RETURNCMD,
                                     pt.x, pt.y, 0, this->hwnd(), NULL);
            PostMessage(this->hwnd(), WM_NULL, 0, 0);
            if (cmd == IDM_EXIT)
                Master::getInstance().pushCommand(Command::Shutdown);
        }
            break;
        case WM_LBUTTONDBLCLK:
            // Handle double-click
            break;
        }
        return FALSE;
    });
    this->base_msg_pubm::on_command({IDOK,IDCANCEL}, [this](wl::params params) noexcept -> INT_PTR {
        hide();
        return 0;
    });
    this->base_msg_pubm::on_command(IDC_BUTTON_STOP_NEW, [this](wl::params p){
        on_command_stop_new(p);
        return 0;
    });
    this->base_msg_pubm::on_command(IDC_BUTTON_PAUSE_RESUME, [this](wl::params p){
        on_command_pause_resume(p);
        return 0;
    });
    this->base_msg_pubm::on_command(IDC_BUTTON_WATCH, [this](wl::params p){
        UIManager::showDebugWindow();
        return 0;
    });
    this->base_msg_pubm::on_message(WM_TIMER, [this](wl::params p){
        update_curr_task();
        return 0;
    });
}


int UIMainDialog::initialize(wl::params &params) {
    HINSTANCE hInstance = GetModuleHandle(nullptr);
    mNotifyIconData.cbSize = sizeof(NOTIFYICONDATA);
    mNotifyIconData.hWnd = hwnd();
    mNotifyIconData.uID = TRAY_ICONUID;
    mNotifyIconData.uVersion = NOTIFYICON_VERSION;
    mNotifyIconData.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_DIALOGS));
    LoadString(hInstance, IDS_APP_TITLE, mNotifyIconData.szTip, 128);
    mNotifyIconData.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    mNotifyIconData.uCallbackMessage = WM_TRAY_NOTIFY;
    BOOL ok = Shell_NotifyIcon(NIM_ADD, &mNotifyIconData);
    LOG_IF(!ok,ERROR) << "Failed to set tray icon";

    lbl_curr_task.assign(hwnd(), IDC_CURRENT_TASK);
    lbl_task_status.assign(hwnd(), IDC_TASK_STATUS);
    lbl_status.assign(hwnd(), IDC_STATUS);
    btn_stop_new.assign(hwnd(), IDC_BUTTON_STOP_NEW);
    btn_pause_resume.assign(hwnd(), IDC_BUTTON_PAUSE_RESUME);
    btn_watch.assign(hwnd(), IDC_BUTTON_WATCH);

    update_curr_task();

    return TRUE;
}

bool UIMainDialog::show() {
    ShowWindow(this->hwnd(), SW_RESTORE);
    SetForegroundWindow(this->hwnd());
    mUpdateTimerId = SetTimer(this->hwnd(), mUpdateTimerId, 800, NULL);
    return true;
}

bool UIMainDialog::hide() {
    ShowWindow(this->hwnd(), SW_HIDE);
    KillTimer(this->hwnd(), mUpdateTimerId);
    mUpdateTimerId = {};
    return true;
}

int UIMainDialog::on_command_stop_new(wl::params &params) {
    try {
        ai::spTask task = ai::curr_task();
        if (!task) {
            //hide();
            UIAddTask addTaskDlg;
            int res = addTaskDlg.show(this);
            if (res == IDOK)
                return TRUE;
            //show();
        } else {
            ai::stop();
        }
        update_curr_task();
    } catch (const std::system_error& ex) {
        LOG(ERROR) << "System error: code " << ex.code() << ": " << getErrorMessage(ex.code().value()) << ": " << ex.what();
    } catch (const std::exception& ex) {
        LOG(ERROR) << ex.what();
    }
    return TRUE;
}

int UIMainDialog::on_command_pause_resume(wl::params &params) {
    try {
        ai::spTask task = ai::curr_task();
        if (!task) {
            hide();
            UIAddTask addTaskDlg;
            int res = addTaskDlg.show(this);
            show();
            if (res == IDOK)
                return TRUE;
        } else {
            if (ai::active())
                ai::interrupt();
            else
                ai::resume();
        }
        update_curr_task();
    } catch (const std::system_error& ex) {
        LOG(ERROR) << "System error: code " << ex.code() << ": " << getErrorMessage(ex.code().value()) << ": " << ex.what();
    } catch (const std::exception& ex) {
        LOG(ERROR) << ex.what();
    }
    return TRUE;
}

void addStepStatus(std::string& status, int indent, ai::spStep step) {
    if (!step)
        return;
    status += std::string(indent, ' ');
    status += step->getTitle();
    status += ":\n";
    indent += 4;
    for (auto& msg : step->getMessages()) {
        if (msg.empty())
            continue;
        status += std::string(indent, ' ');
        status += msg;
        status += "\n";
    }
    for (auto& msg : split(step->getStatus(), '\n')) {
        if (msg.empty())
            continue;
        status += std::string(indent, ' ');
        status += msg;
        status += "\n";
    }
}

void UIMainDialog::update_curr_task() {
    ai::spTask task = ai::curr_task();
    if (!task) {
        lbl_curr_task.set_text(L"No active task");
        btn_stop_new.set_text(L"New");
        btn_pause_resume.set_text(L"Repeat");
    } else {
        lbl_curr_task.set_text(toUtf16(task->getTitle()).c_str());
        btn_stop_new.set_text(L"Stop");
        if (ai::active())
            btn_pause_resume.set_text(L"Pause");
        else
            btn_pause_resume.set_text(L"Resume");
    }
    bool completed = false;
    bool failed = false;
    if (!task) {
        task = ai::last_task();
        completed = true;
        failed = task && task->failed;
    }
    std::string status;
    int indent = 0;
    for (ai::spStep step=task; step; step = step->currSubStep) {
        status += std::string(indent, ' ');
        status += step->getTitle();
        status += ":\n";
        indent += 4;
        if (step->prevSubStep) {
            status += std::string(indent, ' ');
            status += step->prevSubStep->getTitle();
            status += "\n";
            for (auto& msg : step->prevSubStep->getMessages()) {
                if (msg.empty())
                    continue;
                status += std::string(indent+4, ' ');
                status += msg;
                status += "\n";
            }
        }
        if (!step->currSubStep || failed) {
            for (auto& msg : step->getMessages()) {
                if (msg.empty())
                    continue;
                status += std::string(indent, ' ');
                status += msg;
                status += "\n";
            }
            for (auto& msg : split(step->getStatus(), '\n')) {
                if (msg.empty())
                    continue;
                status += std::string(indent, ' ');
                status += msg;
                status += "\n";
            }
        }
        if (failed)
            break;
    }
    lbl_task_status.set_text(toUtf16(status));

    if (!task) {
        lbl_status.set_text(L"");
    }
    else if (completed) {
        if (task->failed)
            lbl_status.set_text(L"Finished (failed)");
        else
            lbl_status.set_text(L"Finished");
    }
    else if (!ai::active()) {
        lbl_status.set_text(L"Paused (inactive)");
    }
    else if (ai::isDebugPause()) {
        lbl_status.set_text(L"Paused (active)");
    }
    else if (st::ship.flags.docked) {
        lbl_status.set_text(L"Docked");
    }
    else if (st::ship.flags.landed) {
        lbl_status.set_text(L"Landed");
    }
    else if (st::ship.flags.fsd_jump) {
        lbl_status.set_text(L"Hyperspace");
    }
    else {
        std::string space = st::ship.flags.cruise ? "Cruise" : "Space";
        std::string spd = "??";
        if (st::autopilot.speed_set_to.has_value())
            spd = std::to_string(st::autopilot.speed_set_to.value());
        std::string dist = "??";
        if (st::autopilot.isDestBodyTargeted && st::autopilot.distanceToBody.valid())
            dist = st::autopilot.distanceToBody.to_string();
        if (st::autopilot.isDestDockTargeted && st::autopilot.distanceToDock.valid())
            dist = st::autopilot.distanceToDock.to_string();
        lbl_status.set_text(toUtf16(std::format("{}: spd {}%, dist {}", space, spd, dist)));
    }
    mUpdateTimerId = SetTimer(this->hwnd(), mUpdateTimerId, 800, NULL);
}
