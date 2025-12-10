//
// Created by mkizub on 16.06.2025.
//

#pragma once

#ifndef EDROBOT_CAPTURERWIN32_H
#define EDROBOT_CAPTURERWIN32_H

#include "../Capturer.h"
#include <boost/circular_buffer.hpp>

class CapturerWin32 final : public Capturer {
public:
    ~CapturerWin32() override;

    bool start() override;
    bool stop() override;
    upFrame capture(upFrame&& recycle) override;

    bool recycle(Frame* frame) const override;

private:
    friend class Capturer;
    friend class FrameWin32;

    CapturerWin32(HMONITOR hMonitor, LPMONITORINFOEX monitorInfoEx);
    bool trySetup(HWND hWnd, cv::Rect windowRect, cv::Rect clientRect) override;

    HDC hdcScreen;
    HDC hdcMem;
    HBITMAP hBitmap;
    BITMAPV5HEADER bitmapInfoHeader;

    mutable boost::circular_buffer<Frame*> recycledFrames;
    mutable std::mutex mCaptureMutex;

};


#endif //EDROBOT_CAPTURERWIN32_H
