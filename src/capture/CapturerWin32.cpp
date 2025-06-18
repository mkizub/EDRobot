//
// Created by mkizub on 16.06.2025.
//

#include "../pch.h"

#include "CapturerWin32.h"

class FrameWin32 : public Frame {
public:
    FrameWin32(CapturerWin32* owner, cv::Size size);
    ~FrameWin32() override;
    bool valid() const override;
    const cv::UMat& getColorTexture() const override;
    const cv::Mat& getColorImage() const override;
    const cv::Mat& getGrayImage() const override;
          cv::Mat& getDebugImage() const override;

    void cleanup();

    mutable cv::UMat colorTexture;
    mutable cv::Mat colorImage;
    mutable cv::Mat grayImage;
    mutable cv::Mat debugImage;
    mutable bool colorTextureValid {false};
    mutable bool colorImageValid {false};
    mutable bool grayImageValid {false};
    mutable bool debugIMageValid {false};
};

FrameWin32::FrameWin32(CapturerWin32* owner, cv::Size size)
        : Frame(owner, size)
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
    debugIMageValid = false;
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
        cv::cvtColor(colorImage, grayImage, cv::COLOR_RGBA2GRAY);
        grayImageValid = true;
    }
    return grayImage;
}

cv::Mat& FrameWin32::getDebugImage() const {
    if (!debugIMageValid) {
        colorImage.copyTo(debugImage);
        debugIMageValid = true;
    }
    return debugImage;
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

bool CapturerWin32::trySetup(HWND hWnd, RECT &captRect) {
    if (!hWnd)
        return false;
    this->hWndED = hWnd;
    this->captureRect = captRect;
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
    hBitmap = CreateCompatibleBitmap(hdcScreen, screenWidth, screenHeight);
    memset(&bitmapInfoHeader, 0, sizeof(bitmapInfoHeader));
    bitmapInfoHeader.biSize = sizeof(BITMAPINFOHEADER);
    bitmapInfoHeader.biWidth = screenWidth;
    bitmapInfoHeader.biHeight = -screenHeight;  // Negative height to flip the image vertically
    bitmapInfoHeader.biPlanes = 1;
    bitmapInfoHeader.biBitCount = 32;
    bitmapInfoHeader.biCompression = BI_RGB;
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
    return Capturer::stop();
}

upFrame CapturerWin32::capture(upFrame&& recycle) {
    if (!isStarted())
        return {};
    auto hOldBitmap = (HBITMAP)SelectObject(hdcMem, hBitmap);

    int ok = BitBlt(hdcMem, 0, 0, screenWidth, screenHeight, hdcScreen, 0, 0, SRCCOPY);
    if (!ok) {
        LOG(ERROR) << "BitBlt failed: " << getErrorMessage();
        return {};
    }

    std::unique_lock<std::mutex> lock(mCaptureMutex);
    cv::Size size(screenWidth, screenHeight);
    FrameWin32* frame;
    if (recycle && recycle->owner == this && recycle->size == size) {
        frame = (FrameWin32*)recycle.release();
        frame->cleanup();
    } else {
        if (recycledFrames.empty()) {
            frame = new FrameWin32(this, size);
        } else {
            frame = (FrameWin32*)recycledFrames.back();
            recycledFrames.pop_back();
            frame->cleanup();
        }
    }
    assert (frame->colorImageValid);

    // TODO: align start and lines if window is out of monitor borders
    int lines = GetDIBits(hdcMem, hBitmap, 0, screenHeight, frame->colorImage.data, (BITMAPINFO*)&bitmapInfoHeader, DIB_RGB_COLORS);
    LOG_IF(lines != screenHeight, ERROR) << "GetDIBits failed: " << getErrorMessage();
    //cv::imwrite("captured-screen.png", mat);

    SelectObject(hdcMem, hOldBitmap);
    return upFrame(frame);
}
