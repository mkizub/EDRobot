#include "pch.h"

#include "UI.h"

#include <memory.h>
#include "../ui/resource.h"
#include <windowsx.h>
//#include "wintoastlib.h"
//using namespace WinToastLib;


#ifndef WINBOOL
typedef int WINBOOL;
#endif

namespace UI {

static INT_PTR CALLBACK AboutProc(HWND, UINT, WPARAM, LPARAM);
static INT_PTR CALLBACK CalibrationProc(HWND, UINT, WPARAM, LPARAM);
static INT_PTR CALLBACK SellProc(HWND, UINT, WPARAM, LPARAM);

static void initSelectRectDialog();
static void initShowPopupMessageWindow();
static void selectRectWindow();
static void popupMessageWindow();

static bool isToastSupported;
static bool dlgResult;
static int sellTotal;
static int sellChunk;
static std::string sellCargo;
static const Cargo* shipCargo;
static std::wstring sellCargoAll;
static std::wstring popupTitle;
static std::wstring popupText;

//static std::thread uiThread;
//static void loopUi();
//static std::atomic<bool> shutdownRequested;
//static HWND hWndUILoop;
static HWND hWndSelectRect;
static HWND hWndPopupMessage;

enum class UICommand {
    NoOp,
    UserNotify,
    Shutdown,
};
struct UICommandEntry {
    UICommandEntry(UICommand command) : command(command) {}
    virtual ~UICommandEntry() = default;
    const UICommand command;
};
struct UICommandNotify : public UICommandEntry {
    UICommandNotify(bool error, int timeout, const std::wstring& title, const std::wstring& text)
            : UICommandEntry(UICommand::UserNotify)
            , error(error)
            , timeout(timeout)
            , title(title)
            , text(text)
    {}
    ~UICommandNotify() override = default;
    const bool error;
    const int timeout;
    const std::wstring title;
    const std::wstring text;
};

std::queue<std::unique_ptr<UICommandEntry>> mCommandQueue;
std::mutex mCommandMutex;
std::condition_variable mCommandCond;

void pushCommand(UICommandEntry* cmd) {
    std::unique_lock<std::mutex> lock(mCommandMutex);
    mCommandQueue.emplace(cmd);
    mCommandCond.notify_one();
}

void popCommand(std::unique_ptr<UICommandEntry>& cmd) {
    std::unique_lock<std::mutex> lock(mCommandMutex);
    mCommandCond.wait(lock, []() { return !mCommandQueue.empty(); });
    std::swap(cmd, mCommandQueue.front());
    mCommandQueue.pop();
}


//class WinToastHandler final : public IWinToastHandler {
//    static WinToastHandler instance;
//    ~WinToastHandler() final {}
//public:
//    WinToastHandler* getInstance() { return &instance; }
//    void toastActivated() const {}
//    void toastActivated(int actionIndex) const {}
//    void toastActivated(const char* response) const {}
//    void toastDismissed(WinToastDismissalReason state) const {}
//    void toastFailed() const {}
//};

bool initializeUI() {
//    isToastSupported = WinToast::isCompatible();
//    if (isToastSupported) {
//        WinToast::instance()->setAppName(L"EDRobot");
//        WinToast::instance()->setAppUserModelId(L"EDRobot");
//        isToastSupported = WinToast::instance()->initialize();
//    }
//    LOG_IF(!isToastSupported,ERROR) << "Toast notifications are not supported";

    //uiThread = std::thread(loopUi);

    initSelectRectDialog();
    initShowPopupMessageWindow();

    return true;
}

void shutdownUI() {
//    shutdownRequested = true;
//    if (hWndUILoop)
//        SendMessage(hWndUILoop, WM_DESTROY, 0, 0);
//    if (uiThread.joinable()) {
//        uiThread.join();
//    }
}

//bool showToast(const std::wstring& title, const std::wstring& text) {
//    if (!isToastSupported || !WinToast::instance()->isInitialized())
//        return false;
//    WinToastTemplate templ(WinToastTemplate::Text02);
//    templ.setTextField(title, WinToastTemplate::FirstLine);
//    templ.setTextField(text, WinToastTemplate::SecondLine);
//    templ.setAudioOption(WinToastTemplate::AudioOption::Silent);
//    //templ.setAttributionText(attribute);
//    templ.setScenario(WinToastTemplate::Scenario::Alarm);
//    templ.setExpiration(3000);
//    templ.setDuration(WinToastTemplate::Duration::Short);
//    return WinToast::instance()->showToast(templ, new WinToastHandler());
//}

bool showStartupDialog(const std::string& line1, const std::string& line2) {
    //if (!isToastSupported) {
        popupTitle = toUtf16(line1);
        popupText = toUtf16(line2);
        DialogBox(GetModuleHandle(nullptr), MAKEINTRESOURCE(IDD_ABOUTBOX), NULL, AboutProc);
    //} else {
    //    showToast(_("EDRobot started"), _("Press 'Print Screen' key to start selling\nPress 'Esc' to stop"));
    //}
    return true;
}

bool showCalibrationDialog(const std::string& line1) {
    popupTitle = toUtf16(line1);
    DialogBox(GetModuleHandle(nullptr), MAKEINTRESOURCE(IDD_CALIBRATIONBOX), NULL, CalibrationProc);
    return true;
}


bool askSellInput(int &total, int &chunk, std::string& commodity, const Cargo& cargo) {
    sellTotal = total;
    sellChunk = chunk;
    sellCargo = commodity;
    shipCargo = &cargo;
    HINSTANCE hInstance = GetModuleHandle(nullptr);
    DialogBoxW(hInstance, MAKEINTRESOURCE(IDD_SELLBOX), nullptr, SellProc);
    shipCargo = nullptr;
    if (dlgResult) {
        total = sellTotal;
        chunk = sellChunk;
        commodity = sellCargo;
    }
    return dlgResult;
}

bool askSelectRectWindow() {
//    if (!hWndUILoop)
//        return false;
//    PostMessage(hWndUILoop, ID_DEV_SELECT_RECT, 0, 0);
//    return true;
    if (hWndSelectRect) {
        SetForegroundWindow(hWndSelectRect);
    } else {
        std::thread thr(selectRectWindow);
        thr.detach();
    }
    return true;
}

UINT_PTR CLOSE_POPUP_TIMER_ID;
void ClosePopupProc(HWND hWnd, UINT, UINT_PTR, DWORD) {
    CLOSE_POPUP_TIMER_ID = 0;
    PostMessage(hWnd, WM_CLOSE, 0, 0);
}

bool showToast(const std::string& title, const std::string& text) {
//    if (!hWndUILoop)
//        return false;
    popupTitle = toUtf16(title);
    popupText = toUtf16(text);
//    PostMessage(hWndUILoop, ID_SHOW_TOAST, 0, 0);
    if (hWndPopupMessage) {
        InvalidateRect(hWndPopupMessage, nullptr, TRUE);
        UpdateWindow(hWndPopupMessage);
        CLOSE_POPUP_TIMER_ID = SetTimer(hWndPopupMessage, CLOSE_POPUP_TIMER_ID, 5000, ClosePopupProc);
    } else {
        std::thread thr(popupMessageWindow);
        thr.detach();
        Sleep(200);
        if (hWndPopupMessage)
            CLOSE_POPUP_TIMER_ID = SetTimer(hWndPopupMessage, CLOSE_POPUP_TIMER_ID, 5000, ClosePopupProc);
    }
    return true;
}

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

INT_PTR CALLBACK AboutProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
    UNREFERENCED_PARAMETER(lParam);
    switch (message) {
    case WM_INITDIALOG:
        positionDialog(hDlg);
        SetDlgItemTextW(hDlg, IDC_STATIC_1, popupTitle.c_str());
        SetDlgItemTextW(hDlg, IDC_STATIC_2, popupText.c_str());
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

INT_PTR CALLBACK CalibrationProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
    UNREFERENCED_PARAMETER(lParam);
    switch (message) {
    case WM_INITDIALOG:
        positionDialog(hDlg);
        SetDlgItemTextW(hDlg, IDC_STATIC_1, popupTitle.c_str());
        //SetDlgItemTextW(hDlg, IDC_STATIC_2, popupText.c_str());
        return (INT_PTR) TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK) {
            EndDialog(hDlg, LOWORD(wParam));
            Master::getInstance().pushCommand(Command::Calibrate);
            return (INT_PTR) TRUE;
        }
        if (LOWORD(wParam) == IDCANCEL) {
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
    case WM_INITDIALOG: {
        positionDialog(hDlg);
        hMenu = GetSystemMenu(hDlg, FALSE);
        if (hMenu != NULL) {
            // Add a separator (if desired)
            InsertMenu(hMenu, 5, MF_BYPOSITION | MF_SEPARATOR, 0, NULL);
            // Add the new menu item
            InsertMenu(hMenu, 6, MF_BYPOSITION | MF_STRING, IDM_EXIT_APP, L"Exit EDRobot");
        }
        SetDlgItemTextW(hDlg, IDC_STATIC_TOTAL, toUtf16(pgettext("SellDialogLabel|","Total")).c_str());
        SetDlgItemTextW(hDlg, IDC_STATIC_ITEMS, toUtf16(pgettext("SellDialogLabel|","Items")).c_str());
        SetDlgItemTextW(hDlg, IDC_STATIC_SELLS, toUtf16(pgettext("SellDialogLabel|","Sells")).c_str());
        HWND hItems = GetDlgItem(hDlg, IDC_COMBO_ITEMS);
        std::wstring str = toUtf16(pgettext("SellDialogLabel|","All"));
        if (str.starts_with(L"SellDialogLabel|"))
            str = L"Everything";
        sellCargoAll = str;
        std::wstring select = str;
        int err = SendMessage(hItems, CB_ADDSTRING, 0L, (LPARAM)str.c_str());
        if (shipCargo) {
            sellTotal = shipCargo->count;
            for (auto& c : shipCargo->inventory) {
                str = c->commodity.wide;
                SendMessage(hItems, CB_ADDSTRING, 0L, (LPARAM)str.c_str());
                if (c->commodity.name == sellCargo) {
                    select = str;
                    sellTotal = shipCargo->count;
                }
            }
        }
        SendMessageW(hItems, CB_SELECTSTRING, -1L, (LPARAM)select.c_str());
        SetDlgItemTextA(hDlg, IDC_EDIT_TOTAL, std::to_string(sellTotal).c_str());
        SetDlgItemTextA(hDlg, IDC_EDIT_ITEMS, std::to_string(sellChunk).c_str());
        int sellTimes = (int)std::floor(double(sellTotal) / sellChunk);
        SetDlgItemTextA(hDlg, IDC_EDIT_SELLS, std::to_string(sellTimes).c_str());
        return (INT_PTR) TRUE;
    }

//    case WM_KILLFOCUS: {
//        LOG(ERROR) << "WM_KILLFOCUS: " << std::hex << wParam;
//        HWND hwnd = (HWND) wParam;
//        DWORD processId = 0;
//        DWORD threadId = GetWindowThreadProcessId(hwnd, &processId);
//
//        if (processId != 0) {
//            LOG(ERROR) << "Process ID: " << processId;
//            LOG(ERROR) << "Thread ID: " << threadId;
//        } else {
//            LOG(ERROR) << "Failed to get process ID.";
//        }
//    }
//        break;

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
            sellTotal = GetDlgItemInt(hDlg, IDC_EDIT_TOTAL, &translated, FALSE);
            sellChunk = GetDlgItemInt(hDlg, IDC_EDIT_ITEMS, &translated, FALSE);
            int index = SendDlgItemMessage(hDlg, IDC_COMBO_ITEMS, CB_GETCURSEL, 0L, 0L);
            if (index <= 0 || !shipCargo || index > shipCargo->inventory.size()) {
                sellCargo = "";
            } else {
                sellCargo = shipCargo->inventory[index-1]->commodity.name;
            }
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
            askSelectRectWindow();
            return (INT_PTR) TRUE;
        }
        break;
    }
    return (LRESULT) DefWindowProc(hDlg, message, wParam, lParam);
}

