//
// Created by mkizub on 15.06.2025.
//

#pragma once

#ifndef EDROBOT_UIDEBUG_H
#define EDROBOT_UIDEBUG_H

#include "UIWindow.h"

struct ID2D1Factory;
struct ID2D1HwndRenderTarget;
struct ID2D1SolidColorBrush;
struct ID2D1Bitmap;

class UIDebug : public UIWindow {
public:
    ~UIDebug() final;
private:
    friend class UIManager;
    static std::shared_ptr<UIDebug> getInstance(bool create);
    static bool initialize();

    UIDebug();

    void setGameImage(const cv::Mat& image);
    void setDebugOverlay(const cv::Mat& overlay);

    bool createWindow() final;
    void windowCreated() final;

    void onDestroy() final;
    INT_PTR onMessage(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) final;
    void onPaint() final;

    void resizeWindow();

    float mOutputScale = 1.0f; //0.5f;
    float mDPIScale = 1.0f;
    cv::Size mGameSize;
    cv::Size mOutputSize;

    ID2D1Factory* pD2DFactory {nullptr};
    ID2D1HwndRenderTarget* pRenderTarget {nullptr};
    ID2D1SolidColorBrush* pBrush {nullptr};
    ID2D1Bitmap* pGameBitmap {nullptr};
    ID2D1Bitmap* pOverlayBitmap {nullptr};

    std::recursive_mutex mMutex; // protect current image
    cv::Mat mGameImage;
    cv::Mat mDebugOverlay;
    bool mGameImageUpdated {false};
    bool mDebugOverlayUpdated {false};
    bool mDebugOverlayPresent {false};

};


#endif //EDROBOT_UIDEBUG_H
