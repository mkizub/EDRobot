//
// Created by mkizub on 11.06.2025.
//

#pragma once

#ifndef EDROBOT_UITOAST_H
#define EDROBOT_UITOAST_H

#include "UIWindow.h"
#include "UILayout.h"

#include <shellapi.h>
#include <winlamb/font.h>

class UIToast : public UIWindow {
public:
    ~UIToast() final;
private:
    friend class UIManager;
    static std::shared_ptr<UIToast> getInstance();
    static bool initialize();

    UIToast();

    void showText(const std::wstring& title, const std::wstring& text);
    bool createWindow() final;
    void windowCreated() final;

    INT_PTR onMessage(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) final;
    void onPaint() final;

    std::wstring mTitle;
    std::wstring mText;

    int uiPercent = 100;
    UINT uiDpi = USER_DEFAULT_SCREEN_DPI;
    wl::font fontTitle;
    wl::font fontMessage;

    ULONGLONG mAnimationStartTick {};
    int mAnimationProgress {};
    bool mAnimationStarted {};
    bool mShowingToast {};
};


#endif //EDROBOT_UITOAST_H
