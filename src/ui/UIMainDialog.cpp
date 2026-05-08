//
// Created by mkizub on 28.06.2025.
//

#include "../pch.h"

#include "UIMainDialog.h"
#include "UIControl.h"
#include "UIControlDialog.h"
#include "UIManager.h"
#include "UIShowStartup.h"
#include "UIShowTask.h"
#include "UIEditTask.h"
#include "UIShowCargo.h"
#include "UIEditBookmarks.h"
#include "UIEditSystem.h"
#include "UILayout.h"
#include "../net/RavenColonial.h"
#include "../../ui/resource.h"

const int TRAY_ICONUID = 100;
const int WM_TRAY_NOTIFY = WM_APP + 100;

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

#define W(STR) toUtf16(STR).c_str()
#define S(N) MulDiv((N), uiDpi*uiPercent, 100*USER_DEFAULT_SCREEN_DPI)

UIMainDialog::UIMainDialog()
    : mNotifyIconData{}
{
    HINSTANCE hInstance = GetModuleHandle(nullptr);

    setup.wndClassEx.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_DIALOGS));
    setup.wndClassEx.hIconSm = setup.wndClassEx.hIcon;
    setup.wndClassEx.lpszClassName = Master::ROBOT_WINDOW_CLASS;
    setup.style |= WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN;
    setup.exStyle |= WS_EX_APPWINDOW;
    setup.title = Master::ROBOT_WINDOW_NAME;

    keepOnTop = Cfg.jprefs["ui"]["main"]["keepOnTop"].as_bool_or();
    minimizeToTray = Cfg.jprefs["ui"]["main"]["minimizeToTray"].as_bool_or();
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

    bool consoleVisible = false;
    if (HWND consoleWindow = GetConsoleWindow())
        consoleVisible = IsWindowVisible(consoleWindow);

    setup.position = wndPosition;
    setup.size = wndSize;
    menu = CreateMenu();
    menu.append_submenu(W(_gt("Robot")))
            .append_item(IDM_TASK_NEW,W(_gt("New task")))
            .append_item(IDM_TASK_STOP,W(_gt("Stop task")))
            .append_item(IDM_TASK_REPEAT,W(_gt("Repeat task")))
            .append_item(IDM_TASK_PAUSE,W(_gt("Pause task")))
            .append_item(IDM_TASK_RESUME,W(_gt("Resume task")))
            .append_separator()
            .append_item(IDC_EXIT,W(_gt("Exit")))
            ;
    menu.append_submenu(W(_gt("Window")))
            .append_item(IDM_SHOW_TASK_STATUS,W(_gt("Show task state")))
            .append_item(IDM_SHOW_TASK_EDITOR,W(_gt("Show task editor")))
            .append_item(IDM_SHOW_COMMODITIES,W(_gt("Show commodities")))
            .append_item(IDM_SHOW_BOOKMARKS,W(_gt("Edit bookmarks")))
            .append_item(IDM_SHOW_STAR_SYSTEM,W(_gt("Edit star system")))
            .append_separator()
            .append_item(IDM_DETACH,W(_gt("Detach window")))
            .append_item(IDM_KEEP_ON_TOP,W(_gt("Keep on top"))).set_item_check_by_id(IDM_KEEP_ON_TOP, keepOnTop)
            .append_item(IDM_MINIMIZE_TO_TRAY,W(_gt("Minimize to tray"))).set_item_check_by_id(IDM_MINIMIZE_TO_TRAY, minimizeToTray)
            ;
    menu.append_submenu(W(_gt("Network")))
            .append_item(IDM_NETW_RAVEN_ENABLED,W(_gt("Enable RavenColonial")))
                .set_item_check_by_id(IDM_NETW_RAVEN_ENABLED, Cfg.isRavenColonialEnabled())
            .append_item(IDM_NETW_RAVEN_CARRIER_CARGO,W(_gt("Report carrier cargo")))
                .set_item_check_by_id(IDM_NETW_RAVEN_CARRIER_CARGO, Cfg.isRavenColonialReportCarrierCargo())
                .enable_item_by_id(IDM_NETW_RAVEN_CARRIER_CARGO, Cfg.isRavenColonialEnabled() && st::cmdr.fleetCarrierId != 0)
            .append_item(IDM_NETW_RAVEN_SHIP_CARGO,W(_gt("Report ship cargo")))
                .set_item_check_by_id(IDM_NETW_RAVEN_SHIP_CARGO, Cfg.isRavenColonialReportShipCargo())
                .enable_item_by_id(IDM_NETW_RAVEN_SHIP_CARGO, Cfg.isRavenColonialEnabled() && !st::cmdr.ravenKey.empty())
            .append_separator()
            .append_item(IDM_NETW_EDDN_SYSTEMS,W(_gt("Report EDDN star system")))
                .set_item_check_by_id(IDM_NETW_EDDN_SYSTEMS, Cfg.isEddnSystemsEnabled())
            .append_item(IDM_NETW_EDDN_MARKETS,W(_gt("Report EDDN markets")))
                .set_item_check_by_id(IDM_NETW_EDDN_MARKETS, Cfg.isEddnMarketsEnabled())
            ;
    menu.append_submenu(W(_gt("Debug")))
            .append_item(IDM_DEBUG_CONSOLE,W(_gt("Console window")))
                .set_item_check_by_id(IDM_DEBUG_CONSOLE, consoleVisible)
            .append_item(IDM_DEBUG_WATCH,W(_gt("Watch window")))
            .append_separator()
            .append_item(IDM_DEBUG_DEFAULT_LOG,W(_gt("Debug log level")))
                .set_item_check_by_id(IDM_DEBUG_DEFAULT_LOG, spdlog::default_logger()->level() <= spdlog::level::debug)
            .append_item(IDM_DEBUG_NETWORK_LOG,W(_gt("Debug log level for network")))
                .set_item_check_by_id(IDM_DEBUG_NETWORK_LOG, spdlog::default_logger()->level() <= spdlog::level::debug)
            .append_item(IDM_DEBUG_CONSOLE_LOG,W(_gt("Debug log level for console")))
                .set_item_check_by_id(IDM_DEBUG_CONSOLE_LOG, console_sink->level() <= spdlog::level::debug)
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
            InsertMenu(hmenu, 0, MF_BYPOSITION | MF_STRING, IDM_EXIT, W(_gt("Exit")));
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
    this->base_msg_pubm::on_command(IDM_DETACH, [this](wl::params p){
        on_command_show_detach();
        return 0;
    });
    this->base_msg_pubm::on_command(IDM_SHOW_TASK_STATUS, [this](wl::params p){
        on_command_show_task();
        return 0;
    });
    this->base_msg_pubm::on_command(IDM_SHOW_TASK_EDITOR, [this](wl::params p){
        on_command_edit_task();
        return 0;
    });
    this->base_msg_pubm::on_command(IDM_SHOW_COMMODITIES, [this](wl::params p){
        on_command_show_cargo();
        return 0;
    });
    this->base_msg_pubm::on_command(IDM_SHOW_BOOKMARKS, [this](wl::params p){
        on_command_show_bookmarks();
        return 0;
    });
    this->base_msg_pubm::on_command(IDM_SHOW_STAR_SYSTEM, [this](wl::params p){
        on_command_edit_star_system();
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
    this->base_msg_pubm::on_command(IDC_CURRENT_TASK, [this](wl::params p){
        if (HIWORD(p.wParam) == STN_CLICKED) {
            if (!ai::curr_task() || !ai::active() || ai::isDebugPause())
                on_command_task_new();
            else
                on_command_task_pause();
        }
        return 0;
    });
    this->base_msg_pubm::on_command(IDM_DEBUG_CONSOLE, [this](wl::params p){
        if (HWND consoleWindow = GetConsoleWindow()) {
            if (IsWindowVisible(consoleWindow)) {
                Cfg.jprefs["console"] = false;
                ShowWindow(consoleWindow, SW_HIDE);
                menu.set_item_check_by_id(IDM_DEBUG_CONSOLE, false);
            } else {
                Cfg.jprefs["console"] = true;
                menu.set_item_check_by_id(IDM_DEBUG_CONSOLE, true);
                ShowWindow(consoleWindow, SW_RESTORE);
                BringWindowToTop(consoleWindow);
            }
            this->savePrefs();
        }
        return 0;
    });
    this->base_msg_pubm::on_command({IDC_BUTTON_WATCH,IDM_DEBUG_WATCH}, [](wl::params p){
        UIManager::showDebugWindow();
        return 0;
    });
    this->base_msg_pubm::on_command(IDM_DEBUG_DEFAULT_LOG, [this](wl::params p){
        auto logger = spdlog::default_logger();
        if (logger->level() <= spdlog::level::debug) {
            logger->set_level(spdlog::level::info);
            menu.set_item_check_by_id(IDM_DEBUG_DEFAULT_LOG, false);
        } else {
            logger->set_level(spdlog::level::debug);
            menu.set_item_check_by_id(IDM_DEBUG_DEFAULT_LOG, true);
        }
        this->savePrefs();
        return 0;
    });
    this->base_msg_pubm::on_command(IDM_DEBUG_NETWORK_LOG, [this](wl::params p){
        auto logger = spdlog::get("http");
        if (logger->level() <= spdlog::level::debug) {
            logger->set_level(spdlog::level::info);
            menu.set_item_check_by_id(IDM_DEBUG_NETWORK_LOG, false);
        } else {
            logger->set_level(spdlog::level::debug);
            menu.set_item_check_by_id(IDM_DEBUG_NETWORK_LOG, true);
        }
        this->savePrefs();
        return 0;
    });
    this->base_msg_pubm::on_command(IDM_DEBUG_CONSOLE_LOG, [this](wl::params p){
        if (console_sink->level() <= spdlog::level::debug) {
            console_sink->set_level(spdlog::level::info);
            menu.set_item_check_by_id(IDM_DEBUG_CONSOLE_LOG, false);
        } else {
            console_sink->set_level(spdlog::level::debug);
            menu.set_item_check_by_id(IDM_DEBUG_CONSOLE_LOG, true);
        }
        this->savePrefs();
        return 0;
    });
    this->base_msg_pubm::on_command(IDM_KEEP_ON_TOP, [this](wl::params p){
        keepOnTop = !keepOnTop;
        menu.set_item_check_by_id(IDM_KEEP_ON_TOP, keepOnTop);
        if (keepOnTop) {
            this->style.set_style_ex(true, WS_EX_TOPMOST);
            SetWindowPos(this->hwnd(), HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        } else {
            this->style.set_style_ex(false, WS_EX_LAYERED|WS_EX_TOPMOST|WS_EX_TRANSPARENT);
            SetWindowPos(this->hwnd(), HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        }
        this->savePrefs();
        return 0;
    });
    this->base_msg_pubm::on_command(IDM_MINIMIZE_TO_TRAY, [this](wl::params p){
        minimizeToTray = !minimizeToTray;
        menu.set_item_check_by_id(IDM_MINIMIZE_TO_TRAY, minimizeToTray);
        this->savePrefs();
        return 0;
    });
    this->base_msg_pubm::on_command(IDM_NETW_RAVEN_ENABLED, [this](wl::params p){
        if (Cfg.isRavenColonialEnabled()) {
            if (auto rc = RavenColonial::getInstance())
                rc->setShipCargoReport(false);
        }
        bool on = !Cfg.isRavenColonialEnabled();
        Cfg.setRavenColonialEnabled(on);
        menu.set_item_check_by_id(IDM_NETW_RAVEN_ENABLED, on);
        menu.enable_item_by_id(IDM_NETW_RAVEN_CARRIER_CARGO, on && st::cmdr.fleetCarrierId != 0);
        menu.enable_item_by_id(IDM_NETW_RAVEN_SHIP_CARGO, on && !st::cmdr.ravenKey.empty());
        this->savePrefs();
        if (Cfg.isRavenColonialEnabled()) {
            if (auto rc = RavenColonial::getInstance())
                rc->setShipCargoReport(Cfg.isRavenColonialReportShipCargo());
        }
        return 0;
    });
    this->base_msg_pubm::on_command(IDM_NETW_RAVEN_CARRIER_CARGO, [this](wl::params p){
        bool on = !Cfg.isRavenColonialReportCarrierCargo();
        Cfg.setRavenColonialReportCarrierCargo(on);
        menu.set_item_check_by_id(IDM_NETW_RAVEN_CARRIER_CARGO, on);
        this->savePrefs();
        return 0;
    });
    this->base_msg_pubm::on_command(IDM_NETW_RAVEN_SHIP_CARGO, [this](wl::params p){
        bool on = !Cfg.isRavenColonialReportShipCargo();
        Cfg.setRavenColonialReportShipCargo(on);
        menu.set_item_check_by_id(IDM_NETW_RAVEN_SHIP_CARGO, on);
        this->savePrefs();
        if (auto rc = RavenColonial::getInstance())
            rc->setShipCargoReport(on);
        return 0;
    });
    this->base_msg_pubm::on_command(IDM_NETW_EDDN_SYSTEMS, [this](wl::params p){
        bool on = !Cfg.isEddnSystemsEnabled();
        Cfg.setEddnSystemsEnabled(on);
        menu.set_item_check_by_id(IDM_NETW_EDDN_SYSTEMS, on);
        this->savePrefs();
        return 0;
    });
    this->base_msg_pubm::on_command(IDM_NETW_EDDN_MARKETS, [this](wl::params p){
        bool on = !Cfg.isEddnMarketsEnabled();
        Cfg.setEddnMarketsEnabled(on);
        menu.set_item_check_by_id(IDM_NETW_EDDN_MARKETS, on);
        this->savePrefs();
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
        this->savePrefs();
        return 0;
    });
    on_message(WM_SIZE, [this](wl::params params) {
        if (params.wParam == SIZE_MINIMIZED) {
            if (minimizeToTray)
                hide(true);
        } else {
            relayout();
            update_curr_task();
        }
        return 0;
    });
    on_message(WM_EXITSIZEMOVE, [this](wl::params params) {
        this->savePrefs();
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

    lbl_task.create(hwnd(), IDC_STATIC, W(_gt("Task:")), {10, 10}, {80, 20})
            .style.set_style(true, SS_RIGHT);

    lbl_curr_task.create(hwnd(), IDC_CURRENT_TASK, L"", {100, 10}, {324, 20})
            .style.set_style(true, WS_BORDER | SS_CENTER | SS_NOTIFY);

    btn_stop_new.create(hwnd(), IDC_BUTTON_STOP_NEW, "task-new", S(LO_ICN_S), {244,32}, {24,24});
    btn_pause_resume.create(hwnd(), IDC_BUTTON_PAUSE_RESUME, "task-repeat", S(LO_ICN_S), {344,32}, {24,24});

    relayout();
    update_curr_task();
    initializing = false;
}

void UIMainDialog::savePrefs() {
    if (initializing)
        return;

    Cfg.jprefs["ui"]["main"]["keepOnTop"] = keepOnTop;
    Cfg.jprefs["ui"]["main"]["minimizeToTray"] = minimizeToTray;

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

bool UIMainDialog::show_startup(const std::string &message, const std::string& latest_version, const std::string& latest_url) {
    show();
    run_thread_ui([=, this](){
        control = std::unique_ptr<UIControl>(new UIShowStartup(message, latest_version, latest_url));
        control->create(hwnd(), 0, {10,10}, {400,400});
        relayout();
        menu.set_item_radio_by_id(IDM_SHOW_TASK_STATUS, 5, IDM_SHOW_TASK_STATUS);
    });
    return true;
}

bool UIMainDialog::show_task_status() {
    if (control && dynamic_cast<UIShowTask*>(control.get())) {
        update_curr_task();
    } else {
        on_command_show_task();
    }
    return true;
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

bool UIMainDialog::toggle() {
    if (ai::isDebugPause()) {
        if (!IsWindowVisible(hwnd()))
            ShowWindow(this->hwnd(), SW_MINIMIZE);
    } else {
        hide(false);
    }
    return true;
}

bool UIMainDialog::hide(bool force_close) {
    if (!force_close && keepOnTop && Cfg.getGameScreenMode() != Configuration::GameScreenMode::FullScreen) {
        HWND hWndED = FindWindow(Master::ED_WINDOW_CLASS, Master::ED_WINDOW_NAME);
        RECT rectED {};
        GetWindowRect(hWndED, &rectED);
        RECT rectRobot {};
        GetWindowRect(hwnd(), &rectRobot);
        if ((fromRECT(rectED) & fromRECT(rectRobot)).empty())
            return false;
        this->style.set_style_ex(true, WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TRANSPARENT);
        SetWindowPos(this->hwnd(), HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        float transparency_percentage = 0.6f;
        SetLayeredWindowAttributes(hwnd(), 0, (BYTE) (255 * transparency_percentage), LWA_ALPHA);
        return false;
    }
    if (this->style.has_style_ex(WS_EX_LAYERED|WS_EX_TOPMOST|WS_EX_TRANSPARENT)) {
        this->style.set_style_ex(false, WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TRANSPARENT);
        SetWindowPos(this->hwnd(), HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    }

    if (!force_close && !minimizeToTray) {
        ShowWindow(this->hwnd(), SW_MINIMIZE);
        mUpdateTimerId = SetTimer(this->hwnd(), mUpdateTimerId, 2000, nullptr);
    } else {
        ShowWindow(this->hwnd(), SW_HIDE);
        KillTimer(this->hwnd(), mUpdateTimerId);
        mUpdateTimerId = {};
    }
    return true;
}

void UIMainDialog::updateCommander() {
    menu.set_item_check_by_id(IDM_NETW_RAVEN_ENABLED, Cfg.isRavenColonialEnabled());
    menu.set_item_check_by_id(IDM_NETW_RAVEN_CARRIER_CARGO, Cfg.isRavenColonialReportCarrierCargo());
    menu.set_item_check_by_id(IDM_NETW_RAVEN_SHIP_CARGO, Cfg.isRavenColonialReportShipCargo());
    menu.enable_item_by_id(IDM_NETW_RAVEN_CARRIER_CARGO, Cfg.isRavenColonialEnabled() && st::cmdr.fleetCarrierId != 0);
    menu.enable_item_by_id(IDM_NETW_RAVEN_SHIP_CARGO, Cfg.isRavenColonialEnabled() && !st::cmdr.ravenKey.empty());
    menu.set_item_check_by_id(IDM_NETW_EDDN_SYSTEMS, Cfg.isEddnSystemsEnabled());
    menu.set_item_check_by_id(IDM_NETW_EDDN_MARKETS, Cfg.isEddnMarketsEnabled());
    menu.set_item_check_by_id(IDM_DEBUG_DEFAULT_LOG, spdlog::default_logger()->level() <= spdlog::level::debug);
    menu.set_item_check_by_id(IDM_DEBUG_NETWORK_LOG, spdlog::default_logger()->level() <= spdlog::level::debug);
}

void UIMainDialog::on_command_task_new() {
    if (dynamic_cast<UIEditTask*>(control.get()))
        return;
    try_again:;
    for (auto& d : detached) {
        if (dynamic_cast<UIEditTask*>(d->control.get())) {
            if (!IsWindow(d->hwnd())) {
                std::erase(detached, d);
                goto try_again;
            }
            ShowWindow(d->hwnd(), SW_RESTORE);
            SetForegroundWindow(d->hwnd());
            BringWindowToTop(d->hwnd());
            return;
        }
    }
    if (control)
        DestroyWindow(control->hwnd());
    control = std::unique_ptr<UIControl>(new UIEditTask);
    control->create(hwnd(), 0, {10,10}, {400,400});
    relayout();
    menu.set_item_radio_by_id(IDM_SHOW_TASK_STATUS, 5, IDM_SHOW_TASK_EDITOR);
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
        ai::resume();
        hide(false);
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

void UIMainDialog::on_command_show_detach() {
    for (auto& d : detached) {
        if (typeid(d->control.get()) == typeid(control.get())) {
            ShowWindow(d->hwnd(), SW_RESTORE);
            SetForegroundWindow(d->hwnd());
            BringWindowToTop(d->hwnd());
            return;
        }
    }

    auto& dlg = detached.emplace_back();
    dlg.reset(new UIControlDialog(control));
    on_command_show_task();
    run_thread_detached([&dlg,this](){
        dlg->winmain_run(GetModuleHandle(nullptr), SW_SHOW);
        std::erase(this->detached, dlg);
    });
}

void UIMainDialog::on_command_show_task() {
    try {
        if (dynamic_cast<UIShowTask*>(control.get()))
            return;
        if (control)
            DestroyWindow(control->hwnd());
        control = std::unique_ptr<UIControl>(new UIShowTask);
        control->create(hwnd(), 0, {10,10}, {400,400});
        relayout();
        menu.set_item_radio_by_id(IDM_SHOW_TASK_STATUS, 5, IDM_SHOW_TASK_STATUS);
        update_curr_task();
    } catch (const std::system_error& ex) {
        LOG(ERROR) << "System error: code " << ex.code() << ": " << getErrorMessage(ex.code().value()) << ": " << ex.what();
    } catch (const std::exception& ex) {
        LOG(ERROR) << ex.what();
    }
}

void UIMainDialog::on_command_edit_task() {
    try {
        if (dynamic_cast<UIEditTask*>(control.get()))
            return;
        if (control)
            DestroyWindow(control->hwnd());
        control = std::unique_ptr<UIControl>(new UIEditTask);
        control->create(hwnd(), 0, {10,10}, {400,400});
        relayout();
        menu.set_item_radio_by_id(IDM_SHOW_TASK_STATUS, 5, IDM_SHOW_TASK_EDITOR);
    } catch (const std::system_error& ex) {
        LOG(ERROR) << "System error: code " << ex.code() << ": " << getErrorMessage(ex.code().value()) << ": " << ex.what();
    } catch (const std::exception& ex) {
        LOG(ERROR) << ex.what();
    }
}

void UIMainDialog::on_command_show_cargo() {
    try {
        if (dynamic_cast<UIShowCargo*>(control.get()))
            return;
        if (control)
            DestroyWindow(control->hwnd());
        control = std::unique_ptr<UIControl>(new UIShowCargo);
        control->create(hwnd(), 0, {10,10}, {400,400});
        relayout();
        menu.set_item_radio_by_id(IDM_SHOW_TASK_STATUS, 5, IDM_SHOW_COMMODITIES);
        ((UIShowCargo*)control.get())->initControls();

        //if (auto dlg = UIShowCargo::getInstance())
        //    SetForegroundWindow(dlg->hwnd());
        //else
        //    UIShowCargo::makeInstance();
    } catch (const std::system_error& ex) {
        LOG(ERROR) << "System error: code " << ex.code() << ": " << getErrorMessage(ex.code().value()) << ": " << ex.what();
    } catch (const std::exception& ex) {
        LOG(ERROR) << ex.what();
    }
}

void UIMainDialog::on_command_show_bookmarks() {
    try {
        if (dynamic_cast<UIEditBookmarks*>(control.get()))
            return;
        if (control)
            DestroyWindow(control->hwnd());
        control = std::unique_ptr<UIControl>(new UIEditBookmarks);
        control->create(hwnd(), 0, {10,10}, {400,400});
        relayout();
        menu.set_item_radio_by_id(IDM_SHOW_TASK_STATUS, 5, IDM_SHOW_BOOKMARKS);
    } catch (const std::system_error& ex) {
        LOG(ERROR) << "System error: code " << ex.code() << ": " << getErrorMessage(ex.code().value()) << ": " << ex.what();
    } catch (const std::exception& ex) {
        LOG(ERROR) << ex.what();
    }
}

void UIMainDialog::on_command_edit_star_system() {
    try {
        if (dynamic_cast<UIEditSystem*>(control.get()))
            return;
        if (control)
            DestroyWindow(control->hwnd());
        control = std::unique_ptr<UIControl>(new UIEditSystem);
        control->create(hwnd(), 0, {10,10}, {400,400});
        relayout();
        menu.set_item_radio_by_id(IDM_SHOW_TASK_STATUS, 5, IDM_SHOW_STAR_SYSTEM);
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

    bool needUpdate = false;
    if (control) {
        control->on_update();
        needUpdate = control->need_timer_update();
    }

    if (needUpdate && IsWindowVisible(hwnd())) {
        mUpdateTimerId = SetTimer(this->hwnd(), mUpdateTimerId, 800, nullptr);
    }
    else if (needUpdate && IsIconic(hwnd())) {
        mUpdateTimerId = SetTimer(this->hwnd(), mUpdateTimerId, 2000, nullptr);
    }
    else {
        KillTimer(this->hwnd(), mUpdateTimerId);
        mUpdateTimerId = {};
    }
}

void UIMainDialog::relayout() {
    RECT rect{};
    GetClientRect(hwnd(), &rect);

    int uiPercent = Cfg.getUiScalePercents();
    int uiDpi = GetDpiForWindow(hwnd());
    UILayout lo(uiDpi, uiPercent, rect);
    if (uiDpi != scaled_to_dpi) {
        scaled_to_dpi = uiDpi;
        loCreateFont(font, uiDpi, uiPercent);
        lo.font = &font;
    }

    lo.wpi = BeginDeferWindowPos(10);
    lo.left += lo.border;
    lo.top += lo.border;
    lo.width -= 2*lo.border;
    lo.height -= 2*lo.border;

    if (lo.font) {
        lo.font->set_on(lbl_task);
        lo.font->set_on(lbl_curr_task);
    }
    btn_stop_new.set_icon_size(lo.icsz);
    btn_pause_resume.set_icon_size(lo.icsz);

    int x = lo.left;
    int w = lo.txt6w + lo.hgap/2;
    lo.wpi = DeferWindowPos(lo.wpi, lbl_task.hwnd(), nullptr, x, lo.top, w, lo.vrow, SWP_NOZORDER);

    x = lo.left + lo.txt6w + lo.hgap;
    w = lo.width - x - 2*lo.btnh;
    lo.wpi = DeferWindowPos(lo.wpi, lbl_curr_task.hwnd(), nullptr, x, lo.top, w, lo.vrow, SWP_NOZORDER);

    w = lo.btnh;
    x = lo.left + lo.width - lo.btnh;
    int y = lo.top - (lo.btnh-lo.vrow)/2;
    lo.wpi = DeferWindowPos(lo.wpi, btn_pause_resume.hwnd(), nullptr, x, y, lo.btnh, lo.btnh, SWP_NOZORDER);

    x -= lo.hgap/2 + lo.btnh;
    lo.wpi = DeferWindowPos(lo.wpi, btn_stop_new.hwnd(), nullptr, x, y, lo.btnh, lo.btnh, SWP_NOZORDER);

    lo.top += lo.vrow + lo.vgap;

    EndDeferWindowPos(lo.wpi);

    lo.wpi = nullptr;
    lo.font = nullptr;

    if (control) {
        POINT pos = {lo.left, lo.top};
        SIZE sz = {lo.width, rect.bottom - lo.top - lo.border};
        SetWindowPos(control->hwnd(), nullptr, pos.x, pos.y, sz.cx, sz.cy, SWP_NOZORDER);
        control->relayout();
    }

    RedrawWindow(this->hwnd(), 0, 0, RDW_INVALIDATE | RDW_ALLCHILDREN);
    InvalidateRect(this->hwnd(), nullptr, true);
    UpdateWindow(this->hwnd());
}
