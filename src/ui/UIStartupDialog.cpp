//
// Created by mkizub on 11.06.2025.
//

#include "../pch.h"

#include "UIStartupDialog.h"
#include <shellapi.h>

#include "../../ui/resource.h"

UIStartupDialog::UIStartupDialog(const string &message, std::string latest_version, std::string latest_url) {
    mMessage = toUtf16(message);
    if (latest_version == EDROBOT_VERSION)
        mVersion = toUtf16(lc_format("Version: {}", EDROBOT_VERSION));
    else
        mVersion = toUtf16(lc_format("Version: {}, available {}", EDROBOT_VERSION, latest_version));
    mVersionUrl = toUtf16(latest_url);
}

int UIStartupDialog::getDialogResId() {
    return IDD_ABOUTBOX;
}

bool UIStartupDialog::onInitDialog(HWND hDlg) {
    SetDlgItemTextW(hDlg, IDC_STATIC_1, mMessage.c_str());
    SetDlgItemTextW(hDlg, IDC_VERSION, mVersion.c_str());
    return true;
}

bool UIStartupDialog::onCommand(HWND hDlg, WPARAM wParam) {
    if (HIWORD(wParam) == STN_CLICKED && LOWORD(wParam) == IDC_VERSION && !mVersionUrl.empty()) {
        const wchar_t* url = L"https://api.github.com/repos/mkizub/EDRobot/releases/latest";
        // Use ShellExecute to open the URL in the default browser.
        // The "open" verb is used to perform the default operation on the specified file or URL.
        // SW_SHOWNORMAL or SW_SHOW specifies how the window is to be shown.
        HINSTANCE result = ShellExecuteW(
                NULL,           // hWnd: Parent window handle (NULL for no parent)
                L"open",        // lpOperation: Operation to perform ("open", "edit", "print", etc.)
                mVersionUrl.c_str(), // lpFile: The file, folder, or URL to operate on
                NULL,           // lpParameters: Parameters for the executable (not needed for URLs)
                NULL,           // lpDirectory: Default directory (NULL for current directory)
                SW_SHOWNORMAL   // nShowCmd: How to show the launched application's window
        );
        return true;
    }
    return false;
}