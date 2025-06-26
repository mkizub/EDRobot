//
// Created by mkizub on 16.06.2025.
//

#pragma once

#ifndef EDROBOT_CAPTURERWIN32_H
#define EDROBOT_CAPTURERWIN32_H

#include "../Capturer.h"

class CapturerWin32 final : public Capturer {
public:
    ~CapturerWin32() override;

    bool start() override;
    bool stop() override;
    upFrame capture(upFrame&& recycle) override;

    void recycle(Frame* frame) const override;

private:
    friend class Capturer;
    friend class FrameWin32;

    CapturerWin32(HMONITOR hMonitor, LPMONITORINFOEX monitorInfoEx, HDC hdcMonitor);
    bool trySetup(HWND hWnd, cv::Rect windowRect, cv::Rect clientRect) override;

    bool isHdcScreenCreated;
    HDC hdcScreen;
    HDC hdcMem;
    HBITMAP hBitmap;
    BITMAPV5HEADER bitmapInfoHeader;

    mutable std::deque<Frame*> recycledFrames;
    mutable std::mutex mCaptureMutex;

};


#endif //EDROBOT_CAPTURERWIN32_H
