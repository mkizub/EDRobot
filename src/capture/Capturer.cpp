//
// Created by mkizub on 24.05.2025.
//

#include "../pch.h"

#include <d3d11_4.h>
#include <dxgi1_6.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Graphics.Capture.h>
#include <dwmapi.h>

#include "../Capturer.h"
#include "CapturerWin32.h"
#include "CapturerWinRT.h"
#include "CapturerDXGI.h"

#include <opencv2/core/directx.hpp>
#include <shellscalingapi.h>

#ifdef DEBUG
# undef DEBUG
#endif

static CComPtr<ID3D11Device1> D3dDevice;
static CComPtr<ID3D11DeviceContext1> D3dContext;
static CComPtr<ID3D11Query> D3dQuery;
static int numGPUs;

std::unique_ptr<Capturer> Capturer::TheCapturer;


ID3D11Device1* Capturer::getID3D11Device() {
    assert (main_thread_id == std::this_thread::get_id());
    return D3dDevice;
}
ID3D11DeviceContext1* Capturer::getID3D11DeviceContext() {
    assert (main_thread_id == std::this_thread::get_id());
    return D3dContext;
}

bool Capturer::flushID3D11DeviceContext() {
    assert (main_thread_id == std::this_thread::get_id());
    if (!D3dQuery || !D3dContext)
        return false;
    D3dContext->End(D3dQuery);
    D3dContext->Flush();
    while (S_OK != D3dContext->GetData(D3dQuery, nullptr, 0, 0)) {
        std::this_thread::yield(); // Optionally, yield while waiting
    }
    return true;
}

void Frame::recycle(Frame* p) {
    if (!p)
        return;
    if (!p->owner || !p->owner->recycle(p))
        delete p;
}

