//
// Created by mkizub on 28.06.2025.
//

#include "../pch.h"

#include "UIMainDialog.h"
#include "UIManager.h"
#include "UIAddTask.h"
#include "UIShowCargo.h"
#include "UILayout.h"
#include "../../ui/resource.h"

const int TRAY_ICONUID = 100;
const int WM_TRAY_NOTIFY = WM_APP + 100;

UIMainDialog::UIMainDialog()
    : mNotifyIconData{}
{
    setup.dialogId = IDD_ED_ROBOT;
    setup.iconId = IDI_DIALOGS;

    on_message(WM_INITDIALOG, [this](wl::params p) -> INT_PTR {
        initialize();
        return TRUE;
    });

    this->base_msg_pubm::on_message(WM_CLOSE, [this](wl::params) noexcept -> INT_PTR {
        hide(true);
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
            InsertMenu(hmenu, 0, MF_BYPOSITION | MF_STRING, IDM_EXIT, toUtf16(_gt("Exit")).c_str());
            SetForegroundWindow(this->hwnd());
            BringWindowToTop(this->hwnd());
            int cmd = TrackPopupMenu(hmenu,
                                     TPM_LEFTALIGN | TPM_LEFTBUTTON | TPM_BOTTOMALIGN | TPM_NONOTIFY | TPM_RETURNCMD,
                                     pt.x, pt.y, 0, this->hwnd(), nullptr);
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
        hide(false);
        return 0;
    });
    this->base_msg_pubm::on_command(IDM_COMMODITIES, [this](wl::params p){
        on_command_show_cargo();
        return 0;
    });
    this->base_msg_pubm::on_command(IDC_BUTTON_STOP_NEW, [this](wl::params p){
        on_command_stop_new();
        return 0;
    });
    this->base_msg_pubm::on_command(IDC_BUTTON_PAUSE_RESUME, [this](wl::params p){
        on_command_pause_resume();
        return 0;
    });
    this->base_msg_pubm::on_command(IDC_BUTTON_WATCH, [](wl::params p){
        UIManager::showDebugWindow();
        return 0;
    });
    this->base_msg_pubm::on_command(IDM_DEBUG_WATCH, [](wl::params p){
        UIManager::showDebugWindow();
        return 0;
    });
    this->base_msg_pubm::on_command(IDC_KEEP_ON_TOP, [this](wl::params p){
        if (cb_keep_on_top.is_checked()) {
            this->style.set_style_ex(true, WS_EX_TOPMOST);
        } else {
            this->style.set_style_ex(false, WS_EX_LAYERED|WS_EX_TOPMOST|WS_EX_TRANSPARENT);
            SetWindowPos(this->hwnd(), HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        }
        return 0;
    });
    this->base_msg_pubm::on_command(IDC_EXIT, [](wl::params p){
        Master::getInstance().pushCommand(Command::Shutdown);
        return 0;
    });
    this->base_msg_pubm::on_command(IDM_FILE_EXIT, [](wl::params p){
        Master::getInstance().pushCommand(Command::Shutdown);
        return 0;
    });
    this->base_msg_pubm::on_message(WM_TIMER, [this](wl::params p){
        update_curr_task();
        return 0;
    });
    on_message(WM_DPICHANGED, [this](wl::params params) {
        cv::Rect r = fromRECT(*(PRECT)params.lParam);
        SetWindowPos(this->hwnd(), HWND_TOPMOST, 0, 0, r.width, r.height, SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);
        LOG(INFO) << "on_dpi_change: DPI " << LOWORD(params.wParam) << " size " << r.size();
        relayout();
        return 0;
    });
    on_message(WM_SIZE, [this](wl::params params) {
        relayout();
        return 0;
    });
}

void UIMainDialog::initialize() {
    SetDialogDpiChangeBehavior(hwnd(), DDC_DISABLE_ALL, DDC_DISABLE_ALL);

    HINSTANCE hInstance = GetModuleHandle(nullptr);
    HMENU hMenu = LoadMenu(hInstance, MAKEINTRESOURCE(IDR_MENU));
    SetMenu(hwnd(), hMenu);

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

    lbl_task.assign(hwnd(), IDC_STATIC);
    lbl_curr_task.assign(hwnd(), IDC_CURRENT_TASK);
    lbl_task_status.assign(hwnd(), IDC_TASK_STATUS);
    lbl_status.assign(hwnd(), IDC_STATUS);
    cb_keep_on_top.assign(hwnd(), IDC_KEEP_ON_TOP);
    btn_stop_new.assign(hwnd(), IDC_BUTTON_STOP_NEW);
    btn_pause_resume.assign(hwnd(), IDC_BUTTON_PAUSE_RESUME);
    btn_ok.assign(hwnd(), IDOK);
    btn_watch.assign(hwnd(), IDC_BUTTON_WATCH);
    btn_exit.assign(hwnd(), IDC_EXIT);
    if (GetSystemMetrics(SM_CMONITORS) < 2)
        btn_watch.set_enabled(false);

    lbl_task.set_text(toUtf16(_gt("Task:")).c_str());
    cb_keep_on_top.set_text(toUtf16(_gt("Top")).c_str());
    btn_watch.set_text(toUtf16(_gt("Watch")).c_str());
    btn_exit.set_text(toUtf16(_gt("Exit")).c_str());

    int uiDpi = GetDpiForWindow(hwnd());
    int uiPercent = Cfg.getUiScalePercents();
    font.create(L"Segoe UI", MulDiv(LO_FONT_SIZE, uiDpi*uiPercent, 100*USER_DEFAULT_SCREEN_DPI));
    {
        auto hMonitor = MonitorFromWindow(hwnd(), MONITOR_DEFAULTTOPRIMARY);
        MONITORINFOEX monitorInfo;
        monitorInfo.cbSize = sizeof(monitorInfo);
        GetMonitorInfo(hMonitor, &monitorInfo);
        int w = MulDiv(450, uiDpi*uiPercent, 100*USER_DEFAULT_SCREEN_DPI);
        int h = MulDiv(600, uiDpi*uiPercent, 100*USER_DEFAULT_SCREEN_DPI);
        int border = MulDiv(50, uiDpi*uiPercent, 100*USER_DEFAULT_SCREEN_DPI);
        int l = monitorInfo.rcMonitor.right - border - w;
        int t = monitorInfo.rcMonitor.top + border;
        SetWindowPos(this->hwnd(), HWND_TOPMOST, l, t, w, h, SWP_NOOWNERZORDER);
    }

    update_curr_task();
}

bool UIMainDialog::show() {
    struct ClipboardLocker {
        ClipboardLocker() { OpenClipboard(nullptr); }
        ~ClipboardLocker() { CloseClipboard(); }
    } locker;
    if (this->style.has_style_ex(WS_EX_LAYERED|WS_EX_TOPMOST|WS_EX_TRANSPARENT))
        this->style.set_style_ex(false, WS_EX_LAYERED|WS_EX_TOPMOST|WS_EX_TRANSPARENT);
    ShowWindow(this->hwnd(), SW_RESTORE);
    SetForegroundWindow(this->hwnd());
    BringWindowToTop(this->hwnd());
    update_curr_task();
    return true;
}

bool UIMainDialog::hide(bool force) {
    if (!force && cb_keep_on_top.is_checked()) {
        if (GetSystemMetrics(SM_CMONITORS) > 1 || Cfg.getGameScreenMode() == Configuration::GameScreenMode::Window)
            return false;
        if (Cfg.getGameScreenMode() == Configuration::GameScreenMode::Borderless) {
            this->style.set_style_ex(true, WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TRANSPARENT);
            float transparency_percentage = 0.6f;
            SetLayeredWindowAttributes(hwnd(), 0, (BYTE) (255 * transparency_percentage), LWA_ALPHA);
            return false;
        }
        // Configuration::GameScreenMode::FullScreen => hide
    }
    if (this->style.has_style_ex(WS_EX_LAYERED|WS_EX_TOPMOST|WS_EX_TRANSPARENT))
        this->style.set_style_ex(false, WS_EX_LAYERED|WS_EX_TOPMOST|WS_EX_TRANSPARENT);
    ShowWindow(this->hwnd(), SW_HIDE);
    KillTimer(this->hwnd(), mUpdateTimerId);
    mUpdateTimerId = {};
    return true;
}

void UIMainDialog::on_command_stop_new() {
    try {
        ai::spTask task = ai::curr_task();
        if (!task) {
            UIAddTask addTaskDlg;
            int res = addTaskDlg.show(this);
            if (res == IDOK) {
                if (hide(false))
                    return;
            }
        } else {
            ai::stop();
        }
        update_curr_task();
    } catch (const std::system_error& ex) {
        LOG(ERROR) << "System error: code " << ex.code() << ": " << getErrorMessage(ex.code().value()) << ": " << ex.what();
    } catch (const std::exception& ex) {
        LOG(ERROR) << ex.what();
    }
}

void UIMainDialog::on_command_pause_resume() {
    try {
        ai::spTask task = ai::curr_task();
        if (!task) {
            // repeat
            task = ai::last_task();
            if (task && ai::new_task(task->templ)) {
                if (hide(false))
                    return;
            }
        } else {
            if (!ai::active() || ai::isDebugPause()) {
                // resume
                if (ai::isDebugPause())
                    ai::toggleDebugPause();
                if (!ai::active())
                    ai::resume();
                if (hide(false))
                    return;
            } else {
                // pause
                ai::interrupt(ai::InterruptReason::UNKNOWN);
            }
        }
        update_curr_task();
    } catch (const std::system_error& ex) {
        LOG(ERROR) << "System error: code " << ex.code() << ": " << getErrorMessage(ex.code().value()) << ": " << ex.what();
    } catch (const std::exception& ex) {
        LOG(ERROR) << ex.what();
    }
}

void UIMainDialog::on_command_show_cargo() {
    try {
        if (dlg_showCargo && !dlg_showCargo->isDestroyed) {
            SetForegroundWindow(dlg_showCargo->hwnd());
        } else {
            dlg_showCargo = std::make_unique<UIShowCargo>();
            dlg_showCargo->create(this);
        }
    } catch (const std::system_error& ex) {
        LOG(ERROR) << "System error: code " << ex.code() << ": " << getErrorMessage(ex.code().value()) << ": " << ex.what();
    } catch (const std::exception& ex) {
        LOG(ERROR) << ex.what();
    }
}

void UIMainDialog::update_curr_task() {
    ai::spTask task = ai::curr_task();
    if (!task) {
        lbl_curr_task.set_text(toUtf16(_gt("No active task")).c_str());
        btn_stop_new.set_text(toUtf16(_gt("New task")).c_str());
        btn_pause_resume.set_text(toUtf16(_gt("Repeat")).c_str());
        btn_pause_resume.set_enabled(bool(ai::last_task()));
    } else {
        lbl_curr_task.set_text(toUtf16(task->getTitle()).c_str());
        btn_stop_new.set_text(toUtf16(_gt("Stop")).c_str());
        if (ai::active() && !ai::isDebugPause())
            btn_pause_resume.set_text(toUtf16(_gt("Pause")).c_str());
        else
            btn_pause_resume.set_text(toUtf16(_gt("Resume")).c_str());
        btn_pause_resume.set_enabled(true);
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
            lbl_status.set_text(toUtf16(_gt("Finished (failed)")).c_str());
        else
            lbl_status.set_text(toUtf16(_gt("Finished")).c_str());
    }
    else if (!ai::active()) {
        lbl_status.set_text(toUtf16(_gt("Paused (inactive)")).c_str());
    }
    else if (ai::isDebugPause()) {
        lbl_status.set_text(toUtf16(_gt("Paused (active)")).c_str());
    }
    else if (st::ship.flags.docked) {
        lbl_status.set_text(toUtf16(_gt("Docked")).c_str());
    }
    else if (st::ship.flags.landed) {
        lbl_status.set_text(toUtf16(_gt("Landed")).c_str());
    }
    else if (st::ship.flags.fsd_jump) {
        lbl_status.set_text(toUtf16(_gt("Hyperspace")).c_str());
    }
    else {
        std::string space = st::ship.flags.cruise ? _gt("Cruise") : _gt("Space");
        std::string spd = "??";
        if (st::autopilot.speed_set_to.has_value())
            spd = std::to_string(st::autopilot.speed_set_to.value());
        std::string dist = "??";
        if (st::autopilot.isDestBodyTargeted && st::autopilot.distanceToBody)
            dist = st::autopilot.distanceToBody.to_string();
        if (st::autopilot.isDestDockTargeted && st::autopilot.distanceToDock)
            dist = st::autopilot.distanceToDock.to_string();
        lbl_status.set_text(toUtf16(lc_format("{}: speed {}%, distance {}", space, spd, dist)));
    }
    if (IsWindowVisible(hwnd())) {
        if (cb_keep_on_top.is_checked() && ai::curr_task())
            SetWindowPos(this->hwnd(), HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
        mUpdateTimerId = SetTimer(this->hwnd(), mUpdateTimerId, 800, NULL);
    } else {
        KillTimer(this->hwnd(), mUpdateTimerId);
        mUpdateTimerId = {};
    }
}

#define S(N) MulDiv((N), uiDpi*uiPercent, 100*USER_DEFAULT_SCREEN_DPI)
void UIMainDialog::relayout() {
    RECT rect{};
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
        font.set_on(lbl_task);
        font.set_on(lbl_curr_task);
        font.set_on(lbl_task_status);
        font.set_on(lbl_status);
        font.set_on(cb_keep_on_top);
        font.set_on(btn_stop_new);
        font.set_on(btn_pause_resume);
        font.set_on(btn_ok);
        font.set_on(btn_watch);
        font.set_on(btn_exit);
    }

    auto wpi = BeginDeferWindowPos(10);
    int x = l + S(LO_DLG_BORDER);
    int y = t + S(LO_DLG_BORDER);
    int w = S(LO_BTN_W);
    int h = S(LO_V_ROW);
    wpi = DeferWindowPos(wpi, lbl_task.hwnd(), nullptr, x, y, w, h, SWP_NOZORDER);

    x = l + S(LO_DLG_BORDER+LO_BTN_W+LO_H_GAP);
    w = width - x - S(LO_DLG_BORDER);
    wpi = DeferWindowPos(wpi, lbl_curr_task.hwnd(), nullptr, x, y, w, h, SWP_NOZORDER);

    y += S(LO_V_ROW+LO_V_GAP);

    x = l + S(LO_DLG_BORDER);
    w = width - S(2*LO_DLG_BORDER+2*LO_BTN_W+3*LO_H_GAP);
    h = S(LO_BTN_H);
    wpi = DeferWindowPos(wpi, cb_keep_on_top.hwnd(), nullptr, x, y, w, h, SWP_NOZORDER);

    w = S(LO_BTN_W);
    x = width - S(LO_DLG_BORDER+LO_BTN_W);
    wpi = DeferWindowPos(wpi, btn_pause_resume.hwnd(), nullptr, x, y, w, h, SWP_NOZORDER);

    x -= S(2*LO_H_GAP+LO_BTN_W);
    wpi = DeferWindowPos(wpi, btn_stop_new.hwnd(), nullptr, x, y, w, h, SWP_NOZORDER);

    int status_t = y + S(LO_BTN_H+LO_V_GAP);

    int cx = (l + r) / 2;

    w = S(LO_BTN_W);
    h = S(LO_BTN_H);
    y = b - S(LO_DLG_BORDER+LO_BTN_H);

    x = cx - w/2 - S(LO_H_GAP+LO_BTN_W);
    wpi = DeferWindowPos(wpi, btn_ok.hwnd(), nullptr, x, y, w, h, SWP_NOZORDER);
    x = cx - w/2;
    wpi = DeferWindowPos(wpi, btn_watch.hwnd(), nullptr, x, y, w, h, SWP_NOZORDER);
    x = cx + w/2 + S(LO_H_GAP);
    wpi = DeferWindowPos(wpi, btn_exit.hwnd(), nullptr, x, y, w, h, SWP_NOZORDER);

    y -= S(LO_V_GAP+LO_BTN_H);

    x = l + S(LO_DLG_BORDER);
    w = width - S(2*LO_DLG_BORDER);
    h = S(LO_V_ROW);
    wpi = DeferWindowPos(wpi, lbl_status.hwnd(), nullptr, x, y, w, h, SWP_NOZORDER);

    int status_b = y - S(LO_V_GAP);

    x = l + S(LO_DLG_BORDER);
    y = status_t;
    w = width - S(2*LO_DLG_BORDER);
    h = status_b - status_t;
    wpi = DeferWindowPos(wpi, lbl_task_status.hwnd(), nullptr, x, y, w, h, SWP_NOZORDER);

    EndDeferWindowPos(wpi);

    RedrawWindow(this->hwnd(), 0, 0, RDW_INVALIDATE | RDW_ALLCHILDREN);
    InvalidateRect(this->hwnd(), nullptr, true);
    UpdateWindow(this->hwnd());
}
#undef S
