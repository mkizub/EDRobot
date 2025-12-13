//
// Created by mkizub on 11.06.2025.
//

#pragma once

#ifndef EDROBOT_UISTARTUPDIALOG_H
#define EDROBOT_UISTARTUPDIALOG_H

#include "UIDialog.h"

class UIStartupDialog : public UIDialog {
    friend class UIManager;

    UIStartupDialog(const std::string &message, std::string latest_version, std::string latest_url);

    int getDialogResId() final;
    bool onInitDialog(HWND hDlg) final;
    bool onCommand(HWND hDlg, WPARAM wParam) final;

    std::wstring mMessage;
    std::wstring mVersion;
    std::wstring mVersionUrl;
};


#endif //EDROBOT_UISTARTUPDIALOG_H
