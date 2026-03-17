//
// Created by mkizub on 11.06.2025.
//

#include "../pch.h"

#include "UIToast.h"
#include <ShellScalingApi.h>

static std::weak_ptr<UIToast> gInstance;
static const wchar_t* gWindowClass = L"ShowPopupMessageWindowClass";
static const wchar_t* gWindowName = L"EDRobot Toast";

bool UIToast::initialize() {
    return UIWindow::registerClass(gWindowClass, true, false);
}

std::shared_ptr<UIToast> UIToast::getInstance() {
    std::shared_ptr<UIToast> alive = gInstance.lock();
    if (!alive) {
        alive = std::shared_ptr<UIToast>(new UIToast());
        gInstance = alive;
    }
    return alive;
}

UIToast::UIToast() : UIWindow(gWindowClass) {
//    WNDCLASSEX wc {};
//    wc.cbSize = sizeof(WNDCLASSEX);
//    if (GetClassInfoEx(nullptr, L"tooltips_class32", &wc))
//        LOG(INFO) << "Tooltip: style=" << wc.style;
}

UIToast::~UIToast() {
}

#define S(N) MulDiv((N), uiDpi*uiPercent, 100*USER_DEFAULT_SCREEN_DPI)
bool UIToast::createWindow() {
    DWORD dwExStyle = WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE;
    DWORD dwStyle = WS_POPUP | WS_DISABLED;

    HWND hWndED = FindWindow(Master::ED_WINDOW_CLASS, Master::ED_WINDOW_NAME);

    uiPercent = Cfg.getUiScalePercents();
    uiDpi = USER_DEFAULT_SCREEN_DPI;
    hMonitor = MonitorFromWindow(hWndED, MONITOR_DEFAULTTOPRIMARY);
    GetDpiForMonitor(hMonitor, MDT_EFFECTIVE_DPI, &uiDpi, &uiDpi);

    UIWindow::createWindow(hMonitor, gWindowName, dwExStyle, dwStyle, ALIGN_TOP|ALIGN_RIGHT, {S(40-300), S(80)}, {S(300), S(150)});
    if (!hWnd)
        return false;

    loCreateFont(fontTitle, uiDpi, uiPercent, FW_BOLD);
    loCreateFont(fontMessage, uiDpi, uiPercent, FW_NORMAL);

    //set window background to white
    HBRUSH hbr = CreateSolidBrush(RGB(0, 0, 0));
    SetClassLongPtr(hWnd, GCLP_HBRBACKGROUND, (LONG_PTR) hbr);
    // Set transparency (25% opaque)
    float transparency_percentage = 0.60f;
    SetLayeredWindowAttributes(hWnd, 0, (BYTE) (255 * transparency_percentage), LWA_ALPHA);
    return true;
}

void UIToast::windowCreated() {
    PostMessage(hWnd, WM_CANCELMODE, 0, 0);
}

void UIToast::onPaint() {
    RECT rc;
    HGDIOBJ hTmp;
    HBRUSH hBrush;
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hWnd, &ps);
    hBrush = CreateSolidBrush(RGB(0, 0, 0));
    FillRect(hdc, &ps.rcPaint, hBrush);
    DeleteObject(hBrush);
    SetTextAlign(hdc, TA_LEFT|TA_TOP);
    SetTextColor(hdc, RGB(255,255,255));
    hTmp = SelectObject(hdc,fontTitle.hfont());
    SetBkMode(hdc, TRANSPARENT);
    rc = ps.rcPaint;
    auto border = S(LO_DLG_BORDER);
    rc.top += border;
    rc.left += border;
    rc.right -= border;
    rc.bottom = rc.top + 2*border;
    DrawTextW(hdc, mTitle.c_str(), mTitle.size(), &rc, DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);
    SelectObject(hdc,hTmp);
    hTmp = SelectObject(hdc,fontMessage.hfont());
    rc.top = rc.bottom + S(LO_X_GAP);
    rc.bottom = ps.rcPaint.bottom - border;
    DrawTextW(hdc, mText.c_str(), mText.size(), &rc, DT_WORDBREAK|DT_NOPREFIX);
    SelectObject(hdc,hTmp);
    EndPaint(hWnd, &ps);
}

const int ANIMATION_TIME = 100;
const int POPUP_TIME = 3000;
const int TIMER_ID = 1;

void UIToast::showText(const std::wstring& title, const std::wstring& text) {
    mTitle = title;
    mText = text;
    show();
    if (mAnimationStarted || mShowingToast) {
        InvalidateRect(hWnd, nullptr, TRUE);
        UpdateWindow(hWnd);
    }
    mShowingToast = true;
    if (mAnimationProgress && mAnimationStartTick >= ANIMATION_TIME) {
        mAnimationStartTick = GetTickCount64() - ANIMATION_TIME;
        SetTimer(hWnd, TIMER_ID, 500, nullptr);
    } else {
        if (!mAnimationProgress)
            mAnimationStartTick = GetTickCount64();
        SetTimer(hWnd, TIMER_ID, 10, nullptr);
    }
}

INT_PTR UIToast::onMessage(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    if (message == WM_NCHITTEST)
        return HTNOWHERE;
    if (message == WM_TIMER) {
        if (!mShowingToast) {
            KillTimer(hWnd, wParam);
            mAnimationProgress = {};
            mAnimationStartTick = {};
            ShowWindow(hWnd, SW_HIDE);
        }
        else if (mAnimationProgress < ANIMATION_TIME) {
            mAnimationProgress = GetTickCount64() - mAnimationStartTick;
            if (mAnimationProgress >= ANIMATION_TIME) {
                mAnimationProgress = ANIMATION_TIME;
                SetTimer(hWnd, TIMER_ID, 500, nullptr);
            }
            int x = 300 * mAnimationProgress / ANIMATION_TIME;
            cv::Rect winRect = calcWindowRect(ALIGN_TOP|ALIGN_RIGHT, {S(40-300+x), S(80)}, {S(300), S(150)});
            MoveWindow(hWnd, winRect.x, winRect.y, winRect.width, winRect.height, TRUE);
        }
        else if (GetTickCount64() - mAnimationStartTick >= POPUP_TIME) {
            //PostMessage(hWnd, WM_CLOSE, 0, 0);
            KillTimer(hWnd, TIMER_ID);
            mShowingToast = false;
            mAnimationProgress = 0;
            mAnimationStartTick = 0;
            ShowWindow(hWnd, SW_HIDE);
        }
        return (LRESULT) 0;
    }
    return UIWindow::onMessage(hWnd, message, wParam, lParam);
}

