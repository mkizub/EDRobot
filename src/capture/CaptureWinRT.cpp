//
// Created by mkizub on 13.06.2025.
//

#include "../pch.h"

#include <dxgi.h>
#include <inspectable.h>
#include <dxgi1_2.h>
#include <d3d11.h>
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
        : Frame(owner, owner->captureVirtRect.size())
{
    stagingTextureValid = false;
}

FrameWinRT::~FrameWinRT() {
    LOG(ERROR) << "FrameWinRT deleted";
    cleanup();
}

bool FrameWinRT::valid() const {
    return stagingTextureValid && mStagingTexture;
}

void FrameWinRT::cleanup() {
    if (colorImageValid) {
        colorImage = XMat();
        colorImageValid = false;
    }
}

const XMat& FrameWinRT::getImage() const {
    if (colorImageValid)
        return colorImage;
    cv::directx::convertFromD3D11Texture2D(mStagingTexture.get(), colorImage);
    colorImageValid = true;
    return colorImage;
}


CapturerWinRT::CapturerWinRT(HMONITOR hMonitor, LPMONITORINFOEX monitorInfoEx, HDC hdcMonitor)
    : Capturer(hMonitor, monitorInfoEx)
{
}

CapturerWinRT::~CapturerWinRT() {
    CapturerWinRT::stop();
}

void CapturerWinRT::recycle(Frame* p) const {
    std::unique_lock<std::mutex> lock(mCaptureMutex);
    if (!p || recycledFrames.size() >= 3)
        return;
    auto it = std::find(recycledFrames.begin(), recycledFrames.end(), p);
    if (it != recycledFrames.end())
        return;
    assert(p->owner == this);
    recycledFrames.push_back(p);
}


bool CapturerWinRT::trySetup(HWND hWnd, cv::Rect windowRect, cv::Rect clientRect) {
    if (!hWnd)
        return false;
    if (!getID3D11Device() || !getID3D11DeviceContext()) {
        LOG(ERROR) << "D3dDevice not initialized";
        return false;
    }
    // Init COM
    winrt::init_apartment();

    this->hWndED = hWnd;
    this->windowVirtRect = windowRect;
    this->captureVirtRect = clientRect;
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

    auto dxgiDevice2 = dxgiDevice.as<IDXGIDevice2>();
    winrt::com_ptr<IDXGIAdapter> adapter;
    winrt::check_hresult(dxgiDevice2->GetParent(winrt::guid_of<IDXGIAdapter>(), adapter.put_void()));
    winrt::com_ptr<IDXGIFactory2> factory;
    winrt::check_hresult(adapter->GetParent(winrt::guid_of<IDXGIFactory2>(), factory.put_void()));

    //m_d3dDevice->GetImmediateContext(m_d3dContext.put());

    RECT rect{};
    DwmGetWindowAttribute(hWndED, DWMWA_EXTENDED_FRAME_BOUNDS, &rect, sizeof(RECT));
    const auto size = winrt::Windows::Graphics::SizeInt32{rect.right - rect.left, rect.bottom - rect.top};

    const auto activationFactory = winrt::get_activation_factory<winrt::Windows::Graphics::Capture::GraphicsCaptureItem>();
    auto interopFactory = activationFactory.as<IGraphicsCaptureItemInterop>();
    interopFactory->CreateForWindow(hWndED, winrt::guid_of<ABI::Windows::Graphics::Capture::IGraphicsCaptureItem>(),
                                    reinterpret_cast<void**>(winrt::put_abi(m_captureItem)));
    auto xxx = m_captureItem.Closed(winrt::auto_revoke, { this, &CapturerWinRT::OnCaptureClosed });

    m_framePool = winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool::CreateFreeThreaded(
            device,
            winrt::Windows::Graphics::DirectX::DirectXPixelFormat::B8G8R8A8UIntNormalized,
            2,
            size);
    if (!m_framePool) {
        LOG(ERROR) << "Cannot create Direct3D11CaptureFramePool";
        return false;
    }

    m_frameArrived = m_framePool.FrameArrived({ this, &CapturerWinRT::OnFrameArrived });
    m_session = m_framePool.CreateCaptureSession(m_captureItem);

    Capturer::start();
    std::unique_lock<std::mutex> lock(mCaptureMutex);
    m_frames = 0;
    m_session.StartCapture();
    mCaptureCond.wait_for(lock, std::chrono::milliseconds(3000), [this](){ return m_frames > 0; });
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
    //m_frame = frame;
    uint64_t old_frames = m_frames;
    bool ok = mCaptureCond.wait_for(
            lock, std::chrono::milliseconds(500), [this,old_frames] { return this->m_frames > old_frames; });
    LOG_IF(!ok,ERROR) << "CapturerWinRT failed to capture window";
    if (m_nextFrame) {
        copyTexture(frame, m_nextFrame);
        m_nextFrame = nullptr;
    }
    return {(Frame*)frame, FrameRecycler()};
}

