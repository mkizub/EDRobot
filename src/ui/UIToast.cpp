//
// Created by mkizub on 11.06.2025.
//

#include "../pch.h"

#include "UIToast.h"

static std::weak_ptr<UIToast> gInstance;
static const wchar_t* gWindowClass = L"ShowPopupMessageWindowClass";
static const wchar_t* gWindowName = L"EDRobot Toast";

bool UIToast::initialize() {
    return UIWindow::registerClass(gWindowClass);
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
}

UIToast::~UIToast() {
}

bool UIToast::createWindow() {
    DWORD dwExStyle = WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE;
    DWORD dwStyle = WS_POPUP | WS_DISABLED;

    UIWindow::createWindow(gWindowName, dwExStyle, dwStyle, ALIGN_TOP|ALIGN_RIGHT, {40-300, 80}, {300, 150});
    if (!hWnd)
        return false;

    //set window background to white
    HBRUSH hbr = CreateSolidBrush(RGB(0, 0, 0));
    SetClassLongPtr(hWnd, GCLP_HBRBACKGROUND, (LONG_PTR) hbr);
    // Set transparency (25% opaque)
    float transparency_percentage = 0.60f;
    SetLayeredWindowAttributes(hWnd, 0, (BYTE) (255 * transparency_percentage), LWA_ALPHA);
    return true;
}

void UIToast::onPaint() {
    RECT rc;
    HFONT hFont,hTmp;
    HBRUSH hBrush;
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hWnd, &ps);
    // All painting occurs here, between BeginPaint and EndPaint.
    hBrush = CreateSolidBrush(RGB(0, 0, 0));
    FillRect(hdc, &ps.rcPaint, hBrush/*(HBRUSH) (COLOR_WINDOW+1)*/);
    //hBrush=CreateSolidBrush(RGB(255,0,0));
    //FillRect(hdc,&rc,hBrush);
    DeleteObject(hBrush);
    SetTextAlign(hdc, TA_LEFT|TA_TOP);
    SetTextColor(hdc, RGB(255,255,255));
    hFont = CreateFont(20,0,0,0,FW_BOLD,0,0,0,0,0,0,PROOF_QUALITY,VARIABLE_PITCH|FF_MODERN,nullptr);
    hTmp = (HFONT)SelectObject(hdc,hFont);
    SetBkMode(hdc, TRANSPARENT);
    //TextOutW(hdc, 20, 20, popupTitle.c_str(), popupTitle.size());
    rc = ps.rcPaint;
    rc.top += 10;
    rc.left += 10;
    rc.right -= 10;
    rc.bottom = rc.top + 20;
    DrawTextW(hdc, mTitle.c_str(), mTitle.size(), &rc, DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);
    DeleteObject(SelectObject(hdc,hTmp));
    hFont = CreateFont(18,0,0,0,FW_NORMAL,0,0,0,0,0,0,PROOF_QUALITY,VARIABLE_PITCH|FF_MODERN,nullptr);
    hTmp = (HFONT)SelectObject(hdc,hFont);
    //TextOutW(hdc, 20, 50, popupText.c_str(), popupText.size());
    rc.top = rc.bottom + 6;
    rc.bottom = ps.rcPaint.bottom - 10;
    DrawTextW(hdc, mText.c_str(), mText.size(), &rc, DT_WORDBREAK|DT_NOPREFIX);
    DeleteObject(SelectObject(hdc,hTmp));
    EndPaint(hWnd, &ps);
}

static void ClosePopupProc(HWND hWnd, UINT, UINT_PTR, DWORD) {
    PostMessage(hWnd, WM_CLOSE, 0, 0);
}


void UIToast::showText(const std::wstring& title, const std::wstring& text) {
    mTitle = title;
    mText = text;
    if (mPopupTimerId) {
        if (mAnimationProgress >= mAnimationTime)
            mPopupTimerId = SetTimer(hWnd, 0, 5000, ClosePopupProc);
        InvalidateRect(hWnd, nullptr, TRUE);
        UpdateWindow(hWnd);
    } else {
        show();
        if (hWnd) {
            if (mAnimationProgress >= mAnimationTime)
                mPopupTimerId = SetTimer(hWnd, mPopupTimerId, 5000, ClosePopupProc);
            else {
                if (!mAnimationProgress)
                    mAnimationStartTick = GetTickCount64();
                mPopupTimerId = SetTimer(hWnd, mPopupTimerId, 10, nullptr);
            }
        }
    }
}

INT_PTR UIToast::onMessage(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    if (message == WM_TIMER) {
        if (mAnimationProgress < mAnimationTime) {
            mAnimationProgress = GetTickCount64() - mAnimationStartTick;
            if (mAnimationProgress > mAnimationTime)
                mAnimationProgress = mAnimationTime;
            else
                mPopupTimerId = SetTimer(hWnd, mPopupTimerId, 10, nullptr);
            int x = 300 * mAnimationProgress / mAnimationTime;
            cv::Rect winRect = calcWindowRect(ALIGN_TOP|ALIGN_RIGHT, {40-300+x, 80}, {300, 150});
            MoveWindow(hWnd, winRect.x, winRect.y, winRect.width, winRect.height, TRUE);
        } else {
            mPopupTimerId = SetTimer(hWnd, mPopupTimerId, 5000, ClosePopupProc);
        }
        return (LRESULT) 0;
    }
    return UIWindow::onMessage(hWnd, message, wParam, lParam);
}

