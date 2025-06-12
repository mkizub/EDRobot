//
// Created by mkizub on 11.06.2025.
//

#include "../pch.h"

#include "UIDialog.h"

#include "../../ui/resource.h"


static void positionDialog(HWND hDlg) {
    // Get the screen dimensions
    RECT desktopRect;
    GetWindowRect(GetDesktopWindow(), &desktopRect);
    int screenWidth = desktopRect.right - desktopRect.left;
    int screenHeight = desktopRect.bottom - desktopRect.top;

    // Get the dialog dimensions
    RECT dialogRect;
    GetWindowRect(hDlg, &dialogRect);
    int dialogWidth = dialogRect.right - dialogRect.left;
    int dialogHeight = dialogRect.bottom - dialogRect.top;

    // Calculate the center position
    int xPos = screenWidth - dialogWidth - 20;
    int yPos = 20;

    // Set the dialog position
    SetWindowPos(hDlg, HWND_TOP, xPos, yPos, 0, 0, SWP_NOSIZE);
}

INT_PTR CALLBACK UIDialog::DialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
    UIDialog* uiDialog;

    switch (message) {
    case WM_INITDIALOG:
        SetWindowLongPtr(hDlg, GWLP_USERDATA, lParam);
        positionDialog(hDlg);
        return (INT_PTR)((UIDialog*)lParam)->onInitDialog(hDlg);

    case WM_SYSCOMMAND:
    case WM_COMMAND:
        uiDialog = (UIDialog*)GetWindowLongPtr(hDlg, GWLP_USERDATA);
        if (uiDialog->onCommand(hDlg, LOWORD(wParam)))
            return (INT_PTR) TRUE;
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL) {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR) TRUE;
        }
        break;
    }
    return (INT_PTR) FALSE;
}


bool UIDialog::show() {
    INT_PTR res = DialogBoxParam(GetModuleHandle(nullptr),
                                 MAKEINTRESOURCE(getDialogResId()),
                                 nullptr, // parent
                                 DialogProc,
                                 (LPARAM)this);
    return res == 0 || res == IDOK || res == IDYES;
}
