//
// Created by mkizub on 16.06.2025.
//

#include "../pch.h"

#include "CapturerWin32.h"

class FrameWin32 : public Frame {
public:
    FrameWin32(CapturerWin32* owner);
    ~FrameWin32() override = default;
    bool valid() const override;
    const XMat& getImage() const override { return colorImage; }

    mutable XMat colorImage;
};

FrameWin32::FrameWin32(CapturerWin32* owner)
        : Frame(owner, owner->captureVirtRect.size())
{
    colorImage.create(size, CV_8UC4);
}

bool FrameWin32::valid() const {
    return !colorImage.empty();
}

CapturerWin32::CapturerWin32(HMONITOR hMonitor, LPMONITORINFOEX monitorInfoEx, HDC hdcMonitor)
        : Capturer(hMonitor, monitorInfoEx)
        , isHdcScreenCreated(hdcMonitor == nullptr)
        , hdcMem(nullptr)
        , hBitmap(nullptr)
        , bitmapInfoHeader{}
{
    if (monitorInfoEx) {
        hdcScreen = hdcMonitor;
        if (!hdcScreen) {
            hdcScreen = CreateDC(monitorInfo.szDevice, nullptr, nullptr, nullptr);
        }
    } else {
        hdcScreen = GetDC(nullptr);
    }
}

CapturerWin32::~CapturerWin32() {
    CapturerWin32::stop();
    if (isHdcScreenCreated)
        ReleaseDC(nullptr, hdcScreen);
}

void CapturerWin32::recycle(Frame *p) const {
    std::unique_lock<std::mutex> lock(mCaptureMutex);
    if (!p || recycledFrames.size() >= 3)
        return;
    auto it = std::find(recycledFrames.begin(), recycledFrames.end(), p);
    if (it != recycledFrames.end())
        return;
    assert(p->owner == this);
    recycledFrames.push_back(p);
}

bool CapturerWin32::trySetup(HWND hWnd, cv::Rect windowRect, cv::Rect clientRect) {
    if (!hWnd)
        return false;
    this->hWndED = hWnd;
    this->windowVirtRect = windowRect;
    this->captureVirtRect = clientRect;
    this->titleHeight = clientRect.y - windowRect.y;
    this->borderWidth = clientRect.x - windowRect.x;
    return true;
}

bool CapturerWin32::start() {
    stop();
    hdcMem = CreateCompatibleDC(hdcScreen);
    hBitmap = CreateCompatibleBitmap(hdcScreen, captureVirtRect.width, captureVirtRect.height);
    memset(&bitmapInfoHeader, 0, sizeof(bitmapInfoHeader));
    bitmapInfoHeader.bV5Size = sizeof(BITMAPV5HEADER);
    bitmapInfoHeader.bV5Width = captureVirtRect.width;
    bitmapInfoHeader.bV5Height = -captureVirtRect.height;  // Negative height to flip the image vertically
    bitmapInfoHeader.bV5Planes = 1;
    bitmapInfoHeader.bV5BitCount = 32;
    bitmapInfoHeader.bV5Compression = BI_RGB;
    bitmapInfoHeader.bV5CSType = LCS_sRGB;
    return Capturer::start();
}

bool CapturerWin32::stop() {
    if (hBitmap) {
        DeleteObject(hBitmap);
        hBitmap = nullptr;
    }
    if (hdcMem) {
        DeleteDC(hdcMem);
        hdcMem = nullptr;
    }
    while (!recycledFrames.empty()) {
        delete (FrameWin32*)recycledFrames.back();
        recycledFrames.pop_back();
    }
    return Capturer::stop();
}

upFrame CapturerWin32::capture(upFrame&& recycle) {
    if (!isStarted())
        return {};
    auto hOldBitmap = (HBITMAP)SelectObject(hdcMem, hBitmap);

    if (captureVirtRect != monitorVirtRect) {
        // window mode, get current captureVirtRect
        RECT windowRECT;
        BOOL ok = GetWindowRect(hWndED, &windowRECT);
        if (!ok) {
            LOG_IF(!this->isDefaultCapturer(),ERROR) << "Cannot get window for capturer";
        } else {
            cv::Rect wRect = fromRECT(windowRECT);
            if (wRect != windowVirtRect) {
                windowVirtRect.x = wRect.x;
                windowVirtRect.y = wRect.y;
                captureVirtRect.x = wRect.x + borderWidth;
                captureVirtRect.y = wRect.y + titleHeight;
            }
        }
    }

    cv::Rect blitRect = captureVirtRect;
    blitRect &= monitorVirtRect;
    blitRect -= monitorVirtRect.tl();
    int ok = BitBlt(hdcMem, 0, 0, blitRect.width, blitRect.height, hdcScreen, blitRect.x, blitRect.y, SRCCOPY);
    if (!ok) {
        LOG(ERROR) << "BitBlt failed: " << getErrorMessage();
        return {};
    }

    std::unique_lock<std::mutex> lock(mCaptureMutex);
    FrameWin32* frame;
    if (recycle && recycle->owner == this && recycle->size == captureVirtRect.size()) {
        frame = (FrameWin32*)recycle.release();
    } else {
        if (recycledFrames.empty()) {
            frame = new FrameWin32(this);
        } else {
            frame = (FrameWin32*)recycledFrames.back();
            recycledFrames.pop_back();
        }
    }
    assert (frame->valid());

#ifdef EDROBOT_USE_OPENCL
    cv::Mat colorImage = frame->colorImage.getMat(cv::ACCESS_RW);
#else
    cv::Mat& colorImage = frame->colorImage;
#endif
    int lines = 0;
    if (blitRect.size() == captureVirtRect.size()) {
        lines = GetDIBits(hdcMem, hBitmap, 0, blitRect.height, colorImage.data,
                              (BITMAPINFO *) &bitmapInfoHeader, DIB_RGB_COLORS);
        LOG_IF(lines != blitRect.height, ERROR) << "GetDIBits failed: " << getErrorMessage();
    } else {
        frame->colorImage.setTo(cv::Vec4b::zeros());
        cv::Point orig;
        if (captureVirtRect.x < monitorVirtRect.x)
            orig.x = monitorVirtRect.x - captureVirtRect.x;
        if (captureVirtRect.y < monitorVirtRect.y)
            orig.y = monitorVirtRect.y - captureVirtRect.y;
        BITMAPV5HEADER bInfoHeader = bitmapInfoHeader;
        bInfoHeader.bV5Width = blitRect.width;
        for (int y=0; y < blitRect.height; y++) {
            uchar* data_ptr = colorImage.ptr(blitRect.height-1-y-orig.y, orig.x);
            int l = GetDIBits(hdcMem, hBitmap, y, 1, data_ptr, (BITMAPINFO *) &bInfoHeader, DIB_RGB_COLORS);
            if (l == 1)
                lines += 1;
        }
    }
    LOG_IF(lines != blitRect.height, ERROR) << "GetDIBits failed: " << getErrorMessage();
    //cv::imwrite("captured-screen.png", frame->colorImage);

    SelectObject(hdcMem, hOldBitmap);
    return upFrame(frame);
}
