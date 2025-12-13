//
// Created by mkizub on 13.06.2025.
//

#include "../pch.h"

#include <inspectable.h>
#include <dxgi1_6.h>
#include <d3d11_4.h>
#include <winstring.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Graphics.Capture.h>
#include <windows.graphics.capture.interop.h>
#include <windows.graphics.directx.direct3d11.interop.h>
#include <dwmapi.h>

#include <opencv2/core/directx.hpp>

#include "CapturerWinRT.h"

class FrameWinRT : public Frame {
public:
    FrameWinRT(CapturerWinRT* owner);
    ~FrameWinRT() override;
    bool valid() const override;
    const XMat& getImage() const override;

    void cleanup();

    D3D11_TEXTURE2D_DESC mStagingTextureDesc;
    winrt::com_ptr<ID3D11Texture2D> mStagingTexture;

    mutable XMat colorImage;
    mutable bool stagingTextureValid {false};
    mutable bool colorImageValid {false};
};

FrameWinRT::FrameWinRT(CapturerWinRT* owner)
        : Frame(owner, owner->captureSize)
{
    stagingTextureValid = false;
}

FrameWinRT::~FrameWinRT() {
    LOG(ERROR) << "FrameWinRT deleted";
    cleanup();
}

bool FrameWinRT::valid() const {
    if (colorImageValid)
        return true;
    return stagingTextureValid && mStagingTexture;
}

void FrameWinRT::cleanup() {
    colorImageValid = false;
    colorImage.release();
}

const XMat& FrameWinRT::getImage() const {
    if (colorImageValid)
        return colorImage;

#ifdef EDROBOT_USE_OPENCL
    cv::directx::convertFromD3D11Texture2D(mStagingTexture.get(), colorImage);
#else
    D3D11_MAPPED_SUBRESOURCE stagingMappedTex {};
    HRESULT hr = Capturer::getID3D11DeviceContext()->Map(mStagingTexture.get(), 0, D3D11_MAP_READ, 0, &stagingMappedTex);
    if (FAILED(hr)) {
        LOG(ERROR) << "CapturerDXGI Failed to map staging texture: " << getErrorMessage(hr);
        colorImageValid = false;
    } else {
        cv::Mat mappedImage(mStagingTextureDesc.Height, mStagingTextureDesc.Width,
                            CV_8UC4, stagingMappedTex.pData, stagingMappedTex.RowPitch);
        cv::copyTo(mappedImage, colorImage, cv::noArray());
        Capturer::getID3D11DeviceContext()->Unmap(mStagingTexture.get(), 0);
    }
#endif
    if (cv::Size(colorImage.cols,colorImage.rows) != this->size) {
        // need rescaling
        XMat tmp;
        cv::resize(colorImage, tmp, this->size);
        colorImage = tmp;
    }
    colorImageValid = true;
    return colorImage;
}


CapturerWinRT::CapturerWinRT(HMONITOR hMonitor, LPMONITORINFOEX monitorInfoEx)
    : Capturer(hMonitor, monitorInfoEx)
    , recycledFrames(3)
{
}

CapturerWinRT::~CapturerWinRT() {
    CapturerWinRT::stop();
}

bool CapturerWinRT::recycle(Frame* p) const {
    std::unique_lock<std::mutex> lock(mCaptureMutex);
    if (!p || recycledFrames.full())
        return false;
    assert(p->owner == this);
    auto it = std::find(recycledFrames.begin(), recycledFrames.end(), p);
    if (it != recycledFrames.end())
        recycledFrames.push_back(p);
    return true;
}

bool CapturerWinRT::trySetup(HWND hWnd, cv::Rect windowRect, cv::Rect clientRect) {
    if (!hWnd)
        return false;
    if (!getID3D11Device() || !getID3D11DeviceContext()) {
        LOG(ERROR) << "D3dDevice not initialized";
        return false;
    }
    auto gameSize = Cfg.getConfigDisplaySize();
    if (gameSize.width > clientRect.width || gameSize.height > clientRect.height)
        return false;
    // Init COM
    winrt::init_apartment();

    this->hWndED = hWnd;
    this->windowVirtRect = windowRect;
    this->captureVirtRect = clientRect;
    this->captureSize = Cfg.getCaptureDisplaySize();
    this->titleHeight = clientRect.y - windowRect.y;
    this->borderWidth = clientRect.x - windowRect.x;

    return true;
}

