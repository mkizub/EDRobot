//
// Created by mkizub on 24.05.2025.
//

#pragma once

#include "pch.h"

#ifndef EDROBOT_CAPTURER_H
#define EDROBOT_CAPTURER_H

class Capturer {
public:
    static Capturer* getEDCapturer(HWND hwnd);

    virtual ~Capturer() = default;

    virtual bool start() { atomicIsStarted.exchange(true); return true; };
    virtual bool stop() { atomicIsStarted.exchange(false); return true; };
    virtual upFrame capture(upFrame&& recycle) = 0;

    cv::Rect getCaptureRect();
    cv::Rect getMonitorVirtualRect();

    [[nodiscard]] bool isStarted() { return atomicIsStarted.load(); }
    [[nodiscard]] bool isDefaultCapturer() { return this == DefaultCapturer; }

    virtual void recycle(Frame* frame) const = 0;

protected:
    Capturer(HMONITOR hMonitor, LPMONITORINFOEX monitorInfoEx);
    virtual bool trySetup(HWND hWnd, cv::Rect windowRect, cv::Rect clientRect);

    // should be const, but I'm lazy to make all const_cast in the initializer
    double dpiScaleX;
    double dpiScaleY;
    cv::Rect monitorVirtRect; // in virtual desktop coordinates
    cv::Rect windowVirtRect;
    cv::Rect captureVirtRect; // in virtual desktop coordinates
    int titleHeight;
    int borderWidth;
    HMONITOR hMonitor;
    HWND hWndED;
    MONITORINFOEX monitorInfo;
    std::atomic<bool> atomicIsStarted;

private:
    static std::vector<std::unique_ptr<class CapturerWin32>> Win32Capturers;
    static std::vector<std::unique_ptr<class CapturerWinRT>> WinRTCapturers;
    static std::vector<std::unique_ptr<class CapturerDXGI>> DXGICapturers;
    static Capturer *DefaultCapturer;

    static void InitCapturers();
    static BOOL CALLBACK MonitorEnumProc(HMONITOR hMonitor, HDC hdcMonitor, LPRECT lprcMonitor, LPARAM dwData);
};


#endif //EDROBOT_CAPTURER_H
