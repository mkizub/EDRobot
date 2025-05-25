#include "UI.h"
#include "easylogging++.h"
#include "Utils.h"

#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
// Windows Header Files
#include <windows.h>
#include <memory.h>
#include "ui/resource.h"


static INT_PTR CALLBACK  AboutProc(HWND, UINT, WPARAM, LPARAM);
static INT_PTR CALLBACK  SellProc(HWND, UINT, WPARAM, LPARAM);
static int sellResult;
static int sellTimes;
static int sellItems;

int UI::showStartupDialog() {
    HINSTANCE hInstance = GetModuleHandle(nullptr);
    DialogBox(hInstance, MAKEINTRESOURCE(IDD_ABOUTBOX), NULL, AboutProc);
    return UI::RES_OK;
}

int UI::askSellInput(int& sells, int& items) {
    sellTimes = sells;
    sellItems = items;
    HINSTANCE hInstance = GetModuleHandle(nullptr);
    DialogBox(hInstance, MAKEINTRESOURCE(IDD_SELLBOX), NULL, SellProc);
    sells = sellTimes;
    items = sellItems;
    return sellResult;
}

#define MAX_LOADSTRING 100

// Global Variables:
//HINSTANCE hInst;                                // current instance
//WCHAR szTitle[MAX_LOADSTRING];                  // The title bar text
//WCHAR szWindowClass[MAX_LOADSTRING];            // the main window class name
//
//// Forward declarations of functions included in this code module:
//ATOM                MyRegisterClass(HINSTANCE hInstance);
//BOOL                InitInstance(HINSTANCE, int);
//LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
//
//void UI::showStartupDialog() {
//    HINSTANCE hInstance = GetModuleHandle(nullptr);
//    DialogBox(hInstance, MAKEINTRESOURCE(IDD_ABOUTBOX), NULL, About);
//
//    // Initialize global strings
//    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
//    LoadStringW(hInstance, IDC_DIALOGS, szWindowClass, MAX_LOADSTRING);
//    MyRegisterClass(hInstance);
//
//    // Perform application initialization:
//    if (!InitInstance(hInstance, SW_SHOWNORMAL))
//        return;
//
//    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_DIALOGS));
//
//    MSG msg;
//
//    // Main message loop:
//    while (GetMessage(&msg, nullptr, 0, 0))
//    {
//        if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
//        {
//            TranslateMessage(&msg);
//            DispatchMessage(&msg);
//        }
//    }
//}

//
//  FUNCTION: MyRegisterClass()
//
//  PURPOSE: Registers the window class.
//
//ATOM MyRegisterClass(HINSTANCE hInstance)
//{
//    WNDCLASSEXW wcex;
//
//    wcex.cbSize = sizeof(WNDCLASSEX);
//
//    wcex.style          = CS_HREDRAW | CS_VREDRAW;
//    wcex.lpfnWndProc    = WndProc;
//    wcex.cbClsExtra     = 0;
//    wcex.cbWndExtra     = 0;
//    wcex.hInstance      = hInstance;
//    wcex.hIcon          = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_DIALOGS));
//    wcex.hCursor        = LoadCursor(nullptr, IDC_ARROW);
//    wcex.hbrBackground  = (HBRUSH)(COLOR_WINDOW+1);
//    wcex.lpszMenuName   = MAKEINTRESOURCEW(IDC_DIALOGS);
//    wcex.lpszClassName  = szWindowClass;
//    wcex.hIconSm        = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));
//
//    return RegisterClassExW(&wcex);
//}

//
//   FUNCTION: InitInstance(HINSTANCE, int)
//
//   PURPOSE: Saves instance handle and creates main window
//
//   COMMENTS:
//
//        In this function, we save the instance handle in a global variable and
//        create and display the main program window.
//
//BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
//{
//    hInst = hInstance; // Store instance handle in our global variable
//
//    HWND hWnd = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
//                              CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, hInstance, nullptr);
//
//    if (!hWnd)
//    {
//        LOG(ERROR) << "Cannot create window: " << getErrorMessage();
//        return FALSE;
//    }
//
//    ShowWindow(hWnd, nCmdShow);
//    UpdateWindow(hWnd);
//
//    return TRUE;
//}

//
//  FUNCTION: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  PURPOSE: Processes messages for the main window.
//
//  WM_COMMAND  - process the application menu
//  WM_PAINT    - Paint the main window
//  WM_DESTROY  - post a quit message and return
//
//
//LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
//{
//    switch (message)
//    {
//        case WM_COMMAND:
//        {
//            int wmId = LOWORD(wParam);
//            // Parse the menu selections:
//            switch (wmId)
//            {
//                case IDM_ABOUT:
//                    DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
//                    break;
//                case IDM_EXIT:
//                    DestroyWindow(hWnd);
//                    break;
//                default:
//                    return DefWindowProc(hWnd, message, wParam, lParam);
//            }
//        }
//            break;
//        case WM_PAINT:
//        {
//            PAINTSTRUCT ps;
//            HDC hdc = BeginPaint(hWnd, &ps);
//            // TODO: Add any drawing code that uses hdc here...
//            EndPaint(hWnd, &ps);
//        }
//            break;
//        case WM_DESTROY:
//            PostQuitMessage(0);
//            break;
//        default:
//            return DefWindowProc(hWnd, message, wParam, lParam);
//    }
//    return 0;
//}

// Message handler for about box.
INT_PTR CALLBACK AboutProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
        case WM_INITDIALOG:
            return (INT_PTR)TRUE;

        case WM_COMMAND:
            if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL) {
                EndDialog(hDlg, LOWORD(wParam));
                return (INT_PTR)TRUE;
            }
            break;
    }
    return (INT_PTR)FALSE;
}

// Message handler for sell box.
INT_PTR CALLBACK SellProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
        case WM_INITDIALOG:
            SetDlgItemText(hDlg, IDC_EDIT_SELLS, std::to_string(sellTimes).c_str());
            SetDlgItemText(hDlg, IDC_EDIT_ITEMS, std::to_string(sellItems).c_str());
            return (INT_PTR)TRUE;

        case WM_COMMAND:
            if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL || LOWORD(wParam) == IDEXIT) {
                WINBOOL translated;
                sellTimes = GetDlgItemInt(hDlg, IDC_EDIT_SELLS, &translated, FALSE);
                sellItems = GetDlgItemInt(hDlg, IDC_EDIT_ITEMS, &translated, FALSE);
                if (LOWORD(wParam) == IDOK)
                    sellResult = UI::RES_OK;
                else if (LOWORD(wParam) == IDEXIT)
                    sellResult = UI::RES_EXIT;
                else
                    sellResult = UI::RES_CANCEL;
                EndDialog(hDlg, LOWORD(wParam));
                return (INT_PTR)TRUE;
            }
            break;
    }
    return (INT_PTR)FALSE;
}
