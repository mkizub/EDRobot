//
// Created by mkizub on 11.06.2025.
//

#include "../pch.h"

#include "UIStartupDialog.h"

#include "../../ui/resource.h"

UIStartupDialog::UIStartupDialog(const string &message) {
    mMessage = toUtf16(message);
}

int UIStartupDialog::getDialogResId() {
    return IDD_ABOUTBOX;
}

bool UIStartupDialog::onInitDialog(HWND hDlg) {
    SetDlgItemTextW(hDlg, IDC_STATIC_1, mMessage.c_str());
    return true;
}

