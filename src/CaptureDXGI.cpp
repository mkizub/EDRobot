//
// Created by mkizub on 13.06.2025.
//

#include <iostream>
#include <Windows.h>
#include <dxgi.h>
#include <inspectable.h>
#include <dxgi1_2.h>
#include <d3d11.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Graphics.Capture.h>
#include <windows.graphics.capture.interop.h>
#include <windows.graphics.directx.direct3d11.interop.h>
#include <dwmapi.h>
#pragma comment(lib,"Dwmapi.lib")
#pragma comment(lib,"windowsapp.lib")

#include "CaptureDX11.h"

class CaptureDXGImpl : public CaptureDX11 {
public:
    CaptureDXGImpl(HWND hWndED) : CaptureDX11(hWndED) {}

    bool captureWindow() override;
    bool playWindow() override;
    void OnFrameArrived(
            winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool const& sender,
            winrt::Windows::Foundation::IInspectable const&);

    std::atomic<bool> m_closed = false;
    winrt::com_ptr<ID3D11Device> m_d3dDevice;
    ID3D11DeviceContext* m_d3dContext {nullptr};
    winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool m_framePool {nullptr};
    winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool::FrameArrived_revoker m_frameArrived;

    RECT m_rect{};
    cv::Mat capturedImage;
    int m_frames {};

};

CaptureDX11 *CaptureDX11::getEDCapturer(HWND hWndED) {
    RECT rect;
    if (!GetWindowRect(hWndED, &rect))
        return nullptr;
    RECT clientRect;
    RECT captureRect;
    if (!GetClientRect(hWndED, &clientRect))
        return nullptr;
    ClientToScreen(hWndED, (LPPOINT)&clientRect.left);
    ClientToScreen(hWndED, (LPPOINT)&clientRect.right);
    captureRect = clientRect;

    auto* capturer = new CaptureDXGImpl(hWndED);
    return capturer;
}

CaptureDX11::~CaptureDX11() {
}


