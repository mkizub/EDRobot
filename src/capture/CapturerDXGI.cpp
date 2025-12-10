//
// Created by mkizub on 19.06.2025.
//

#include "../pch.h"

#include <atlbase.h>
#include <dxgi.h>
#include <dxgi1_6.h>
#include <d3d11_4.h>

#include <opencv2/core/directx.hpp>

#include "CapturerDXGI.h"


class FrameDXGI : public Frame {
public:
    FrameDXGI(CapturerDXGI* owner);
    ~FrameDXGI() override;
    bool valid() const override;
    const XMat& getImage() const override;

    void cleanup();

    D3D11_TEXTURE2D_DESC mStagingTextureDesc;
    mutable D3D11_MAPPED_SUBRESOURCE mStagingMappedTex {};
    CComPtr<ID3D11Texture2D> mStagingTexture;

    mutable XMat rawColorImage;
    mutable XMat colorImage;
    mutable bool stagingTextureValid {false};
    mutable bool stagingTextureMapped {false};
    mutable bool rawColorImageValid {false};
    mutable bool colorImageValid {false};
};

FrameDXGI::FrameDXGI(CapturerDXGI* owner)
        : Frame(owner, owner->captureSize)
{
    stagingTextureValid = false;
}

FrameDXGI::~FrameDXGI() {
    LOG(ERROR) << "FrameDXGI deleted";
    cleanup();
}

bool FrameDXGI::valid() const {
    if (colorImageValid || rawColorImageValid)
        return true;
    return stagingTextureValid && mStagingTexture;
}

void FrameDXGI::cleanup() {
    timestamp = {};
    if (stagingTextureMapped) {
        Capturer::getID3D11DeviceContext()->Unmap(mStagingTexture, 0);
        mStagingMappedTex = {};
        stagingTextureMapped = false;
    }
    colorImageValid = false;
    rawColorImageValid = false;
    colorImage.release();
    rawColorImage.release();
}

const XMat& FrameDXGI::getImage() const {
    if (!colorImageValid) {
        if (rawColorImageValid) {
#ifdef EDROBOT_USE_OPENCL
            auto* owner = (CapturerDXGI*)this->owner;
            cv::Rect captureRect = owner->captureVirtRect - owner->monitorVirtRect.tl();
            if (owner->captureVirtRect == owner->monitorVirtRect) {
                colorImage = rawColorImage;
            } else {
                if (captureRect.x >= 0 && captureRect.y >= 0 &&
                    captureRect.x+captureRect.width <= rawColorImage.cols &&
                    captureRect.y+captureRect.height <= rawColorImage.rows)
                {
                    colorImage = rawColorImage(captureRect);
                } else {
                    colorImage.create(this->size, CV_8UC4);
                    cv::Rect srcRect = captureRect;
                    cv::Rect dstRect(0, 0, colorImage.cols, colorImage.rows);
                    if (captureRect.x < 0) {
                        dstRect.x = -captureRect.x;
                        srcRect.x = 0;
                        srcRect.width += captureRect.x;
                    }
                    if (captureRect.y < 0) {
                        dstRect.y = -captureRect.y;
                        srcRect.y = 0;
                        srcRect.height += captureRect.y;
                    }
                    if (srcRect.x+srcRect.width > rawColorImage.cols)
                        srcRect.width -= srcRect.x+srcRect.width - rawColorImage.cols;
                    if (srcRect.y+srcRect.height > rawColorImage.rows)
                        srcRect.height -= srcRect.y+srcRect.height - rawColorImage.rows;
                    if (srcRect.width > 0 && srcRect.height > 0) {
                        dstRect.width = srcRect.width;
                        dstRect.height = srcRect.height;
                        cv::copyTo(rawColorImage(srcRect), colorImage(dstRect), cv::noArray());
                    }
                }
            }
            if (Cfg.getCaptureDisplaySize() != captureRect.size()) {
                // need rescaling
                XMat tmp;
                cv::resize(colorImage, tmp, Cfg.getCaptureDisplaySize());
                colorImage = tmp;
            }
#else
            if (Cfg.getCaptureDisplaySize() != rawColorImage.size()) {
                // need rescaling
                cv::resize(rawColorImage, colorImage, Cfg.getCaptureDisplaySize());
            } else {
                colorImage = rawColorImage;
            }
#endif
            colorImageValid = true;
        }
    }
    return colorImage;
}

