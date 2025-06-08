//
// Created by mkizub on 23.05.2025.
//

#pragma once

#ifndef EDROBOT_MASTER_H
#define EDROBOT_MASTER_H

#include <queue>
#include <condition_variable>
#include <thread>
#include "opencv2/core/mat.hpp"
#include "EDWidget.h"

class Task;

enum class DetectLevel {
    Buttons, ListRows, ListRowStates, ListOcrFocusedRow, ListOcrAllRows
};
enum class Command {
    NoOp,
    TaskFinished,
    Start,
    Pause,
    Stop,
    UserNotify,
    Calibrate,
    DebugTemplates,
    DebugButtons,
    DevRectSelect,
    DevRectScreenshot,
    Shutdown,
};

struct CommandEntry {
    CommandEntry(Command command) : command(command) {}
    virtual ~CommandEntry() = default;
    const Command command;
};

struct UIState {
    UIState() = default;
    void clear();
    const std::string& path() const;
    const widget::Widget* widget;
    const widget::Widget* focused;
    ClassifyEnv cEnv;
    friend std::ostream& operator<<(std::ostream& os, const UIState& obj);
    std::string to_string() const;
};

class Master {
public:
    static Master& getInstance();

    int initialize(int argc, char* argv[]);
    void loop();
    const UIState& detectEDState(DetectLevel);
    const UIState& lastEDState() { return mLastEDState; }
    bool isEDStateMatch(const std::string& state);
    const json5pp::value& getTaskActions(const std::string& action);
    cv::Rect resolveWidgetRect(const std::string& name);
    Configuration* getConfiguration() const { return mConfiguration.get(); }
    int getDefaultKeyHoldTime() const { return mConfiguration->defaultKeyHoldTime; }
    int getDefaultKeyAfterTime() const { return mConfiguration->defaultKeyAfterTime; }
    int getSearchRegionExtent() const { return mConfiguration->searchRegionExtent; }

    void setDevScreenRect(cv::Rect rect) { mDevScreenRect = rect; mDevScaledRect = {}; }
    void setDevScaledRect(cv::Rect rect) { mDevScaledRect = rect; mDevScreenRect = {}; }

    void pushCommand(Command cmd);
    void notifyProgress(const std::string& title, const std::string& text);
    void notifyError(const std::string& title, const std::string& text);

    void setCalibrationResult(const std::array<cv::Vec3b,4>& buttonLuv, const std::array<cv::Vec3b,4>& lstRowLuv);

private:
    friend class Configuration;

    Master();
    ~Master();

    int initializeInternal();
    void initButtonStateDetector();

    static void tradingKbHook(int code, int scancode, int flags, const std::string& name);

    typedef std::unique_ptr<CommandEntry> pCommand;

    void pushCommand(CommandEntry* cmd);
    void popCommand(pCommand& cmd);

    bool captureWindow(cv::Rect& captureRect, cv::Mat& colorImg, cv::Mat& grayImg);

    void showNotification(pCommand& cmd);
    bool preInitTask(bool checkCalibration=true);
    bool startCalibration();
    bool startTrade();
    bool stopTrade();
    void clearCurrentTask();
    bool isForeground();
    static void runCurrentTask();

    const ClassifyEnv::ResultListRow* getFocusedRow(const std::string& lst_name);
    std::vector<std::string> parseState(const std::string& name);
    double detectButtonState(const widget::Widget* item);
    void detectListState(const widget::List* item, DetectLevel level);
    widget::Widget* detectAllButtonsStates(const widget::Widget* parent, DetectLevel level);
    widget::Widget* getCfgItem(std::string state);
    widget::Widget* matchWithSubItems(widget::Widget* item);
    bool matchItem(widget::Widget* item);
    bool debugTemplates(widget::Widget* item, ClassifyEnv& env);
    bool debugMatchItem(widget::Widget* item, ClassifyEnv& env);
    void drawButton(widget::Widget* item);
    bool debugButtons();
    bool debugRectScreenshot(std::string name);

    std::unique_ptr<widget::Root> mScreensRoot;
    std::map<std::string,json5pp::value> mActions;
    HWND hWndED;
    cv::Mat colorED;
    cv::Mat grayED;
    cv::Rect mCaptureRect; // in virtual desktop coordinated
    cv::Rect mDevScreenRect;
    cv::Rect mDevScaledRect;
    UIState mLastEDState;
    std::unique_ptr<Configuration> mConfiguration;
    std::unique_ptr<HistogramTemplate> mButtonStateDetector;
    std::unique_ptr<HistogramTemplate> mLstRowStateDetector;

    std::queue<pCommand> mCommandQueue;
    std::mutex mCommandMutex;
    std::condition_variable mCommandCond;

    std::thread currentTaskThread;
    std::unique_ptr<Task> currentTask;
};


#endif //EDROBOT_MASTER_H
