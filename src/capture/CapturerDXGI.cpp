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
    const cv::UMat& getColorTexture() const override;
    const cv::Mat& getColorImage() const override;
    const cv::Mat& getGrayImage() const override;

    void cleanup();

    D3D11_TEXTURE2D_DESC mStagingTextureDesc;
    mutable D3D11_MAPPED_SUBRESOURCE mStagingMappedTex {};
    CComPtr<ID3D11Texture2D> mStagingTexture;

    mutable cv::UMat colorTexture;
    mutable cv::Mat colorImage;
    mutable cv::Mat grayImage;
    mutable bool stagingTextureValid {false};
    mutable bool stagingTextureMapped {false};
    mutable bool colorImageMapped {false};
    mutable bool colorTextureValid {false};
    mutable bool colorImageValid {false};
    mutable bool grayImageValid {false};
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
        ((CapturerDXGI*)owner)->m_d3dContext->Unmap(mStagingTexture, 0);
        mStagingMappedTex = {};
        stagingTextureMapped = false;
    }
    if (colorImageValid) {
        colorImage = cv::Mat();
        colorImageValid = false;
        colorImageMapped = false;
    }
    if (colorTextureValid) {
        colorTexture = cv::UMat();
        colorTextureValid = false;
    }
    grayImageValid = false;
}

const cv::UMat& FrameDXGI::getColorTexture() const {
    if (colorTextureValid)
        return colorTexture;
    if (!stagingTextureValid)
        return colorTexture;
    if (stagingTextureMapped) {
        if (colorImageMapped) {
            colorImage = cv::Mat();
            colorImageMapped = false;
        }
        ((CapturerDXGI*)owner)->m_d3dContext->Unmap(mStagingTexture, 0);
        mStagingMappedTex = {};
        stagingTextureMapped = false;
    }
    if (cv::ocl::OpenCLExecutionContext::getCurrentRef().empty()) {
        auto capt = (CapturerDXGI*)owner;
        cv::directx::ocl::initializeContextFromD3D11Device(capt->m_d3dDevice);
    }
    cv::directx::convertFromD3D11Texture2D(mStagingTexture, colorTexture);
    colorTextureValid = true;
    if (colorImageValid) {
        colorImage = colorTexture.getMat(cv::ACCESS_READ);
    }
    return colorTexture;
}

const cv::Mat& FrameDXGI::getColorImage() const {
    if (colorImageValid)
        return colorImage;
    if (colorTextureValid) {
        assert (!stagingTextureMapped);
        colorImage = colorTexture.getMat(cv::ACCESS_READ);
        colorTextureValid = true;
    }
    auto capt = (CapturerDXGI *) owner;
    if (!stagingTextureMapped) {
        capt->m_d3dContext->Map(mStagingTexture, 0, D3D11_MAP_READ, 0, &mStagingMappedTex);
        stagingTextureMapped = true;
    }
    colorImage = cv::Mat(size.height, size.width, CV_8UC4, mStagingMappedTex.pData, mStagingMappedTex.RowPitch);
    colorImageMapped = true;
    colorImageValid = true;
    return colorImage;
}

const cv::Mat& FrameDXGI::getGrayImage() const {
    if (grayImageValid)
        return grayImage;
    cv::cvtColor(getColorImage(), grayImage, cv::COLOR_RGBA2GRAY);
    grayImageValid = true;
    return grayImage;
}


CapturerDXGI::CapturerDXGI(HMONITOR hMonitor, LPMONITORINFOEX monitorInfoEx, HDC hdcMonitor)
        : Capturer(hMonitor, monitorInfoEx)
{
}

CapturerDXGI::~CapturerDXGI() {
    CapturerDXGI::stop();
}

void CapturerDXGI::recycle(Frame* p) const {
    if (!p)
        return;
    std::unique_lock<std::mutex> lock(mCaptureMutex);
    auto it = std::find(recycledFrames.begin(), recycledFrames.end(), p);
    if (it != recycledFrames.end())
        return;
    assert(p->owner == this);
    recycledFrames.push_back(p);
}