CapturerDXGI::CapturerDXGI(HMONITOR hMonitor, LPMONITORINFOEX monitorInfoEx)
    : Capturer(hMonitor, monitorInfoEx)
    , recycledFrames(3)
{
}

CapturerDXGI::~CapturerDXGI() {
    CapturerDXGI::stop();
}

bool CapturerDXGI::recycle(Frame* p) const {
    std::unique_lock<std::mutex> lock(mCaptureMutex);
    if (!p || recycledFrames.full())
        return false;
    assert(p->owner == this);
    auto it = std::find(recycledFrames.begin(), recycledFrames.end(), p);
    if (it != recycledFrames.end())
        recycledFrames.push_back(p);
    return true;
}


bool CapturerDXGI::trySetup(HWND hWnd, cv::Rect windowRect, cv::Rect clientRect) {
    if (!hWnd)
        return false;
    if (!getID3D11Device() || !getID3D11DeviceContext()) {
        LOG(ERROR) << "D3dDevice not initialized";
        return false;
    }
    auto gameSize = Cfg.getConfigDisplaySize();
    if (gameSize.width > clientRect.width || gameSize.height > clientRect.height)
        return false;

    this->hWndED = hWnd;
    this->windowVirtRect = windowRect;
    this->captureVirtRect = clientRect;
    this->captureSize = Cfg.getCaptureDisplaySize();
    this->titleHeight = clientRect.y - windowRect.y;
    this->borderWidth = clientRect.x - windowRect.x;

    return true;
}

bool CapturerDXGI::start() {
    HRESULT hr;

    if (!hWndED) {
        LOG(ERROR) << "Cannot start CapturerDXGI because ED window not found";
        return false;
    }
    hr = getID3D11Device()->QueryInterface(IID_PPV_ARGS(&m_dxgiDevice));
    if (FAILED(hr)) {
        LOG(ERROR) << "Failed to acquire IDXGIDevice interface" << getErrorMessage(hr);
        return false;
    }
    CComPtr<IDXGIAdapter> dxgiAdapter {nullptr};
    hr = m_dxgiDevice->GetParent(IID_PPV_ARGS(&dxgiAdapter));
    if (FAILED(hr)) {
        LOG(ERROR) << "Failed to acquire IDXGIAdapter interface" << getErrorMessage(hr);
        return false;
    }

    CComPtr<IDXGIOutput> dxgiOutput;
    for (unsigned i=0; ; i++) {
        CComPtr<IDXGIOutput> output;
        hr = dxgiAdapter->EnumOutputs(i, &output);
        if (FAILED(hr))
            break;
        DXGI_OUTPUT_DESC desc {};
        hr = output->GetDesc(&desc);
        if (FAILED(hr))
            continue;
        if (desc.Monitor == hMonitor) {
            dxgiOutput = output;
            break;
        }
    }
    if (!dxgiOutput) {
        LOG(ERROR) << "Failed to find IDXGIOutput for monitor " << toUtf8(monitorInfo.szDevice);
        return false;
    }

    CComPtr<IDXGIOutput6> dxgiOutput6 {};
    hr = dxgiOutput->QueryInterface(IID_PPV_ARGS(&dxgiOutput6));
    if (SUCCEEDED(hr)) {
        DXGI_OUTPUT_DESC1 desc1 {};
        hr = dxgiOutput6->GetDesc1(&desc1);
        if (SUCCEEDED(hr)) {
            LOG(INFO) << "Monitor BitsPerColor: " << desc1.BitsPerColor << ", ColorSpace: " << desc1.ColorSpace;
            DXGI_OUTDUPL_FLAG flags = DXGI_OUTDUPL_COMPOSITED_UI_CAPTURE_ONLY;
            DXGI_FORMAT formats[] {
                    //DXGI_FORMAT_R10G10B10A2_UNORM, // TODO: for HDR, need to convert to float
                    DXGI_FORMAT_B8G8R8A8_UNORM, // the only compatible with OpenCL
            };
            hr = dxgiOutput6->DuplicateOutput1(getID3D11Device(), flags, 4, formats, &m_outputDuplication);
            if (SUCCEEDED(hr)) {
                LOG(INFO) << "CapturerDXGI started";
                hpcStartTimestamp = std::chrono::high_resolution_clock::now();
                utcStartTimestamp = std::chrono::utc_clock::now();
                return Capturer::start();
            }
        }
    }

    CComPtr<IDXGIOutput1> dxgiOutput1 {};
    hr = dxgiOutput->QueryInterface(IID_PPV_ARGS(&dxgiOutput1));
    if (SUCCEEDED(hr)) {
        hr = dxgiOutput1->DuplicateOutput(getID3D11Device(), &m_outputDuplication);
        if (SUCCEEDED(hr)) {
            LOG(INFO) << "CapturerDXGI started";
            hpcStartTimestamp = std::chrono::high_resolution_clock::now();
            utcStartTimestamp = std::chrono::utc_clock::now();
            return Capturer::start();
        }
    }

    LOG(ERROR) << "Failed to acquire DuplicateOutput " << getErrorMessage(hr);
    return false;
}