//bool CapturerWinRT::capture() {
//    // Init COM
//    winrt::init_apartment(winrt::apartment_type::multi_threaded);
//
//    // Create Direct 3D Device
//    winrt::com_ptr<ID3D11Device> d3dDevice;
//
//    winrt::check_hresult(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT,
//                                           nullptr, 0,D3D11_SDK_VERSION, d3dDevice.put(), nullptr, nullptr));
//
//
//    winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice device;
//    const auto dxgiDevice = d3dDevice.as<IDXGIDevice>();
//    {
//        winrt::com_ptr<::IInspectable> inspectable;
//        winrt::check_hresult(CreateDirect3D11DeviceFromDXGIDevice(dxgiDevice.get(), inspectable.put()));
//        device = inspectable.as<winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice>();
//    }
//
//
//    auto idxgiDevice2 = dxgiDevice.as<IDXGIDevice2>();
//    winrt::com_ptr<IDXGIAdapter> adapter;
//    winrt::check_hresult(idxgiDevice2->GetParent(winrt::guid_of<IDXGIAdapter>(), adapter.put_void()));
//    winrt::com_ptr<IDXGIFactory2> factory;
//    winrt::check_hresult(adapter->GetParent(winrt::guid_of<IDXGIFactory2>(), factory.put_void()));
//
//    ID3D11DeviceContext* d3dContext {nullptr};
//    d3dDevice->GetImmediateContext(&d3dContext);
//
//    RECT rect{};
//    DwmGetWindowAttribute(hWndED, DWMWA_EXTENDED_FRAME_BOUNDS, &rect, sizeof(RECT));
//    const auto size = winrt::Windows::Graphics::SizeInt32{rect.right - rect.left, rect.bottom - rect.top};
//
//    winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool framePool =
//            winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool::Create(
//                    device,
//                    winrt::Windows::Graphics::DirectX::DirectXPixelFormat::B8G8R8A8UIntNormalized,
//                    2,
//                    size);
//
//    const auto activationFactory = winrt::get_activation_factory<winrt::Windows::Graphics::Capture::GraphicsCaptureItem>();
//    auto interopFactory = activationFactory.as<IGraphicsCaptureItemInterop>();
//    winrt::Windows::Graphics::Capture::GraphicsCaptureItem captureItem = {nullptr};
//    interopFactory->CreateForWindow(hWndED, winrt::guid_of<ABI::Windows::Graphics::Capture::IGraphicsCaptureItem>(),
//                                    reinterpret_cast<void**>(winrt::put_abi(captureItem)));
//
//    auto isFrameArrived = false;
//    winrt::com_ptr<ID3D11Texture2D> texture;
//    const auto session = framePool.CreateCaptureSession(captureItem);
//    framePool.FrameArrived([&](auto &framePool, auto &) {
//        if (isFrameArrived) return;
//        auto frame = framePool.TryGetNextFrame();
//
//        struct __declspec(uuid("A9B3D012-3DF2-4EE3-B8D1-8695F457D3C1"))
//        IDirect3DDxgiInterfaceAccess : ::IUnknown {
//            virtual HRESULT __stdcall GetInterface(GUID const &id, void **object) = 0;
//        };
//
//        auto access = frame.Surface().as<IDirect3DDxgiInterfaceAccess>();
//        access->GetInterface(winrt::guid_of<ID3D11Texture2D>(), texture.put_void());
//        isFrameArrived = true;
//        return;
//    });
//
//    session.IsCursorCaptureEnabled(false);
//    session.StartCapture();
//
//    // Message pump
//    MSG msg;
//    clock_t timer = clock();
//    while (!isFrameArrived)
//    {
//        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE) > 0)
//            DispatchMessage(&msg);
//
//        if (clock() - timer > 20000)
//        {
//            // TODO: try to make here a better error handling
//            return false;
//        }
//    }
//
//    session.Close();
//
//    D3D11_TEXTURE2D_DESC capturedTextureDesc;
//    texture->GetDesc(&capturedTextureDesc);
//
//    capturedTextureDesc.Usage = D3D11_USAGE_STAGING;
//    capturedTextureDesc.BindFlags = 0;
//    capturedTextureDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
//    capturedTextureDesc.MiscFlags = 0;
//
//    winrt::com_ptr<ID3D11Texture2D> userTexture = nullptr;
//    winrt::check_hresult(d3dDevice->CreateTexture2D(&capturedTextureDesc, NULL, userTexture.put()));
//
//    d3dContext->CopyResource(userTexture.get(), texture.get());
//
//
//    D3D11_MAPPED_SUBRESOURCE resource;
//    winrt::check_hresult(d3dContext->Map(userTexture.get(), NULL, D3D11_MAP_READ, 0, &resource));
//
//    cv::Mat capturedImage(rect.bottom-rect.top, rect.right-rect.left, CV_8UC4, resource.pData, resource.RowPitch);
//    cv::imshow("Captured", capturedImage);
//    cv::waitKey();
//    cv::destroyAllWindows();
//
//    return true;
//}

