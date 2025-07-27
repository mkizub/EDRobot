//
// Created by mkizub on 28.06.2025.
//

#include "../pch.h"

#include "Main.h"
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
        case WM_RBUTTONDOWN:
            // Handle right-click (e.g., display context menu)
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
    this->base_msg_pubm::on_command(IDC_BUTTON_CURRENT_TASK, [this](wl::params p){
        curr_task_command(p);
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
    btn_curr_task.assign(hwnd(), IDC_BUTTON_CURRENT_TASK);

    update_curr_task();

    return TRUE;
}

bool Main::show() {
    ShowWindow(this->hwnd(), SW_RESTORE);
    SetForegroundWindow(this->hwnd());
    return true;
}

bool Main::hide() {
    ShowWindow(this->hwnd(), SW_HIDE);
    return true;
}

int Main::curr_task_command(wl::params &params) {
    try {
        auto &curr_templ = aiManager->curr_task();
        if (curr_templ.name.empty()) {
            hide();
            AddTask addTaskDlg;
            int res = addTaskDlg.show(this);
            if (res == IDOK)
                return TRUE;
            show();
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

void Main::update_curr_task() {
    auto& curr_templ = aiManager->curr_task();
    if (curr_templ.name.empty()) {
        lbl_curr_task.set_text(L"No active task");
        btn_curr_task.set_text(L"New");
    } else {
        lbl_curr_task.set_text(toUtf16(curr_templ.name).c_str());
        btn_curr_task.set_text(L"Stop");
    }
}
