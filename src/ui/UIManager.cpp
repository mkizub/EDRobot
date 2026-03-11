//
// Created by mkizub on 11.06.2025.
//

#include "../pch.h"

#include "../../ui/resource.h"
#include <commctrl.h>

#include "UIManager.h"
#include "UIMainDialog.h"
#include "UIControl.h"
#include "UIControlDialog.h"
#include "UIShowCargo.h"
#include "UIStartupDialog.h"
#include "UIToast.h"
#include "UIAddTask.h"
#include "UISelectRect.h"
#include "UIDebug.h"

#pragma comment(linker,"\"/manifestdependency:type='win32' \
    name='Microsoft.Windows.Common-Controls' version='6.0.0.0' \
    processorArchitecture='amd64' publicKeyToken='6595b64144ccf1df' language='*'\"")

UIManager &UIManager::getInstance() {
    static UIMainDialog mainDialog;
    static UIManager uiManager(mainDialog);
    return uiManager;
}

UIManager::UIManager(UIMainDialog& main)
    : uiMain(main)
{
}

UIManager::~UIManager() {
    SendMessage(uiMain.hwnd(), WM_CLOSE, 0, 0);
    if (uiThread.joinable()) {
        uiThread.join();
    }
}

bool UIManager::initialize() {
    bool ok = true;

    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_STANDARD_CLASSES; // Or other specific classes
    InitCommonControlsEx(&icex);

    ok &= UIToast::initialize();
    ok &= UISelectRect::initialize();
    ok &= UIDebug::initialize();
    UIManager& mgr = getInstance();
    mgr.uiThread = std::thread(&UIManager::uiThreadLoop, &mgr);
    return ok;
}

bool UIManager::shutdown() {
    getInstance().uiMain.run_thread_ui([](){
        DestroyWindow(getInstance().uiMain.hwnd());
    });
//    if (auto dlg = UIShowCargo::getInstance()) {
//        dlg->run_thread_ui([dlg](){
//            DestroyWindow(dlg->hwnd());
//        });
//    }
    return true;
}

void UIManager::uiThreadLoop() {
    SetThreadDescription(GetCurrentThread(), L"UIManager main thread");
    SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    HINSTANCE hInstance = GetModuleHandle(nullptr);
    uiMain.winmain_run(hInstance, SW_SHOW);
}

bool UIManager::showStartupDialog(const std::string &message, const std::string& latest_version, const std::string& latest_url) {
//    UIStartupDialog dlg(message, latest_version, latest_url);
//    return dlg.show();
    UIManager& mgr = getInstance();
    return mgr.uiMain.show_startup(message, latest_version, latest_url);
}

bool UIManager::showMainDialog() {
    UIManager& mgr = getInstance();
    return mgr.uiMain.show();
}

bool UIManager::hideMainDialog(bool force) {
    UIManager& mgr = getInstance();
    return mgr.uiMain.hide(force);
}

bool UIManager::updateCargoDialog() {
//    auto dlg = UIShowCargo::getInstance();
//    if (!dlg)
//        return false;
//    return dlg->updateCargo();
    return true;
}

bool UIManager::showToast(const std::string &title, const std::string &text) {
//    std::shared_ptr<UIToast> wnd = UIToast::getInstance();
//    if (!wnd)
//        return false;
//    wnd->showText(toUtf16(title), toUtf16(text));
    return true;
}

bool UIManager::askSelectRectWindow() {
    std::shared_ptr<UISelectRect> wnd = UISelectRect::getInstance();
    if (!wnd)
        return false;
    wnd->show();
    return true;
}

bool UIManager::hasDebugWindow() {
    std::shared_ptr<UIDebug> wnd = UIDebug::getInstance(false);
    return bool(wnd);
}

bool UIManager::showDebugWindow() {
    std::shared_ptr<UIDebug> wnd = UIDebug::getInstance(true);
    if (!wnd)
        return false;
    wnd->show();
    return true;
}

bool UIManager::postToDebugWindow(const XMat& image) {
    std::shared_ptr<UIDebug> wnd = UIDebug::getInstance(false);
    if (!wnd)
        return false;
    // TODO: sharing UMat is a bad idea
    wnd->setGameImage(toMat(image));
    return true;
}

bool UIManager::postToDebugWindow(const XMat& image, const cv::Mat& overlay) {
    std::shared_ptr<UIDebug> wnd = UIDebug::getInstance(false);
    if (!wnd)
        return false;
    // TODO: sharing UMat is a bad idea
    wnd->setGameImage(toMat(image));
    wnd->setDebugOverlay(overlay);
    return true;
}


