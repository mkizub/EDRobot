//
// Created by mkizub on 15.06.2025.
//

#include "../pch.h"

#include "UIDebug.h"

#include <d2d1_3.h>

static std::weak_ptr<UIDebug> gInstance;
static const wchar_t* gWindowClass = L"DebugWindowClass";
static const wchar_t* gWindowName = L"EDRobot Debug";

bool UIDebug::initialize() {
    return UIWindow::registerClass(gWindowClass, true, true);
}

std::shared_ptr<UIDebug> UIDebug::getInstance(bool create) {
    std::shared_ptr<UIDebug> alive = gInstance.lock();
    if (!alive && create) {
        alive = std::shared_ptr<UIDebug>(new UIDebug());
        gInstance = alive;
    }
    return alive;
}

UIDebug::UIDebug() : UIWindow(gWindowClass) {
}

UIDebug::~UIDebug() {
    onDestroy();
}

void UIDebug::onDestroy() {
    if (pGameBitmap) {
        pGameBitmap->Release();
        pGameBitmap = nullptr;
    }
    if (pOverlayBitmap) {
        pOverlayBitmap->Release();
        pOverlayBitmap = nullptr;
    }
    if (pRenderTarget) {
        pRenderTarget->Release();
        pRenderTarget = nullptr;
    }
    if (pD2DFactory) {
        pD2DFactory->Release();
        pD2DFactory = nullptr;
    }
    if (pBrush) {
        pBrush->Release();
        pBrush = nullptr;
    }
}


void UIDebug::setGameImage(const cv::Mat& image) {
    {
        std::scoped_lock<std::recursive_mutex> lock(mMutex);
        if (image.type() != CV_8UC4 || image.empty())
            return;
        if (mGameSize != image.size()) {
            mGameSize = image.size();
            resizeWindow();
        }
        assert (!mOutputSize.empty());
        cv::resize(image, mGameImage, mOutputSize, 0, 0);
        mGameImageUpdated = true;
    }

    InvalidateRect(hWnd, nullptr, TRUE);
    //PostMessage(hWnd, WM_PAINT, 0, 0);
}

void UIDebug::setDebugOverlay(const cv::Mat& overlay) {
    {
        std::scoped_lock<std::recursive_mutex> lock(mMutex);
        if (overlay.type() != CV_8UC4 || overlay.empty()) {
            mDebugOverlayPresent = false;
        } else {
            assert (!mOutputSize.empty());
            // TODO: do not resize, let pRenderTarget resizes in hardware
            cv::resize(overlay, mDebugOverlay, mOutputSize, 0, 0);
            mDebugOverlayUpdated = true;
            mDebugOverlayPresent = true;
        }
    }

    InvalidateRect(hWnd, nullptr, TRUE);
    //PostMessage(hWnd, WM_PAINT, 0, 0);
}

const DWORD dwExStyle = 0;
const DWORD dwStyle = WS_OVERLAPPEDWINDOW;

struct MonitorChoice {
    HMONITOR hEDMonitor;
    HMONITOR hDebugMonitor;
    int debugMonitorSize;
};
static BOOL CALLBACK MonitorEnumProc(HMONITOR hMonitor, HDC hdcMonitor, LPRECT lprcMonitor, LPARAM dwData) {
    UNREFERENCED_PARAMETER(lprcMonitor);
    MonitorChoice* mc = (MonitorChoice*)dwData;
    if (hMonitor == mc->hEDMonitor)
        return TRUE;
    MONITORINFOEX monitorInfoEx;
    monitorInfoEx.cbSize = sizeof(MONITORINFOEX);
    if (GetMonitorInfo(hMonitor, &monitorInfoEx)) {
        int w = monitorInfoEx.rcMonitor.right - monitorInfoEx.rcMonitor.left;
        int h = monitorInfoEx.rcMonitor.bottom - monitorInfoEx.rcMonitor.top;
        int sz = w*h;
        if (sz > mc->debugMonitorSize) {
            mc->hDebugMonitor = hMonitor;
            mc->debugMonitorSize = sz;
        }
    }
    return TRUE;
}

