//
// Created by mkizub on 28.06.2025.
//

#include "../pch.h"

#include "Main.h"
#include "UIManager.h"
#include "AddTask.h"
#include "../../ui/resource.h"

#define TRAY_ICONUID 100
#define WM_TRAY_NOTIFY WM_APP + 100

Main::Main()
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


int Main::initialize(wl::params &params) {
    aiManager = Master::getInstance().getAIManager();

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
    btn_stop_new.assign(hwnd(), IDC_BUTTON_STOP_NEW);
    btn_pause_resume.assign(hwnd(), IDC_BUTTON_PAUSE_RESUME);
    btn_watch.assign(hwnd(), IDC_BUTTON_WATCH);

    update_curr_task();

    return TRUE;
}

bool Main::show() {
    ShowWindow(this->hwnd(), SW_RESTORE);
    SetForegroundWindow(this->hwnd());
    mUpdateTimerId = SetTimer(this->hwnd(), mUpdateTimerId, 800, NULL);
    return true;
}

bool Main::hide() {
    ShowWindow(this->hwnd(), SW_HIDE);
    KillTimer(this->hwnd(), mUpdateTimerId);
    mUpdateTimerId = {};
    return true;
}

int Main::on_command_stop_new(wl::params &params) {
    try {
        ai::spTask task = aiManager->activeTask;
        if (!task) {
            //hide();
            AddTask addTaskDlg;
            int res = addTaskDlg.show(this);
            if (res == IDOK)
                return TRUE;
            //show();
        } else {
            aiManager->stop();
        }
        update_curr_task();
    } catch (const std::system_error& ex) {
        LOG(ERROR) << "System error: code " << ex.code() << ": " << getErrorMessage(ex.code().value()) << ": " << ex.what();
    } catch (const std::exception& ex) {
        LOG(ERROR) << ex.what();
    }
    return TRUE;
}

int Main::on_command_pause_resume(wl::params &params) {
    try {
        ai::spTask task = aiManager->activeTask;
        if (!task) {
            hide();
            AddTask addTaskDlg;
            int res = addTaskDlg.show(this);
            show();
            if (res == IDOK)
                return TRUE;
        } else {
            if (aiManager->active())
                aiManager->interrupt();
            else
                aiManager->resume();
        }
        update_curr_task();
    } catch (const std::system_error& ex) {
        LOG(ERROR) << "System error: code " << ex.code() << ": " << getErrorMessage(ex.code().value()) << ": " << ex.what();
    } catch (const std::exception& ex) {
        LOG(ERROR) << ex.what();
    }
    return TRUE;
}

void Main::update_curr_task() {
    ai::spTask task = aiManager->curr_task();
    if (!task)
        task = aiManager->lastTask;
    if (!task) {
        lbl_curr_task.set_text(L"No active task");
        btn_stop_new.set_text(L"New");
        btn_pause_resume.set_text(L"Repeat");
    } else {
        lbl_curr_task.set_text(toUtf16(task->templ.name).c_str());
        btn_stop_new.set_text(L"Stop");
        if (aiManager->active())
            btn_pause_resume.set_text(L"Pause");
        else
            btn_pause_resume.set_text(L"Resume");
    }
    std::string status;
    int indent = 0;
    for (ai::spStep step=task; step; step = step->currentSubStep) {
        status += std::string(indent, ' ');
        status += step->getName();
        status += ":\n";
        indent += 4;
        for (auto& msg : step->getMessages()) {
            status += std::string(indent, ' ');
            status += msg;
            status += "\n";
        }
        for (auto& msg : split(step->getStatus(), '\n')) {
            status += std::string(indent, ' ');
            status += msg;
            status += "\n";
        }
    }
    lbl_task_status.set_text(toUtf16(status));
    mUpdateTimerId = SetTimer(this->hwnd(), mUpdateTimerId, 800, NULL);
}
