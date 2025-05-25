//
// Created by mkizub on 24.05.2025.
//

#include "Capturer.h"
#include <vector>
#include "easylogging++.h"
#include "Utils.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>


static std::vector<std::unique_ptr<Capturer>> AllCapturers;
static Capturer *DefaultCapturer;

static bool operator==(const RECT& lhs, const RECT& rhs) {
    return memcmp(&lhs, &rhs, sizeof(RECT)) == 0;
}

static bool operator!=(const RECT& lhs, const RECT& rhs) {
    return memcmp(&lhs, &rhs, sizeof(RECT)) != 0;
}

BOOL CALLBACK Capturer::MonitorEnumProc(HMONITOR hMonitor, HDC hdcMonitor, LPRECT lprcMonitor, LPARAM dwData) {
    MONITORINFOEX monitorInfoEx;
    monitorInfoEx.cbSize = sizeof(MONITORINFOEX);
    if (GetMonitorInfo(hMonitor, &monitorInfoEx)) {
        AllCapturers.push_back(std::unique_ptr<Capturer>(new Capturer(&monitorInfoEx, hdcMonitor)));
    }
    return TRUE;
}

void Capturer::InitCapturers() {
    DefaultCapturer = nullptr;
    AllCapturers.clear();

    EnumDisplayMonitors(nullptr, nullptr, MonitorEnumProc, (LPARAM)nullptr);

    DefaultCapturer = new Capturer(nullptr, nullptr);
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

    for (auto& c : AllCapturers) {
        for (int i=0; i < CCHDEVICENAME; i++) {
            auto c1 = c->monitorInfo.szDevice[i];
            auto c2 = monitorInfo.szDevice[i];
            if (c1 != c2)
                break;
            if (c1 == 0) {
                RECT rect;
                if (GetWindowRect(hwnd, &rect) && rect != c->monitorInfo.rcMonitor) {
                    c->captureHwnd = hwnd;
                    c->captureRect = rect;
                    RECT clientRect;
                    if (GetClientRect(hwnd, &clientRect)) {
                        ClientToScreen(hwnd, (LPPOINT)&clientRect.left);
                        ClientToScreen(hwnd, (LPPOINT)&clientRect.right);
                        c->captureRect = clientRect;
                    }
                } else {
                    c->captureHwnd = nullptr;
                    c->captureRect = {0, 0, c->screenWidth, c->screenHeight};
                }
                return c.get();
            }
        }
    }

    LOG(ERROR) << "Cannot find monitor " << monitorInfo.szDevice;
    return DefaultCapturer;
}

Capturer::Capturer(LPMONITORINFOEX monitor, HDC hdcMonitor)
    : isHdcScreenCreated(hdcMonitor == nullptr)
    , hdcMem(nullptr)
    , hBitmap(nullptr)
    , bitmapInfoHeader{}
{
    if (monitor) {
        memcpy(&monitorInfo, monitor, sizeof(monitorInfo));
        screenWidth = monitorInfo.rcMonitor.right - monitorInfo.rcMonitor.left;
        screenHeight = monitorInfo.rcMonitor.bottom - monitorInfo.rcMonitor.top;
        hdcScreen = hdcMonitor;
        if (!hdcScreen) {
            hdcScreen = CreateDCA(monitorInfo.szDevice, nullptr, nullptr, nullptr);
        }
    } else {
        screenWidth = GetSystemMetrics(SM_CXSCREEN);
        screenHeight = GetSystemMetrics(SM_CYSCREEN);
        memset(&monitorInfo, 0, sizeof(monitorInfo));
        monitorInfo.cbSize = sizeof(monitorInfo);
        monitorInfo.rcMonitor = RECT(0, 0, screenWidth, screenHeight);
        monitorInfo.rcWork = monitorInfo.rcMonitor;
        hdcScreen = GetDC(nullptr);
    }
    captureRect = {0, 0, screenWidth, screenHeight};
}

Capturer::~Capturer() {
    if (hBitmap)
        DeleteObject(hBitmap);
    if (hdcMem)
        DeleteDC(hdcMem);
    if (isHdcScreenCreated)
        ReleaseDC(nullptr, hdcScreen);
}

void Capturer::initBuffers() {
    if (hBitmap)
        DeleteObject(hBitmap);
    if (hdcMem)
        DeleteDC(hdcMem);
    hdcMem = CreateCompatibleDC(hdcScreen);
    hBitmap = CreateCompatibleBitmap(hdcScreen, screenWidth, screenHeight);
    memset(&bitmapInfoHeader, 0, sizeof(bitmapInfoHeader));
    bitmapInfoHeader.biSize = sizeof(BITMAPINFOHEADER);
    bitmapInfoHeader.biWidth = screenWidth;
    bitmapInfoHeader.biHeight = -screenHeight;  // Negative height to flip the image vertically
    bitmapInfoHeader.biPlanes = 1;
    bitmapInfoHeader.biBitCount = 32;
    bitmapInfoHeader.biCompression = BI_RGB;
    capturedImage = cv::Mat(screenHeight, screenWidth, CV_8UC4);
    grayImage = cv::Mat(screenHeight, screenWidth, CV_8UC1);
}


cv::Mat Capturer::getCapturedImage() {
    RECT screenRect{0, 0, screenWidth, screenHeight};
    if (captureRect == screenRect)
        return capturedImage;
    int x = captureRect.left;
    int y = captureRect.top;
    int w = captureRect.right - x;
    int h = captureRect.bottom -y;
    return cv::Mat(capturedImage, cv::Rect(x,y,w,h));
}

cv::Mat Capturer::getGrayImage() {
    RECT screenRect{0, 0, screenWidth, screenHeight};
    if (captureRect == screenRect)
        return grayImage;
    int x = captureRect.left;
    int y = captureRect.top;
    int w = captureRect.right - x;
    int h = captureRect.bottom -y;
    return cv::Mat(grayImage, cv::Rect(x,y,w,h));
}

bool Capturer::captureDisplay() {
    if (!hdcMem)
        initBuffers();

    HBITMAP hOldBitmap = (HBITMAP)SelectObject(hdcMem, hBitmap);

    int ok = BitBlt(hdcMem, 0, 0, screenWidth, screenHeight, hdcScreen, 0, 0, SRCCOPY);
    if (!ok) {
        LOG(ERROR) << "BitBlt failed: " << getErrorMessage();
        return false;
    }

    int lines = GetDIBits(hdcMem, hBitmap, 0, screenHeight, capturedImage.data, (BITMAPINFO*)&bitmapInfoHeader, DIB_RGB_COLORS);
    if (lines != screenHeight) {
        LOG(ERROR) << "GetDIBits failed: " << getErrorMessage();
        return false;
    }

    cv::cvtColor(capturedImage, grayImage, cv::COLOR_RGBA2GRAY);
    //cv::imwrite("captured-screen.png", capturedImage);
    //cv::imwrite("captured-screen-gray.png", grayImage);

    SelectObject(hdcMem, hOldBitmap);
    return true;
}