bool UIDebug::createWindow() {
    mGameSize = Cfg.getCroppedDisplayRect().size();
    HMONITOR hDebugMonitor {};
    if (GetSystemMetrics(SM_CMONITORS) > 1) {
        HWND hWndED = FindWindow(Master::ED_WINDOW_CLASS, Master::ED_WINDOW_NAME);
        if (hWndED) {
            HMONITOR hEDMonitor = MonitorFromWindow(hWndED, MONITOR_DEFAULTTONEAREST);
            MonitorChoice mc {hEDMonitor};
            EnumDisplayMonitors(NULL, NULL, MonitorEnumProc, (LPARAM)&mc);
            hDebugMonitor = mc.hDebugMonitor;
        }
    }
    mOutputSize.width = mGameSize.width * mOutputScale;
    mOutputSize.height = mGameSize.height * mOutputScale;
    RECT windowRect {0, 0, mOutputSize.width, mOutputSize.height};

    AdjustWindowRectEx(&windowRect, dwStyle, FALSE, dwExStyle);
    int adjW = windowRect.right - windowRect.left;
    int adjH = windowRect.bottom - windowRect.top;
    UIWindow::createWindow(hDebugMonitor, gWindowName, dwExStyle, dwStyle, ALIGN_TOP|ALIGN_RIGHT, {0, 0}, {adjW, adjH});
    if (!hWnd)
        return false;

    return true;
}

void UIDebug::windowCreated() {
    float dpi = GetDpiForWindow(hWnd);
    mDPIScale = dpi / USER_DEFAULT_SCREEN_DPI;

    HRESULT hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &pD2DFactory);
    if (FAILED(hr)) {
        LOG(ERROR) << "Failed to create ID2D1Factory";
        return;
    }
    resizeWindow();
}

void UIDebug::resizeWindow() {
    int clientWidth = mGameSize.width * mOutputScale;
    int clientHeight = mGameSize.height * mOutputScale;
    mOutputSize = {clientWidth, clientHeight};
    mGameImage = cv::Mat(mOutputSize, CV_8UC4, cv::Vec4b::ones());
    mDebugOverlay = cv::Mat(mOutputSize, CV_8UC4, cv::Vec4b::zeros());

    LOG(INFO) << "Resizing debug window: " << mOutputSize;

    if (pRenderTarget && pRenderTarget->GetSize().width == clientWidth && pRenderTarget->GetSize().width == clientHeight)
        return;

    if (pGameBitmap) {
        pGameBitmap->Release();
        pGameBitmap = nullptr;
    }
    if (pOverlayBitmap) {
        pOverlayBitmap->Release();
        pOverlayBitmap = nullptr;
    }

    D2D1_SIZE_U size = D2D1::SizeU(clientWidth, clientHeight);
    HRESULT hr;
    if (pRenderTarget) {
        hr = pRenderTarget->Resize(size);
        if (FAILED(hr)) {
            LOG(ERROR) << "Failed to resize ID2D1HwndRenderTarget" << getErrorMessage(hr);
            pRenderTarget->Release();
            pRenderTarget = nullptr;
        }
    }
    if (!pRenderTarget) {
        D2D1_RENDER_TARGET_PROPERTIES rtProps = {
                .type =  D2D1_RENDER_TARGET_TYPE_DEFAULT,
                .pixelFormat = D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED),
                .dpiX = USER_DEFAULT_SCREEN_DPI,
                .dpiY = USER_DEFAULT_SCREEN_DPI,
                .usage = D2D1_RENDER_TARGET_USAGE_GDI_COMPATIBLE,
                .minLevel = D2D1_FEATURE_LEVEL_DEFAULT,
        };
        hr = pD2DFactory->CreateHwndRenderTarget(rtProps, D2D1::HwndRenderTargetProperties(hWnd, size), &pRenderTarget);
        if (FAILED(hr)) {
            LOG(ERROR) << "Failed to create ID2D1HwndRenderTarget: " << getErrorMessage(hr);
            return;
        }
    }

    D2D1_BITMAP_PROPERTIES bitmapProperties {
        .pixelFormat = {.format=DXGI_FORMAT_B8G8R8X8_UNORM, .alphaMode=D2D1_ALPHA_MODE_IGNORE},
        .dpiX = 96,
        .dpiY = 96,
    };
    hr = pRenderTarget->CreateBitmap(size, mGameImage.data, mGameImage.step, &bitmapProperties, &pGameBitmap);
    LOG_IF(FAILED(hr),ERROR) << "Failed to create ID2D1Bitmap: " << getErrorMessage(hr);

    bitmapProperties.pixelFormat = {.format=DXGI_FORMAT_B8G8R8A8_UNORM, .alphaMode=D2D1_ALPHA_MODE_PREMULTIPLIED};
    hr = pRenderTarget->CreateBitmap(size, mDebugOverlay.data, mDebugOverlay.step, &bitmapProperties, &pOverlayBitmap);
    LOG_IF(FAILED(hr),ERROR) << "Failed to create ID2D1Bitmap for debug overlay: " << getErrorMessage(hr);

