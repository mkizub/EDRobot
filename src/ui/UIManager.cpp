//
// Created by mkizub on 11.06.2025.
//

#include "../pch.h"

#include "../../ui/resource.h"
#include <commctrl.h>

#include "UIManager.h"
#include "UIMainDialog.h"
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
    if (auto dlg = UIShowCargo::getInstance()) {
        dlg->run_thread_ui([dlg](){
            DestroyWindow(dlg->hwnd());
        });
    }
    return true;
}

void UIManager::uiThreadLoop() {
    SetThreadDescription(GetCurrentThread(), L"UIManager main thread");
    SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    HINSTANCE hInstance = GetModuleHandle(nullptr);
    uiMain.winmain_run(hInstance, SW_HIDE);
}

bool UIManager::showStartupDialog(const std::string &message, std::string latest_version, std::string latest_url) {
    UIStartupDialog dlg(message, latest_version, latest_url);
    return dlg.show();
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
    auto dlg = UIShowCargo::getInstance();
    if (!dlg)
        return false;
    return dlg->updateCargo();
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
