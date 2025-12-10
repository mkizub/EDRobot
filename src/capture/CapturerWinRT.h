//
// Created by mkizub on 13.06.2025.
//

#pragma once

#ifndef EDROBOT_CAPTUREDXGI_H
#define EDROBOT_CAPTUREDXGI_H

#include "../Capturer.h"
#include <boost/circular_buffer.hpp>

class CapturerWinRT : public Capturer {
public:
    ~CapturerWinRT() override;

    bool start() override;
    bool stop() override;
    upFrame capture(upFrame&& recycle) override;
    bool recycle(Frame* frame) const override;

private:
    friend class Capturer;
    friend class FrameWinRT;
    CapturerWinRT(HMONITOR hMonitor, LPMONITORINFOEX monitorInfoEx);
    bool trySetup(HWND hWnd, cv::Rect windowRect, cv::Rect clientRect) override;

    void OnCaptureClosed(
            winrt::Windows::Graphics::Capture::GraphicsCaptureItem const& sender,
            winrt::Windows::Foundation::IInspectable const&);

    void copyTexture(FrameWinRT* myFrame, winrt::Windows::Graphics::Capture::Direct3D11CaptureFrame& captureFrame);

    winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool m_framePool {nullptr};
    winrt::Windows::Graphics::Capture::GraphicsCaptureSession m_session {nullptr};
    winrt::Windows::Foundation::TimeSpan startTimeSpan;
    Timestamp utcStartTimestamp;

    mutable boost::circular_buffer<Frame*> recycledFrames;

    mutable std::mutex mCaptureMutex;
    mutable std::condition_variable mCaptureCond;
};


#endif //EDROBOT_CAPTUREDXGI_H