//    pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Blue), &pBrush);

    MONITORINFOEX monitorInfo;
    monitorInfo.cbSize = sizeof(monitorInfo);
    HMONITOR hCurrMonitor = MonitorFromWindow(hWnd, MONITOR_DEFAULTTOPRIMARY);
    GetMonitorInfo(hCurrMonitor, &monitorInfo);
    if (hCurrMonitor != hMonitor) {
        hMonitor = hCurrMonitor;
        mMonitorFullRect = fromRECT(monitorInfo.rcMonitor);
        mMonitorWorkRect = fromRECT(monitorInfo.rcWork);
        float dpi = GetDpiForWindow(hWnd);
        mDPIScale = dpi / USER_DEFAULT_SCREEN_DPI;
    }

    RECT windowRect;
    RECT clientRect;
    GetWindowRect(hWnd, &windowRect);
    GetClientRect(hWnd, &clientRect);
    clientRect.right = clientRect.left + clientWidth;
    clientRect.bottom = clientRect.top + clientHeight;
    AdjustWindowRectEx(&clientRect, dwStyle, FALSE, dwExStyle);
    cv::Rect wr = fromRECT(clientRect) + fromRECT(windowRect).tl();
    fitToMonitor(wr);
    SetWindowPos(hWnd, nullptr, wr.x, wr.y, wr.width, wr.height,
                 SWP_ASYNCWINDOWPOS/*|SWP_NOACTIVATE|SWP_NOOWNERZORDER|SWP_NOZORDER|SWP_NOREDRAW*/);
}

INT_PTR UIDebug::onMessage(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    if (message == WM_ERASEBKGND)
        return 0;
    if (message == WM_SIZING)
        return onSizing((LPRECT)lParam, (UINT)wParam);
    if (message == WM_EXITSIZEMOVE && wasResized) {
        resizeWindow();
        return 0;
    }
    return UIWindow::onMessage(hWnd, message, wParam, lParam);
}

