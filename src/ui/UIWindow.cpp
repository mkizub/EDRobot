//
// Created by mkizub on 11.06.2025.
//

#include "../pch.h"

#include "UIWindow.h"

#include "../../ui/resource.h"

INT_PTR CALLBACK UIWindow::WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    auto* uiWindow = (UIWindow*)GetWindowLongPtr(hWnd, GWLP_USERDATA);
    if (message == WM_CREATE) {
        uiWindow = (UIWindow*)(((CREATESTRUCT *)lParam)->lpCreateParams);
        SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)uiWindow);
    }
    if (uiWindow) {
        switch (message) {
        case WM_DESTROY:
            SetWindowLongPtr(hWnd, GWLP_USERDATA, 0);
            PostQuitMessage(0);
            return 0L;
        case WM_PAINT:
            uiWindow->onPaint();
            return 0L;
        default:
            break;
        }
        return uiWindow->onMessage(hWnd, message, wParam, lParam);
    }
    return DefWindowProc(hWnd, message, wParam, lParam);
}

bool UIWindow::registerClass(const wchar_t* windowClass) {
    static std::set<const wchar_t*> registeredClasses;
    if (registeredClasses.contains(windowClass))
        return true;

    HINSTANCE hInstance = GetModuleHandle(nullptr);

    WNDCLASSEX wc {};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH) (COLOR_WINDOW + 1);
    wc.lpszClassName = windowClass;
    wc.hIconSm = LoadIcon(nullptr, IDI_APPLICATION);
    if (!RegisterClassEx(&wc)) {
        LOG(ERROR) << "Failed to register window class '" << toUtf8(windowClass) << "'; error: " << getErrorMessage();
        return false;
    }

    registeredClasses.insert(windowClass);
    return true;
}

UIWindow::UIWindow(const wchar_t *windowClass)
    : mWindowClass(windowClass)
    , hMonitor(nullptr)
    , hWnd(nullptr)
{
    registerClass(windowClass);
}

cv::Rect UIWindow::calcWindowRect(int align, cv::Point winOffset, cv::Size winSize) {
    cv::Rect winRect;
    if (align & ALIGN_FULLSCREEN) {
        winRect = mMonitorFullRect;
    } else {
        if ((align & (ALIGN_LEFT | ALIGN_RIGHT)) == (ALIGN_LEFT | ALIGN_RIGHT)) {
            winRect.x = mMonitorWorkRect.x + winOffset.x;
            winRect.width = mMonitorWorkRect.width - (2 * winOffset.x + winSize.width);
        } else if ((align & (ALIGN_LEFT | ALIGN_RIGHT)) == 0) {
            winRect.x = mMonitorWorkRect.x + mMonitorWorkRect.width / 2 - winOffset.x - winSize.width / 2;
            winRect.width = winSize.width;
        } else if (align & ALIGN_LEFT) {
            winRect.x = mMonitorWorkRect.x + winOffset.x;
            winRect.width = winSize.width;
        } else if (align & ALIGN_RIGHT) {
            winRect.x = mMonitorWorkRect.br().x - winOffset.x - winSize.width;
            winRect.width = winSize.width;
        }

        if ((align & (ALIGN_TOP | ALIGN_BOTTOM)) == (ALIGN_TOP | ALIGN_BOTTOM)) {
            winRect.y = mMonitorWorkRect.y + winOffset.y;
            winRect.height = mMonitorWorkRect.height - (2 * winOffset.y + winSize.height);
        } else if ((align & (ALIGN_LEFT | ALIGN_RIGHT)) == 0) {
            winRect.y = mMonitorWorkRect.y + mMonitorWorkRect.height / 2 - winOffset.y - winSize.height / 2;
            winRect.height = winSize.height;
        } else if (align & ALIGN_TOP) {
            winRect.y = mMonitorWorkRect.y + winOffset.y;
            winRect.height = winSize.height;
        } else if (align & ALIGN_BOTTOM) {
            winRect.y = mMonitorWorkRect.br().y - winOffset.y - winSize.height;
            winRect.height = winSize.height;
        }
    }
    return winRect;
}
HWND UIWindow::createWindow(const wchar_t *windowName, DWORD dwExStyle, DWORD dwStyle, int align, cv::Point winOffset, cv::Size winSize) {
    MONITORINFOEX monitorInfo;
    monitorInfo.cbSize = sizeof(monitorInfo);
    hMonitor = MonitorFromWindow(nullptr, MONITOR_DEFAULTTOPRIMARY);
    GetMonitorInfo(hMonitor, &monitorInfo);
    mMonitorFullRect = fromRECT(monitorInfo.rcMonitor);
    mMonitorWorkRect = fromRECT(monitorInfo.rcWork);

    cv::Rect winRect = calcWindowRect(align, winOffset, winSize);

    return createWindow(windowName, dwExStyle, dwStyle, winRect);
}