bool CaptureDXGImpl::captureWindow() {
    // Init COM
    winrt::init_apartment(winrt::apartment_type::multi_threaded);

    // Create Direct 3D Device
    winrt::com_ptr<ID3D11Device> d3dDevice;

    winrt::check_hresult(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT,
                                           nullptr, 0,D3D11_SDK_VERSION, d3dDevice.put(), nullptr, nullptr));


    winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice device;
    const auto dxgiDevice = d3dDevice.as<IDXGIDevice>();
    {
        winrt::com_ptr<::IInspectable> inspectable;
        winrt::check_hresult(CreateDirect3D11DeviceFromDXGIDevice(dxgiDevice.get(), inspectable.put()));
        device = inspectable.as<winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice>();
    }


    auto idxgiDevice2 = dxgiDevice.as<IDXGIDevice2>();
    winrt::com_ptr<IDXGIAdapter> adapter;
    winrt::check_hresult(idxgiDevice2->GetParent(winrt::guid_of<IDXGIAdapter>(), adapter.put_void()));
    winrt::com_ptr<IDXGIFactory2> factory;
    winrt::check_hresult(adapter->GetParent(winrt::guid_of<IDXGIFactory2>(), factory.put_void()));

    ID3D11DeviceContext* d3dContext {nullptr};
    d3dDevice->GetImmediateContext(&d3dContext);

    RECT rect{};
    DwmGetWindowAttribute(hWndED, DWMWA_EXTENDED_FRAME_BOUNDS, &rect, sizeof(RECT));
    const auto size = winrt::Windows::Graphics::SizeInt32{rect.right - rect.left, rect.bottom - rect.top};

    winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool framePool =
            winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool::Create(
                    device,
                    winrt::Windows::Graphics::DirectX::DirectXPixelFormat::B8G8R8A8UIntNormalized,
                    2,
                    size);

    const auto activationFactory = winrt::get_activation_factory<winrt::Windows::Graphics::Capture::GraphicsCaptureItem>();
    auto interopFactory = activationFactory.as<IGraphicsCaptureItemInterop>();
    winrt::Windows::Graphics::Capture::GraphicsCaptureItem captureItem = {nullptr};
    interopFactory->CreateForWindow(hWndED, winrt::guid_of<ABI::Windows::Graphics::Capture::IGraphicsCaptureItem>(),
                                    reinterpret_cast<void**>(winrt::put_abi(captureItem)));

    auto isFrameArrived = false;
    winrt::com_ptr<ID3D11Texture2D> texture;
    const auto session = framePool.CreateCaptureSession(captureItem);
    framePool.FrameArrived([&](auto &framePool, auto &) {
        if (isFrameArrived) return;
        auto frame = framePool.TryGetNextFrame();

        struct __declspec(uuid("A9B3D012-3DF2-4EE3-B8D1-8695F457D3C1"))
        IDirect3DDxgiInterfaceAccess : ::IUnknown {
            virtual HRESULT __stdcall GetInterface(GUID const &id, void **object) = 0;
        };

        auto access = frame.Surface().as<IDirect3DDxgiInterfaceAccess>();
        access->GetInterface(winrt::guid_of<ID3D11Texture2D>(), texture.put_void());
        isFrameArrived = true;
        return;
    });

    session.IsCursorCaptureEnabled(false);
    session.StartCapture();

    // Message pump
    MSG msg;
    clock_t timer = clock();
    while (!isFrameArrived)
    {
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE) > 0)
            DispatchMessage(&msg);

        if (clock() - timer > 20000)
        {
            // TODO: try to make here a better error handling
            return false;
        }
    }

    session.Close();

    D3D11_TEXTURE2D_DESC capturedTextureDesc;
    texture->GetDesc(&capturedTextureDesc);

    capturedTextureDesc.Usage = D3D11_USAGE_STAGING;
    capturedTextureDesc.BindFlags = 0;
    capturedTextureDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    capturedTextureDesc.MiscFlags = 0;

    winrt::com_ptr<ID3D11Texture2D> userTexture = nullptr;
    winrt::check_hresult(d3dDevice->CreateTexture2D(&capturedTextureDesc, NULL, userTexture.put()));

    d3dContext->CopyResource(userTexture.get(), texture.get());


    D3D11_MAPPED_SUBRESOURCE resource;
    winrt::check_hresult(d3dContext->Map(userTexture.get(), NULL, D3D11_MAP_READ, 0, &resource));

    cv::Mat capturedImage(rect.bottom-rect.top, rect.right-rect.left, CV_8UC4, resource.pData, resource.RowPitch);
    cv::imshow("Captured", capturedImage);
    cv::waitKey();
    cv::destroyAllWindows();

    return true;
}

bool CaptureDXGImpl::playWindow() {
    // Init COM
    winrt::init_apartment(winrt::apartment_type::multi_threaded);

    winrt::check_hresult(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT,
                                           nullptr, 0,D3D11_SDK_VERSION, m_d3dDevice.put(), nullptr, nullptr));


    winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice device;
    const auto dxgiDevice = m_d3dDevice.as<IDXGIDevice>();
    {
        winrt::com_ptr<::IInspectable> inspectable;
        winrt::check_hresult(CreateDirect3D11DeviceFromDXGIDevice(dxgiDevice.get(), inspectable.put()));
        device = inspectable.as<winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice>();
    }


    auto idxgiDevice2 = dxgiDevice.as<IDXGIDevice2>();
    winrt::com_ptr<IDXGIAdapter> adapter;
    winrt::check_hresult(idxgiDevice2->GetParent(winrt::guid_of<IDXGIAdapter>(), adapter.put_void()));
    winrt::com_ptr<IDXGIFactory2> factory;
    winrt::check_hresult(adapter->GetParent(winrt::guid_of<IDXGIFactory2>(), factory.put_void()));

    m_d3dContext = nullptr;
    m_d3dDevice->GetImmediateContext(&m_d3dContext);

    DwmGetWindowAttribute(hWndED, DWMWA_EXTENDED_FRAME_BOUNDS, &m_rect, sizeof(RECT));
    const auto size = winrt::Windows::Graphics::SizeInt32{m_rect.right - m_rect.left, m_rect.bottom - m_rect.top};

