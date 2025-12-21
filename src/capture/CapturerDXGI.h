//
// Created by mkizub on 19.06.2025.
//

#pragma once

#ifndef EDROBOT_CAPTURERDXGI_H
#define EDROBOT_CAPTURERDXGI_H

#include "../Capturer.h"
#include <boost/circular_buffer.hpp>

#include <atlbase.h>

class CapturerDXGI : public Capturer {
public:
    ~CapturerDXGI() override;

    bool start() override;
    bool stop() override;
    upFrame capture(upFrame&& recycle) override;
    bool recycle(Frame* frame) const override;

private:
    friend class Capturer;
    friend class FrameDXGI;
    CapturerDXGI(HMONITOR hMonitor, LPMONITORINFOEX monitorInfoEx);
    bool trySetup(HWND hWnd, cv::Rect windowRect, cv::Rect clientRect) override;

    CComPtr<IDXGIOutput1> m_dxgiOutput {nullptr};
    CComPtr<IDXGIOutputDuplication> m_outputDuplication {nullptr};
    std::chrono::time_point<std::chrono::high_resolution_clock> hpcStartTimestamp;
    Timestamp utcStartTimestamp;

    mutable boost::circular_buffer<Frame*> recycledFrames;

    mutable std::mutex mCaptureMutex;
    mutable std::condition_variable mCaptureCond;

    uint64_t m_frames {};

};


#endif //EDROBOT_CAPTURERDXGI_H
