//
// Created by mkizub on 15.06.2025.
//

#include "../pch.h"

#include "UIDebug.h"

#include <d2d1.h>

static std::weak_ptr<UIDebug> gInstance;
static const wchar_t* gWindowClass = L"DebugWindowClass";
static const wchar_t* gWindowName = L"EDRobot Debug";

bool UIDebug::initialize() {
    return UIWindow::registerClass(gWindowClass, true);
}

std::shared_ptr<UIDebug> UIDebug::getInstance() {
    std::shared_ptr<UIDebug> alive = gInstance.lock();
    if (!alive) {
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
    if (pBitmap) {
        pBitmap->Release();
        pBitmap = nullptr;
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


void UIDebug::showImage(const cv::Mat& image) {
    if (image.type() != CV_8UC4)
        return;
    {
        std::unique_lock<std::mutex> lock(mMutex);
        cv::resize(image, cvImage, cvImage.size(), 0, 0);
        imageUpdated = true;
    }
    InvalidateRect(hWnd, nullptr, TRUE);
    UpdateWindow(hWnd);
}

bool UIDebug::createWindow() {
    DWORD dwExStyle = 0;
    DWORD dwStyle = WS_OVERLAPPEDWINDOW;

    HWND hWndED = FindWindow(Master::ED_WINDOW_CLASS, Master::ED_WINDOW_NAME);
    RECT windowRect;
    if (GetWindowRect(hWndED, &windowRect)) {
        RECT clientRect;
        if (GetClientRect(hWndED, &clientRect)) {
            ClientToScreen(hWndED, (LPPOINT)&clientRect.left);
            ClientToScreen(hWndED, (LPPOINT)&clientRect.right);
            windowRect = {0, 0, (clientRect.right-clientRect.left)/2, (clientRect.bottom-clientRect.top)/2};
        }
    } else {
        windowRect = {0, 0, 1920/2, 1080/2};
    }
    AdjustWindowRectEx(&windowRect, dwStyle, TRUE, dwExStyle);
    int width = windowRect.right - windowRect.left;
    int height = windowRect.bottom - windowRect.top;
    outputSize = cv::Size(width, height);
    cvImage = cv::Mat(height, width, CV_8UC4);

    UIWindow::createWindow(gWindowName, dwExStyle, dwStyle, ALIGN_TOP|ALIGN_RIGHT, {0, 0}, {width, height});
    if (!hWnd)
        return false;

    return true;
}

void UIDebug::windowCreated() {
    RECT clientRect;
    GetClientRect(hWnd, &clientRect);

    HRESULT hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &pD2DFactory);
    if (FAILED(hr)) {
        LOG(ERROR) << "Failed to create ID2D1Factory";
        return;
    }

    D2D1_SIZE_U size = D2D1::SizeU(clientRect.right - clientRect.left, clientRect.bottom - clientRect.top);

    hr = pD2DFactory->CreateHwndRenderTarget(
            D2D1::RenderTargetProperties(),
            D2D1::HwndRenderTargetProperties(hWnd, size),
            &pRenderTarget);

    if (FAILED(hr)) {
        LOG(ERROR) << "Failed to create ID2D1HwndRenderTarget";
        return;
    }

    pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Blue), &pBrush);
    D2D1_BITMAP_PROPERTIES bitmapProperties {
        .pixelFormat = {.format=DXGI_FORMAT_R8G8B8A8_UNORM, .alphaMode=D2D1_ALPHA_MODE_IGNORE},
        .dpiX = 96,
        .dpiY = 96,
    };
    hr = pRenderTarget->CreateBitmap(size, nullptr, cvImage.step, &bitmapProperties, &pBitmap);
    if (FAILED(hr)) {
        LOG(ERROR) << "Failed to create ID2D1Bitmap";
        return;
    }
}

INT_PTR UIDebug::onMessage(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    return UIWindow::onMessage(hWnd, message, wParam, lParam);
}

void UIDebug::onPaint() {
    if (imageUpdated) {
        std::unique_lock<std::mutex> lock(mMutex);
        imageUpdated = false;
        D2D1_RECT_U dstRect {0, 0, (unsigned)outputSize.width, (unsigned)outputSize.height};
        HRESULT hr = pBitmap->CopyFromMemory(&dstRect, cvImage.data, cvImage.step);
        LOG_IF(FAILED(hr),ERROR) << "ID2D1Bitmap CopyFromMemory error: " << getErrorMessage();
    }

    if (pRenderTarget) {
        pRenderTarget->BeginDraw();
        pRenderTarget->Clear(D2D1::ColorF(D2D1::ColorF::White));

        D2D1_RECT_F bitmapRect = D2D1::RectF(0, 0, outputSize.width, outputSize.height);
        pRenderTarget->DrawBitmap(pBitmap, &bitmapRect, 1.0, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR, &bitmapRect);
        D2D1_RECT_F tmpRect = D2D1::RectF(250, 100, 450, 300);
        D2D1_ROUNDED_RECT roundedRect = D2D1::RoundedRect(tmpRect, 10.f, 10.f);
        pRenderTarget->DrawRoundedRectangle(roundedRect, pBrush, 5.0f, nullptr);

        pRenderTarget->EndDraw();
    }
}


