//
// Created by mkizub on 24.05.2025.
//

#include <Windows.h>
#include <d3d11.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Graphics.Capture.h>
#include <dwmapi.h>

#include "../pch.h"

#include "../Capturer.h"
#include "CapturerWin32.h"
#include "CapturerWinRT.h"

#include <shellscalingapi.h>


std::vector<std::unique_ptr<Capturer>> Capturer::AllCapturers;
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
    MONITORINFOEX monitorInfoEx;
    monitorInfoEx.cbSize = sizeof(MONITORINFOEX);
    if (GetMonitorInfo(hMonitor, &monitorInfoEx)) {
        AllCapturers.push_back(std::unique_ptr<Capturer>(new CapturerWinRT(hMonitor, &monitorInfoEx, hdcMonitor)));
        AllCapturers.push_back(std::unique_ptr<Capturer>(new CapturerWin32(hMonitor, &monitorInfoEx, hdcMonitor)));
    }
    return TRUE;
}

void Capturer::InitCapturers() {
    DefaultCapturer = nullptr;
    AllCapturers.clear();

    SetProcessDpiAwareness(PROCESS_PER_MONITOR_DPI_AWARE);

    EnumDisplayMonitors(nullptr, nullptr, MonitorEnumProc, (LPARAM)nullptr);

    DefaultCapturer = new CapturerWin32(nullptr, nullptr, nullptr);
    AllCapturers.push_back(std::unique_ptr<Capturer>(DefaultCapturer));
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
        DefaultCapturer->trySetup(hwnd, captureRect);
        return DefaultCapturer;
    }
    for (auto& c : AllCapturers) {
        for (int i=0; i < CCHDEVICENAME; i++) {
            auto c1 = c->monitorInfo.szDevice[i];
            auto c2 = monitorInfo.szDevice[i];
            if (c1 != c2)
                break;
            if (c1 == 0) {
                if (windowRect != c->monitorInfo.rcMonitor) {
                    captureRect = windowRect;
                    RECT clientRect;
                    if (GetClientRect(hwnd, &clientRect)) {
                        ClientToScreen(hwnd, (LPPOINT)&clientRect.left);
                        ClientToScreen(hwnd, (LPPOINT)&clientRect.right);
                        captureRect = clientRect;
                    }
                } else {
                    captureRect = c->monitorInfo.rcMonitor;
                }
                if (c->trySetup(hwnd, captureRect))
                    return c.get();
            }
        }
    }

    LOG(ERROR) << "Cannot find capturer for monitor " << monitorInfo.szDevice;
    DefaultCapturer->trySetup(hwnd, captureRect);
    return DefaultCapturer;
}

bool Capturer::trySetup(HWND hWnd, RECT& captRect) {
    if (this != DefaultCapturer)
        return false;
    this->hWndED = hWnd;
    this->captureRect = captRect;
    return true;
}

Capturer::Capturer(HMONITOR hMonitor, LPMONITORINFOEX monitorInfoEx)
    : dpiScaleX{1.0}
    , dpiScaleY{1.0}
    , needScaling(false)
    , hMonitor(hMonitor)
{
    if (monitorInfoEx) {
        memcpy(&monitorInfo, monitorInfoEx, sizeof(monitorInfo));
        screenWidth = monitorInfo.rcMonitor.right - monitorInfo.rcMonitor.left;
        screenHeight = monitorInfo.rcMonitor.bottom - monitorInfo.rcMonitor.top;
    } else {
        screenWidth = GetSystemMetrics(SM_CXSCREEN);
        screenHeight = GetSystemMetrics(SM_CYSCREEN);
        memset(&monitorInfo, 0, sizeof(monitorInfo));
        monitorInfo.cbSize = sizeof(monitorInfo);
        monitorInfo.rcMonitor = RECT(0, 0, screenWidth, screenHeight);
        monitorInfo.rcWork = monitorInfo.rcMonitor;
    }
    captureRect = {0, 0, screenWidth, screenHeight};
    UINT dpiX = USER_DEFAULT_SCREEN_DPI;
    UINT dpiY = USER_DEFAULT_SCREEN_DPI;
    if (SUCCEEDED(GetDpiForMonitor(hMonitor, MDT_EFFECTIVE_DPI, &dpiX, &dpiY))) {
        if (dpiX != USER_DEFAULT_SCREEN_DPI || dpiY != USER_DEFAULT_SCREEN_DPI) {
            dpiScaleX = double(dpiX) / USER_DEFAULT_SCREEN_DPI;
            dpiScaleY = double(dpiY) / USER_DEFAULT_SCREEN_DPI;
            needScaling = true;
        }
    }
}

cv::Rect Capturer::getCaptureRect() {
    cv::Point lt {captureRect.left - monitorInfo.rcMonitor.left, captureRect.top - monitorInfo.rcMonitor.top};
    cv::Point rb {captureRect.right - monitorInfo.rcMonitor.left, captureRect.bottom - monitorInfo.rcMonitor.top};
    return cv::Rect(lt, rb);
}

cv::Rect Capturer::getMonitorVirtualRect() {
    cv::Point lt {monitorInfo.rcMonitor.left, monitorInfo.rcMonitor.top};
    cv::Point rb {monitorInfo.rcMonitor.right, monitorInfo.rcMonitor.bottom};
    return cv::Rect(lt, rb);
}

