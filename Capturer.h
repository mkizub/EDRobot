//
// Created by mkizub on 24.05.2025.
//

#pragma once

#ifndef EDROBOT_CAPTURER_H
#define EDROBOT_CAPTURER_H

#include <windef.h>
#include <winuser.h>
#include <wingdi.h>
#include <opencv2/opencv.hpp>

class Capturer {
public:
    static Capturer* getEDCapturer(HWND hwnd);


    ~Capturer();
    bool captureDisplay();
    cv::Mat getCapturedImage();
    cv::Mat getGrayImage();

private:
    static void InitCapturers();
    static BOOL CALLBACK MonitorEnumProc(HMONITOR hMonitor, HDC hdcMonitor, LPRECT lprcMonitor, LPARAM dwData);
    Capturer(LPMONITORINFOEX monitor, HDC hdcMonitor);
    void initBuffers();

    const bool isHdcScreenCreated;
    MONITORINFOEX monitorInfo;
    int screenWidth;
    int screenHeight;
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