HWND UIWindow::createWindow(const wchar_t *windowName, DWORD dwExStyle, DWORD dwStyle, cv::Rect rect) {
    if (!hMonitor) {
        MONITORINFOEX monitorInfo;
        monitorInfo.cbSize = sizeof(monitorInfo);
        hMonitor = MonitorFromWindow(nullptr, MONITOR_DEFAULTTOPRIMARY);
        GetMonitorInfo(hMonitor, &monitorInfo);
        mMonitorFullRect = fromRECT(monitorInfo.rcMonitor);
        mMonitorWorkRect = fromRECT(monitorInfo.rcMonitor);
    }

    HINSTANCE hInstance = GetModuleHandle(nullptr);

    hWnd = CreateWindowEx(
            dwExStyle,
            mWindowClass,
            windowName,
            dwStyle,
            rect.x,
            rect.y,
            rect.width,
            rect.height,
            nullptr, // parent
            nullptr, // menu
            hInstance,
            this // initialize pointer to self in WindowProc
    );
    return hWnd;
}

void UIWindow::moveWindow(int dx, int dy) {
    RECT windowRect;
    if (!GetWindowRect(hWnd, &windowRect))
        return;
    cv::Rect tmp = fromRECT(windowRect);
    tmp += cv::Point(dx, dy);
    if ((tmp & mMonitorFullRect) != tmp) {
        if (tmp.x < mMonitorFullRect.x)
            tmp.x = mMonitorFullRect.x;
        if (tmp.br().x > mMonitorFullRect.br().x)
            tmp.x = mMonitorFullRect.br().x - tmp.width;
        if (tmp.y < mMonitorFullRect.y)
            tmp.y = mMonitorFullRect.y;
        if (tmp.br().y > mMonitorFullRect.br().y)
            tmp.y = mMonitorFullRect.br().y - tmp.height;
    }
    MoveWindow(hWnd, tmp.x, tmp.y, tmp.width, tmp.height, TRUE);
}

void UIWindow::resizeWindow(int dw, int dh, int min_size) {
    RECT rect;
    if (!GetWindowRect(hWnd, &rect))
        return;
    auto width = std::max(LONG(min_size), rect.right - rect.left + dw);
    auto height = std::max(LONG(min_size), rect.bottom - rect.top + dh);
    cv::Rect tmp(rect.left, rect.top, width, height);
    tmp &= mMonitorFullRect;
    MoveWindow(hWnd, tmp.x, tmp.y, tmp.width, tmp.height, TRUE);
}

INT_PTR UIWindow::onMessage(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    return (LRESULT) DefWindowProc(hwnd, message, wParam, lParam);
}

void UIWindow::show() {
    if (hWnd) {
        SetForegroundWindow(hWnd);
        return;
    }

    auto spSelf = shared_from_this();
    StartLock startLock;
    std::unique_lock<std::mutex> lock(startLock.mMutex);
    std::thread thr(&UIWindow::windowLoopProc, spSelf, &startLock);
    startLock.mCond.wait(lock, [&startLock] { return startLock.started; });
    thr.detach();
}

void UIWindow::windowLoopProc(std::shared_ptr<UIWindow> spWnd, StartLock* startLock) {
    if (!spWnd->createWindow() || !spWnd->hWnd) {
        std::lock_guard<std::mutex> lock(startLock->mMutex);
        startLock->started = true;
        startLock->mCond.notify_all();
        return;
    }

    ShowWindow(spWnd->hWnd, SW_SHOW);
    UpdateWindow(spWnd->hWnd);
    SetForegroundWindow(spWnd->hWnd);

    {
        std::lock_guard<std::mutex> lock(startLock->mMutex);
        startLock->started = true;
        startLock->mCond.notify_all();
    }

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}

