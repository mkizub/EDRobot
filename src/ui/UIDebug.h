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
    static std::shared_ptr<UIDebug> getInstance();
    static bool initialize();

    UIDebug();

    void showImage(const cv::Mat& image);

    bool createWindow() final;
    void windowCreated() final;

    void onDestroy() final;
    INT_PTR onMessage(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) final;
    void onPaint() final;

    cv::Size outputSize;

    ID2D1Factory* pD2DFactory {nullptr};
    ID2D1HwndRenderTarget* pRenderTarget {nullptr};
    ID2D1SolidColorBrush* pBrush {nullptr};
    ID2D1Bitmap* pBitmap;

    std::mutex mMutex; // protect current image
    cv::Mat cvImage;
    bool imageUpdated {false};

};


#endif //EDROBOT_UIDEBUG_H