bool Capturer::InitD3DDevice() {
    assert (main_thread_id == std::this_thread::get_id());
    if (!D3dDevice) {
        HRESULT hr;
        CComPtr<IDXGIAdapter1> dxgiAdapter1;
        CComPtr<IDXGIFactory1> dxgiFactory1;
        hr = CreateDXGIFactory1(IID_PPV_ARGS(&dxgiFactory1));
        if (FAILED(hr)) {
            LOG(ERROR) << "Failed to create DXGI factory1: " << getErrorMessage(hr);
        } else {
            std::set<UINT> uniqueGPUs;
            LOG(INFO) << "DXGI adapters:";
            std::wstring forcedName = toUtf16(Cfg.getForcedDXGIDeviceName());
            int forcedId = Cfg.getForcedDXGIDeviceId();
            for (int i=0;; i++) {
                CComPtr<IDXGIAdapter1> dxgiAdapterTmp;
                hr = dxgiFactory1->EnumAdapters1(i, &dxgiAdapterTmp);
                if (FAILED(hr))
                    break;
                DXGI_ADAPTER_DESC1 desc {};
                dxgiAdapterTmp->GetDesc1(&desc);
                if (desc.Flags != DXGI_ADAPTER_FLAG_NONE)
                    continue;
                uniqueGPUs.insert(desc.DeviceId);
                bool forced = !dxgiAdapter1 && (forcedId == desc.DeviceId || forcedName == desc.Description);
                LOG_INFO(L"DXGI adapter[{}]: device id: {}, name: '{}'{}", i,
                                         desc.DeviceId, desc.Description,
                                         (forced ? L" (force use this device)" : L""));
                CComPtr<IDXGIOutput> dxgiOutput;
                for (unsigned o=0; ; o++) {
                    CComPtr<IDXGIOutput> output;
                    if (FAILED(dxgiAdapterTmp->EnumOutputs(o, &output)))
                        break;
                    DXGI_OUTPUT_DESC odesc {};
                    if (SUCCEEDED(output->GetDesc(&odesc)))
                        LOG_INFO(L"     output monitor {}", odesc.DeviceName);
                }

                if (forced)
                    dxgiAdapter1.Attach(dxgiAdapterTmp.Detach());
                else if (!dxgiAdapter1 && std::wstring(desc.Description).starts_with(L"Intel(R)"))
                    dxgiAdapter1.Attach(dxgiAdapterTmp.Detach());
            }
            numGPUs = uniqueGPUs.size();
        }
        static const D3D_FEATURE_LEVEL featureLevels[] = {
                D3D_FEATURE_LEVEL_11_1,
                D3D_FEATURE_LEVEL_11_0,
        };
        D3D_FEATURE_LEVEL featureLevel;
        CComPtr<ID3D11Device> d3dDeviceTmp;
        hr = D3D11CreateDevice(dxgiAdapter1, dxgiAdapter1 ? D3D_DRIVER_TYPE_UNKNOWN : D3D_DRIVER_TYPE_HARDWARE, nullptr,
                               D3D11_CREATE_DEVICE_BGRA_SUPPORT,
                               featureLevels, std::size(featureLevels), D3D11_SDK_VERSION,
                               &d3dDeviceTmp, &featureLevel, nullptr);
        if (SUCCEEDED(hr))
            hr = d3dDeviceTmp->QueryInterface(IID_PPV_ARGS(&D3dDevice));
        if (FAILED(hr)) {
            LOG(ERROR) << "Failed to create D3D11 device: " << getErrorMessage(hr);
        } else {
            D3dDevice->GetImmediateContext1(&D3dContext);
            if (useOpenCL()) {
                cv::ocl::setUseOpenCL(true);
                std::vector<cv::ocl::PlatformInfo> cl_platforms;
                cv::ocl::getPlatfomsInfo(cl_platforms);
                LOG(INFO) << "OpenCL platforms & devices:";
                for (int i = 0; i < cl_platforms.size(); i++) {
                    cv::ocl::PlatformInfo sdk = cl_platforms.at(i);
                    for (int j = 0; j < sdk.deviceNumber(); j++) {
                        cv::ocl::Device oclDevice;
                        sdk.getDevice(oclDevice, j);
                        LOG_INFO("    Device[{}/{}]: '{}', version: '{}' {}",
                                                 i, j, oclDevice.name(), oclDevice.version(),
                                                 (oclDevice.available() ? "(available)" : "(not available)"));
                    }
                }
                cv::ocl::Context& cl_context = cv::directx::ocl::initializeContextFromD3D11Device(D3dDevice);
                LOG_INFO("Using OpenCL device: name='{}', version='{}'",
                                         cl_context.device(0).name(), cl_context.device(0).version());
            } else {
                cv::ocl::setUseOpenCL(false);
                LOG(INFO) << "OpenCL: disabled";
            }
        }
    }
    if (!D3dQuery && D3dDevice) {
        D3D11_QUERY_DESC queryDesc = {};
        queryDesc.Query = D3D11_QUERY_EVENT;
        queryDesc.MiscFlags = 0;
        D3dDevice->CreateQuery(&queryDesc, &D3dQuery);
    }

    return bool(D3dDevice);
}

void Capturer::shutdown() {
    assert (main_thread_id == std::this_thread::get_id());
    if (useOpenCL())
        cv::directx::ocl::finish();
    D3dQuery.Release();
    D3dContext.Release();
    D3dDevice.Release();
    TheCapturer.reset();
}

void Capturer::resetEDCapturer() {
    assert (main_thread_id == std::this_thread::get_id());
    TheCapturer.reset();
}

