#include "pch.h"

#include "UI.h"

#include <memory.h>
#include "ui/resource.h"
#include <commctrl.h>

#ifndef WINBOOL
typedef int WINBOOL;
#endif

namespace UI {

static INT_PTR CALLBACK AboutProc(HWND, UINT, WPARAM, LPARAM);

static INT_PTR CALLBACK SellProc(HWND, UINT, WPARAM, LPARAM);

static bool dlgResult;
static int sellTimes;
static int sellItems;

bool showStartupDialog() {
    HINSTANCE hInstance = GetModuleHandle(nullptr);
    DialogBox(hInstance, MAKEINTRESOURCE(IDD_ABOUTBOX), NULL, AboutProc);
    return true;
}

bool askSellInput(int &sells, int &items) {
    sellTimes = sells;
    sellItems = items;
    HINSTANCE hInstance = GetModuleHandle(nullptr);
    DialogBox(hInstance, MAKEINTRESOURCE(IDD_SELLBOX), NULL, SellProc);
    if (dlgResult) {
        sells = sellTimes;
        items = sellItems;
    }
    return dlgResult;
}


// Message handler for about box.
INT_PTR CALLBACK AboutProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
    UNREFERENCED_PARAMETER(lParam);
    switch (message) {
    case WM_INITDIALOG:
        return (INT_PTR) TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL) {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR) TRUE;
        }
        break;
    }
    return (INT_PTR) FALSE;
}

// Message handler for sell box.
INT_PTR CALLBACK SellProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
    UNREFERENCED_PARAMETER(lParam);
    HMENU hMenu = nullptr;
    switch (message) {
    case WM_INITDIALOG:
        hMenu = GetSystemMenu(hDlg, FALSE);
        if (hMenu != NULL) {
            // Add a separator (if desired)
            InsertMenu(hMenu, 5, MF_BYPOSITION | MF_SEPARATOR, 0, NULL);
            // Add the new menu item
            InsertMenu(hMenu, 6, MF_BYPOSITION | MF_STRING, IDM_EXIT_APP, "Exit EDRobot");
        }
        SetDlgItemTextA(hDlg, IDC_EDIT_SELLS, std::to_string(sellTimes).c_str());
        SetDlgItemTextA(hDlg, IDC_EDIT_ITEMS, std::to_string(sellItems).c_str());
        return (INT_PTR) TRUE;

    case WM_SYSCOMMAND:
        if (LOWORD(wParam) == IDM_EXIT_APP) {
            dlgResult = false;
            Master::getInstance().pushCommand(Command::Shutdown);
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR) TRUE;
        }
        break;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL || LOWORD(wParam) == IDM_EXIT_APP) {
            WINBOOL translated;
            sellTimes = GetDlgItemInt(hDlg, IDC_EDIT_SELLS, &translated, FALSE);
            sellItems = GetDlgItemInt(hDlg, IDC_EDIT_ITEMS, &translated, FALSE);
            if (LOWORD(wParam) == IDOK) {
                dlgResult = true;
            } else {
                dlgResult = false;
                if (LOWORD(wParam) == IDM_EXIT_APP)
                    Master::getInstance().pushCommand(Command::Shutdown);
            }
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR) TRUE;
        }
        if (LOWORD(wParam) == ID_FILE_CALIBRATE) {
            dlgResult = false;
            EndDialog(hDlg, LOWORD(wParam));
            Master::getInstance().pushCommand(Command::Calibrate);
            return (INT_PTR) TRUE;
        }
        if (LOWORD(wParam) == ID_DEV_SELECT_RECT) {
            dlgResult = false;
            DWORD dwStyle = GetWindowLong(hDlg, GWL_STYLE); // 0x94c8024c : WS_SYSMENU | WS_DLGFRAME | WS_BORDER | WS_CLIPSIBLINGS | WS_VISIBLE | WS_POPUP
            DWORD dwExStyle = GetWindowLong(hDlg, GWL_EXSTYLE); // 0x10108 : WS_EX_TOPMOST
            EndDialog(hDlg, LOWORD(wParam));
            selectRectDialog();
            return (INT_PTR) TRUE;
        }
        break;
    }
    return (INT_PTR) FALSE;
}