const long minSz = 20;
static cv::Rect screenRect;
static cv::Rect selectedRect;
static int repeatKeyCount;
const TCHAR selectRectWindowClass[] = L"SelectRectWindowClass";
const TCHAR showPopupMessageWindowClass[] = L"ShowPopupMessageWindowClass";

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
        hWndSelectRect = nullptr;
        return 0;

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
                Master::getInstance().pushDevRectScreenshotCommand(selectedRect);
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
            if (isSizeAndIncr(y))
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

void initSelectRectDialog() {
    static bool registered;
    if (registered)
        return;

    HINSTANCE hInstance = GetModuleHandle(nullptr);

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
    wc.lpszClassName = selectRectWindowClass;
    wc.hIconSm = LoadIcon(nullptr, IDI_APPLICATION);
    if (!RegisterClassEx(&wc)) {
        LOG(ERROR) << "Failed to register class" << getErrorMessage();
        return;
    }
    registered = true;
}

void selectRectWindow() {
    HINSTANCE hInstance = GetModuleHandle(nullptr);

    MONITORINFOEX monitorInfo;
    monitorInfo.cbSize = sizeof(monitorInfo);
    GetMonitorInfo(MonitorFromWindow(nullptr, MONITOR_DEFAULTTOPRIMARY), &monitorInfo);
    screenRect = fromRECT(monitorInfo.rcMonitor);
    if (selectedRect.empty()) {
        cv::Size size{100, 100};
        cv::Point center = (screenRect.tl() + screenRect.br()) * 0.5;
        selectedRect = {center, size};
    }

    hWndSelectRect = CreateWindowEx(
            WS_EX_LAYERED | WS_EX_TOPMOST,
            selectRectWindowClass,
            L"",
            WS_POPUP | WS_BORDER,
            selectedRect.x,
            selectedRect.y,
            selectedRect.width,
            selectedRect.height,
            nullptr, nullptr, hInstance, nullptr
    );

    if (!hWndSelectRect) {
        LOG(ERROR) << "Failed to create window" << getErrorMessage();
        return;
    }

    //set window background to white
    HBRUSH hbr = CreateSolidBrush(RGB(255, 255, 255));
    SetClassLongPtr(hWndSelectRect, GCLP_HBRBACKGROUND, (LONG_PTR) hbr);
    // Set transparency (25% opaque)
    float transparency_percentage = 0.25f;
    SetLayeredWindowAttributes(hWndSelectRect, 0, (BYTE) (255 * transparency_percentage), LWA_ALPHA);

    ShowWindow(hWndSelectRect, SW_SHOW);
    UpdateWindow(hWndSelectRect);
    SetForegroundWindow(hWndSelectRect);

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    hWndSelectRect = nullptr;
}

