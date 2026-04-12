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
#include "UIEditTask.h"
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

bool UIManager::hideMainDialog() {
    UIManager& mgr = getInstance();
    return mgr.uiMain.hide(true);
}

bool UIManager::toggleMainDialog() {
    UIManager& mgr = getInstance();
    return mgr.uiMain.toggle();
}

bool UIManager::updateCommander() {
    UIManager& mgr = getInstance();
    mgr.uiMain.updateCommander();
    return true;
}

bool UIManager::showToast(const std::string &title, const std::string &text) {
    std::shared_ptr<UIToast> wnd = UIToast::getInstance();
    if (!wnd)
        return false;
    wnd->showText(toUtf16(title), toUtf16(text));
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
        {"task-run",
                R"SVG(<?xml version="1.0" encoding="utf-8"?>
<svg width="800px" height="800px" viewBox="0 0 16 16" xmlns="http://www.w3.org/2000/svg" fill="#000000">
  <path d="M2.78 2L2 2.41v12l.78.42 9-6V8l-9-6zM3 13.48V3.35l7.6 5.07L3 13.48z"/>
  <path fill-rule="evenodd" clip-rule="evenodd" d="M6 14.683l8.78-5.853V8L6 2.147V3.35l7.6 5.07L6 13.48v1.203z"/>
</svg>)SVG"
        },

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
<svg width="800px" height="800px" viewBox="0 0 24 24" xmlns="http://www.w3.org/2000/svg">
  <path fill="none" stroke="#000000" stroke-width="2" d="M1,20 L6,20 L6,4 L1,4 L1,20 Z M11,19.0000002 L22,12 L11,5 L11,19.0000002 Z"/>
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

        {"icon-add",
                R"SVG(<?xml version="1.0" encoding="UTF-8"?>
<svg width="800px" height="800px" viewBox="0 0 512 512" version="1.1" xmlns="http://www.w3.org/2000/svg">
 <g id="Page-1" stroke="none" stroke-width="1" fill="none" fill-rule="evenodd">
  <g id="uncollapse" fill="#000000" transform="translate(64.000000, 64.000000)">
   <path d="M213.333333,1.42108547e-14 L213.333,170.666 L384,170.666667 L384,213.333333 L213.333,213.333 L213.333333,384 L170.666667,384 L170.666,213.333 L1.42108547e-14,213.333333 L1.42108547e-14,170.666667 L170.666,170.666 L170.666667,1.42108547e-14 L213.333333,1.42108547e-14 Z"/>
  </g>
 </g>
</svg>)SVG"
        },

        {"icon-save",
                R"SVG(<?xml version="1.0" encoding="utf-8"?>
<svg width="800px" height="800px" viewBox="0 0 16 16" xmlns="http://www.w3.org/2000/svg" id="svg2" version="1.1">
  <g id="layer1" transform="matrix(.875 0 0 .875 -1.625 -903.192)">
    <g id="layer1-7">
      <path id="path821" d="M3 1033.362v16h16v-13.714l-2.286-2.286zm1.143 1.143H7.57v4.571h6.858v-4.571h1.142l2.286 2.286v11.428h-1.143v-6.857H5.286v6.857H4.143zm4.571 0h3.429v3.429H8.714zm-2.285 8h9.142v5.714H6.43z" style="fill:#373737;fill-opacity:1;stroke:none;stroke-width:1.14285707px;stroke-linecap:butt;stroke-linejoin:miter;stroke-opacity:1"/>
    </g>
  </g>
</svg>)SVG"
        },

        {"icon-del",
                R"SVG(<?xml version="1.0" encoding="utf-8"?>
<svg fill="#000000" width="800px" height="800px" viewBox="0 0 256 256" id="Flat" xmlns="http://www.w3.org/2000/svg">
  <path d="M215.99609,48H180V36A28.03146,28.03146,0,0,0,152,8H104A28.03146,28.03146,0,0,0,76,36V48H39.99609a12,12,0,0,0,0,24h4V208a20.0226,20.0226,0,0,0,20,20h128a20.0226,20.0226,0,0,0,20-20V72h4a12,12,0,0,0,0-24ZM100,36a4.00458,4.00458,0,0,1,4-4h48a4.00458,4.00458,0,0,1,4,4V48H100Zm87.99609,168h-120V72h120ZM116,104v64a12,12,0,0,1-24,0V104a12,12,0,0,1,24,0Zm48,0v64a12,12,0,0,1-24,0V104a12,12,0,0,1,24,0Z"/>
</svg>)SVG"
        },

        {"icon-bookmark",
                R"SVG(<?xml version="1.0" encoding="utf-8"?>
<svg fill="none" stroke="black" stroke-width="3" width="800px" height="800px" viewBox="0 0 32 32" version="1.1" xmlns="http://www.w3.org/2000/svg">
<path d="M24 4v24l-8-8-8 8v-24h16z"/>
</svg>)SVG"
        },

        {"icon-bookmark-fill",
                R"SVG(<?xml version="1.0" encoding="utf-8"?>
<svg fill="black" stroke="black" stroke-width="3" width="800px" height="800px" viewBox="0 0 32 32" version="1.1" xmlns="http://www.w3.org/2000/svg">
<path d="M24 4v24l-8-8-8 8v-24h16z"/>
</svg>)SVG"
        },

        {"icon-expanded",
                R"SVG(<?xml version="1.0" encoding="utf-8"?>
<svg fill="#000000" width="800px" height="800px" viewBox="0 0 8 8" xmlns="http://www.w3.org/2000/svg">
  <path d="M0 0l4 4 4-4h-8z" transform="translate(0 2)" />
</svg>)SVG"
        },

        {"icon-collapsed",
                R"SVG(<?xml version="1.0" encoding="utf-8"?>
<svg fill="#000000" width="800px" height="800px" viewBox="0 0 8 8" xmlns="http://www.w3.org/2000/svg">
  <path d="M0 0v8l4-4-4-4z" transform="translate(2)" />
</svg>)SVG"
        },

        {"body-unknown",
                R"SVG(<?xml version="1.0" encoding="utf-8"?>
<svg width="800px" height="800px" viewBox="0 0 24 24" fill="none" xmlns="http://www.w3.org/2000/svg">
<path fill-rule="evenodd" clip-rule="evenodd" d="M12.7704 11.8217C11.9421 12.4566 11 13.1787 11 15H13C13 13.9046 13.711 13.2833 14.4408 12.6455C15.21 11.9733 16 11.2829 16 10C16 7.79 14.21 6 12 6C9.79 6 8 7.79 8 10H10C10 8.9 10.9 8 12 8C13.1 8 14 8.9 14 10C14 10.8792 13.4202 11.3236 12.7704 11.8217ZM13 18.5V16.5H11V18.5H13Z" fill="#000000"/>
<path fill-rule="evenodd" clip-rule="evenodd" d="M22 2H2V22H22V2ZM20 4H4V20H20V4Z" fill="#000000"/>
</svg>)SVG"
        },

        {"body-star",
                R"SVG(<?xml version="1.0" encoding="utf-8"?>
<svg width="800px" height="800px" viewBox="0 0 24 24" fill="none" xmlns="http://www.w3.org/2000/svg">
<path d="M11 1C11 0.447715 11.4477 0 12 0C12.5523 0 13 0.447715 13 1V3C13 3.55228 12.5523 4 12 4C11.4477 4 11 3.55228 11 3V1Z" fill="#0F0F0F"/>
<path fill-rule="evenodd" clip-rule="evenodd" d="M18 12C18 15.3137 15.3137 18 12 18C8.68629 18 6 15.3137 6 12C6 8.68629 8.68629 6 12 6C15.3137 6 18 8.68629 18 12ZM8.06167 12C8.06167 14.1751 9.82492 15.9383 12 15.9383C14.1751 15.9383 15.9383 14.1751 15.9383 12C15.9383 9.82492 14.1751 8.06167 12 8.06167C9.82492 8.06167 8.06167 9.82492 8.06167 12Z" fill="#0F0F0F"/>
<path d="M20.4853 3.51472C20.0947 3.12419 19.4616 3.12419 19.0711 3.51472L17.6568 4.92893C17.2663 5.31946 17.2663 5.95262 17.6568 6.34315C18.0474 6.73367 18.6805 6.73367 19.0711 6.34315L20.4853 4.92893C20.8758 4.53841 20.8758 3.90524 20.4853 3.51472Z" fill="#0F0F0F"/>
<path d="M1 13C0.447715 13 0 12.5523 0 12C0 11.4477 0.447715 11 1 11H3C3.55228 11 4 11.4477 4 12C4 12.5523 3.55228 13 3 13H1Z" fill="#0F0F0F"/>
<path d="M3.51472 3.51472C3.1242 3.90524 3.1242 4.53841 3.51472 4.92893L4.92894 6.34315C5.31946 6.73367 5.95263 6.73367 6.34315 6.34315C6.73368 5.95262 6.73368 5.31946 6.34315 4.92893L4.92894 3.51472C4.53841 3.12419 3.90525 3.12419 3.51472 3.51472Z" fill="#0F0F0F"/>
<path d="M11 21C11 20.4477 11.4477 20 12 20C12.5523 20 13 20.4477 13 21V23C13 23.5523 12.5523 24 12 24C11.4477 24 11 23.5523 11 23V21Z" fill="#0F0F0F"/>
<path d="M6.34315 17.6569C5.95263 17.2663 5.31946 17.2663 4.92894 17.6569L3.51473 19.0711C3.1242 19.4616 3.1242 20.0948 3.51473 20.4853C3.90525 20.8758 4.53842 20.8758 4.92894 20.4853L6.34315 19.0711C6.73368 18.6805 6.73368 18.0474 6.34315 17.6569Z" fill="#0F0F0F"/>
<path d="M21 13C20.4477 13 20 12.5523 20 12C20 11.4477 20.4477 11 21 11H23C23.5523 11 24 11.4477 24 12C24 12.5523 23.5523 13 23 13H21Z" fill="#0F0F0F"/>
<path d="M17.6568 17.6569C17.2663 18.0474 17.2663 18.6805 17.6568 19.0711L19.0711 20.4853C19.4616 20.8758 20.0947 20.8758 20.4853 20.4853C20.8758 20.0948 20.8758 19.4616 20.4853 19.0711L19.0711 17.6569C18.6805 17.2663 18.0474 17.2663 17.6568 17.6569Z" fill="#0F0F0F"/>
</svg>)SVG"
        },

        {"body-barycenter",
                R"SVG(<?xml version="1.0" encoding="utf-8"?>
<svg width="800px" height="800px" viewBox="0 0 48 48" fill="none" xmlns="http://www.w3.org/2000/svg">
<path d="M20 31L24 35L20 39" stroke="#000000" stroke-width="4" stroke-linecap="round" stroke-linejoin="round"/>
<path d="M32 34.1679C39.0636 32.6248 44 29.1006 44 25C44 19.4772 35.0457 15 24 15C12.9543 15 4 19.4772 4 25C4 30.5228 12.9543 35 24 35" stroke="#000000" stroke-width="4" stroke-linecap="round" stroke-linejoin="round"/>
</svg>)SVG"
        },

        {"body-elw",
                R"SVG(<?xml version="1.0" encoding="utf-8"?>
<svg fill="#000000" width="800px" height="800px" viewBox="0 0 16 16" xmlns="http://www.w3.org/2000/svg">
  <path d="M8 .5A7.76 7.76 0 0 0 0 8a7.76 7.76 0 0 0 8 7.5A7.76 7.76 0 0 0 16 8 7.76 7.76 0 0 0 8 .5z
m6.71 6.8L13.48 7c-.25-.07-.27-.09-.29-.12-.15-.2-.32-.47-.48-.73 0-.09-.13-.23-.16-.31s.35-.6.51-.84a2.43 2.43 0 0 1 .59-.45 5.87 5.87 0 0 1 1.06 2.75z
M8 1.75l-.09.17a.19.19 0 0 1 0-.1c0 .06-.15.15-.25.25l-.3.29a.85.85 0 0 0-.08 1.08h-.12a1.05 1.05 0 0 0-.81.42 1.27 1.27 0 0 0-.2 1.07
V5a3 3 0 0 0-.43.11l-.24.08-.64.21a1.2 1.2 0 0 0-.81.8 1 1 0 0 0 .2.93 5.67 5.67 0 0 0 1.38 1.09 4.17 4.17 0 0 0 1.67.65h1.68
a1.2 1.2 0 0 1 1.04.51.49.49 0 0 1 .13.43.77.77 0 0 1-.15.35 2.71 2.71 0 0 0-.95 1.61 11.11 11.11 0 0 1-.48 1.38c-.12.31-.23.61-.31.85
a3.32 3.32 0 0 1-1-.08 3.28 3.28 0 0 0-.5-2.12 2.24 2.24 0 0 1-.53-1.42 2.11 2.11 0 0 0-1.47-2.29 10.81 10.81 0 0 1-2.9-2.64A6.79 6.79 0 0 1 8 1.75z
M1.25 8a5.64 5.64 0 0 1 .12-1.16 10.29 10.29 0 0 0 2.94 2.42c.6.22.69.45.69 1.12a3.45 3.45 0 0 0 .86 2.27A3.05 3.05 0 0 1 6 14a6.35 6.35 0 0 1-4.75-6z
m8.32 6.08c0-.15.12-.32.18-.48a10.2 10.2 0 0 0 .55-1.6 1.55 1.55 0 0 1 .54-.86 1.91 1.91 0 0 0 .57-1.3 1.71 1.71 0 0 0-.47-1.27 2.45 2.45 0 0 0-2-.9
H7.35a4.77 4.77 0 0 1-2-1.11l.47-.16.27-.08a.79.79 0 0 1 .38-.07l.09.15a.64.64 0 0 0 .81.29.65.65 0 0 0 .34-.8v-.18c-.11-.3-.24-.72-.32-1
A1.42 1.42 0 0 0 8.68 4a1 1 0 0 0-.18-1 3.44 3.44 0 0 0 .33-.34 1 1 0 0 0 .22-.8 6.93 6.93 0 0 1 3.73 1.8 3 3 0 0 0-.79.7 9.14 9.14 0 0 0-.64 1.09 1.46 1.46 0 0 0 .24 1.39
c.18.31.38.61.56.86a1.58 1.58 0 0 0 1 .58c.22.06 1 .22 1.55.33a6.44 6.44 0 0 1-5.13 5.47z"/>
</svg>)SVG"
        },

        {"body-ww",
                R"SVG(<?xml version="1.0" encoding="utf-8"?>
<svg width="800px" height="800px" viewBox="0 0 24 24" fill="none" xmlns="http://www.w3.org/2000/svg">
<path d="M22 12C22 17.5228 17.5228 22 12 22C6.47715 22 2 17.5228 2 12C2 6.47715 6.47715 2 12 2C17.5228 2 22 6.47715 22 12Z" stroke="#1C274C" stroke-width="1.5"/>
<path d="M3 8.0077C3 8.0077 5.93717 11 10.4372 11C13.5 11 15.1257 9.22689 16.5 8.75574C19.0829 7.87029 21 8.0077 21 8.0077" stroke="#1C274C" stroke-width="1.5" stroke-linecap="round"/>
<path d="M2.99986 14.0077C2.99986 14.0077 5.08875 13.8703 7.90309 14.7558C9.40057 15.2269 11.1719 17 14.5092 17C17.521 17 19.8903 15.871 21.27 15" stroke="#1C274C" stroke-width="1.5" stroke-linecap="round"/>
</svg>)SVG"
        },

        {"body-planet",
                R"SVG(<?xml version="1.0" encoding="utf-8"?>
<svg width="800px" height="800px" viewBox="0 0 48 48" fill="none" xmlns="http://www.w3.org/2000/svg">
<path d="M24 40C32.8366 40 40 32.8366 40 24C40 15.1634 32.8366 8 24 8C15.1634 8 8 15.1634 8 24C8 32.8366 15.1634 40 24 40Z" fill="#2F88FF" stroke="#000000" stroke-width="4" stroke-linejoin="round"/>
<path d="M37.5641 15.5098V15.5098C41.7833 15.878 44.6787 17.1724 45.2504 19.306C46.3939 23.5737 37.8068 29.5827 26.0705 32.7274C14.3343 35.8721 3.89316 34.9617 2.74963 30.694C2.1505 28.458 4.22245 25.744 8.01894 23.2145V23.2145" stroke="#000000" stroke-width="4" stroke-linecap="round" stroke-linejoin="round"/>
</svg>)SVG"
        },

        {"body-landable",
                R"SVG(<?xml version="1.0" encoding="iso-8859-1"?>
<svg fill="#000000" version="1.1" xmlns="http://www.w3.org/2000/svg" width="800px" height="800px" viewBox="0 0 44.863 44.864" xml:space="preserve">
<path d="M19.024,20.959c0.251-0.392,0.087-1.673,0.087-1.673C9.67,16.034,2.607,11.957,3.007,10.3
    c0.197-0.819,0.47-1.178,0.659-1.241c0.423-0.138,1.378,0.363,2.548,1.134l1.945-2.284c-0.92-0.565-3.188-2.431-5.416-1.705
    C1.833,6.498,0.636,7.326,0.09,9.598c-1.28,5.32,11.41,10.122,18.217,12.484C18.306,22.083,18.773,21.352,19.024,20.959z"/>
<path d="M41.112,10.647c-0.312-0.439-0.638-0.868-0.98-1.281c-3.752-4.501-9.396-7.372-15.701-7.372
    c-0.011,0-0.02,0.001-0.03,0.001c-0.007,0-0.013-0.001-0.019-0.001c-0.023,0-0.046,0.003-0.07,0.003
    c-6.256,0.036-11.852,2.898-15.58,7.37c-0.344,0.413-0.67,0.841-0.98,1.28c-0.236,0.334-0.456,0.681-0.673,1.029
    c0.427,0.27,0.91,0.557,1.449,0.859c0.186-0.297,0.372-0.595,0.574-0.881c1.271,0.849,2.739,1.59,4.364,2.202
    c-0.09,0.325-0.158,0.667-0.238,1c0.497,0.217,1.02,0.435,1.553,0.652c0.084-0.375,0.16-0.754,0.256-1.117
    c2.581,0.791,5.481,1.266,8.557,1.34v2.131l0.104,0.066l0.146,0.876l0.226,1.342l1.2,0.311v-4.726
    c0.596-0.014,1.177-0.056,1.756-0.099l0.439-1.244l0.169-0.48c-0.774,0.074-1.56,0.128-2.364,0.147V3.745
    c3.404,0.526,6.335,4.051,8.001,9.097c-0.806,0.239-1.658,0.433-2.529,0.605c0.569,0.286,1.135,0.729,1.537,1.356
    c0.491-0.122,0.985-0.242,1.459-0.385c0.626,2.384,0.989,5.035,1.005,7.841h-2.599c-0.027,0.17-0.054,0.33-0.083,0.508
    c1.018,0.418,2.228,0.916,2.49,1.024c0.777,0.321,1.357,0.804,1.745,1.392c0.033-0.414,0.069-0.826,0.088-1.248h4.024
    c-0.071,0.52-0.138,1.043-0.23,1.543c0.255-0.131,0.478-0.387,0.594-0.869c0.063-0.26,0.07-0.477,0.057-0.674h2.299
    c-0.274,3.436-1.483,6.607-3.364,9.273c-1.293-0.863-2.791-1.619-4.453-2.236c0.091-0.332,0.166-0.677,0.246-1.018
    c-0.532,0.417-1.199,0.65-1.953,0.65c-0.33,0-0.608-0.043-0.795-0.069l-0.057-0.011c-0.2-0.014-0.481-0.037-2.181-0.619
    c0.2-0.27,0.438-0.623,0.699-1.035v-0.838c0.79,0.271,1.463,0.49,1.621,0.501c0.437,0.03,1.559,0.414,1.881-0.921
    c0.323-1.336-0.516-1.777-0.982-1.971c-0.265-0.107-1.495-0.613-2.52-1.035v-1.697l-0.584,1.457
    c-0.508-0.209-0.873-0.359-0.873-0.359s1.266-6.982,0.92-7.866s-1.383-1.085-1.383-1.085l-2.758,7.812l-2.998-0.775l0,0
    l-1.283-0.333l-0.439-2.623l-1.399-0.895c-0.001,0,0.082,2.162-0.124,3.328c-0.039,0.22-0.089,0.405-0.151,0.532
    c-0.023,0.049-0.065,0.108-0.099,0.162c-0.146,0.242-0.375,0.531-0.629,0.824c-0.001,0.002-0.003,0.002-0.004,0.004
    c-0.12,0.139-0.246,0.276-0.371,0.41c-0.014,0.016-0.027,0.03-0.041,0.045c-0.119,0.125-0.236,0.247-0.349,0.361
    c-0.381,0.389-0.686,0.676-0.686,0.676l1.223,0.904l2.036-1.553h0.001l0.461-0.353l0.731,0.353l0,0l1.853,0.891l0.943,0.455l0,0
    l0.211,0.102l-3.074,7.664c0,0,1.008,0.905,1.766,0.321c0.759-0.584,3.992-6.446,3.992-6.446s0.565,0.207,1.306,0.475
    l-0.172,0.429c0.1-0.003,0.195-0.015,0.296-0.017v1.758c-0.074-0.026-0.143-0.051-0.221-0.078
    c-0.108-0.016-0.222-0.025-0.329-0.041c-0.33,0.583-0.632,1.105-0.904,1.568c1.816,0.203,3.548,0.537,5.136,1.006
    c-1.667,5.048-4.602,8.57-8.006,9.097v-6.086c-0.493,0.343-1.067,0.532-1.676,0.532v5.566c-3.439-0.472-6.41-4.002-8.094-9.082
    c1.715-0.517,3.599-0.879,5.584-1.076l0.696-1.737c-2.403,0.185-4.683,0.604-6.751,1.239c-0.529-2.01-0.867-4.215-0.971-6.539
    h1.73l0.44-0.412c-1.368-0.469-2.659-0.939-3.875-1.414c-0.001,0.05-0.005,0.099-0.005,0.148H5.674
    c0.009-1.039,0.109-2.056,0.283-3.048c-0.539-0.29-1.052-0.58-1.536-0.872c-0.271,1.324-0.429,2.689-0.429,4.093
    c0,4.387,1.395,8.451,3.756,11.783c0.311,0.439,0.637,0.868,0.98,1.28c3.729,4.472,9.324,7.334,15.58,7.37
    c0.024,0,0.047,0.003,0.07,0.003c0.006,0,0.012-0.001,0.019-0.001c0.012,0,0.021,0.001,0.03,0.001
    c6.305,0,11.949-2.871,15.701-7.371c0.344-0.413,0.67-0.842,0.979-1.28c2.361-3.334,3.756-7.397,3.756-11.785
    C44.867,18.046,43.473,13.98,41.112,10.647z M13.941,12.292c-1.441-0.538-2.738-1.186-3.856-1.919
    c2.192-2.604,5.077-4.61,8.372-5.72C16.579,6.465,15.018,9.114,13.941,12.292z M23.592,14.056
    c-2.918-0.071-5.665-0.511-8.09-1.239c1.684-5.08,4.65-8.613,8.09-9.085V14.056z M5.737,23.936h6.643
    c0.109,2.511,0.489,4.894,1.085,7.072c-1.625,0.61-3.094,1.354-4.363,2.201C7.221,30.542,6.011,27.372,5.737,23.936z
     M10.09,34.488c1.117-0.731,2.412-1.378,3.85-1.916c1.076,3.18,2.639,5.827,4.518,7.638C15.162,39.1,12.284,37.092,10.09,34.488z
     M30.254,40.262c1.908-1.82,3.494-4.498,4.58-7.721c1.473,0.544,2.797,1.201,3.937,1.947
    C36.545,37.131,33.612,39.162,30.254,40.262z M30.254,4.602c3.357,1.1,6.293,3.128,8.52,5.772
    c-0.979,0.641-2.102,1.211-3.329,1.706c-0.057-0.016-0.097-0.025-0.097-0.025l-0.025,0.074c-0.163,0.065-0.322,0.133-0.49,0.195
    C33.746,9.102,32.162,6.422,30.254,4.602z M36.416,22.26c-0.014-2.987-0.413-5.818-1.108-8.37
    c1.659-0.617,3.158-1.371,4.451-2.234c2.12,3.008,3.39,6.658,3.426,10.604H36.416L36.416,22.26z"/>
</svg>)SVG"
        },

        {"site-construction",
                R"SVG(<svg width="800" height="800" fill="#000000" version="1.1" viewBox="0 0 800 800" xmlns="http://www.w3.org/2000/svg">
 <path d="m633 423c-12.9 0-23.3 10.4-23.3 23.3 0 12.8-10.4 23.3-23.3 23.3s-23.3-10.4-23.3-23.3 10.4-23.3 23.3-23.3c12.9 0 23.8-10.4 23.8-23.3v-120h30c11.3 0 20-8.76 20-20 0-11.3-9.46-16-20-20l-247-94.5c-2.61-0.979-5.38-1.48-8.17-1.48l-154 0.028c-7.78 1e-3 -15.7 3.86-20 10.3l-64.2 85.6c-4.69 5.95-5.77 15.4-5.77 20v110c0 12.9 6.33 22.6 19.2 22.6s20.8-8.57 20.8-21.1v-91.6h60v340h-10c-9.86 0-20 10-20 20s10 16.3 20 16.2l30.4-0.224h93.1l46.5 0.112c12.9 0.031 21.9-6.24 21.9-19.1s-9.09-17-21.9-17h-20v-340h190v102c-27.1 9.61-53.6 33.9-53.6 64.2 0 38.5 31.3 69.8 69.8 69.8s69.8-31.3 69.8-69.8c0-12.9-10.4-23.3-23.3-23.3zm-423-183 30-50h50v50h-50zm130 380h-60v-340h60zm13-380h-23v-50h50l150 50z"/>
</svg>)SVG"
        },

        {"site-coriolis",
                R"SVG(<svg width="800" height="800" version="1.1" xmlns="http://www.w3.org/2000/svg">
 <g fill="none" stroke="#000" stroke-width="60">
  <path d="m400 20-350 30-30 350 30 350 350 30 350-30 30-350-30-350z"/>
  <path d="m400 30-370 370 370 370 370-370z"/>
 </g>
 <rect x="250" y="360" width="300" height="80"/>
</svg>)SVG"
        },

        {"site-ocellus",
                R"SVG(<svg width="800" height="800" version="1.1" xmlns="http://www.w3.org/2000/svg">
 <g fill="none" stroke="#000" stroke-width="60">
  <circle cx="400" cy="400" r="200" />
  <circle cx="400" cy="400" r="380" />
 </g>
 <rect x="250" y="360" width="300" height="80"/>
</svg>
)SVG"
        },

        {"site-orbis",
                R"SVG(<svg width="800" height="800" version="1.1" xmlns="http://www.w3.org/2000/svg">
 <g fill="none" stroke="#000" stroke-width="60">
  <circle cx="400" cy="400" r="180"/>
  <circle cx="400" cy="400" r="380"/>
  <line x1="400" x2="400" y1="20" y2="220"/>
  <line x1="400" x2="400" y1="20" y2="220" transform="rotate(120,400,400)"/>
  <line x1="400" x2="400" y1="20" y2="220" transform="rotate(240,400,400)"/>
 </g>
 <rect x="250" y="360" width="300" height="80"/>
</svg>)SVG"
        },

        {"site-dodec",
                R"SVG(<svg width="800" height="800" version="1.1" viewBox="0 0 800 800" xmlns="http://www.w3.org/2000/svg">
 <g fill="none" stroke="#000" stroke-width="60">
  <path d="m550 193 75-104"/>
  <path d="m645 480 121 37.8"/>
  <path d="m400 659-0.0758 125"/>
  <path d="m154 483-119 36.2"/>
  <path d="m249 193-74-103"/>
  <path d="m400 659-245-178 92.8-288 302-0.617 94.6 288z"/>
  <path d="m625 710-225 73.1-225-73.1-139-191 0.011-237 139-192 225-73.1 225 73.1 139 191-0.011 237z"/>
 </g>
 <rect x="250" y="360" width="300" height="80"/>
</svg>
)SVG"
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

