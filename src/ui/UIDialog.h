//
// Created by mkizub on 11.06.2025.
//

#pragma once

#ifndef EDROBOT_UIDIALOG_H
#define EDROBOT_UIDIALOG_H

class UIDialog {
protected:
    bool show();

    virtual int getDialogResId() = 0;
    virtual bool onInitDialog(HWND hDlg) = 0;
    virtual bool onCommand(HWND hDlg, WORD cmd) { return false; }

    static INT_PTR CALLBACK DialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
};


#endif //EDROBOT_UIDIALOG_H