INT_PTR CALLBACK PopupMessageProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    UNREFERENCED_PARAMETER(lParam);
    switch (message) {
    case WM_DESTROY:
        hWndPopupMessage = nullptr;
        PostQuitMessage(0);
        return 0L;

    case WM_PAINT:
    {
        RECT rc;
        HFONT hFont,hTmp;
        HBRUSH hBrush;
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);
        // All painting occurs here, between BeginPaint and EndPaint.
        hBrush = CreateSolidBrush(RGB(0, 0, 0));
        FillRect(hdc, &ps.rcPaint, hBrush/*(HBRUSH) (COLOR_WINDOW+1)*/);
        //hBrush=CreateSolidBrush(RGB(255,0,0));
        //FillRect(hdc,&rc,hBrush);
        DeleteObject(hBrush);
        SetTextAlign(hdc, TA_LEFT|TA_TOP);
        SetTextColor(hdc, RGB(255,255,255));
        hFont = CreateFont(20,0,0,0,FW_BOLD,0,0,0,0,0,0,PROOF_QUALITY,VARIABLE_PITCH|FF_MODERN,nullptr);
        hTmp = (HFONT)SelectObject(hdc,hFont);
        SetBkMode(hdc, TRANSPARENT);
        //TextOutW(hdc, 20, 20, popupTitle.c_str(), popupTitle.size());
        rc = ps.rcPaint;
        rc.top += 10;
        rc.left += 10;
        rc.right -= 10;
        rc.bottom = rc.top + 20;
        DrawTextW(hdc, popupTitle.c_str(), popupTitle.size(), &rc, DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);
        DeleteObject(SelectObject(hdc,hTmp));
        hFont = CreateFont(18,0,0,0,FW_NORMAL,0,0,0,0,0,0,PROOF_QUALITY,VARIABLE_PITCH|FF_MODERN,nullptr);
        hTmp = (HFONT)SelectObject(hdc,hFont);
        //TextOutW(hdc, 20, 50, popupText.c_str(), popupText.size());
        rc.top = rc.bottom + 6;
        rc.bottom = ps.rcPaint.bottom - 10;
        DrawTextW(hdc, popupText.c_str(), popupText.size(), &rc, DT_WORDBREAK|DT_NOPREFIX);
        DeleteObject(SelectObject(hdc,hTmp));
        EndPaint(hWnd, &ps);
    }
        return 0;
    }
    return (LRESULT) DefWindowProc(hWnd, message, wParam, lParam);
}