void CapturerWinRT::OnFrameArrived(
        winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool const& sender,
        winrt::Windows::Foundation::IInspectable const&)
{
    if (!isStarted())
        return;
    LOG(INFO) << "CapturerWinRT::OnFrameArrived";
    std::unique_lock<std::mutex> lock(mCaptureMutex);

    auto nextFrame = sender.TryGetNextFrame();
    if (!nextFrame) {
        LOG(WARNING) << "CapturerWinRT no next frame";
    } else {
        m_frames += 1;
        mCaptureCond.notify_one();
    }
    if (m_frame)
        copyTexture( m_frame, nextFrame);
    else
        m_nextFrame = nextFrame;
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

    {
        auto frame_tp = std::chrono::high_resolution_clock::time_point(captureFrame.SystemRelativeTime());
        auto elapsed_since_start = frame_tp - hpcStartTimestamp;
        auto utc_tp = utcStartTimestamp + elapsed_since_start;
        frame->timestamp = std::chrono::time_point_cast<Timestamp::duration>(utc_tp);
    }
    auto access = captureFrame.Surface().as<IDirect3DDxgiInterfaceAccess>();
    winrt::com_ptr<ID3D11Texture2D> texture;
    access->GetInterface(winrt::guid_of<ID3D11Texture2D>(), texture.put_void());

    if (!frame->mStagingTexture || !frame->stagingTextureValid) {
        D3D11_TEXTURE2D_DESC desc;
        texture->GetDesc(&desc);
        frame->mStagingTextureDesc = desc;
        frame->mStagingTextureDesc.Usage = D3D11_USAGE_STAGING;
        frame->mStagingTextureDesc.BindFlags = 0;
        frame->mStagingTextureDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        frame->mStagingTextureDesc.MiscFlags = 0;
        frame->mStagingTexture = nullptr;
        HRESULT hr = getID3D11Device()->CreateTexture2D(&frame->mStagingTextureDesc, nullptr, frame->mStagingTexture.put());
        if (FAILED(hr)) {
            LOG(ERROR) << "CapturerWinRT Failed to create staging texture";
            return;
        }
        frame->stagingTextureValid = true;
    }

    getID3D11DeviceContext()->CopyResource(frame->mStagingTexture.get(), texture.get());
}
