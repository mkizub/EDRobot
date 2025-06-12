//
// Created by mkizub on 11.06.2025.
//

#pragma once

#ifndef EDROBOT_UISTARTUPDIALOG_H
#define EDROBOT_UISTARTUPDIALOG_H

#include "UIDialog.h"

class UIStartupDialog : public UIDialog {
    friend class UIManager;

    UIStartupDialog(const std::string &message);

    int getDialogResId() final;
    bool onInitDialog(HWND hDlg) final;

    std::wstring mMessage;
};


#endif //EDROBOT_UISTARTUPDIALOG_H
