//
// Created by mkizub on 11.06.2025.
//

#include "../pch.h"

#include "UISelectRect.h"

static std::weak_ptr<UISelectRect> gInstance;
cv::Rect UISelectRect::gScreenRect;
cv::Rect UISelectRect::gSelectedRect;

static const wchar_t* gWindowClass = L"SelectRectWindowClass";
static const wchar_t* gWindowName = L"EDRobot Rect Selector";

bool UISelectRect::initialize() {
    return UIWindow::registerClass(gWindowClass);
}

std::shared_ptr<UISelectRect> UISelectRect::getInstance() {
    std::shared_ptr<UISelectRect> alive = gInstance.lock();
    if (!alive) {
        alive = std::shared_ptr<UISelectRect>(new UISelectRect());
        gInstance = alive;
    }
    return alive;
}

UISelectRect::UISelectRect() : UIWindow(gWindowClass) {
}

UISelectRect::~UISelectRect() {
}

bool UISelectRect::createWindow() {
    DWORD dwExStyle = WS_EX_LAYERED | WS_EX_TOPMOST;
    DWORD dwStyle = WS_POPUP | WS_BORDER;

    HWND hWndED = FindWindow(Master::ED_WINDOW_CLASS, Master::ED_WINDOW_NAME);

    MONITORINFOEX monitorInfo;
    monitorInfo.cbSize = sizeof(monitorInfo);
    hMonitor = MonitorFromWindow(hWndED, MONITOR_DEFAULTTOPRIMARY);
    GetMonitorInfo(hMonitor, &monitorInfo);
    mMonitorFullRect = fromRECT(monitorInfo.rcMonitor);
    mMonitorWorkRect = fromRECT(monitorInfo.rcWork);

    if (gSelectedRect.empty() || gScreenRect != mMonitorFullRect) {
        gScreenRect = fromRECT(monitorInfo.rcMonitor);
        cv::Size size{100, 100};
        cv::Point center = (gScreenRect.tl() + gScreenRect.br()) * 0.5;
        gSelectedRect = {center, size};
    }

    UIWindow::createWindow(gWindowName, dwExStyle, dwStyle, gSelectedRect);
    if (!hWnd)
        return false;

    //set window background to white
    HBRUSH hbr = CreateSolidBrush(RGB(255, 255, 255));
    SetClassLongPtr(hWnd, GCLP_HBRBACKGROUND, (LONG_PTR) hbr);
    // Set transparency (25% opaque)
    float transparency_percentage = 0.25f;
    SetLayeredWindowAttributes(hWnd, 0, (BYTE) (255 * transparency_percentage), LWA_ALPHA);

    return true;
}

void UISelectRect::onPaint() {
    DefWindowProc(hWnd, WM_PAINT, 0, 0);
}

static const long minSz = 20;

int UISelectRect::isSizeAndIncr(int& incr) {
    mRepeatKeyCount += 1;
    incr = 1 + std::min(20, (mRepeatKeyCount / 3));
    return (GetKeyState(VK_CONTROL) & 0x8000);
}

INT_PTR UISelectRect::onMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    int x, y;
    switch (msg) {
    case WM_GETMINMAXINFO: {
        MINMAXINFO* mmi = (MINMAXINFO*)lParam;
        mmi->ptMinTrackSize.x = minSz;
        mmi->ptMinTrackSize.y = minSz;
        return (LRESULT) 0;
    }

    case WM_NCHITTEST: {
        x = LOWORD(lParam);
        y = HIWORD(lParam);
        {
            RECT rect;
            if (!GetWindowRect(hWnd, &rect))
                break;
            bool lc = std::abs(x - rect.left) < minSz / 4;
            bool rc = std::abs(x - rect.right) < minSz / 4;
            bool tc = std::abs(y - rect.top) < minSz / 4;
            bool bc = std::abs(y - rect.bottom) < minSz / 4;
            if (tc && lc) return HTTOPLEFT;
            if (tc && rc) return HTTOPRIGHT;
            if (bc && lc) return HTBOTTOMLEFT;
            if (bc && rc) return HTBOTTOMRIGHT;
            if (tc) return HTTOP;
            if (bc) return HTBOTTOM;
            if (lc) return HTLEFT;
            if (rc) return HTRIGHT;
            SetCursor(LoadCursor(nullptr, IDC_SIZEALL));
            return HTCAPTION;
        }
        return HTCLIENT;
    }
    case WM_SETCURSOR:
        if (LOWORD(lParam) == HTCAPTION) {
            SetCursor(LoadCursor(nullptr, IDC_SIZEALL));
            return (LRESULT) 0;
        }
        break;

    case WM_KEYUP:
        mRepeatKeyCount = 0;
        break;
    case WM_KEYDOWN:
        switch (wParam) {
        case VK_ESCAPE:
            PostMessage(hWnd, WM_CLOSE, 0, 0);
            break;
        case VK_RETURN:
        {
            RECT rect;
            if (!GetWindowRect(hWnd, &rect))
                break;
            gSelectedRect = fromRECT(rect);
            Master::getInstance().pushDevRectScreenshotCommand(gSelectedRect - mMonitorFullRect.tl());
        }
            PostMessage(hWnd, WM_CLOSE, 0, 0);
            break;
        case VK_LEFT:
            if (isSizeAndIncr(x))
                resizeWindow(-x, 0, minSz);
            else
                moveWindow(-x, 0);
            break;
        case VK_RIGHT:
            if (isSizeAndIncr(x))
                resizeWindow(+x, 0, minSz);
            else
                moveWindow(+x, 0);
            break;
        case VK_UP:
            if (isSizeAndIncr(y))
                resizeWindow(0, -y, minSz);
            else
                moveWindow(0, -y);
            break;
        case VK_DOWN:
            if (isSizeAndIncr(y))
                resizeWindow(0, +y, minSz);
            else
                moveWindow(0, +y);
            break;
        default:
            break;
        }
    }
    return (LRESULT) DefWindowProc(hWnd, msg, wParam, lParam);
}
