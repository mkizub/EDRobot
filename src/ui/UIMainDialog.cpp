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

#define S(N) MulDiv((N), uiDpi*uiPercent, 100*USER_DEFAULT_SCREEN_DPI)

void loCreateFont(wl::font& font, UINT uiDpi, UINT uiPercent) {
    NONCLIENTMETRICS ncm{};
    ncm.cbSize = sizeof(ncm);
    SystemParametersInfoForDpi(SPI_GETNONCLIENTMETRICS, ncm.cbSize, &ncm, 0, MulDiv(uiDpi, uiPercent, 100));
    font.create(ncm.lfMessageFont); // Tahoma/Segoe
}

static cv::Rect rect_from_json(const js::value& v) {
    cv::Rect rect;
    rect.x = v.at(0,0).as_int();
    rect.y = v.at(1,0).as_int();
    rect.width = v.at(2,0).as_int();
    rect.height = v.at(3,0).as_int();
    return rect;
}

static js::value rect_to_json(const cv::Rect& r) {
    js::value jr = js::array({r.x, r.y, r.width, r.height});
    jr.add_flags(js::force::no_indent);
    return jr;
}

#define W(STR) toUtf16(gettext(STR)).c_str()

UIMainDialog::UIMainDialog()
    : mNotifyIconData{}
{
    HINSTANCE hInstance = GetModuleHandle(nullptr);

    setup.wndClassEx.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_DIALOGS));
    setup.wndClassEx.hIconSm = setup.wndClassEx.hIcon;
    setup.wndClassEx.lpszClassName = L"EDRobotMain";

    keepOnTop = Cfg.jprefs["ui"]["main"]["keepOnTop"].as_bool_or();
    cv::Rect wndRect = rect_from_json(Cfg.jprefs["ui"]["main"]["rect"]);
    POINT wndPosition {wndRect.x, wndRect.y};
    SIZE wndSize {wndRect.width, wndRect.height};
    if (wndRect.empty()) {
        int uiPercent = Cfg.getUiScalePercents();
        UINT uiDpi = USER_DEFAULT_SCREEN_DPI;
        auto hMonitor = MonitorFromPoint(wndPosition, MONITOR_DEFAULTTOPRIMARY);
        GetDpiForMonitor(hMonitor, MDT_EFFECTIVE_DPI, &uiDpi, &uiDpi);
        MONITORINFOEX monitorInfo {sizeof(monitorInfo)};
        GetMonitorInfo(hMonitor, &monitorInfo);
        int w = S(450);
        int h = S(600);
        int border = S(50);
        int l = monitorInfo.rcMonitor.right - border - w;
        int t = monitorInfo.rcMonitor.top + border;
        wndPosition = {l, t};
        wndSize = {w, h};
    }

    setup.title = L"EDRobot";
    setup.style = WS_CAPTION | WS_SYSMENU | WS_CLIPCHILDREN | WS_BORDER | WS_THICKFRAME;
    setup.exStyle = 0;
    setup.position = wndPosition;
    setup.size = wndSize;
    menu = CreateMenu();
    menu.append_submenu(W("Robot"))
            .append_item(IDM_TASK_NEW,W("New task"))
            .append_item(IDM_TASK_STOP,W("Stop"))
            .append_item(IDM_TASK_REPEAT,W("Repeat"))
            .append_item(IDM_TASK_PAUSE,W("Pause"))
            .append_item(IDM_TASK_RESUME,W("Resume"))
            .append_separator()
            .append_item(IDC_EXIT,W("Exit"))
            ;
    menu.append_submenu(W("More"))
            .append_item(IDM_COMMODITIES,W("Commodities"))
            .append_item(IDC_KEEP_ON_TOP,W("Keep on top")).set_item_check_by_id(IDC_KEEP_ON_TOP, keepOnTop)
            ;
    menu.append_submenu(W("Debug"))
            .append_item(IDM_DEBUG_WATCH,W("Watch"))
            ;

    setup.menu = menu.hmenu();

    if (keepOnTop)
        setup.exStyle = WS_EX_TOPMOST;

    on_message(WM_CREATE, [this](wl::params p) -> INT_PTR {
        initialize();
        return 0;
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
            InsertMenu(hmenu, 0, MF_BYPOSITION | MF_STRING, IDM_EXIT, W("Exit"));
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
    this->base_msg_pubm::on_command(IDM_TASK_NEW, [this](wl::params p){
        on_command_task_new();
        return 0;
    });
    this->base_msg_pubm::on_command(IDM_TASK_STOP, [this](wl::params p){
        on_command_task_stop();
        return 0;
    });
    this->base_msg_pubm::on_command(IDC_BUTTON_STOP_NEW, [this](wl::params p){
        if (ai::curr_task())
            on_command_task_stop();
        else
            on_command_task_new();
        return 0;
    });
    this->base_msg_pubm::on_command(IDM_TASK_REPEAT, [this](wl::params p){
        on_command_task_repeat();
        return 0;
    });
    this->base_msg_pubm::on_command(IDM_TASK_RESUME, [this](wl::params p){
        on_command_task_resume();
        return 0;
    });
    this->base_msg_pubm::on_command(IDM_TASK_PAUSE, [this](wl::params p){
        on_command_task_pause();
        return 0;
    });
    this->base_msg_pubm::on_command(IDC_BUTTON_PAUSE_RESUME, [this](wl::params p){
        if (!ai::curr_task())
            on_command_task_repeat();
        else if (!ai::active() || ai::isDebugPause())
            on_command_task_resume();
        else
            on_command_task_pause();
        return 0;
    });
    this->base_msg_pubm::on_command({IDC_BUTTON_WATCH,IDM_DEBUG_WATCH}, [](wl::params p){
        UIManager::showDebugWindow();
        return 0;
    });
    this->base_msg_pubm::on_command(IDC_KEEP_ON_TOP, [this](wl::params p){
        STD_PASTE;
        keepOnTop = !keepOnTop;
        menu.get_submenu(1).set_item_check_by_id(IDC_KEEP_ON_TOP, keepOnTop);
        if (keepOnTop) {
            this->style.set_style_ex(true, WS_EX_TOPMOST);
        } else {
            this->style.set_style_ex(false, WS_EX_LAYERED|WS_EX_TOPMOST|WS_EX_TRANSPARENT);
            SetWindowPos(this->hwnd(), HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        }
        savePrefs();
        return 0;
    });
    this->base_msg_pubm::on_command({IDC_EXIT,IDM_FILE_EXIT}, [](wl::params p){
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
        savePrefs();
        return 0;
    });
    on_message(WM_SIZE, [this](wl::params params) {
        relayout();
        return 0;
    });
    on_message(WM_EXITSIZEMOVE, [this](wl::params params) {
        savePrefs();
        return 0;
    });
}

void UIMainDialog::initialize() {
    initializing = true;
    SetDialogDpiChangeBehavior(hwnd(), DDC_DISABLE_ALL, DDC_DISABLE_ALL);

    UINT uiDpi = GetDpiForWindow(hwnd());
    UINT uiPercent = Cfg.getUiScalePercents();
    loCreateFont(font, uiDpi, uiPercent);

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

    lbl_task.create(hwnd(), IDC_STATIC, W("Task:"), {10, 10}, {80, 20})
            .style.set_style(true, SS_RIGHT);
    font.set_on(lbl_task);

    lbl_curr_task.create(hwnd(), IDC_CURRENT_TASK, L"", {100, 10}, {324, 20})
            .style.set_style(true, WS_BORDER | SS_CENTER);
    font.set_on(lbl_curr_task);

    btn_stop_new.create(hwnd(), IDC_BUTTON_STOP_NEW, "task-new", S(LO_ICN_S), {244,32}, {24,24});
    btn_pause_resume.create(hwnd(), IDC_BUTTON_PAUSE_RESUME, "task-repeat", S(LO_ICN_S), {344,32}, {24,24});

    lbl_task_status.create(hwnd(), IDC_TASK_STATUS, L"", {10,58}, {414,421})
            .style.set_style(true, WS_BORDER);
    font.set_on(lbl_task_status);

    lbl_status.create(hwnd(), IDC_STATUS, L"", {10,481}, {414,20})
            .style.set_style(true, WS_BORDER | WS_EX_TRANSPARENT);
    font.set_on(lbl_status);

    update_curr_task();
    initializing = false;
}

void UIMainDialog::savePrefs() {
    if (initializing)
        return;

    Cfg.jprefs["ui"]["main"]["keepOnTop"] = keepOnTop;

    cv::Rect rect;
    WINDOWPLACEMENT wp{sizeof(WINDOWPLACEMENT)};
    if (GetWindowPlacement(hwnd(), &wp))
        rect = fromRECT(wp.rcNormalPosition);

    if (!rect.empty())
        Cfg.jprefs["ui"]["main"]["rect"] = rect_to_json(rect);
    else
        Cfg.jprefs["ui"]["main"]["rect"] = nullptr;

    Cfg.savePrefs();
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
    if (!force && keepOnTop) {
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

void UIMainDialog::on_command_task_new() {
    try {
        UIAddTask addTaskDlg;
        int res = addTaskDlg.show(this);
        if (res == IDOK) {
            if (hide(false))
                return;
        }
        update_curr_task();
    } catch (const std::system_error& ex) {
        LOG(ERROR) << "System error: code " << ex.code() << ": " << getErrorMessage(ex.code().value()) << ": " << ex.what();
    } catch (const std::exception& ex) {
        LOG(ERROR) << ex.what();
    }
}
void UIMainDialog::on_command_task_stop() {
    try {
        ai::stop();
        update_curr_task();
    } catch (const std::system_error& ex) {
        LOG(ERROR) << "System error: code " << ex.code() << ": " << getErrorMessage(ex.code().value()) << ": " << ex.what();
    } catch (const std::exception& ex) {
        LOG(ERROR) << ex.what();
    }
}
void UIMainDialog::on_command_task_resume() {
    try {
        ai::spTask task = ai::curr_task();
        if (task && (!ai::active() || ai::isDebugPause())) {
            if (ai::isDebugPause())
                ai::toggleDebugPause();
            if (!ai::active())
                ai::resume();
            if (hide(false))
                return;
        }
        update_curr_task();
    } catch (const std::system_error& ex) {
        LOG(ERROR) << "System error: code " << ex.code() << ": " << getErrorMessage(ex.code().value()) << ": " << ex.what();
    } catch (const std::exception& ex) {
        LOG(ERROR) << ex.what();
    }
}
void UIMainDialog::on_command_task_repeat() {
    try {
        ai::spTask task = ai::curr_task();
        if (!task) {
            task = ai::last_task();
            if (task && ai::new_task(task->templ)) {
                if (hide(false))
                    return;
            }
        }
        update_curr_task();
    } catch (const std::system_error& ex) {
        LOG(ERROR) << "System error: code " << ex.code() << ": " << getErrorMessage(ex.code().value()) << ": " << ex.what();
    } catch (const std::exception& ex) {
        LOG(ERROR) << ex.what();
    }
}
void UIMainDialog::on_command_task_pause() {
    try {
        ai::spTask task = ai::curr_task();
        if (task) {
            ai::interrupt(ai::InterruptReason::UNKNOWN);
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
        if (auto dlg = UIShowCargo::getInstance())
            SetForegroundWindow(dlg->hwnd());
        else
            UIShowCargo::makeInstance();
    } catch (const std::system_error& ex) {
        LOG(ERROR) << "System error: code " << ex.code() << ": " << getErrorMessage(ex.code().value()) << ": " << ex.what();
    } catch (const std::exception& ex) {
        LOG(ERROR) << ex.what();
    }
}

void UIMainDialog::update_curr_task() {
    if (!ai::curr_task() && !ai::last_task() && !startup_message.empty()) {
        lbl_task_status.set_text(toUtf16("\n\n\n"+startup_message));
        lbl_task_status.style.set_style(true, SS_CENTER);

        std::string version;
        if (latest_version == EDROBOT_VERSION)
            version = lc_format("Version: {}", EDROBOT_VERSION);
        else
            version = lc_format("Version: {}, available {}", EDROBOT_VERSION, latest_version);

        lbl_status.set_text(toUtf16(version));
        lbl_status.style.set_style(true, SS_CENTER);
        return;
    }
    else if (!startup_message.empty()) {
        startup_message.clear();
        lbl_task_status.style.set_style(false, SS_CENTER);
        lbl_status.style.set_style(false, SS_CENTER);
    }

    ai::spTask task = ai::curr_task();
    if (!task) {
        lbl_curr_task.set_text(toUtf16(_gt("No active task")).c_str());
        btn_stop_new.set_icon("task-new");

        btn_pause_resume.set_icon("task-repeat");
        btn_pause_resume.set_enabled(bool(ai::last_task()));
    } else {
        lbl_curr_task.set_text(toUtf16(task->getTitle()).c_str());
        btn_stop_new.set_icon("task-stop");

        if (ai::active() && !ai::isDebugPause()) {
            btn_pause_resume.set_icon("task-pause");
        } else {
            btn_pause_resume.set_icon("task-resume");
        }
        btn_pause_resume.set_enabled(true);
    }
    bool completed = false;
    bool failed = false;
    if (!task) {
        task = ai::last_task();
        completed = true;
        failed = task && task->progress == ai::TaskExitReason::FAILED;
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
        if (task->progress == ai::TaskExitReason::FAILED)
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
        if (keepOnTop && ai::curr_task())
            SetWindowPos(this->hwnd(), HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
        mUpdateTimerId = SetTimer(this->hwnd(), mUpdateTimerId, 800, NULL);
    } else {
        KillTimer(this->hwnd(), mUpdateTimerId);
        mUpdateTimerId = {};
    }
}

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
        loCreateFont(font, uiDpi, uiPercent);
        font.set_on(lbl_task);
        font.set_on(lbl_curr_task);
        font.set_on(lbl_task_status);
        font.set_on(lbl_status);
        btn_stop_new.set_icon_size(S(LO_ICN_S));
        btn_pause_resume.set_icon_size(S(LO_ICN_S));
    }

    auto wpi = BeginDeferWindowPos(10);
    int x = l + S(LO_DLG_BORDER);
    int y = t + S(LO_DLG_BORDER);
    int w = S(LO_TXT_6_W);
    int h = S(LO_V_ROW);
    wpi = DeferWindowPos(wpi, lbl_task.hwnd(), nullptr, x, y, w, h, SWP_NOZORDER);

    x = l + S(LO_DLG_BORDER+LO_TXT_6_W+LO_H_GAP);
    w = width - x - S(LO_DLG_BORDER+LO_H_GAP+2*LO_BTN_H);
    wpi = DeferWindowPos(wpi, lbl_curr_task.hwnd(), nullptr, x, y, w, h, SWP_NOZORDER);

    w = S(LO_BTN_H);
    x = width - S(LO_DLG_BORDER+LO_BTN_H);
    wpi = DeferWindowPos(wpi, btn_pause_resume.hwnd(), nullptr, x, y, w, h, SWP_NOZORDER);

    x -= S(LO_H_GAP/2+LO_BTN_H);
    wpi = DeferWindowPos(wpi, btn_stop_new.hwnd(), nullptr, x, y, w, h, SWP_NOZORDER);

    int status_t = y + S(LO_V_ROW+LO_V_GAP);

    int cx = (l + r) / 2;

    w = S(LO_BTN_W);
    h = S(LO_BTN_H);
    y = b - S(LO_DLG_BORDER+LO_BTN_H);

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