Capturer* Capturer::getEDCapturer(HWND hwnd) {
    assert (main_thread_id == std::this_thread::get_id());
    if (!hwnd)
        return nullptr;
    if (TheCapturer) {
        if (TheCapturer->hWndED == hwnd)
            return TheCapturer.get();
        TheCapturer.reset();
    }

    HMONITOR hMonitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    if (!hMonitor) {
        LOG_ERROR("Could not get monitor handle.");
        return nullptr;
    }

    MONITORINFOEX monitorInfo;
    monitorInfo.cbSize = sizeof(monitorInfo);
    if (!GetMonitorInfo(hMonitor, &monitorInfo)) {
        LOG_ERROR("Could not get monitor information.");
        return nullptr;
    } else {
        LOG_DEBUG("Lookup capturer for monitor {}", toUtf8(monitorInfo.szDevice));
    }

    RECT windowRect;
    RECT captureRect;
    BOOL ok = hwnd && GetWindowRect(hwnd, &windowRect);
    if (!ok) {
        LOG_ERROR("Cannot get window for capturer");
        return nullptr;
    }
    bool fullscreen;
    {
        if (windowRect != monitorInfo.rcMonitor) {
            fullscreen = false;
            captureRect = windowRect;
            RECT clientRect;
            if (GetClientRect(hwnd, &clientRect)) {
                ClientToScreen(hwnd, (LPPOINT)&clientRect.left);
                ClientToScreen(hwnd, (LPPOINT)&clientRect.right);
                captureRect = clientRect;
            }
        } else {
            fullscreen = true;
            captureRect = monitorInfo.rcMonitor;
        }
    }
    // try WinRT capturer first
    if (!Cfg.isCapturerWinRTDisabled()) {
        auto c = std::unique_ptr<Capturer>(new CapturerWinRT(hMonitor, &monitorInfo));
        if (c->trySetup(hwnd, fromRECT(windowRect), fromRECT(captureRect))) {
            TheCapturer.swap(c);
            return TheCapturer.get();
        }
    }
    // otherwise try DXGI capturer
    if (!Cfg.isCapturerDXGIDisabled()) {
        auto c = std::unique_ptr<Capturer>(new CapturerDXGI(hMonitor, &monitorInfo));
        if (c->trySetup(hwnd, fromRECT(windowRect), fromRECT(captureRect))) {
            TheCapturer.swap(c);
            return TheCapturer.get();
        }
    }
    // fallback to Win32 (bitmap)
    if (!Cfg.isCapturerWin32Disabled()) {
        auto c = std::unique_ptr<Capturer>(new CapturerWin32(hMonitor, &monitorInfo));
        if (c->trySetup(hwnd, fromRECT(windowRect), fromRECT(captureRect))) {
            TheCapturer.swap(c);
            return TheCapturer.get();
        }
    }

    LOG_ERROR("Cannot find capturer for monitor {}", toUtf8(monitorInfo.szDevice));
    return nullptr;
}

Capturer::Capturer(HMONITOR hMonitor, LPMONITORINFOEX monitorInfoEx)
    : dpiScaleX{1.0}
    , dpiScaleY{1.0}
    , hMonitor(hMonitor)
    , hWndED(nullptr)
    , monitorInfo {}
    , titleHeight {}
    , borderWidth {}
{
    if (monitorInfoEx) {
        memcpy(&monitorInfo, monitorInfoEx, sizeof(monitorInfo));
    } else {
        memset(&monitorInfo, 0, sizeof(monitorInfo));
        monitorInfo.cbSize = sizeof(monitorInfo);
        monitorInfo.rcMonitor = RECT(0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN));
        monitorInfo.rcWork = monitorInfo.rcMonitor;
    }
    monitorVirtRect = {
            monitorInfo.rcMonitor.left,
            monitorInfo.rcMonitor.top,
            monitorInfo.rcMonitor.right - monitorInfo.rcMonitor.left,
            monitorInfo.rcMonitor.bottom - monitorInfo.rcMonitor.top
    };
    windowVirtRect = monitorVirtRect;
    captureVirtRect = monitorVirtRect;
    UINT dpiX = USER_DEFAULT_SCREEN_DPI;
    UINT dpiY = USER_DEFAULT_SCREEN_DPI;
    if (SUCCEEDED(GetDpiForMonitor(hMonitor, MDT_EFFECTIVE_DPI, &dpiX, &dpiY))) {
        if (dpiX != USER_DEFAULT_SCREEN_DPI || dpiY != USER_DEFAULT_SCREEN_DPI) {
            dpiScaleX = double(dpiX) / USER_DEFAULT_SCREEN_DPI;
            dpiScaleY = double(dpiY) / USER_DEFAULT_SCREEN_DPI;
        }
    }
}

cv::Rect Capturer::getCaptureRect() {
    return captureVirtRect - monitorVirtRect.tl();
}

cv::Rect Capturer::getMonitorVirtualRect() {
    return monitorVirtRect;
}