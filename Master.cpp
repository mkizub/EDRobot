//
// Created by mkizub on 23.05.2025.
//

#include "Master.h"
#include "TradeItems.h"
#include "easylogging++.h"
#include "Keyboard.h"
#include "Task.h"
#include "Template.h"
#include "Utils.h"
#include <opencv2/opencv.hpp>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
//#include <gdiplus.h>
//#pragma comment(lib, "Gdiplus.lib")

const char ED_WINDOW_NAME[] = "Elite - Dangerous (CLIENT)";
const char ED_WINDOW_CLASS[] = "FrontierDevelopmentsAppWinClass";
const char ED_WINDOW_EXE[] = "EliteDangerous64.exe";

ImageTemplate ED_TEMPLATE_AT_MARKET({92, 57, "templates/at_market.png"});
ImageTemplate ED_TEMPLATE_AT_MARKET_SELL({245, 172, "templates/at_market_sell.png"});
ImageTemplate ED_TEMPLATE_AT_MARKET_BUY({245, 172, "templates/at_market_buy.png"});
ImageTemplate ED_TEMPLATE_AT_MARKET_DIALOG({507, 714, 300, "templates/at_market_dialog.png"});

Master::Master() {
    mSells = 3;
    mItems = 5;
    hWndED = FindWindow(ED_WINDOW_CLASS, ED_WINDOW_NAME);
    keyboard::start([this](int code, int scancode, int flags) { tradingKbHook(code, scancode, flags); });
}

Master::~Master() {
    keyboard::stop();
    stopTrade();
}


void Master::loop() {
    while (true) {
        switch (popCommand()) {
            case Command::NoOp:
                break;
            case Command::TaskFinished:
                clearCurrentTask();
                break;
            case Command::Start:
                startTrade();
                break;
            case Command::Pause:
                stopTrade();
                break;
            case Command::Stop:
                stopTrade();
                break;
            case Command::Shutdown:
                clearCurrentTask();
                return;
        }
    }
}

bool Master::isForeground() {
    return hWndED && hWndED == GetForegroundWindow();
}

void Master::tradingKbHook(int code, int scancode, int flags) {
    (void)scancode;
    (void)flags;

    switch(code) {
        case VK_ESCAPE:
            LOG(INFO) << "ESC pressed";
            pushCommand(Command::Stop);
            return;
        case VK_SNAPSHOT:
            LOG(INFO) << "PrintScreen pressed";
            pushCommand(Command::Start);
            return;
        case VK_PAUSE:
            LOG(INFO) << "Pause pressed";
            pushCommand(Command::Pause);
            return;
        default:
            LOG(DEBUG) << "Unknown command";
    }
}

void Master::pushCommand(Command cmd) {
    std::unique_lock<std::mutex> lock(mCommandMutex);
    mCommandQueue.push(cmd);
    mCommandCond.notify_one();
}

Command Master::popCommand() {
    std::unique_lock<std::mutex> lock(mCommandMutex);
    mCommandCond.wait(lock, [this]() { return !mCommandQueue.empty(); });
    Command cmd = mCommandQueue.front();
    mCommandQueue.pop();
    return cmd;
}

bool Master::startTrade() {
    LOG(INFO) << "Start trading";
    hWndED = FindWindow(ED_WINDOW_CLASS, ED_WINDOW_NAME);
    if (!hWndED) {
        LOG(INFO) << "Window [class " << ED_WINDOW_CLASS << "; name " << ED_WINDOW_NAME << "] not found" ;
        return false;
    }
    if (currentTask) {
        LOG(ERROR) << "Exiting current task";
        clearCurrentTask();
    }
    if (!askSellInput())
        return false;
    if (!isForeground()) {
        SetForegroundWindow(hWndED);
        Sleep(100);
        if (!isForeground()) {
            LOG(ERROR) << "ED is not foreground";
            return false;
        }
    }
    LOG(ERROR) << "Staring new trade task";
    currentTask.reset(new TaskSell(*this, mSells, mItems));
    currentTaskThread = std::thread([this](){
        bool ok = currentTask->run();
        pushCommand(Command::TaskFinished);
    });
    return true;
}

