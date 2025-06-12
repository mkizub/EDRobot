//
// Created by mkizub on 11.06.2025.
//

#pragma once

#ifndef EDROBOT_UISELECTRECT_H
#define EDROBOT_UISELECTRECT_H

#include "UIWindow.h"

class UISelectRect : public UIWindow {
public:
    ~UISelectRect() final;
private:
    friend class UIManager;
    static std::shared_ptr<UISelectRect> getInstance();
    static bool initialize();

    UISelectRect();

    bool createWindow() final;

    void onPaint() final;
    INT_PTR onMessage(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) final;

    int isSizeAndIncr(int& incr);

    int mRepeatKeyCount {};

    static cv::Rect gScreenRect;
    static cv::Rect gSelectedRect;
};


#endif //EDROBOT_UISELECTRECT_H