std::map<std::string,const std::string> UIManager::iconSVG
{
        {"task-new",
         R"SVG(<?xml version="1.0" encoding="UTF-8"?>
<svg width="800px" height="800px" viewBox="0 0 512 512" version="1.1" xmlns="http://www.w3.org/2000/svg">
 <g id="Page-1" stroke="none" stroke-width="1" fill="none" fill-rule="evenodd">
  <g id="uncollapse" fill="#000000" transform="translate(64.000000, 64.000000)">
   <path d="M213.333333,1.42108547e-14 L213.333,170.666 L384,170.666667 L384,213.333333 L213.333,213.333 L213.333333,384 L170.666667,384 L170.666,213.333 L1.42108547e-14,213.333333 L1.42108547e-14,170.666667 L170.666,170.666 L170.666667,1.42108547e-14 L213.333333,1.42108547e-14 Z"/>
  </g>
 </g>
</svg>)SVG"
        },

        {"task-pause",
         R"SVG(<?xml version="1.0" encoding="utf-8"?>
<svg fill="#000000" width="800px" height="800px" viewBox="0 0 512 512" xmlns="http://www.w3.org/2000/svg">
 <title>ionicons-v5-c</title>
 <path d="M224,432H144V80h80Z"/>
 <path d="M368,432H288V80h80Z"/>
</svg>)SVG"
        },

        {"task-stop",
         R"SVG(<?xml version="1.0" encoding="utf-8"?>
<svg fill="#000000" version="1.1" baseProfile="tiny" id="Layer_1" xmlns="http://www.w3.org/2000/svg" width="800px" height="800px" viewBox="0 0 42 42" xml:space="preserve">
  <path fill-rule="evenodd" d="M21.002,26.588l10.357,10.604c1.039,1.072,1.715,1.083,2.773,0l2.078-2.128
	c1.018-1.042,1.087-1.726,0-2.839L25.245,21L36.211,9.775c1.027-1.055,1.047-1.767,0-2.84l-2.078-2.127
	c-1.078-1.104-1.744-1.053-2.773,0L21.002,15.412L10.645,4.809c-1.029-1.053-1.695-1.104-2.773,0L5.794,6.936
	c-1.048,1.073-1.029,1.785,0,2.84L16.759,21L5.794,32.225c-1.087,1.113-1.029,1.797,0,2.839l2.077,2.128
	c1.049,1.083,1.725,1.072,2.773,0L21.002,26.588z"/>
</svg>)SVG"
        },

        {"task-resume",
         R"SVG(<?xml version="1.0" encoding="utf-8"?>
<svg width="800px" height="800px" viewBox="0 0 76 76" xmlns="http://www.w3.org/2000/svg" version="1.1" baseProfile="full" enable-background="new 0 0 76.00 76.00" xml:space="preserve">
  <path fill="#000000" fill-opacity="1" stroke-width="0.2" stroke-linejoin="round" d="M 38,22.1667L 58.5832,37.6043L 58.5832,38.7918L 38,53.8333L 38,22.1667 Z M 33.25,22.1667L 33.25,53.8333L 26.9167,53.8333L 26.9167,22.1667L 33.25,22.1667 Z "/>
</svg>)SVG"
        },

        {"task-repeat",
         R"SVG(<?xml version="1.0" encoding="utf-8"?>
<svg width="800px" height="800px" viewBox="0 0 24 24" xmlns="http://www.w3.org/2000/svg">
 <g>
  <path fill="none" d="M0 0h24v24H0z"/>
  <path d="M6 4h15a1 1 0 0 1 1 1v7h-2V6H6v3L1 5l5-4v3zm12 16H3a1 1 0 0 1-1-1v-7h2v6h14v-3l5 4-5 4v-3z"/>
 </g>
</svg>)SVG"
        },

        {"cargo-add",
                R"SVG(<?xml version="1.0" encoding="UTF-8"?>
<svg width="800px" height="800px" viewBox="0 0 512 512" version="1.1" xmlns="http://www.w3.org/2000/svg">
 <g id="Page-1" stroke="none" stroke-width="1" fill="none" fill-rule="evenodd">
  <g id="uncollapse" fill="#000000" transform="translate(64.000000, 64.000000)">
   <path d="M213.333333,1.42108547e-14 L213.333,170.666 L384,170.666667 L384,213.333333 L213.333,213.333 L213.333333,384 L170.666667,384 L170.666,213.333 L1.42108547e-14,213.333333 L1.42108547e-14,170.666667 L170.666,170.666 L170.666667,1.42108547e-14 L213.333333,1.42108547e-14 Z"/>
  </g>
 </g>
</svg>)SVG"
        },

        {"cargo-save",
                R"SVG(<?xml version="1.0" encoding="utf-8"?>
<svg width="800px" height="800px" viewBox="0 0 16 16" xmlns="http://www.w3.org/2000/svg" id="svg2" version="1.1">
  <g id="layer1" transform="matrix(.875 0 0 .875 -1.625 -903.192)">
    <g id="layer1-7">
      <path id="path821" d="M3 1033.362v16h16v-13.714l-2.286-2.286zm1.143 1.143H7.57v4.571h6.858v-4.571h1.142l2.286 2.286v11.428h-1.143v-6.857H5.286v6.857H4.143zm4.571 0h3.429v3.429H8.714zm-2.285 8h9.142v5.714H6.43z" style="fill:#373737;fill-opacity:1;stroke:none;stroke-width:1.14285707px;stroke-linecap:butt;stroke-linejoin:miter;stroke-opacity:1"/>
    </g>
  </g>
</svg>)SVG"
        },

};

