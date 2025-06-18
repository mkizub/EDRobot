//
// Created by mkizub on 13.06.2025.
//

#pragma once

#ifndef EDROBOT_CAPTUREDXGI_H
#define EDROBOT_CAPTUREDXGI_H

#include "../Capturer.h"

class CapturerWinRT : public Capturer {
public:
    ~CapturerWinRT() override;

    bool start() override;
    bool stop() override;
    upFrame capture(upFrame&& recycle) override;
    void recycle(Frame* frame) const override;

private:
    friend class Capturer;
    friend class FrameWinRT;
    CapturerWinRT(HMONITOR hMonitor, LPMONITORINFOEX monitorInfoEx, HDC hdcMonitor);
    bool trySetup(HWND hWnd, RECT& captRect) override;

    void OnFrameArrived(
            winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool const& sender,
            winrt::Windows::Foundation::IInspectable const&);
    void OnCaptureClosed(
            winrt::Windows::Graphics::Capture::GraphicsCaptureItem const& sender,
            winrt::Windows::Foundation::IInspectable const&);

    void copyTexture(FrameWinRT* frame, winrt::Windows::Graphics::Capture::Direct3D11CaptureFrame nextFrame);

    winrt::com_ptr<ID3D11Device> m_d3dDevice;
    winrt::com_ptr<ID3D11DeviceContext> m_d3dContext;
    winrt::Windows::Graphics::Capture::GraphicsCaptureItem m_captureItem {nullptr};
    winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool m_framePool {nullptr};
    winrt::Windows::Graphics::Capture::GraphicsCaptureSession m_session {nullptr};
    winrt::event_token m_frameArrived;
    winrt::Windows::Graphics::Capture::Direct3D11CaptureFrame m_nextFrame {nullptr};
    int captureWidth {};
    int captureHeight {};
    D3D11_TEXTURE2D_DESC mStagingTextureDesc;

    mutable std::deque<Frame*> recycledFrames;

    mutable std::mutex mCaptureMutex;
    mutable std::condition_variable mCaptureCond;

    uint64_t m_frames {};
};


#endif //EDROBOT_CAPTUREDXGI_H