bool CapturerWinRT::start() {
    if (!hWndED) {
        LOG(ERROR) << "Cannot start CapturerWinRT because ED window not found";
        return false;
    }
    winrt::com_ptr<ID3D11Device> d3dDevice;
    d3dDevice.attach(getID3D11Device());

    winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice device;
    const auto dxgiDevice = d3dDevice.as<IDXGIDevice>();
    {
        winrt::com_ptr<::IInspectable> inspectable;
        winrt::check_hresult(CreateDirect3D11DeviceFromDXGIDevice(dxgiDevice.get(), inspectable.put()));
        device = inspectable.as<winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice>();
    }
    d3dDevice.detach();

    RECT rect{};
    DwmGetWindowAttribute(hWndED, DWMWA_EXTENDED_FRAME_BOUNDS, &rect, sizeof(RECT));
    const auto size = winrt::Windows::Graphics::SizeInt32{rect.right - rect.left, rect.bottom - rect.top};

    const auto activationFactory = winrt::get_activation_factory<winrt::Windows::Graphics::Capture::GraphicsCaptureItem>();
    auto interopFactory = activationFactory.as<IGraphicsCaptureItemInterop>();
    winrt::Windows::Graphics::Capture::GraphicsCaptureItem captureItem {nullptr};
    interopFactory->CreateForWindow(hWndED, winrt::guid_of<ABI::Windows::Graphics::Capture::IGraphicsCaptureItem>(),
                                    reinterpret_cast<void**>(winrt::put_abi(captureItem)));
//    auto xxx = captureItem.Closed(winrt::auto_revoke, { this, &CapturerWinRT::OnCaptureClosed });

    m_framePool = winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool::Create(
            device,
            winrt::Windows::Graphics::DirectX::DirectXPixelFormat::B8G8R8A8UIntNormalized,
            1,
            size);
    if (!m_framePool) {
        LOG(ERROR) << "Cannot create Direct3D11CaptureFramePool";
        return false;
    }

    //m_frameArrived = m_framePool.FrameArrived({ this, &CapturerWinRT::OnFrameArrived });
    m_session = m_framePool.CreateCaptureSession(captureItem);

    Capturer::start();
    std::unique_lock<std::mutex> lock(mCaptureMutex);
    m_session.StartCapture();
    utc_timer timer(1s);
    while (!timer.expired()) {
        auto nextFrame = m_framePool.TryGetNextFrame();
        if (nextFrame) {
            startTimeSpan = nextFrame.SystemRelativeTime();
            utcStartTimestamp = Timestamp::clock::now();
            break;
        }
        MSG msg;
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
            DispatchMessage(&msg);
    }
    LOG(INFO) << "CapturerWinRT started";
    return true;
}

bool CapturerWinRT::stop() {
    LOG(ERROR) << "CapturerWinRT stop";
//    if (m_session) {
//        m_session.Close();
//    }
    m_session = nullptr;
    m_framePool = nullptr;

    Capturer::stop();

    while (!recycledFrames.empty()) {
        delete (FrameWinRT*)recycledFrames.back();
        recycledFrames.pop_back();
    }

    LOG(INFO) << "CapturerWinRT stopped";
    return true;
}

upFrame CapturerWinRT::capture(upFrame&& recycle) {
    if (!isStarted()) {
        LOG(ERROR) << "CapturerWinRT not started";
        return {};
    }
    Timestamp utc_now = Timestamp::clock::now();
    winrt::Windows::Graphics::Capture::Direct3D11CaptureFrame capturedFrame {nullptr};
    while (!capturedFrame && (Timestamp::clock::now()-utc_now) < 150ms) {
        auto nextFrame = m_framePool.TryGetNextFrame();
        if (!nextFrame) {
            MSG msg;
            if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
                DispatchMessage(&msg);
            continue;
        }
        auto frame_srt = nextFrame.SystemRelativeTime() - startTimeSpan;
        auto utc_tp = utcStartTimestamp + frame_srt;
        if (utc_tp >= utc_now || utc_now-utc_tp < 20ms) {
            //LOG(INFO) << std::format("CapturerWinRT: next frame capture took {}ms frame age {}ms",
            //                         std::chrono::duration_cast<std::chrono::milliseconds>(Timestamp::clock::now()-utc_now).count(),
            //                         std::chrono::duration_cast<std::chrono::milliseconds>(Timestamp::clock::now()-utc_tp).count());
            capturedFrame = nextFrame;
        }
    }
    if (!capturedFrame) {
        LOG(WARNING) << "CapturerWinRT no next frame";
        Mgr.pushCommand(Command::ResetCapturer);
        return {};
    }

    std::unique_lock<std::mutex> lock(mCaptureMutex);
    FrameWinRT* frame;
    if (recycle && recycle->owner == this && recycle->size == captureVirtRect.size()) {
        frame = (FrameWinRT*)recycle.release();
        frame->cleanup();
    } else {
        if (recycledFrames.empty()) {
            frame = new FrameWinRT(this);
        } else {
            frame = (FrameWinRT*)recycledFrames.back();
            recycledFrames.pop_back();
            frame->cleanup();
        }
    }
    copyTexture(frame, capturedFrame);

    return {(Frame*)frame, FrameRecycler()};
}

