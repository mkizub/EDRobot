//
// Created by mkizub on 24.05.2025.
//

#include "../pch.h"

#include <d3d11.h>
#include <dxgi.h>
#include <dxgi1_2.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Graphics.Capture.h>
#include <dwmapi.h>

#include "../Capturer.h"
#include "CapturerWin32.h"
#include "CapturerWinRT.h"
#include "CapturerDXGI.h"

#include <shellscalingapi.h>


std::vector<std::unique_ptr<CapturerWin32>> Capturer::Win32Capturers;
std::vector<std::unique_ptr<CapturerWinRT>> Capturer::WinRTCapturers;
std::vector<std::unique_ptr<CapturerDXGI>> Capturer::DXGICapturers;
Capturer* Capturer::DefaultCapturer;

void Frame::recycle(Frame* p) {
    if (!p)
        return;
    if (p->owner) {
        p->owner->recycle(p);
    } else {
        delete p;
    }
}

BOOL CALLBACK Capturer::MonitorEnumProc(HMONITOR hMonitor, HDC hdcMonitor, LPRECT lprcMonitor, LPARAM dwData) {
    UNREFERENCED_PARAMETER(lprcMonitor);
    UNREFERENCED_PARAMETER(dwData);
    MONITORINFOEX monitorInfoEx;
    monitorInfoEx.cbSize = sizeof(MONITORINFOEX);
    if (GetMonitorInfo(hMonitor, &monitorInfoEx)) {
        Configuration* cfg = Master::getInstance().getConfiguration();
        if (!cfg->isCapturerDXGIDisabled())
            DXGICapturers.push_back(std::unique_ptr<CapturerDXGI>(new CapturerDXGI(hMonitor, &monitorInfoEx, hdcMonitor)));
        if (!cfg->isCapturerWin32Disabled())
            Win32Capturers.push_back(std::unique_ptr<CapturerWin32>(new CapturerWin32(hMonitor, &monitorInfoEx, hdcMonitor)));
        if (!cfg->isCapturerWinRTDisabled())
            WinRTCapturers.push_back(std::unique_ptr<CapturerWinRT>(new CapturerWinRT(hMonitor, &monitorInfoEx, hdcMonitor)));
    }
    return TRUE;
}

void Capturer::InitCapturers() {
    DefaultCapturer = nullptr;
    Win32Capturers.clear();
    WinRTCapturers.clear();

    SetProcessDpiAwareness(PROCESS_PER_MONITOR_DPI_AWARE);

    EnumDisplayMonitors(nullptr, nullptr, MonitorEnumProc, (LPARAM)nullptr);

    DefaultCapturer = new CapturerWin32(nullptr, nullptr, nullptr);
}

