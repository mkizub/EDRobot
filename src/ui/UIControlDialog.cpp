//
// Created by mkizub on 11.03.2026.
//

#include "../pch.h"

#include "UIControlDialog.h"
#include "UIControl.h"
#include "UILayout.h"

#include "../../ui/resource.h"

UIControlDialog::UIControlDialog(std::unique_ptr<UIControl>& ctrl) {
    HINSTANCE hInstance = GetModuleHandle(nullptr);

    setup.wndClassEx.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_DIALOGS));
    setup.wndClassEx.hIconSm = setup.wndClassEx.hIcon;
    setup.wndClassEx.lpszClassName = L"EDRobotDetached";

    setup.title = ctrl->title();
    setup.style |= WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN;
    setup.exStyle |= WS_EX_APPWINDOW;
    RECT prect {};
    if (auto pwnd = GetParent(ctrl->hwnd()); pwnd && GetWindowRect(pwnd, &prect)) {
        auto hMonitor = MonitorFromWindow(pwnd, MONITOR_DEFAULTTOPRIMARY);
        MONITORINFOEX monitorInfo {sizeof(monitorInfo)};
        GetMonitorInfo(hMonitor, &monitorInfo);
        if (PtInRect(&monitorInfo.rcMonitor, {prect.right+(prect.right-prect.left), prect.top}))
            setup.position = {prect.right, prect.top};
        else
            setup.position = {prect.left-(prect.right-prect.left), prect.top};
        setup.size = {prect.right-prect.left, prect.bottom-prect.top};
    }

    on_message(WM_CREATE, [this](wl::params p) -> INT_PTR {
        initialize();
        return 0;
    });
    on_message(WM_CLOSE, [this](wl::params p) -> INT_PTR {
        bool ok = DestroyWindow(hwnd());
        return 0;
    });

    on_message(WM_DPICHANGED, [this](wl::params params) {
        relayout();
        return 0;
    });
    on_message(WM_SIZE, [this](wl::params params) {
        if (params.wParam == SIZE_RESTORED || params.wParam == SIZE_MAXIMIZED || params.wParam == SIZE_MAXSHOW) {
            relayout();
            update_control();
        } else {
            //ShowWindow(this->hwnd(), SW_SHOWMINIMIZED);
            KillTimer(this->hwnd(), mUpdateTimerId);
            mUpdateTimerId = {};
        }

        return 0;
    });
    this->base_msg_pubm::on_message(WM_TIMER, [this](wl::params p){
        update_control();
        return 0;
    });

    control.swap(ctrl);
    control->detached = true;
}

UIControlDialog::~UIControlDialog() {
}

void UIControlDialog::initialize() {
    SetParent(control->hwnd(), hwnd());
    relayout();
    update_control();
}

void UIControlDialog::relayout() {
    RECT rect{};
    GetClientRect(hwnd(), &rect);

    int uiPercent = Cfg.getUiScalePercents();
    int uiDpi = GetDpiForWindow(hwnd());
    UILayout lo(uiDpi, uiPercent, rect);

    lo.left += lo.border;
    lo.top += lo.border;
    lo.width -= 2*lo.border;
    lo.height -= 2*lo.border;

    POINT pos = {lo.left, lo.top};
    SIZE sz = {lo.width, rect.bottom - lo.top - lo.border};
    SetWindowPos(control->hwnd(), nullptr, pos.x, pos.y, sz.cx, sz.cy, SWP_NOZORDER);
    control->relayout();
}

void UIControlDialog::update_control() {
    control->on_timer_update();
    if (IsWindowVisible(hwnd()) && control->need_timer_update()) {
        mUpdateTimerId = SetTimer(this->hwnd(), mUpdateTimerId, 800, NULL);
    } else {
        KillTimer(this->hwnd(), mUpdateTimerId);
        mUpdateTimerId = {};
    }
}