HBITMAP UIManager::makeIconBitmap(const std::string& icon, int size) {
    UIManager& mgr = UIManager::getInstance();

    auto key = std::format("{}:{}", icon, size);

    if (auto it_bm = mgr.iconBitmaps.find(key); it_bm != mgr.iconBitmaps.end())
        return it_bm->second;

    auto it_svg = iconSVG.find(icon);
    if (it_svg == iconSVG.end())
        throw std::runtime_error("Icon not known");
    HBITMAP hBitmap = mgr.svgToBitmap(size, it_svg->second);
    mgr.iconBitmaps.emplace(key, hBitmap);
    return hBitmap;
}


#include <d2d1_3.h>
#include <atlbase.h>

HBITMAP UIManager::svgToBitmap(int iconSize, const std::string& svg_data) {
    static const float sc_svgSize = 800.0f;

    // initialize Direct2D
    D2D1_FACTORY_OPTIONS options = { D2D1_DEBUG_LEVEL_INFORMATION }; // remove this in release
    CComPtr<ID2D1Factory> factory;
    auto hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, IID_ID2D1Factory, &options, (void**)&factory);

    // create an in-memory bitmap
    BITMAPINFO bitmapInfo = {};
    bitmapInfo.bmiHeader.biSize = sizeof(bitmapInfo.bmiHeader);
    bitmapInfo.bmiHeader.biWidth = iconSize;
    bitmapInfo.bmiHeader.biHeight = iconSize;
    bitmapInfo.bmiHeader.biPlanes = 1;
    bitmapInfo.bmiHeader.biBitCount = 32;
    bitmapInfo.bmiHeader.biCompression = BI_RGB;

    void* bits = 0;
    HBITMAP bitmap = CreateDIBSection(nullptr, &bitmapInfo, DIB_RGB_COLORS, &bits, nullptr, 0);

    // create a DC render target
    CComPtr<ID2D1DCRenderTarget> target;
    D2D1_RENDER_TARGET_PROPERTIES props = {};
    props.pixelFormat.format = DXGI_FORMAT_B8G8R8A8_UNORM;
    props.pixelFormat.alphaMode = D2D1_ALPHA_MODE_PREMULTIPLIED;
    hr = factory->CreateDCRenderTarget(&props, &target);

    // create a DC, select bitmap and bind DC
    HDC hdc = CreateCompatibleDC(nullptr);
    auto old = SelectObject(hdc, bitmap);
    RECT rc = { 0, 0, iconSize, iconSize };
    hr = target->BindDC(hdc, &rc);

    // this requires Windows 10 1703
    CComPtr<ID2D1DeviceContext5> dc;
    hr = target->QueryInterface(&dc);

    CComPtr<IStream> svgStream(SHCreateMemStream((const BYTE*)svg_data.data(), (UINT)svg_data.size()));

    // open the SVG as a document
    CComPtr<ID2D1SvgDocument> svg;
    D2D1_SIZE_F size = { sc_svgSize, sc_svgSize };
    hr = dc->CreateSvgDocument(svgStream, size, &svg);

    // draw it on the render target
    target->BeginDraw();
    target->Clear(D2D1::ColorF(D2D1::ColorF::White, 0.f));

    D2D1_MATRIX_3X2_F transform = D2D1::Matrix3x2F::Scale(
            iconSize / sc_svgSize,
            iconSize / sc_svgSize,
            {0.f, 0.f}
    );
    dc->SetTransform(transform);

    dc->DrawSvgDocument(svg);
    hr = target->EndDraw();

    //cv::Mat mat(iconSize, iconSize, CV_8UC4);
    //GetDIBits(hdc, bitmap, 0, iconSize, mat.data, (BITMAPINFO*)&bitmapInfo, DIB_RGB_COLORS);

    // cleanup, etc.
    SelectObject(hdc, old);

    return bitmap;
}

