//
// Created by mkizub on 11.06.2025.
//

#include "../pch.h"

#include "UIManager.h"
#include "UIStartupDialog.h"
#include "UIToast.h"
#include "UISellInput.h"
#include "UICalibration.h"
#include "UISelectRect.h"

UIManager &UIManager::getInstance() {
    static UIManager uiManager;
    return uiManager;
}

UIManager::UIManager() {

}

bool UIManager::initialize() {
    bool ok = true;
    ok &= UIToast::initialize();
    ok &= UISelectRect::initialize();
    return ok;
}

bool UIManager::shutdown() {
    return true;
}

bool UIManager::showStartupDialog(const std::string &message) {
    UIStartupDialog dlg(message);
    return dlg.show();
}

bool UIManager::askCalibrationDialog(const string &line1) {
    UICalibration dlg(line1);
    return dlg.show();
}


bool UIManager::askSellInput(int &total, int &chunk, Commodity*& commodity) {
    UISellInput dlg(total, chunk);
    bool ok = dlg.show();
    if (ok) {
        total = dlg.mSellTotal;
        chunk = dlg.mSellChunk;
        commodity = dlg.mSellCommodity;
    }
    return ok;
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