const long minSz = 20;
static cv::Rect screenRect;
static cv::Rect selectedRect;
static int repeatKeyCount;

inline RECT toRECT(cv::Rect& r) {
    return {r.x, r.y, r.br().x, r.br().y};
}
inline cv::Rect fromRECT(RECT& r) {
    return {r.left, r.top, r.right-r.left, r.bottom-r.top};
}
static void moveSelectedWindow(HWND hWnd, int dx, int dy) {
    RECT rect;
    if (!GetWindowRect(hWnd, &rect))
        return;
    MoveWindow(hWnd, rect.left+dx, rect.top+dy, rect.right-rect.left, rect.bottom-rect.top, TRUE);
}
static void resizeSelectedWindow(HWND hWnd, int dw, int dh) {
    RECT rect;
    if (!GetWindowRect(hWnd, &rect))
        return;
    auto width = std::max(minSz, rect.right - rect.left + dw);
    auto height = std::max(minSz, rect.bottom - rect.top + dh);
    MoveWindow(hWnd, rect.left, rect.top, width, height, TRUE);
}

static int isSizeAndIncr(int& incr) {
    repeatKeyCount += 1;
    incr = 1 + std::min(20, (repeatKeyCount / 3));
    return (GetKeyState(VK_CONTROL) & 0x8000);
}

