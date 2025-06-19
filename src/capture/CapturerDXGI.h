//
// Created by mkizub on 19.06.2025.
//

#pragma once

#ifndef EDROBOT_CAPTURERDXGI_H
#define EDROBOT_CAPTURERDXGI_H

#include "../Capturer.h"

#include <atlbase.h>

class CapturerDXGI : public Capturer {
public:
    ~CapturerDXGI() override;

    bool start() override;
    bool stop() override;
    upFrame capture(upFrame&& recycle) override;
    void recycle(Frame* frame) const override;

private:
    friend class Capturer;
    friend class FrameDXGI;
    CapturerDXGI(HMONITOR hMonitor, LPMONITORINFOEX monitorInfoEx, HDC hdcMonitor);
    bool trySetup(HWND hWnd, cv::Rect windowRect, cv::Rect clientRect) override;

    CComPtr<ID3D11Device> m_d3dDevice {nullptr};
    CComPtr<ID3D11DeviceContext> m_d3dContext {nullptr};
    CComPtr<IDXGIDevice> m_dxgiDevice {nullptr};
    CComPtr<IDXGIAdapter> m_dxgiAdapter {nullptr};
    CComPtr<IDXGIOutput1> m_dxgiOutput1 {nullptr};
    CComPtr<IDXGIOutputDuplication> m_outputDuplication {nullptr};

    mutable std::deque<Frame*> recycledFrames;

    mutable std::mutex mCaptureMutex;
    mutable std::condition_variable mCaptureCond;

    uint64_t m_frames {};

};


#endif //EDROBOT_CAPTURERDXGI_H
