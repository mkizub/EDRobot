//
// Created by mkizub on 11.06.2025.
//

#pragma once

#ifndef EDROBOT_UICALIBRATION_H
#define EDROBOT_UICALIBRATION_H

#include "UIDialog.h"


class UICalibration : public UIDialog {
    friend class UIManager;

    UICalibration(const std::string &line1);

    int getDialogResId() final;
    bool onInitDialog(HWND hDlg) final;

    std::wstring mLine1;
};


#endif //EDROBOT_UICALIBRATION_H
