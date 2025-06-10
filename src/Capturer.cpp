//
// Created by mkizub on 24.05.2025.
//

#include "pch.h"
#include "Capturer.h"

#include <shellscalingapi.h>


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
        AllCapturers.push_back(std::unique_ptr<Capturer>(new Capturer(hMonitor, &monitorInfoEx, hdcMonitor)));
    }
    return TRUE;
}

void Capturer::InitCapturers() {
    DefaultCapturer = nullptr;
    AllCapturers.clear();

    SetProcessDpiAwareness(PROCESS_PER_MONITOR_DPI_AWARE);

    EnumDisplayMonitors(nullptr, nullptr, MonitorEnumProc, (LPARAM)nullptr);

    DefaultCapturer = new Capturer(nullptr, nullptr, nullptr);
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

Capturer::Capturer(HMONITOR hMonitor, LPMONITORINFOEX monitorInfoEx, HDC hdcMonitor)
    : isHdcScreenCreated(hdcMonitor == nullptr)
    , dpiScaleX{1.0}
    , dpiScaleY{1.0}
    , needScaling(false)
    , hMonitor(hMonitor)
    , hdcMem(nullptr)
    , hBitmap(nullptr)
    , bitmapInfoHeader{}
{
    if (monitorInfoEx) {
        memcpy(&monitorInfo, monitorInfoEx, sizeof(monitorInfo));
        screenWidth = monitorInfo.rcMonitor.right - monitorInfo.rcMonitor.left;
        screenHeight = monitorInfo.rcMonitor.bottom - monitorInfo.rcMonitor.top;
        hdcScreen = hdcMonitor;
        if (!hdcScreen) {
            hdcScreen = CreateDC(monitorInfo.szDevice, nullptr, nullptr, nullptr);
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


cv::Mat Capturer::getColorImage() {
    RECT screenRect{0, 0, screenWidth, screenHeight};
    if (captureRect == screenRect)
        return capturedImage;
    int x = captureRect.left - monitorInfo.rcMonitor.left;
    int y = captureRect.top - monitorInfo.rcMonitor.top;
    int w = captureRect.right - captureRect.left;
    int h = captureRect.bottom - captureRect.top;
    cv::Rect crop(x,y,w,h);
    if (crop.x < 0 || crop.y < 0 || crop.br().x > screenWidth || crop.br().y > screenHeight) {
        LOG(WARNING) << "Window is out of screen borders: " << crop << " on " << cv::Size(screenWidth, screenHeight) << " screen";
        cv::Mat tmp(h, w, capturedImage.type());
        cv::Rect is = crop & cv::Rect(cv::Point(), capturedImage.size());
        int dr = (y < 0) ? -y : 0;
        int dc = (x < 0) ? -x : 0;
        for(int r = dr; r < is.height; r++) {
            memcpy(tmp.ptr<char>(r, dc), capturedImage.ptr<char>(r+is.y-dr, is.x), is.width*4);
        }
        return tmp;
    }
    return cv::Mat(capturedImage, cv::Rect(x,y,w,h));
}

cv::Mat Capturer::getGrayImage() {
    RECT screenRect{0, 0, screenWidth, screenHeight};
    if (captureRect == screenRect)
        return grayImage;
    int x = captureRect.left - monitorInfo.rcMonitor.left;
    int y = captureRect.top - monitorInfo.rcMonitor.top;
    int w = captureRect.right - captureRect.left;
    int h = captureRect.bottom - captureRect.top;
    cv::Rect crop(x,y,w,h);
    if (crop.x < 0 || crop.y < 0 || crop.br().x > screenWidth || crop.br().y > screenHeight) {
        LOG(WARNING) << "Window is out of screen borders: " << crop << " on " << cv::Size(screenWidth, screenHeight) << " screen";
        cv::Mat tmp(h, w, grayImage.type());
        cv::Rect is = crop & cv::Rect(cv::Point(), grayImage.size());
        int dr = (y < 0) ? -y : 0;
        int dc = (x < 0) ? -x : 0;
        for(int r = dr; r < is.height; r++) {
            //for(int c = 0; c < is.width; c++)
            //    tmp.at<char>(r, c+dc) = grayImage.at<char>(r+is.y,c+is.x);
            memcpy(tmp.ptr<char>(r, dc), grayImage.ptr<char>(r+is.y-dr, is.x), is.width);
        }
        return tmp;
    }
    return cv::Mat(grayImage, crop);
}

cv::Rect Capturer::getCaptureRect() {
    cv::Point lt {captureRect.left, captureRect.top};
    cv::Point rb {captureRect.right, captureRect.bottom};
    return cv::Rect(lt, rb);
}

cv::Rect Capturer::getMonitorVirtualRect() {
    cv::Point lt {monitorInfo.rcMonitor.left, monitorInfo.rcMonitor.top};
    cv::Point rb {monitorInfo.rcMonitor.right, monitorInfo.rcMonitor.bottom};
    return cv::Rect(lt, rb);
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
