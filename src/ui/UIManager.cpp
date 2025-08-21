//
// Created by mkizub on 11.06.2025.
//

#include "../pch.h"

#include <shellapi.h>
#include "../../ui/resource.h"

#include "UIManager.h"
#include "Main.h"
#include "UIStartupDialog.h"
#include "UIToast.h"
#include "AddTask.h"
#include "UISellInput.h"
#include "UICalibration.h"
#include "UISelectRect.h"
#include "UIDebug.h"

UIManager &UIManager::getInstance() {
    static Main mainDialog;
    static UIManager uiManager(mainDialog);
    return uiManager;
}

UIManager::UIManager(Main& main)
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
    return true;
}

void UIManager::uiThreadLoop() {
    SetThreadDescription(GetCurrentThread(), L"UIManager main thread");
    HINSTANCE hInstance = GetModuleHandle(nullptr);
    uiMain.winmain_run(hInstance, SW_HIDE);
}

bool UIManager::showStartupDialog(const std::string &message) {
    UIStartupDialog dlg(message);
    return dlg.show();
}

bool UIManager::showMainDialog() {
    UIManager& mgr = getInstance();
    return mgr.uiMain.show();
}

bool UIManager::askCalibrationDialog(const string &line1) {
    UICalibration dlg(line1);
    return dlg.show();
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
