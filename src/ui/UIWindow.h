//
// Created by mkizub on 11.06.2025.
//

#pragma once

#ifndef EDROBOT_UIWINDOW_H
#define EDROBOT_UIWINDOW_H


class UIWindow : public std::enable_shared_from_this<UIWindow> {
protected:
    UIWindow(const wchar_t* windowClass);
    virtual ~UIWindow() = default;

    const int ALIGN_LEFT       = 0x1;
    const int ALIGN_RIGHT      = 0x2;
    const int ALIGN_TOP        = 0x4;
    const int ALIGN_BOTTOM     = 0x8;
    const int ALIGN_FULLSCREEN = 0x10;

    static INT_PTR CALLBACK WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
    static bool registerClass(const wchar_t* windowClass, bool popup, bool no_bg);

    void moveWindow(int dx, int dy);
    void resizeWindow(int dw, int dh, int min_size);
    void fitToMonitor(cv::Rect& windowRect);

    HWND createWindow(HMONITOR hMon, const wchar_t* windowName, DWORD dwExStyle, DWORD dwStyle, cv::Rect rect);
    HWND createWindow(HMONITOR hMon, const wchar_t* windowName, DWORD dwExStyle, DWORD dwStyle, int align, cv::Point offset, cv::Size size);
    cv::Rect calcWindowRect(int align, cv::Point offset, cv::Size size);

    void show();

    virtual bool createWindow() = 0;
    virtual void windowCreated() {}

    virtual void onDestroy() {};
    virtual INT_PTR onMessage(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
    virtual void onPaint() = 0;

    const wchar_t* mWindowClass;
    HMONITOR hMonitor;
    HWND hWnd;
    cv::Rect mMonitorFullRect;
    cv::Rect mMonitorWorkRect;
    bool showNoActivate {};

private:
    struct StartLock {
        StartLock() : started(false) {}
        bool started;
        std::mutex mMutex;
        std::condition_variable mCond;
    };
    static void windowLoopProc(std::shared_ptr<UIWindow> spWnd, StartLock* startLock);
};


#endif //EDROBOT_UIWINDOW_H
