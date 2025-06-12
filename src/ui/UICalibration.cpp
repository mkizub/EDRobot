//
// Created by mkizub on 11.06.2025.
//

#include "../pch.h"

#include "UICalibration.h"

#include "../../ui/resource.h"

UICalibration::UICalibration(const std::string &line1) {
    mLine1 = toUtf16(line1);
}

int UICalibration::getDialogResId() {
    return IDD_CALIBRATIONBOX;
}

bool UICalibration::onInitDialog(HWND hDlg) {
    SetDlgItemTextW(hDlg, IDC_STATIC_1, mLine1.c_str());
    return true;
}