bool CapturerDXGI::trySetup(HWND hWnd, cv::Rect windowRect, cv::Rect clientRect) {
    if (!hWnd)
        return false;
    if (!cv::ocl::haveOpenCL() || !cv::ocl::useOpenCL()) {
        LOG(ERROR) << "OpenCL not supported";
        return false;
    }

    if (!m_d3dDevice) {
        static const D3D_FEATURE_LEVEL featureLevels[] = {
                D3D_FEATURE_LEVEL_11_1,
                D3D_FEATURE_LEVEL_11_0,
                D3D_FEATURE_LEVEL_10_1,
                D3D_FEATURE_LEVEL_10_0,
                D3D_FEATURE_LEVEL_9_1
        };
        D3D_FEATURE_LEVEL featureLevel;
        HRESULT hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT,
                                       featureLevels, std::size(featureLevels), D3D11_SDK_VERSION,
                                       &m_d3dDevice, &featureLevel, &m_d3dContext);
        if (FAILED(hr)) {
            LOG(ERROR) << "Failed to create D3D11 device" << getErrorMessage(hr);
            return false;
        }
        cv::directx::ocl::initializeContextFromD3D11Device(m_d3dDevice);
        LOG(INFO) << "Using OpenCL device: " << cv::ocl::Context::getDefault().device(0).name();
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
    if (!m_d3dDevice) {
        static const D3D_FEATURE_LEVEL featureLevels[] = {
                D3D_FEATURE_LEVEL_11_1,
                D3D_FEATURE_LEVEL_11_0,
                D3D_FEATURE_LEVEL_10_1,
                D3D_FEATURE_LEVEL_10_0,
                D3D_FEATURE_LEVEL_9_1
        };
        D3D_FEATURE_LEVEL featureLevel;
        hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT,
                                       featureLevels, std::size(featureLevels), D3D11_SDK_VERSION,
                                       &m_d3dDevice, &featureLevel, &m_d3dContext);
        if (FAILED(hr)) {
            LOG(ERROR) << "Failed to create D3D11 device" << getErrorMessage(hr);
            return false;
        }
        cv::directx::ocl::initializeContextFromD3D11Device(m_d3dDevice);
        LOG(INFO) << "Using OpenCL device: " << cv::ocl::Context::getDefault().device(0).name();
    }


    hr = m_d3dDevice->QueryInterface(IID_PPV_ARGS(&m_dxgiDevice));
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

    hr = m_dxgiOutput1->DuplicateOutput(m_d3dDevice, &m_outputDuplication);
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
    m_d3dContext = nullptr;
    m_d3dDevice = nullptr;

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
        hr = m_d3dDevice->CreateTexture2D(&frame->mStagingTextureDesc, nullptr, &frame->mStagingTexture);
        if (FAILED(hr)) {
            LOG(ERROR) << "CapturerDXGI Failed to create staging texture";
            return {};
        }
        frame->stagingTextureValid = true;
    }
    if (frame->stagingTextureMapped) {
        m_d3dContext->Unmap(frame->mStagingTexture, 0);
        frame->mStagingMappedTex = {};
        frame->stagingTextureMapped = false;
    }

    m_d3dContext->CopyResource(frame->mStagingTexture, texture);
    frame->stagingTextureValid = true;
    hr = m_d3dContext->Map(frame->mStagingTexture, 0, D3D11_MAP_READ, 0, &frame->mStagingMappedTex);
    if (FAILED(hr)) {
        LOG(ERROR) << "CapturerDXGI Failed to map staging texture";
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
        cv::Rect captureRect = captureVirtRect - monitorVirtRect.tl();
        if (captureRect.x + captureRect.width > monitorVirtRect.width)
            captureRect.width = monitorVirtRect.width - captureRect.x;
        if (captureRect.y + captureRect.height > monitorVirtRect.height)
            captureRect.height = monitorVirtRect.height - captureRect.y;
        for (int r=0; r < captureRect.height; r++) {
            int src_y = r+captureRect.y;
            if (src_y < 0)
                continue;
            int dst_y = r;

            int src_x = captureRect.x;
            int src_w = captureRect.width;
            int dst_x = 0;
            if (src_x < 0) {
                dst_x = -src_x;
                src_w += src_x;
                src_x = 0;
            }
            uchar* src_ptr = (uchar*)frame->mStagingMappedTex.pData;
            src_ptr += src_x * 4 + src_y * frame->mStagingMappedTex.RowPitch;
            uchar* dst_ptr = frame->colorImage.ptr(dst_y, dst_x);

            memcpy(dst_ptr, src_ptr, src_w*4);
        }

        m_d3dContext->Unmap(frame->mStagingTexture, 0);
        frame->mStagingMappedTex = {};
        frame->stagingTextureMapped = false;
        frame->colorImageValid = true;
    }
    m_outputDuplication->ReleaseFrame(); // ?

    return {(Frame*)frame, FrameRecycler()};
}

