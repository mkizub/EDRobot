//
// Created by mkizub on 16.06.2025.
//

#include "../pch.h"

#include "CapturerWin32.h"

class FrameWin32 : public Frame {
public:
    FrameWin32(CapturerWin32* owner);
    ~FrameWin32() override;
    bool valid() const override;
    const cv::UMat& getColorTexture() const override;
    const cv::Mat& getColorImage() const override;
    const cv::Mat& getGrayImage() const override;

    void cleanup();

    mutable cv::UMat colorTexture;
    mutable cv::Mat colorImage;
    mutable cv::Mat grayImage;
    mutable bool colorTextureValid {false};
    mutable bool colorImageValid {false};
    mutable bool grayImageValid {false};
};

FrameWin32::FrameWin32(CapturerWin32* owner)
        : Frame(owner, owner->captureVirtRect.size())
{
    colorImage.create(size, CV_8UC4);
    colorImageValid = true;
}

FrameWin32::~FrameWin32() {
    cleanup();
}

bool FrameWin32::valid() const {
    return colorImageValid && !colorImage.empty();
}

void FrameWin32::cleanup() {
    if (colorTextureValid) {
        colorTexture = cv::UMat();
        colorTextureValid = false;
    }
    grayImageValid = false;
}

const cv::UMat& FrameWin32::getColorTexture() const {
    if (!colorTextureValid) {
        colorTexture = colorImage.getUMat(cv::ACCESS_READ);
        colorTextureValid = true;
    }
    return colorTexture;
}

const cv::Mat& FrameWin32::getColorImage() const {
    return colorImage;
}

const cv::Mat& FrameWin32::getGrayImage() const {
    if (!grayImageValid) {
        cv::cvtColor(colorImage, grayImage, cv::COLOR_BGRA2GRAY);
        grayImageValid = true;
    }
    return grayImage;
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
    if (!p)
        return;
    std::unique_lock<std::mutex> lock(mCaptureMutex);
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

//cv::Mat CapturerWin32::getColorImage() {
//    RECT screenRect{0, 0, screenWidth, screenHeight};
//    if (captureRect == screenRect)
//        return capturedImage;
//    int x = captureRect.left - monitorInfo.rcMonitor.left;
//    int y = captureRect.top - monitorInfo.rcMonitor.top;
//    int w = captureRect.right - captureRect.left;
//    int h = captureRect.bottom - captureRect.top;
//    cv::Rect crop(x,y,w,h);
//    if (crop.x < 0 || crop.y < 0 || crop.br().x > screenWidth || crop.br().y > screenHeight) {
//        LOG(WARNING) << "Window is out of screen borders: " << crop << " on " << cv::Size(screenWidth, screenHeight) << " screen";
//        cv::Mat tmp(h, w, capturedImage.type());
//        cv::Rect is = crop & cv::Rect(cv::Point(), capturedImage.size());
//        int dr = (y < 0) ? -y : 0;
//        int dc = (x < 0) ? -x : 0;
//        for(int r = dr; r < is.height; r++) {
//            memcpy(tmp.ptr<char>(r, dc), capturedImage.ptr<char>(r+is.y-dr, is.x), is.width*4);
//        }
//        return tmp;
//    }
//    return cv::Mat(capturedImage, cv::Rect(x,y,w,h));
//}
//
//cv::Mat CapturerWin32::getGrayImage() {
//    RECT screenRect{0, 0, screenWidth, screenHeight};
//    if (captureRect == screenRect)
//        return grayImage;
//    int x = captureRect.left - monitorInfo.rcMonitor.left;
//    int y = captureRect.top - monitorInfo.rcMonitor.top;
//    int w = captureRect.right - captureRect.left;
//    int h = captureRect.bottom - captureRect.top;
//    cv::Rect crop(x,y,w,h);
//    if (crop.x < 0 || crop.y < 0 || crop.br().x > screenWidth || crop.br().y > screenHeight) {
//        LOG(WARNING) << "Window is out of screen borders: " << crop << " on " << cv::Size(screenWidth, screenHeight) << " screen";
//        cv::Mat tmp(h, w, grayImage.type());
//        cv::Rect is = crop & cv::Rect(cv::Point(), grayImage.size());
//        int dr = (y < 0) ? -y : 0;
//        int dc = (x < 0) ? -x : 0;
//        for(int r = dr; r < is.height; r++) {
//            //for(int c = 0; c < is.width; c++)
//            //    tmp.at<char>(r, c+dc) = grayImage.at<char>(r+is.y,c+is.x);
//            memcpy(tmp.ptr<char>(r, dc), grayImage.ptr<char>(r+is.y-dr, is.x), is.width);
//        }
//        return tmp;
//    }
//    return cv::Mat(grayImage, crop);
//}

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
//    capturedImage = cv::Mat(screenHeight, screenWidth, CV_8UC4);
//    grayImage = cv::Mat(screenHeight, screenWidth, CV_8UC1);
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
        frame->cleanup();
    } else {
        if (recycledFrames.empty()) {
            frame = new FrameWin32(this);
        } else {
            frame = (FrameWin32*)recycledFrames.back();
            recycledFrames.pop_back();
            frame->cleanup();
        }
    }
    assert (frame->colorImageValid);

    int lines = 0;
    if (blitRect.size() == captureVirtRect.size()) {
        lines = GetDIBits(hdcMem, hBitmap, 0, blitRect.height, frame->colorImage.data,
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
            uchar* data_ptr = frame->colorImage.ptr(blitRect.height-1-y-orig.y, orig.x);
            int l = GetDIBits(hdcMem, hBitmap, y, 1, data_ptr, (BITMAPINFO *) &bInfoHeader, DIB_RGB_COLORS);
            if (l == 1)
                lines += 1;
        }
    }
    LOG_IF(lines != blitRect.height, ERROR) << "GetDIBits failed: " << getErrorMessage();
    cv::imwrite("captured-screen.png", frame->colorImage);

    SelectObject(hdcMem, hOldBitmap);
    return upFrame(frame);
}