Capturer* Capturer::getEDCapturer(HWND hwnd) {
    if (!DefaultCapturer)
        InitCapturers();

    HMONITOR hMonitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    if (!hMonitor) {
        LOG(ERROR) << "Could not get monitor handle.";
        return DefaultCapturer;
    }

    MONITORINFOEX monitorInfo;
    monitorInfo.cbSize = sizeof(monitorInfo);
    if (!GetMonitorInfo(hMonitor, &monitorInfo)) {
        LOG(ERROR) << "Could not get monitor information.";
        return DefaultCapturer;
    }

    RECT windowRect;
    RECT captureRect;
    BOOL ok = hwnd && GetWindowRect(hwnd, &windowRect);
    if (!ok) {
        LOG(ERROR) << "Cannot get window for capturer";
        captureRect = monitorInfo.rcMonitor;
        DefaultCapturer->trySetup(hwnd, fromRECT(windowRect), fromRECT(captureRect));
        return DefaultCapturer;
    }
    bool fullscreen;
    {
        if (windowRect != monitorInfo.rcMonitor) {
            fullscreen = false;
            captureRect = windowRect;
            RECT clientRect;
            if (GetClientRect(hwnd, &clientRect)) {
                ClientToScreen(hwnd, (LPPOINT)&clientRect.left);
                ClientToScreen(hwnd, (LPPOINT)&clientRect.right);
                captureRect = clientRect;
            }
        } else {
            fullscreen = true;
            captureRect = monitorInfo.rcMonitor;
        }
    }
    Configuration* cfg = Master::getInstance().getConfiguration();
    if (!cfg->isCapturerDXGIDisabled()) {
        for (auto& c : DXGICapturers) {
            if (c->hMonitor == hMonitor && c->trySetup(hwnd, fromRECT(windowRect), fromRECT(captureRect)))
                return c.get();
        }
    }
    if (!cfg->isCapturerWin32Disabled()) {
        for (auto& c : Win32Capturers) {
            if (c->hMonitor == hMonitor && c->trySetup(hwnd, fromRECT(windowRect), fromRECT(captureRect)))
                return c.get();
        }
    }
    if (!cfg->isCapturerWinRTDisabled()) {
        for (auto& c : WinRTCapturers) {
            if (c->hMonitor == hMonitor && c->trySetup(hwnd, fromRECT(windowRect), fromRECT(captureRect)))
                return c.get();
        }
    }

    LOG(ERROR) << "Cannot find capturer for monitor " << monitorInfo.szDevice;
    DefaultCapturer->trySetup(hwnd, fromRECT(windowRect), fromRECT(captureRect));
    return DefaultCapturer;
}

bool Capturer::trySetup(HWND hWnd, cv::Rect windowRect, cv::Rect clientRect) {
    if (this != DefaultCapturer)
        return false;
    this->hWndED = hWnd;
    this->windowVirtRect = windowRect;
    this->captureVirtRect = clientRect;
    this->titleHeight = clientRect.y - windowRect.y;
    this->borderWidth = clientRect.x - windowRect.x;
    return true;
}

Capturer::Capturer(HMONITOR hMonitor, LPMONITORINFOEX monitorInfoEx)
    : dpiScaleX{1.0}
    , dpiScaleY{1.0}
    , hMonitor(hMonitor)
    , hWndED(nullptr)
    , monitorInfo {}
    , titleHeight {}
{
    if (monitorInfoEx) {
        memcpy(&monitorInfo, monitorInfoEx, sizeof(monitorInfo));
    } else {
        memset(&monitorInfo, 0, sizeof(monitorInfo));
        monitorInfo.cbSize = sizeof(monitorInfo);
        monitorInfo.rcMonitor = RECT(0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN));
        monitorInfo.rcWork = monitorInfo.rcMonitor;
    }
    monitorVirtRect = {
            monitorInfo.rcMonitor.left,
            monitorInfo.rcMonitor.top,
            monitorInfo.rcMonitor.right - monitorInfo.rcMonitor.left,
            monitorInfo.rcMonitor.bottom - monitorInfo.rcMonitor.top
    };
    windowVirtRect = monitorVirtRect;
    captureVirtRect = monitorVirtRect;
    UINT dpiX = USER_DEFAULT_SCREEN_DPI;
    UINT dpiY = USER_DEFAULT_SCREEN_DPI;
    if (SUCCEEDED(GetDpiForMonitor(hMonitor, MDT_EFFECTIVE_DPI, &dpiX, &dpiY))) {
        if (dpiX != USER_DEFAULT_SCREEN_DPI || dpiY != USER_DEFAULT_SCREEN_DPI) {
            dpiScaleX = double(dpiX) / USER_DEFAULT_SCREEN_DPI;
            dpiScaleY = double(dpiY) / USER_DEFAULT_SCREEN_DPI;
        }
    }
}

cv::Rect Capturer::getCaptureRect() {
    return captureVirtRect - monitorVirtRect.tl();
}

cv::Rect Capturer::getMonitorVirtualRect() {
    return monitorVirtRect;
}

