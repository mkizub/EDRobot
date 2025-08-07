//
// Created by mkizub on 19.06.2025.
//

#include "../pch.h"

#include <atlbase.h>
#include <dxgi.h>
#include <dxgi1_2.h>
#include <d3d11.h>

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

    mutable XMat colorImage;
    mutable bool stagingTextureValid {false};
    mutable bool stagingTextureMapped {false};
    mutable bool colorImageValid {false};
};

FrameDXGI::FrameDXGI(CapturerDXGI* owner)
        : Frame(owner, owner->captureVirtRect.size())
{
    stagingTextureValid = false;
}

FrameDXGI::~FrameDXGI() {
    LOG(ERROR) << "FrameDXGI deleted";
    cleanup();
}

bool FrameDXGI::valid() const {
    return stagingTextureValid && mStagingTexture;
}

void FrameDXGI::cleanup() {
    if (stagingTextureMapped) {
        ((CapturerDXGI*)owner)->getID3D11DeviceContext()->Unmap(mStagingTexture, 0);
        mStagingMappedTex = {};
        stagingTextureMapped = false;
    }
    if (colorImageValid) {
        colorImage = XMat();
        colorImageValid = false;
    }
}

//const cv::Mat& FrameDXGI::getTexture() const {
//    if (colorTextureValid)
//        return colorTexture;
//    if (!stagingTextureValid)
//        return colorTexture;
//    if (stagingTextureMapped) {
//        if (colorImageMapped) {
//            colorImage = cv::Mat();
//            colorImageMapped = false;
//        }
//        ((CapturerDXGI*)owner)->getID3D11DeviceContext()->Unmap(mStagingTexture, 0);
//        mStagingMappedTex = {};
//        stagingTextureMapped = false;
//    }
//    cv::directx::convertFromD3D11Texture2D(mStagingTexture, colorTexture);
//    colorTextureValid = true;
//    return colorTexture;
//}

const XMat& FrameDXGI::getImage() const {
    return colorImage;
}

CapturerDXGI::CapturerDXGI(HMONITOR hMonitor, LPMONITORINFOEX monitorInfoEx, HDC hdcMonitor)
        : Capturer(hMonitor, monitorInfoEx)
{
}

CapturerDXGI::~CapturerDXGI() {
    CapturerDXGI::stop();
}

void CapturerDXGI::recycle(Frame* p) const {
    std::unique_lock<std::mutex> lock(mCaptureMutex);
    if (!p || recycledFrames.size() >= 3)
        return;
    auto it = std::find(recycledFrames.begin(), recycledFrames.end(), p);
    if (it != recycledFrames.end())
        return;
    assert(p->owner == this);
    recycledFrames.push_back(p);
}


bool CapturerDXGI::trySetup(HWND hWnd, cv::Rect windowRect, cv::Rect clientRect) {
    if (!hWnd)
        return false;
    if (!getID3D11Device() || !getID3D11DeviceContext()) {
        LOG(ERROR) << "D3dDevice not initialized";
        return false;
    }

    this->hWndED = hWnd;
    this->windowVirtRect = windowRect;
    this->captureVirtRect = clientRect;
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
    hr = m_dxgiDevice->GetParent(IID_PPV_ARGS(&m_dxgiAdapter));
    if (FAILED(hr)) {
        LOG(ERROR) << "Failed to acquire IDXGIAdapter interface" << getErrorMessage(hr);
        return false;
    }

    CComPtr<IDXGIOutput> dxgiOutput;
    for (unsigned i=0; ; i++) {
        CComPtr<IDXGIOutput> output;
        hr = m_dxgiAdapter->EnumOutputs(i, &output);
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
    hr = dxgiOutput->QueryInterface(IID_PPV_ARGS(&m_dxgiOutput1));
    if (FAILED(hr)) {
        LOG(ERROR) << "Failed to acquire IDXGIOutput1 interface " << getErrorMessage(hr);
        return false;
    }

    hr = m_dxgiOutput1->DuplicateOutput(getID3D11Device(), &m_outputDuplication);
    if (FAILED(hr)) {
        LOG(ERROR) << "Failed to acquire DuplicateOutput " << getErrorMessage(hr);
        return false;
    }

    LOG(INFO) << "CapturerDXGI started";
    return Capturer::start();
}

bool CapturerDXGI::stop() {
    LOG(ERROR) << "CapturerDXGI stop";

    Capturer::stop();

    m_outputDuplication = nullptr;
    m_dxgiOutput1 = nullptr;
    m_dxgiAdapter = nullptr;
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
            break;
        }
    }
    if (FAILED(hr)) {
        LOG(ERROR) << "CapturerDXGI Failed acquire desktop frame";
        return {};
    }

    CComPtr<ID3D11Texture2D> texture;
    hr = desktopResource->QueryInterface(IID_PPV_ARGS(&texture));
    if(FAILED(hr)) {
        LOG(ERROR) << "CapturerDXGI Failed to acquire ID3D11Texture2D interface";
        return {};
    }

    if (!frame->mStagingTexture || !frame->stagingTextureValid) {
        D3D11_TEXTURE2D_DESC desc;
        texture->GetDesc(&desc);
        frame->mStagingTextureDesc = desc;
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

    getID3D11DeviceContext()->CopyResource(frame->mStagingTexture, texture);
    frame->stagingTextureValid = true;
    hr = getID3D11DeviceContext()->Map(frame->mStagingTexture, 0, D3D11_MAP_READ, 0, &frame->mStagingMappedTex);
    if (FAILED(hr)) {
        LOG(ERROR) << "CapturerDXGI Failed to map staging texture: " << getErrorMessage(hr);
    } else {
        frame->stagingTextureMapped = true;

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

        // TODO: move mapping/unmapping to FrameRT and do it on demand, use mapped memory instead of copy
        frame->colorImage.create(frame->size, CV_8UC4);
#ifdef EDROBOT_USE_OPENCL
        cv::Mat colorImage = frame->colorImage.getMat(cv::ACCESS_RW);
#else
        cv::Mat& colorImage = frame->colorImage;
#endif
        cv::Rect captureRect = captureVirtRect - monitorVirtRect.tl();
        if (captureRect.x + captureRect.width > monitorVirtRect.width)
            captureRect.width = monitorVirtRect.width - captureRect.x;
        if (captureRect.y + captureRect.height > monitorVirtRect.height)
            captureRect.height = monitorVirtRect.height - captureRect.y;
        int row0 = captureRect.y >= 0 ? 0 : -captureRect.y;
        int src_x = captureRect.x;
        int src_w = captureRect.width;
        int dst_x = 0;
        if (src_x < 0) {
            dst_x = -src_x;
            src_w += src_x;
            src_x = 0;
        }
        int src_y = row0 + captureRect.y;
        uchar* src_ptr = (uchar*)frame->mStagingMappedTex.pData;
        src_ptr += src_x * 4 + src_y * frame->mStagingMappedTex.RowPitch;
        uchar* dst_ptr = colorImage.ptr(row0, dst_x);

        for (int dst_y=row0; dst_y < captureRect.height; dst_y++) {
            memcpy(dst_ptr, src_ptr, src_w*4);
            src_ptr += frame->mStagingMappedTex.RowPitch;
            dst_ptr += colorImage.step;
        }

        getID3D11DeviceContext()->Unmap(frame->mStagingTexture, 0);
        frame->mStagingMappedTex = {};
        frame->stagingTextureMapped = false;
        frame->colorImageValid = true;
    }
    m_outputDuplication->ReleaseFrame();

    return {(Frame*)frame, FrameRecycler()};
}