bool Master::stopTrade() {
    LOG(INFO) << "Stop trading";
    clearCurrentTask();
    return false;
}

void Master::clearCurrentTask() {
    if (currentTask) {
        currentTask->stop();
        if (currentTaskThread.joinable())
            currentTaskThread.join();
        currentTask.reset();
    }
    else if (currentTaskThread.joinable()) {
        currentTaskThread.join();
    }
}


EDState Master::getEDState() {
    // make screenshot
    if (!captureWindow()) {
        LOG(ERROR) << "Cannot capture screen";
        return EDState::Unknown;
    }
    // check market
    if (ED_TEMPLATE_AT_MARKET.match(this)) {
        LOG(ERROR) << "At market";
        if (ED_TEMPLATE_AT_MARKET_SELL.match(this)) {
            LOG(ERROR) << "At market sell";
            if (ED_TEMPLATE_AT_MARKET_DIALOG.match(this)) {
                LOG(ERROR) << "At market sell dialog";
                return EDState::MarketSellDialog;
            } else {
                return EDState::MarketSell;
            }
        }
        if (ED_TEMPLATE_AT_MARKET_BUY.match(this)) {
            LOG(ERROR) << "At market buy";
            if (ED_TEMPLATE_AT_MARKET_DIALOG.match(this)) {
                LOG(ERROR) << "At market buy dialog";
                return EDState::MarketBuyDialog;
            } else {
                return EDState::MarketBuy;
            }
        }
    } else {
        LOG(ERROR) << "Unknown screen";
    }

    return EDState::Unknown;
}

bool Master::captureWindow() {
    if (!isForeground())
        return false;
    //CaptureScreenDebug(L"screenshot.png");

    // Get screen dimensions
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);

    // Create a device context for the entire screen
    HDC hdcScreen = GetDC(NULL);
    HDC hdcMem = CreateCompatibleDC(hdcScreen);
    HBITMAP hBitmap = CreateCompatibleBitmap(hdcScreen, screenWidth, screenHeight);
    HBITMAP hOldBitmap = (HBITMAP)SelectObject(hdcMem, hBitmap);

    WINBOOL ok = BitBlt(hdcMem, 0, 0, screenWidth, screenHeight, hdcScreen, 0, 0, SRCCOPY);
    if (!ok)
        LOG(ERROR) << "BitBlt failed: " << getErrorMessage();
    //Gdiplus::Bitmap bitmap(hBitmap, NULL);
    //CLSID pngClsid;
    //GetEncoderClsid(L"image/png", &pngClsid);
    // Save the bitmap as PNG
    //bitmap.Save(L"screenshot.png", &pngClsid, NULL);

    // Create OpenCV Mat from bitmap
    BITMAPINFOHEADER bmi = { 0 };
    bmi.biSize = sizeof(BITMAPINFOHEADER);
    bmi.biWidth = screenWidth;
    bmi.biHeight = -screenHeight;  // Negative height to flip the image vertically
    bmi.biPlanes = 1;
    bmi.biBitCount = 32;
    bmi.biCompression = BI_RGB;

    cv::Mat capturedImage(screenHeight, screenWidth, CV_8UC4);
    int lines = GetDIBits(hdcMem, hBitmap, 0, screenHeight, capturedImage.data, (BITMAPINFO*)&bmi, DIB_RGB_COLORS);
    if (lines != screenHeight)
        LOG(ERROR) << "GetDIBits failed: " << getErrorMessage();
    //cv::imshow("Screenshot", capturedImage);
    //cv::waitKey(0); // Wait for a keystroke in the window

    // Convert to grayscale
    cv::Mat grayImage;
    cv::cvtColor(capturedImage, grayImage, cv::COLOR_RGBA2GRAY);
    grayED = grayImage;

    // Clean up
    SelectObject(hdcMem, hOldBitmap);
    DeleteObject(hBitmap);
    DeleteDC(hdcMem);
    ReleaseDC(NULL, hdcScreen);

    //cv::imshow("Screenshot", grayED);
    //cv::waitKey(0); // Wait for a keystroke in the window

    return true;
}