INT_PTR UIDebug::onSizing(LPRECT pRect, UINT edge) {
    RECT pad {};
    AdjustWindowRectEx(&pad, dwStyle, FALSE, dwExStyle);
    RECT cRect {pRect->left - pad.left, pRect->top - pad.top,
                pRect->right - pad.right, pRect->bottom - pad.bottom };

    int newWidth = cRect.right - cRect.left;
    int newHeight = cRect.bottom - cRect.top;

    // Calculate the current aspect ratio
    double gameAspectRatio = static_cast<double>(mGameSize.width) / mGameSize.height;
    double currentAspectRatio = static_cast<double>(newWidth) / newHeight;

    // Adjust dimensions to maintain aspect ratio
    if (currentAspectRatio != gameAspectRatio) {
        if (edge == WMSZ_LEFT || edge == WMSZ_RIGHT || edge == WMSZ_TOPLEFT || edge == WMSZ_TOPRIGHT || edge == WMSZ_BOTTOMLEFT || edge == WMSZ_BOTTOMRIGHT)
            newHeight = static_cast<int>(newWidth / gameAspectRatio); // If width is being adjusted, calculate new height
        else if (edge == WMSZ_TOP || edge == WMSZ_BOTTOM)
            newWidth = static_cast<int>(newHeight * gameAspectRatio); // If height is being adjusted, calculate new width

        // Update the RECT structure with the adjusted dimensions
        if (edge == WMSZ_LEFT || edge == WMSZ_TOPLEFT || edge == WMSZ_BOTTOMLEFT)
            cRect.left = cRect.right - newWidth;
        else
            cRect.right = cRect.left + newWidth;

        if (edge == WMSZ_TOP || edge == WMSZ_TOPLEFT || edge == WMSZ_TOPRIGHT)
            cRect.top = cRect.bottom - newHeight;
        else
            cRect.bottom = cRect.top + newHeight;
    }
    mOutputScale = float(cRect.right - cRect.left) / mGameSize.width;
    pRect->left = cRect.left + pad.left;
    pRect->top = cRect.top + pad.top;
    pRect->right = cRect.right + pad.right;
    pRect->bottom = cRect.bottom + pad.bottom;
    wasResized = true;
    return TRUE; // Indicate that the message has been handled
}

void UIDebug::onPaint() {
    {
        std::scoped_lock<std::recursive_mutex> lock(mMutex);
        if (mGameImageUpdated) {
            mGameImageUpdated = false;
            D2D1_RECT_U dstRect{0, 0, (unsigned) mGameImage.cols, (unsigned) mGameImage.rows};
            HRESULT hr = pGameBitmap->CopyFromMemory(&dstRect, mGameImage.data, mGameImage.step);
            LOG_IF(FAILED(hr), ERROR) << "ID2D1Bitmap CopyFromMemory error: " << getErrorMessage();
        }
        if (mDebugOverlayUpdated) {
            mDebugOverlayUpdated = false;
            D2D1_RECT_U dstRect{0, 0, (unsigned) mDebugOverlay.cols, (unsigned) mDebugOverlay.rows};
            HRESULT hr = pOverlayBitmap->CopyFromMemory(&dstRect, mDebugOverlay.data, mDebugOverlay.step);
            LOG_IF(FAILED(hr), ERROR) << "ID2D1Bitmap CopyFromMemory error: " << getErrorMessage();
        }
    }

    if (pRenderTarget) {
        pRenderTarget->BeginDraw();
        pRenderTarget->Clear(D2D1::ColorF(D2D1::ColorF::White));

        D2D1_RECT_F bitmapRect = D2D1::RectF(0, 0, mOutputSize.width, mOutputSize.height);
        pRenderTarget->DrawBitmap(pGameBitmap, &bitmapRect, 1.0, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR, &bitmapRect);
        if (mDebugOverlayPresent)
            pRenderTarget->DrawBitmap(pOverlayBitmap, &bitmapRect, 1.0, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR, &bitmapRect);
        //D2D1_RECT_F tmpRect = D2D1::RectF(250, 100, 450, 300);
        //D2D1_ROUNDED_RECT roundedRect = D2D1::RoundedRect(tmpRect, 10.f, 10.f);
        //pRenderTarget->DrawRoundedRectangle(roundedRect, pBrush, 5.0f, nullptr);

        pRenderTarget->EndDraw();
    }
}


