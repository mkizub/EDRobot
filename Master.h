//
// Created by mkizub on 23.05.2025.
//

#pragma once

#ifndef EDROBOT_MASTER_H
#define EDROBOT_MASTER_H

#include <queue>
#include <condition_variable>
#include <thread>
#include <opencv2/core/mat.hpp>
#include "EDWidget.h"

class Task;

const int REFERENCE_SCREEN_WIDTH = 1920;
const int REFERENCE_SCREEN_HEIGHT = 1080;

namespace Gdiplus {
    class Bitmap;
}

enum class Command {
    NoOp,
    TaskFinished,
    Start,
    Pause,
    Stop,
    Calibrate,
    DebugTemplates,
    DebugButtons,
    DevRectSelect,
    DevRectScreenshot,
    Shutdown,
};

struct Calibrarion {
    Calibrarion() = default;
    // key values
    double dashboardGUIBrightness; // Options/Player/Custom.?.misc: <DashboardGUIBrightness Value="0.49999991" />
    double gammaOffset; // Options/Graphics/Settings.xml: <GammaOffset>1.200000</GammaOffset>
    int ScreenWidth;    // Options\Graphics\DisplaySettings.xml: <ScreenWidth>1920</ScreenWidth>
    int ScreenHeight;   // Options\Graphics\DisplaySettings.xml: <ScreenHeight>1080</ScreenHeight>
    // end of key values

    cv::Vec3b normalButtonLuv;
    cv::Vec3b focusedButtonLuv;
    cv::Vec3b disabledButtonLuv;
    cv::Vec3b activatedToggleLuv;
    friend std::ostream& operator<<(std::ostream& os, const Calibrarion& obj);
};

struct UIState {
    UIState() = default;
    void clear();
    const std::string& path() const;
    widget::Widget* widget;
    widget::Widget* focused;
    ClassifyEnv cEnv;
    friend std::ostream& operator<<(std::ostream& os, const UIState& obj);
};

class Master {
public:
    static Master& getInstance();

    int initialize(int argc, char* argv[]);
    void loop();
    const UIState& detectEDState();
    const UIState& lastEDState() { return mLastEDState; }
    bool isEDStateMatch(const std::string& state);
    const json5pp::value& getTaskActions(const std::string& action);
    cv::Rect resolveWidgetRect(const std::string& name);
    int getDefaultKeyHoldTime() const { return defaultKeyHoldTime; }
    int getDefaultKeyAfterTime() const { return defaultKeyAfterTime; }
    int getSearchRegionExtent() const { return searchRegionExtent; }

    void setDevScreenRect(cv::Rect rect) { mDevScreenRect = rect; mDevScaledRect = {}; }
    void setDevScaledRect(cv::Rect rect) { mDevScaledRect = rect; mDevScreenRect = {}; }
    void pushCommand(Command cmd);

    void setCalibrationResult(std::unique_ptr<Calibrarion>& calibration, bool save);

private:
    Master();
    ~Master();
    static void tradingKbHook(int code, int scancode, int flags, const std::string& name);
    Command popCommand();
    void parseShortcutConfig(Command command, const std::string& name, json5pp::value cfg);

    bool captureWindow(cv::Rect& captureRect, cv::Mat* grayImg, cv::Mat* colorImg = nullptr);

    bool preInitTask();
    bool startCalibration();
    bool startTrade();
    bool stopTrade();
    void clearCurrentTask();
    bool isForeground();
    static void runCurrentTask();

    void initializeClassifyEnv(ClassifyEnv& env);
    std::vector<std::string> parseState(const std::string& name);
    widget::Widget* detectFocused(const widget::Widget* parent);
    widget::Widget* getCfgItem(std::string state);
    widget::Widget* matchWithSubItems(widget::Widget* item);
    bool matchItem(widget::Widget* item);
    bool debugTemplates(widget::Widget* item, ClassifyEnv& env);
    bool debugMatchItem(widget::Widget* item, ClassifyEnv& env);
    bool debugButtons();
    cv::Rect calcScaledRect(cv::Rect screenRect);
    bool debugRectScreenshot(std::string name);
    void saveCalibration() const;
    bool loadCalibration();

    int defaultKeyHoldTime = 35;
    int defaultKeyAfterTime = 50;
    int searchRegionExtent = 10;
    std::map<std::pair<std::string,unsigned>, Command> keyMapping;
    std::unique_ptr<widget::Root> mScreensRoot;
    std::map<std::string,json5pp::value> mActions;
    HWND hWndED;
    cv::Mat colorED;
    cv::Mat grayED;
    cv::Rect mCaptureRect;
    cv::Rect mDevScreenRect;
    cv::Rect mDevScaledRect;
    UIState mLastEDState;
    std::unique_ptr<Calibrarion> mCalibration;
    std::unique_ptr<HistogramTemplate> mFocusedButtonDetector;
    std::shared_ptr<ConstRect> mFocusedDetectorRect;

    std::queue<Command> mCommandQueue;
    std::mutex mCommandMutex;
    std::condition_variable mCommandCond;

    std::thread currentTaskThread;
    std::unique_ptr<Task> currentTask;

    int mSells;
    int mItems;

};


#endif //EDROBOT_MASTER_H