cv::Mat Master::getGrayScreenshot() {
    return grayED;
}

#include <FL/Fl.H>
#include <FL/Fl_Window.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_Int_Input.H>
#include <FL/Fl_Output.H>
#include <FL/Fl_Text_Buffer.H>
#include <FL/Fl_Text_Display.H>
#include <FL/fl_message.H>

void get_input_cb(Fl_Widget* btn, void* data) {
    Fl_Output* output = (Fl_Output*)data;
    const char* input_value = fl_input("Enter some text:", "");

    if (input_value != nullptr) {
        output->value(input_value);
    }
}

static int dialogResult;
struct CloseData {
    Fl_Window* window;
    int code;
};

static void close_window_cb(Fl_Widget* widget, void* data) {
    CloseData* cd = (CloseData*)data;
    dialogResult = cd->code;
    cd->window->hide();
}

void Master::showStartDialog(int argc, char *argv[]) {
    Fl_Window window(250, 130, "ED Robot");
    CloseData cd{ &window, 0 };
    Fl_Text_Buffer*  buff = new Fl_Text_Buffer();
    Fl_Text_Display* disp = new Fl_Text_Display(10, 10, 230, 80);
    Fl_Button*       btn  = new Fl_Button      (20, 90,  60, 30, "Ok");
    btn->callback(close_window_cb, &cd);
    btn->shortcut(FL_Enter);
    disp->buffer(buff);
    buff->text("Press 'Print Screen' key to start selling\nPress 'Esc' to stop");
    window.end();
    window.show(argc, argv);
    Fl::run();
}

bool Master::askSellInput() {
    dialogResult = 1;
    Fl_Window window(250, 160, "Sell Items");
    Fl_Int_Input* in_times = new Fl_Int_Input(100,  10, 100, 30, "Sell times: ");
    in_times->value(mSells);
    in_times->insert_position(0, in_times->value() ? strlen(in_times->value()) : 0);
    in_times->selection_color(0xDDDDDD00);
    Fl_Int_Input* in_items = new Fl_Int_Input(100,  50, 100, 30, "By N items: ");
    in_items->value(mItems);
    in_items->insert_position(0, in_items->value() ? strlen(in_items->value()) : 0);
    in_times->selection_color(0xDDDDDD00);
    Fl_Button* btn_ok      = new Fl_Button   (20,   80,  60, 30, "Ok");
    Fl_Button* btn_cancel  = new Fl_Button   (100,  80, 100, 30, "Cancel");
    Fl_Button* btn_exit    = new Fl_Button   (20,  120, 100, 30, "Exit EDRobot");
    CloseData cd_ok{ &window, 0 };
    CloseData cd_cancel{ &window, 1 };
    CloseData cd_exit{ &window, 2 };
    btn_ok->callback(close_window_cb, &cd_ok);
    btn_cancel->callback(close_window_cb, &cd_cancel);
    btn_exit->callback(close_window_cb, &cd_exit);
    btn_ok->shortcut(FL_Enter);
    btn_cancel->shortcut(FL_Escape);
    window.end();
    window.position(1800, 200);
    window.show();
    while (Fl::check()) {
        // check exit
    }
    if (dialogResult == 1)
        return false;
    if (dialogResult == 2) {
        pushCommand(Command::Shutdown);
        return false;
    }

    int n_times = in_times->ivalue();
    if (n_times <= 0)
        return false;
    mSells = n_times;

    int n_items = in_items->ivalue();
    if (n_items <= 0)
        return false;
    mItems = n_items;

    return true;
}