//    winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool m_framePool =
//            winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool::Create(
//                    device,
//                    winrt::Windows::Graphics::DirectX::DirectXPixelFormat::B8G8R8A8UIntNormalized,
//                    2,
//                    size);
//
    const auto activationFactory = winrt::get_activation_factory<winrt::Windows::Graphics::Capture::GraphicsCaptureItem>();
    auto interopFactory = activationFactory.as<IGraphicsCaptureItemInterop>();
    winrt::Windows::Graphics::Capture::GraphicsCaptureItem captureItem = {nullptr};
    interopFactory->CreateForWindow(hWndED, winrt::guid_of<ABI::Windows::Graphics::Capture::IGraphicsCaptureItem>(),
                                    reinterpret_cast<void**>(winrt::put_abi(captureItem)));

    winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool m_framePool =
            winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool::CreateFreeThreaded(
                    device,
                    winrt::Windows::Graphics::DirectX::DirectXPixelFormat::B8G8R8A8UIntNormalized,
                    2,
                    size);


    cv::Mat tmp(m_rect.bottom - m_rect.top, m_rect.right - m_rect.left, CV_8UC4);
    cv::imshow("Captured", tmp);
    m_closed = false;
    m_frameArrived = m_framePool.FrameArrived(winrt::auto_revoke, { this, &CaptureDXGImpl::OnFrameArrived });
    auto session = m_framePool.CreateCaptureSession(captureItem);
    m_frames = 0;
    auto start = std::chrono::high_resolution_clock::now();
    session.StartCapture();
    cv::waitKey();
    m_closed = true;
    session.Close();
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = end - start;
    auto duration_s = std::chrono::duration_cast<std::chrono::seconds>(duration).count();
    int fps = m_frames /  duration_s;
    LOG(INFO) << "Capture FPS=" << fps;
    cv::destroyAllWindows();

    return true;
}

void CaptureDXGImpl::OnFrameArrived(
        winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool const& sender,
        winrt::Windows::Foundation::IInspectable const&)
{
    //while (!m_closed) {
        auto frame = sender.TryGetNextFrame();
        if (frame) {
            struct __declspec(uuid("A9B3D012-3DF2-4EE3-B8D1-8695F457D3C1"))
            IDirect3DDxgiInterfaceAccess : ::IUnknown {
                virtual HRESULT __stdcall GetInterface(GUID const &id, void **object) = 0;
            };

            auto access = frame.Surface().as<IDirect3DDxgiInterfaceAccess>();
            winrt::com_ptr<ID3D11Texture2D> texture;
            access->GetInterface(winrt::guid_of<ID3D11Texture2D>(), texture.put_void());

            D3D11_TEXTURE2D_DESC capturedTextureDesc;
            texture->GetDesc(&capturedTextureDesc);

            capturedTextureDesc.Usage = D3D11_USAGE_STAGING;
            capturedTextureDesc.BindFlags = 0;
            capturedTextureDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
            capturedTextureDesc.MiscFlags = 0;

            winrt::com_ptr<ID3D11Texture2D> userTexture = nullptr;
            winrt::check_hresult(m_d3dDevice->CreateTexture2D(&capturedTextureDesc, NULL, userTexture.put()));

            m_d3dContext->CopyResource(userTexture.get(), texture.get());

            D3D11_MAPPED_SUBRESOURCE resource;
            winrt::check_hresult(m_d3dContext->Map(userTexture.get(), NULL, D3D11_MAP_READ, 0, &resource));
            m_frames += 1;

            //capturedImage = cv::Mat(m_rect.bottom - m_rect.top, m_rect.right - m_rect.left, CV_8UC4, resource.pData, resource.RowPitch);
            //cv::imshow("Captured", capturedImage);
            //cv::waitKey(1);
            //if (cv::pollKey() > 0)
            //    m_closed = true;
        }
    //}
}