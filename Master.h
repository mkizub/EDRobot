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
#include <unordered_map>
//#define JSON_HAS_CPP_17 1
//#include "json.hpp"
#include "json5pp.hpp"

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

namespace cfg {
class Item;
class Screen;
}

class Master {
public:
    Master();
    ~Master();
    void showStartDialog(int argc, char *argv[]);

    void loop();
    const std::string& getEDState(const std::string& expectedState);
    const json5pp::value& getAction(const std::string& action);
    int getDefaultKeyHoldTime() const { return defaultKeyHoldTime; }
    int getDefaultKeyAfterTime() const { return defaultKeyAfterTime; }

    cv::Mat getGrayScreenshot();

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
    cfg::Item* getCfgItem(std::string state);
    cfg::Item* matchWithSubItems(cfg::Item* item);
    bool matchItem(cfg::Item* item);

    int defaultKeyHoldTime = 35;
    int defaultKeyAfterTime = 50;
    std::vector<std::unique_ptr<cfg::Screen>> mScreens;
    std::map<std::string,json5pp::value> mActions;
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
