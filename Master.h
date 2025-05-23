//
// Created by mkizub on 23.05.2025.
//

#pragma once

#ifndef EDROBOT_MASTER_H
#define EDROBOT_MASTER_H

#include <queue>
#include <mutex>
#include <condition_variable>
#include <windef.h>
#include <opencv2/core/mat.hpp>

class Task;

namespace Gdiplus {
    class Bitmap;
}

enum class Command {
    NoOp,
    TaskFinished,
    Start,
    Pause,
    Stop,
    Shutdown,
};

enum class EDState {
    Unknown,
    DockedStationL,
    DockedStationM,
    MarketBuy,
    MarketBuyDialog,
    MarketSell,
    MarketSellDialog,
};

class Master {
public:
    Master();
    ~Master();
    void showStartDialog(int argc, char *argv[]);

    void loop();
    EDState getEDState();

    cv::Mat getGrayScreenshot();
    void stopCurrentTask();

private:
    void tradingKbHook(int code, int scancode, int flags);
    void pushCommand(Command cmd);
    Command popCommand();

    bool captureWindow();

    bool startTrade();
    bool stopTrade();
    void clearCurrentTask();
    bool isForeground();

    bool askSellInput();

    HWND hWndED;
    cv::Mat grayED;

    std::queue<Command> mCommandQueue;
    std::mutex mCommandMutex;
    std::condition_variable mCommandCond;

    std::thread currentTaskThread;
    std::unique_ptr<Task> currentTask;

    int mSells;
    int mItems;
};


#endif //EDROBOT_MASTER_H
