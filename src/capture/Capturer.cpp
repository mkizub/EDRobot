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

#include <opencv2/core/directx.hpp>
#include <shellscalingapi.h>


static CComPtr<ID3D11Device> D3dDevice;
static CComPtr<ID3D11DeviceContext> D3dContext;

std::vector<std::unique_ptr<CapturerWin32>> Capturer::Win32Capturers;
std::vector<std::unique_ptr<CapturerWinRT>> Capturer::WinRTCapturers;
std::vector<std::unique_ptr<CapturerDXGI>> Capturer::DXGICapturers;
Capturer* Capturer::DefaultCapturer;


ID3D11Device* Capturer::getID3D11Device() {
    return D3dDevice;
}
ID3D11DeviceContext* Capturer::getID3D11DeviceContext() {
    return D3dContext;
}

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
        if (!Cfg.isCapturerDXGIDisabled())
            DXGICapturers.push_back(std::unique_ptr<CapturerDXGI>(new CapturerDXGI(hMonitor, &monitorInfoEx, hdcMonitor)));
        if (!Cfg.isCapturerWin32Disabled())
            Win32Capturers.push_back(std::unique_ptr<CapturerWin32>(new CapturerWin32(hMonitor, &monitorInfoEx, hdcMonitor)));
        if (!Cfg.isCapturerWinRTDisabled())
            WinRTCapturers.push_back(std::unique_ptr<CapturerWinRT>(new CapturerWinRT(hMonitor, &monitorInfoEx, hdcMonitor)));
    }
    return TRUE;
}

void Capturer::InitD3DDevice() {
    if (!D3dDevice) {
        static const D3D_FEATURE_LEVEL featureLevels[] = {
                D3D_FEATURE_LEVEL_11_1,
                D3D_FEATURE_LEVEL_11_0,
                D3D_FEATURE_LEVEL_10_1,
                D3D_FEATURE_LEVEL_10_0,
                D3D_FEATURE_LEVEL_9_1
        };
        D3D_FEATURE_LEVEL featureLevel;
        HRESULT hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT,
                                       featureLevels, std::size(featureLevels), D3D11_SDK_VERSION,
                                       &D3dDevice, &featureLevel, &D3dContext);
        if (FAILED(hr)) {
            LOG(ERROR) << "Failed to create D3D11 device" << getErrorMessage(hr);
        } else {
            if (useOpenCL()) {
                cv::directx::ocl::initializeContextFromD3D11Device(D3dDevice);
                LOG(INFO) << "Using OpenCL device: " << cv::ocl::Context::getDefault().device(0).name();
            } else {
                cv::ocl::setUseOpenCL(false);
            }
        }
    }
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
    if (!Cfg.isCapturerDXGIDisabled()) {
        for (auto& c : DXGICapturers) {
            if (c->hMonitor == hMonitor && c->trySetup(hwnd, fromRECT(windowRect), fromRECT(captureRect)))
                return c.get();
        }
    }
    if (!Cfg.isCapturerWin32Disabled()) {
        for (auto& c : Win32Capturers) {
            if (c->hMonitor == hMonitor && c->trySetup(hwnd, fromRECT(windowRect), fromRECT(captureRect)))
                return c.get();
        }
    }
    if (!Cfg.isCapturerWinRTDisabled()) {
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