void initShowPopupMessageWindow() {
    static bool registered;
    if (registered)
        return;

    HINSTANCE hInstance = GetModuleHandle(nullptr);

    WNDCLASSEX wc;
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = 0;
    wc.lpfnWndProc = PopupMessageProc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = hInstance;
    wc.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH) (COLOR_WINDOW + 1);
    wc.lpszMenuName = nullptr;
    wc.lpszClassName = showPopupMessageWindowClass;
    wc.hIconSm = LoadIcon(nullptr, IDI_APPLICATION);
    if (!RegisterClassEx(&wc)) {
        LOG(ERROR) << "Failed to register class" << getErrorMessage();
        return;
    }
    registered = true;
}

void popupMessageWindow() {
    HINSTANCE hInstance = GetModuleHandle(nullptr);

    MONITORINFOEX monitorInfo;
    monitorInfo.cbSize = sizeof(monitorInfo);
    GetMonitorInfo(MonitorFromWindow(nullptr, MONITOR_DEFAULTTOPRIMARY), &monitorInfo);
    cv::Size winSize(300, 150);
    cv::Size winOffset(60, 80);
    cv::Rect monitorRect = fromRECT(monitorInfo.rcMonitor);
    cv::Rect popupRect = cv::Rect(monitorRect.width - winOffset.width - winSize.width, monitorRect.y + winOffset.height, winSize.width, winSize.height);

    hWndPopupMessage = CreateWindowEx(
            WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TRANSPARENT,
            showPopupMessageWindowClass,
            L"",
            WS_POPUP | WS_DISABLED,
            popupRect.x,
            popupRect.y,
            popupRect.width,
            popupRect.height,
            nullptr, nullptr, hInstance, nullptr
    );

    if (!hWndPopupMessage) {
        LOG(ERROR) << "Failed to create window" << getErrorMessage();
        return;
    }

    //set window background to white
    HBRUSH hbr = CreateSolidBrush(RGB(0, 0, 0));
    SetClassLongPtr(hWndPopupMessage, GCLP_HBRBACKGROUND, (LONG_PTR) hbr);
    // Set transparency (25% opaque)
    float transparency_percentage = 0.60f;
    SetLayeredWindowAttributes(hWndPopupMessage, 0, (BYTE) (255 * transparency_percentage), LWA_ALPHA);

    ShowWindow(hWndPopupMessage, SW_SHOW);
    UpdateWindow(hWndPopupMessage);

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    hWndPopupMessage = nullptr;
}

} // namespace UI