bool CapturerDXGI::stop() {
    LOG(ERROR) << "CapturerDXGI stop";

    Capturer::stop();

    m_outputDuplication = nullptr;
    m_dxgiDevice = nullptr;

    while (!recycledFrames.empty()) {
        delete (FrameDXGI*)recycledFrames.back();
        recycledFrames.pop_back();
    }

    LOG(INFO) << "CapturerDXGI stopped";
    return true;
}

upFrame CapturerDXGI::capture(upFrame&& recycle) {
    if (!isStarted()) {
        LOG(ERROR) << "CapturerDXGI not started";
        return {};
    }
    std::unique_lock<std::mutex> lock(mCaptureMutex);
    FrameDXGI* frame;
    if (recycle && recycle->owner == this && recycle->size == captureVirtRect.size()) {
        frame = (FrameDXGI*)recycle.release();
        frame->cleanup();
    } else {
        if (recycledFrames.empty()) {
            frame = new FrameDXGI(this);
        } else {
            frame = (FrameDXGI*)recycledFrames.back();
            recycledFrames.pop_back();
            frame->cleanup();
        }
    }

    CComPtr<IDXGIResource> desktopResource;
    HRESULT hr = E_FAIL;
    for(int i = 0; i < 10; ++i) {
        DXGI_OUTDUPL_FRAME_INFO fi {};
        hr = m_outputDuplication->AcquireNextFrame(50, &fi, &desktopResource);
        if(SUCCEEDED(hr) && (fi.LastPresentTime.QuadPart == 0)) {
            // If AcquireNextFrame() returns S_OK and
            // fi.LastPresentTime.QuadPart == 0, it means
            // AcquireNextFrame() didn't acquire next frame yet.
            // We must wait next frame sync timing to retrieve
            // actual frame data.
            //
            // Since method is successfully completed,
            // we need to release the resource and frame explicitly.
            desktopResource.Release();
            m_outputDuplication->ReleaseFrame();
            Sleep(1);
            continue;
        } else {
            //frame->timestamp = std::chrono::utc_clock::now();
            const long long _Freq = _Query_perf_frequency(); // doesn't change after system boot
            const long long _Ctr  = fi.LastPresentTime.QuadPart;
            const long long _Whole = (_Ctr / _Freq) * std::chrono::steady_clock::period::den;
            const long long _Part  = (_Ctr % _Freq) * std::chrono::steady_clock::period::den / _Freq;
            auto frame_tp = std::chrono::steady_clock::time_point(std::chrono::steady_clock::duration(_Whole + _Part));
            auto elapsed_since_start = frame_tp - hpcStartTimestamp;
            auto utc_tp = utcStartTimestamp + elapsed_since_start;
            frame->timestamp = std::chrono::time_point_cast<Timestamp::duration>(utc_tp);
            //LOG(INFO) << std::format("CapturerDXGI: next frame age {}ms",
            //                         std::chrono::duration_cast<std::chrono::milliseconds>(Timestamp::clock::now()-utc_tp).count());
            break;
        }
    }
    if (FAILED(hr) || !desktopResource) {
        LOG(ERROR) << "CapturerDXGI Failed acquire desktop frame";
        Mgr.pushCommand(Command::ResetCapturer);
        return {};
    }

    CComPtr<ID3D11Texture2D> texture;
    hr = desktopResource->QueryInterface(IID_PPV_ARGS(&texture));
    if(FAILED(hr)) {
        LOG(ERROR) << "CapturerDXGI Failed to acquire ID3D11Texture2D interface";
        Mgr.pushCommand(Command::ResetCapturer);
        return {};
    }

    if (captureVirtRect != monitorVirtRect) {
        // window mode, get current captureVirtRect
        RECT windowRECT;
        BOOL ok = GetWindowRect(hWndED, &windowRECT);
        if (!ok) {
            LOG(ERROR) << "Cannot get window rect for capturer";
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
#ifdef EDROBOT_USE_OPENCL
    //D3D11_TEXTURE2D_DESC texture_desc;
    //texture->GetDesc(&texture_desc);
    cv::directx::convertFromD3D11Texture2D(texture, frame->rawColorImage);
    frame->rawColorImageValid = true;
#else

    D3D11_TEXTURE2D_DESC texture_desc;
    texture->GetDesc(&texture_desc);
    if (!frame->mStagingTexture || !frame->stagingTextureValid) {
        frame->mStagingTextureDesc = texture_desc;
        frame->mStagingTextureDesc.Width = captureVirtRect.width;
        frame->mStagingTextureDesc.Height = captureVirtRect.height;
        frame->mStagingTextureDesc.Usage = D3D11_USAGE_STAGING;
        frame->mStagingTextureDesc.BindFlags = 0;
        frame->mStagingTextureDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        frame->mStagingTextureDesc.MiscFlags = 0;
        frame->mStagingTexture = nullptr;
        hr = getID3D11Device()->CreateTexture2D(&frame->mStagingTextureDesc, nullptr, &frame->mStagingTexture);
        if (FAILED(hr)) {
            LOG(ERROR) << "CapturerDXGI Failed to create staging texture";
            return {};
        }
        frame->stagingTextureValid = true;
    }
    if (frame->stagingTextureMapped) {
        getID3D11DeviceContext()->Unmap(frame->mStagingTexture, 0);
        frame->mStagingMappedTex = {};
        frame->stagingTextureMapped = false;
    }

    if (texture_desc.Width == frame->mStagingTextureDesc.Width && texture_desc.Height == frame->mStagingTextureDesc.Height) {
        getID3D11DeviceContext()->CopyResource(frame->mStagingTexture, texture);
    } else {
        cv::Rect captureRect = captureVirtRect - monitorVirtRect.tl();
        if (captureRect.x + captureRect.width > monitorVirtRect.width)
            captureRect.width = monitorVirtRect.width - captureRect.x;
        if (captureRect.y + captureRect.height > monitorVirtRect.height)
            captureRect.height = monitorVirtRect.height - captureRect.y;
        int dst_y = captureRect.y >= 0 ? 0 : -captureRect.y;
        int src_x = captureRect.x;
        int src_w = captureRect.width;
        int src_h = captureRect.height;
        int dst_x = 0;
        if (src_x < 0) {
            dst_x = -src_x;
            src_w += src_x;
            src_x = 0;
        }
        int src_y = dst_y + captureRect.y;

        D3D11_BOX sourceRegion;
        sourceRegion.left = src_x;
        sourceRegion.right = src_x + src_w;
        sourceRegion.top = src_y;
        sourceRegion.bottom = src_y + src_h;
        sourceRegion.front = 0;
        sourceRegion.back = 1;
        getID3D11DeviceContext()->CopySubresourceRegion(frame->mStagingTexture, 0, dst_x, dst_y, 0, texture, 0, &sourceRegion);
    }
    Capturer::flushID3D11DeviceContext();
    frame->stagingTextureValid = true;
    hr = getID3D11DeviceContext()->Map(frame->mStagingTexture, 0, D3D11_MAP_READ, 0, &frame->mStagingMappedTex);
    if (FAILED(hr)) {
        LOG(ERROR) << "CapturerDXGI Failed to map staging texture: " << getErrorMessage(hr);
    } else {
        frame->stagingTextureMapped = true;

        frame->rawColorImage.create(frame->size, CV_8UC4);
        cv::Mat mappedImage(frame->mStagingTextureDesc.Height, frame->mStagingTextureDesc.Width,
                            CV_8UC4, frame->mStagingMappedTex.pData, frame->mStagingMappedTex.RowPitch);
        cv::copyTo(mappedImage, frame->rawColorImage, cv::noArray());
        getID3D11DeviceContext()->Unmap(frame->mStagingTexture, 0);
        frame->mStagingMappedTex = {};
        frame->stagingTextureMapped = false;
        frame->rawColorImageValid = true;
    }
#endif
    m_outputDuplication->ReleaseFrame();

    return {(Frame*)frame, FrameRecycler()};
}