INT_PTR CALLBACK selectRectProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    int x, y;
    switch (msg) {
    case WM_DESTROY:
        PostQuitMessage(0);
        return (LRESULT) 0;

    case WM_GETMINMAXINFO: {
        MINMAXINFO* mmi = (MINMAXINFO*)lParam;
        mmi->ptMinTrackSize.x = minSz;
        mmi->ptMinTrackSize.y = minSz;
        return (LRESULT) 0;
    }

    case WM_NCHITTEST: {
        x = LOWORD(lParam);
        y = HIWORD(lParam);
        {
            RECT rect;
            if (!GetWindowRect(hWnd, &rect))
                break;
            bool lc = std::abs(x - rect.left) < minSz / 4;
            bool rc = std::abs(x - rect.right) < minSz / 4;
            bool tc = std::abs(y - rect.top) < minSz / 4;
            bool bc = std::abs(y - rect.bottom) < minSz / 4;
            if (tc && lc) return HTTOPLEFT;
            if (tc && rc) return HTTOPRIGHT;
            if (bc && lc) return HTBOTTOMLEFT;
            if (bc && rc) return HTBOTTOMRIGHT;
            if (tc) return HTTOP;
            if (bc) return HTBOTTOM;
            if (lc) return HTLEFT;
            if (rc) return HTRIGHT;
            SetCursor(LoadCursor(nullptr, IDC_SIZEALL));
            return HTCAPTION;
        }
        return HTCLIENT;
    }
    case WM_SETCURSOR:
        if (LOWORD(lParam) == HTCAPTION) {
            SetCursor(LoadCursor(nullptr, IDC_SIZEALL));
            return (LRESULT) 0;
        }
        break;

    case WM_KEYUP:
        repeatKeyCount = 0;
        break;
    case WM_KEYDOWN:
        switch (wParam) {
        case VK_ESCAPE:
            PostMessage(hWnd, WM_CLOSE, 0, 0);
            break;
        case VK_RETURN:
            {
                RECT rect;
                if (!GetWindowRect(hWnd, &rect))
                    break;
                selectedRect = fromRECT(rect);
                std::string text = std::format("[{},{},{},{}]", selectedRect.x, selectedRect.y, selectedRect.width, selectedRect.height);
                LOG(INFO) << "Selected rect: " << selectedRect << ": " << text;
                Master::getInstance().setDevScreenRect(selectedRect);
                pasteToClipboard(text);
                Master::getInstance().pushCommand(Command::DevRectScreenshot);
            }
            PostMessage(hWnd, WM_CLOSE, 0, 0);
            break;
        case VK_LEFT:
            if (isSizeAndIncr(x))
                resizeSelectedWindow(hWnd, -x, 0);
            else
                moveSelectedWindow(hWnd, -x, 0);
            break;
        case VK_RIGHT:
            if (isSizeAndIncr(x))
                resizeSelectedWindow(hWnd, +x, 0);
            else
                moveSelectedWindow(hWnd, +x, 0);
            break;
        case VK_UP:
            if (isSizeAndIncr(y))
                resizeSelectedWindow(hWnd, 0, -y);
            else
                moveSelectedWindow(hWnd, 0, -y);
            break;
        case VK_DOWN:
            if (isSizeAndIncr(x))
                resizeSelectedWindow(hWnd, 0, +y);
            else
                moveSelectedWindow(hWnd, 0, +y);
            break;
        default:
            break;
        }
    }
    return (LRESULT) DefWindowProc(hWnd, msg, wParam, lParam);
}
void selectRectDialog(HWND hWndED) {
    HINSTANCE hInstance = GetModuleHandle(nullptr);
    const TCHAR szWindowClass[] = "SelectRectWindowClass";
    const TCHAR szTitle[] = ""; //"SelectRect Window";

    static bool registered;
    if (!registered) {
        WNDCLASSEX wc;
        wc.cbSize = sizeof(WNDCLASSEX);
        wc.style = 0;
        wc.lpfnWndProc = selectRectProc;
        wc.cbClsExtra = 0;
        wc.cbWndExtra = 0;
        wc.hInstance = hInstance;
        wc.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = (HBRUSH) (COLOR_WINDOW + 1);
        wc.lpszMenuName = nullptr;
        wc.lpszClassName = szWindowClass;
        wc.hIconSm = LoadIcon(nullptr, IDI_APPLICATION);
        if (!RegisterClassEx(&wc)) {
            LOG(ERROR) << "Failed to register class" << getErrorMessage();
            return;
        }
        registered = true;
    }



    MONITORINFOEX monitorInfo;
    monitorInfo.cbSize = sizeof(monitorInfo);
    GetMonitorInfo(MonitorFromWindow(hWndED, MONITOR_DEFAULTTOPRIMARY), &monitorInfo);
    screenRect = fromRECT(monitorInfo.rcMonitor);
    if (selectedRect.empty()) {
        cv::Size size{100, 100};
        cv::Point center = (screenRect.tl() + screenRect.br()) * 0.5;
        selectedRect = {center, size};
    }

    HWND hWnd = CreateWindowEx(
            WS_EX_LAYERED | WS_EX_TOPMOST,
            szWindowClass,
            szTitle,
            WS_POPUP | WS_BORDER,
            selectedRect.x,
            selectedRect.y,
            selectedRect.width,
            selectedRect.height,
            nullptr, nullptr, hInstance, nullptr
    );

    if (!hWnd) {
        LOG(ERROR) << "Failed to create window" << getErrorMessage();
        return;
    }

    //set window background to white
    HBRUSH hbr = CreateSolidBrush(RGB(255, 255, 255));
    SetClassLongPtr(hWnd, GCLP_HBRBACKGROUND, (LONG_PTR) hbr);
    // Set transparency (25% opaque)
    float transparency_percentage = 0.25f;
    SetLayeredWindowAttributes(hWnd, 0, (BYTE) (255 * transparency_percentage), LWA_ALPHA);

    ShowWindow(hWnd, SW_SHOW);
    UpdateWindow(hWnd);
    SetForegroundWindow(hWnd);

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}

} // namespace UI
