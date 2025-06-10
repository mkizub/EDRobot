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


    ~Capturer();
    bool captureDisplay();
    cv::Mat getColorImage();
    cv::Mat getGrayImage();
    cv::Rect getCaptureRect();
    cv::Rect getMonitorVirtualRect();

private:
    static void InitCapturers();
    static BOOL CALLBACK MonitorEnumProc(HMONITOR hMonitor, HDC hdcMonitor, LPRECT lprcMonitor, LPARAM dwData);
    Capturer(HMONITOR hMonitor, LPMONITORINFOEX monitorInfoEx, HDC hdcMonitor);
    void initBuffers();

    const bool isHdcScreenCreated;
    MONITORINFOEX monitorInfo;
    double dpiScaleX;
    double dpiScaleY;
    bool needScaling;
    int screenWidth;
    int screenHeight;
    HMONITOR hMonitor;
    HDC hdcScreen;
    HDC hdcMem;
    HBITMAP hBitmap;
    BITMAPINFOHEADER bitmapInfoHeader;
    HWND captureHwnd;
    RECT captureRect;
    cv::Mat capturedImage;
    cv::Mat grayImage;
};


#endif //EDROBOT_CAPTURER_H
