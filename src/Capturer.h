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

    virtual cv::Rect getCaptureRect();
    virtual cv::Rect getMonitorVirtualRect();

    [[nodiscard]] bool isStarted() { return atomicIsStarted.load(); };

    virtual void recycle(Frame* frame) const = 0;

protected:
    Capturer(HMONITOR hMonitor, LPMONITORINFOEX monitorInfoEx);
    virtual bool trySetup(HWND hWnd, RECT& captRect);

    // should be const, but I'm lazy to make all const_cast in the initializer
    MONITORINFOEX monitorInfo;
    double dpiScaleX;
    double dpiScaleY;
    bool needScaling;
    int screenWidth;
    int screenHeight;
    HMONITOR hMonitor;
    HWND hWndED;
    RECT captureRect;
    std::atomic<bool> atomicIsStarted;

private:
    static std::vector<std::unique_ptr<Capturer>> AllCapturers;
    static Capturer *DefaultCapturer;

    static void InitCapturers();
    static BOOL CALLBACK MonitorEnumProc(HMONITOR hMonitor, HDC hdcMonitor, LPRECT lprcMonitor, LPARAM dwData);
};


#endif //EDROBOT_CAPTURER_H
