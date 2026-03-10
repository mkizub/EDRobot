//
// Created by mkizub on 24.05.2025.
//

#pragma once

#ifndef EDROBOT_CAPTURER_H
#define EDROBOT_CAPTURER_H

struct ID3D11Device;
struct ID3D11DeviceContext;

class Capturer {
public:
    static struct ID3D11Device1* getID3D11Device();
    static struct ID3D11DeviceContext1* getID3D11DeviceContext();
    static bool flushID3D11DeviceContext();

    static Capturer* getEDCapturer(HWND hwnd);
    static void resetEDCapturer();
    static void shutdown();

    virtual ~Capturer() = default;

    virtual bool start() {
        atomicIsStarted.exchange(true);
        return true;
    };
    virtual bool stop() {
        atomicIsStarted.exchange(false);
        return true;
    };
    virtual upFrame capture(upFrame&& recycle) = 0;

    cv::Rect getCaptureRect();
    cv::Rect getMonitorVirtualRect();

    [[nodiscard]] bool isStarted() { return atomicIsStarted.load(); }

    virtual bool recycle(Frame* frame) const = 0;

protected:
    Capturer(HMONITOR hMonitor, LPMONITORINFOEX monitorInfoEx);
    virtual bool trySetup(HWND hWnd, cv::Rect windowRect, cv::Rect clientRect) = 0;

    // should be const, but I'm lazy to make all const_cast in the initializer
    double dpiScaleX;
    double dpiScaleY;
    cv::Rect monitorVirtRect; // in virtual desktop coordinates
    cv::Rect windowVirtRect;
    cv::Rect captureVirtRect; // in virtual desktop coordinates
    cv::Size captureSize;
    int titleHeight;
    int borderWidth;
    HMONITOR hMonitor;
    HWND hWndED;
    MONITORINFOEX monitorInfo;
    std::atomic<bool> atomicIsStarted;

private:
    friend class Master;
    friend class Configuration;
    static std::unique_ptr<Capturer> TheCapturer;

    static bool InitD3DDevice();
};


#endif //EDROBOT_CAPTURER_H