void CapturerWinRT::OnCaptureClosed(const winrt::Windows::Graphics::Capture::GraphicsCaptureItem &sender,
                                    const winrt::Windows::Foundation::IInspectable &)
{
    LOG(ERROR) << "CapturerWinRT capture closed";
    Master::getInstance().pushCommand(Command::ResetCapturer);
}

void CapturerWinRT::copyTexture(FrameWinRT* frame, winrt::Windows::Graphics::Capture::Direct3D11CaptureFrame& captureFrame) {
    assert (frame);
    assert (captureFrame);

    struct __declspec(uuid("A9B3D012-3DF2-4EE3-B8D1-8695F457D3C1"))
    IDirect3DDxgiInterfaceAccess : ::IUnknown {
        virtual HRESULT __stdcall GetInterface(GUID const &id, void **object) = 0;
    };

    auto frame_srt = captureFrame.SystemRelativeTime() - startTimeSpan;
    frame->timestamp = utcStartTimestamp + frame_srt;
    auto access = captureFrame.Surface().as<IDirect3DDxgiInterfaceAccess>();
    winrt::com_ptr<ID3D11Texture2D> texture;
    access->GetInterface(winrt::guid_of<ID3D11Texture2D>(), texture.put_void());

    D3D11_TEXTURE2D_DESC texture_desc;
    texture->GetDesc(&texture_desc);
    if (!frame->mStagingTexture || !frame->stagingTextureValid) {
        frame->mStagingTextureDesc = texture_desc;
        frame->mStagingTextureDesc.Width = captureVirtRect.width;
        frame->mStagingTextureDesc.Height = captureVirtRect.height;
#ifdef EDROBOT_USE_OPENCL
        frame->mStagingTextureDesc.Usage = D3D11_USAGE_DEFAULT;
        frame->mStagingTextureDesc.CPUAccessFlags = 0;
#else
        frame->mStagingTextureDesc.Usage = D3D11_USAGE_STAGING;
        frame->mStagingTextureDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
#endif
        frame->mStagingTextureDesc.BindFlags = 0;
        frame->mStagingTextureDesc.MiscFlags = 0;
        frame->mStagingTexture = nullptr;
        HRESULT hr = getID3D11Device()->CreateTexture2D(&frame->mStagingTextureDesc, nullptr, frame->mStagingTexture.put());
        if (FAILED(hr)) {
            LOG(ERROR) << "CapturerWinRT Failed to create staging texture";
            return;
        }
        frame->stagingTextureValid = true;
    }

    if (texture_desc.Width == captureVirtRect.width && texture_desc.Height == captureVirtRect.height) {
        getID3D11DeviceContext()->CopyResource(frame->mStagingTexture.get(), texture.get());
    } else {
        D3D11_BOX sourceRegion;
        sourceRegion.left = (texture_desc.Width - captureVirtRect.width)/2;
        sourceRegion.right = sourceRegion.left + captureVirtRect.width;
        sourceRegion.top = titleHeight;
        sourceRegion.bottom = titleHeight + captureVirtRect.height;
        sourceRegion.front = 0;
        sourceRegion.back = 1;
        getID3D11DeviceContext()->CopySubresourceRegion1(frame->mStagingTexture.get(), 0, 0, 0, 0, texture.get(), 0, &sourceRegion, D3D11_COPY_DISCARD);
    }
    //Capturer::flushID3D11DeviceContext();
}
