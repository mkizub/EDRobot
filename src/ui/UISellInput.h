//
// Created by mkizub on 11.06.2025.
//

#pragma once

#ifndef EDROBOT_UISELLINPUT_H
#define EDROBOT_UISELLINPUT_H

#include "UIDialog.h"

class UISellInput : public UIDialog {
    friend class UIManager;

    UISellInput(int total, int chunk);

    int getDialogResId() final;
    bool onInitDialog(HWND hDlg) final;
    bool onCommand(HWND hDlg, WORD cmd) final;

    int mSellTotal {1000};
    int mSellChunk {1};
    Commodity* mSellCommodity {};

    std::wstring mLabelSellCargoAll;
};


#endif //EDROBOT_UISELLINPUT_H
