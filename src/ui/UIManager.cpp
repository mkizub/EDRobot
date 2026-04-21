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

        {"body-asteroid-cluster",
                R"SVG(<svg width="800" height="800" version="1.1" xmlns="http://www.w3.org/2000/svg">
 <path d="m535 322c-43-33-70-87-60-142 6-37 15-74 27-109 18-18 37-37 55-55 45 3 91 6 136 10 27 26 54 51 81 77-6 42-13 85-38 121-28 47-65 87-101 128-23-1-47 2-68-2-11-9-22-19-32-28zm122-110c17-32 49-81 12-111-27-16-59-11-89-12-21 40-33 84-30 129-1 14-1 28 12 36 15 18 38 20 59 18 12-20 24-40 36-60z"/>
 <path d="m617 499c-9-10-29-16-29-28 13-58 66-93 106-131h63c26 37 29 83 27 127-20 19-40 38-60 57-25-0-51-0-76-0-10-8-21-16-31-24zm102-68c5-31-59 19-34 24 12-5 31-9 34-24z"/>
 <path d="m334 747c-94-52-160-140-217-230-44-71-77-148-100-227 13-41 36-78 69-105 11-8 19-26 36-22h187c69 35 138 75 182 141 67 94 78 214 77 326 2 14-0 27-11 37-21 30-40 62-63 91-32 13-65 39-100 19-20-9-45-19-60-30zm140-81c9-15 22-29 29-44-7e-3 -85-6-175-53-248-34-57-89-96-140-137h-167c-21 20-52 40-42 73 21 69 62 131 96 195 20 35 38 71 58 104 50 40 101 82 164 99 14 0 31 6 36-11 7-10 13-20 20-30z"/>
</svg>)SVG"
        },

        {"site-colonization-ship",
                R"SVG(<svg width="800" height="800" version="1.1" xmlns="http://www.w3.org/2000/svg">
 <path d="m272 750h248l256-470-96-202h-560l-96 202z" fill="none" stroke="#000" stroke-width="48"/>
 <path d="m481 512c-21 0-37 17-37 37 0 20-17 37-37 37s-37-17-37-37 17-37 37-37 38-17 38-37v-191h48c18 0 32-14 32-32 0-18-15-26-32-32l-333-147c-4-2-48 40-48 40l240 121-312-0.6v56l343-5v163c-43 15-85 54-85 102 0 61 50 111 111 111 61 0 111-50 111-111 0-21-17-37-37-37z"/>
</svg>)SVG"
        },

        {"site-construction",
                R"SVG(<svg width="800" height="800" fill="#000000" version="1.1" viewBox="0 0 800 800" xmlns="http://www.w3.org/2000/svg">
 <path d="m736 427c-19 0-34 16-34 35 0 19-15 35-34 35s-34-16-34-35 15-35 34-35 35-16 35-35v-180h44c17 0 30-13 30-30 0-17-14-24-30-30l-365-142c-4-1-8-2-12-2l-228 0.04c-12 2e-3 -23 6-30 15l-95 128c-7 9-9 23-9 30v165c0 19 9 34 28 34s31-13 31-32v-137h89v510h-15c-15 0-30 15-30 30 0 15 15 24 30 24l45-0.3h138l69 0.2c19 0.05 32-9 32-29s-13-25-32-25h-30v-510h281v153c-40 14-79 51-79 96 0 58 46 105 103 105s103-47 103-105c0-19-15-35-34-35zm-626-274 44-75h74v75h-74zm192 570h-89v-510h89zm19-570h-34v-75h74l222 75z"/>
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

        {"site-asteroid-base",
                R"SVG(<svg width="800" height="800" enable-background="new 106.04 165.532 1784.559 1671.892" version="1.1" viewBox="0 0 800 800" xmlns="http://www.w3.org/2000/svg">
 <g fill="none" stroke="#000">
  <g stroke-width="32">
   <path d="m283 59.1c10.6-0.387 18.3 4.9 26.4 8.25 48.1 20 95.8 40.8 144 60.6 16.5 6.84 34.2 6.58 51.4 7.61 24.3 1.42 46.9 9.42 70 16.3 3.32 0.903 5.95 3.22 8.36 6.19 27.1 34.7 54.4 69.3 81.5 104 11.7 14.8 18.9 32.9 27.8 49.7 22 41.1 43 82.7 64.7 124 3.89 7.35 4.92 13.8 2.29 22.6-13.2 45.4-25.6 91.2-38.2 137-3.66 13-15.1 17-22.9 25.2-8.01 8.26-17.9 14.2-25.5 22.8-14.8 16.4-33.8 13.8-51.5 16.1-28 3.61-56.3 5.03-84.4 8.9-15.1 2.06-28.6 11.2-42.8 17.5-42.1 18.8-84.2 37.7-126 57.5-13.7 6.45-25.1-0.903-36.9-4.26-37.5-10.8-74.7-23-112-34.4-6.75-2.06-11.2-5.68-14-13.5-21.6-61.1-52.2-117-82.8-172-15.2-27.7-29-56.4-43.6-84.4-4.23-8.13-5.61-17.2-6.3-26.2-3.66-43.2-7.55-86.3-12.6-129-0.801-6.97 0.115-13.7 3.2-20.3 25.5-54 50.9-108 76.1-163 3.78-8.26 9.04-12.9 17.1-14.2 21.2-3.61 42.2-7.48 63.4-11.2 19.3-3.48 38.6-6.97 57.9-10.3 2.06-0.517 4.58-0.775 5.49-0.904z"/>
   <path d="m80.9 611c-10.9 3.19-21.2 6.12-31.3 9.31-2.27 0.638-3.83 2.55-4.3 5.23-2.87 13-8.85 24.6-14.9 36-6.58 12.1-4.9 24.8-4.9 37.4 0 1.53 0.598 4.34 1.55 4.59 12.1 3.83 22.8 13.7 36.7 10.1 19.8-5.1 39.6-10.2 59.4-15.1 7.89-1.91 22.6-22.3 22.1-31-0.12-2.68-1.55-4.47-3.11-6.25-8.97-10.5-17.1-21.9-27.1-31.1-9.68-9.32-22-14.4-34.1-19.1z"/>
   <path d="m615 48.9c-0.359-15.7-0.478-15.7-14.6-19.1-8.49-2.04-16.4-2.17-23.9 4.21-3.47 2.93-8.25 3.83-12.2 6-3.35 1.79-8.73 1.66-9.32 6.38-1.08 8.93-5.74 17.1-4.3 26.4 0.12 0.893 0.837 1.91 1.55 2.55 7.53 6.12 14.7 12.6 25.3 7.66 5.26-2.42 11.1-3.83 16.7-5.1 14.5-3.32 21-12.4 20.7-29z"/>
  </g>
  <circle cx="407" cy="412" r="197" stroke-width="48"/>
 </g>
 <rect transform="rotate(13.5)" x="341" y="266" width="300" height="80"/>
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

        {"site-outpost",
                R"SVG(<svg width="800" height="800" xmlns="http://www.w3.org/2000/svg">
 <rect x="104" y="224" width="104" height="280"/>
 <rect x="240" y="224" width="104" height="280"/>
 <rect x="376" y="224" width="104" height="280"/>
 <g fill="none" stroke="#000" stroke-width="48">
  <line x1="208" x2="512" y1="372" y2="372"/>
  <line x1="600" x2="600" y1="55" y2="280"/>
  <line x1="536" x2="536" y1="56" y2="112"/>
  <line x1="664" x2="664" y1="56" y2="115"/>
  <line x1="620" x2="644" y1="218" y2="218"/>
  <line x1="664" x2="664" y1="176" y2="256"/>
  <line x1="664" x2="664" y1="640" y2="736"/>
  <path d="m664 304h-128v229l58.9 82.8h69.1z"/>
  <rect x="24" y="24" width="752" height="752" ry="0"/>
 </g>
</svg>
)SVG"
        },

        {"site-megaship",
                R"SVG(<svg width="800" height="800" xmlns="http://www.w3.org/2000/svg">
 <rect x="250" y="340" width="300" height="80"/>
 <path d="m272 750h248l256-470-96-202h-560l-96 202z" fill="none" stroke="#000" stroke-width="48"/>
 <circle cx="397" cy="378" r="197" fill="none" stroke="#000" stroke-width="48"/>
</svg>)SVG"
        },

        {"site-carrier",
                R"SVG(<svg width="800" height="800" version="1.1" xmlns="http://www.w3.org/2000/svg">
 <g fill="none" stroke="#000" stroke-width="64">
  <path d="m396 760-216-2e-4 -120-160 224-560 112-2e-4m0 720 216-2e-4 128-160-240-560h-104"/>
  <circle cx="400" cy="488" r="176"/>
  <rect x="322" y="472" width="156" height="16"/>
 </g>
</svg>)SVG"
        },

        {"site-space-installation",
                R"SVG(<svg width="800" height="800" version="1.1" xmlns="http://www.w3.org/2000/svg">
 <path d="m416 238 170 170-170 170-170-170z" fill="none" stroke="#000" stroke-width="64"/>
 <g>
  <path d="m457 235 162-142 89.6 102-96.3 84.3-63.2-72.2-66.2 58z"/>
  <path d="m369 239-162-142-89.6 102 96.3 84.3 63.2-72.2 66.2 58z"/>
  <path d="m449 589 162 142 89.6-102-96.3-84.3-63.2 72.2-66.2-58z"/>
  <path d="m380 584-162 142-89.6-102 96.3-84.3 63.2 72.2 66.2-58z"/>
 </g>
</svg>)SVG"
        },

        {"site-port",
                R"SVG(<svg width="800" height="800" xmlns="http://www.w3.org/2000/svg">
 <g fill="none" stroke="#000" stroke-width="64">
  <line x1="288" x2="400" y1="168" y2="168"/>
  <line x1="2.91e-11" x2="800" y1="718" y2="718"/>
  <line x1="288" x2="400" y1="280" y2="280"/>
  <line x1="600" x2="712" y1="528" y2="528"/>
  <rect x="64" y="486" width="328" height="168"/>
  <path d="m256 56h216l-2e-5 344h-80v86l-136-2e-5z"/>
  <rect x="392" y="400" width="348" height="256"/>
 </g>
</svg>)SVG"
        },

        {"site-planetary-installation",
                R"SVG(<svg width="800" height="800" version="1.1" xmlns="http://www.w3.org/2000/svg">
 <g fill="none" stroke="#000" stroke-width="64">
  <line x1="688" x2="688" y1="424" y2="88"/>
  <line x1="528" x2="528" y1="428" y2="260"/>
  <line x1="76" x2="244" y1="480" y2="480"/>
  <line x1="2.91e-11" x2="800" y1="656" y2="656"/>
  <rect x="72" y="368" width="344" height="224"/>
  <rect x="416" y="456" width="312" height="138"/>
 </g>
</svg>)SVG"
        },

        {"site-settlement",
                R"SVG(<svg width="800" height="800" version="1.1" xmlns="http://www.w3.org/2000/svg">
 <g fill="none" stroke="#000" stroke-width="64">
  <line x1="688" x2="688" y1="424" y2="88"/>
  <line x1="528" x2="528" y1="428" y2="260"/>
  <line x1="76" x2="244" y1="480" y2="480"/>
  <line x1="3.63e-12" x2="800" y1="624" y2="624"/>
  <rect x="76" y="352" width="344" height="218" stroke-width="64"/>
  <rect x="420" y="432" width="312" height="138" stroke-width="64"/>
 </g>
 <path d="m779 680v99h-171m-587-99v99h171m587-659v-99h-171m-587 99v-99h171" fill="none" stroke="#000" stroke-width="42"/>
</svg>)SVG"
        },

        {"site-engineer",
                R"SVG(<svg width="800" height="800" version="1.1" xmlns="http://www.w3.org/2000/svg">
 <g fill="none" stroke="#000" stroke-width="64">
  <path d="m704 575-304 175-304-175 1e-6 -350 304-175 304 175z"/>
  <circle cx="400" cy="400" r="160"/>
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

