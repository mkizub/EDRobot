//
// Created by mkizub on 23.05.2025.
//

#pragma once

#ifndef EDROBOT_MASTER_H
#define EDROBOT_MASTER_H

namespace tesseract {
    class TessBaseAPI;
}
namespace ai {
    class AIManager;
}
enum class DetectLevel {
    None, Screen, Buttons, ListRows
};
enum class Command {
    NoOp,
    TaskFinished,
    Start,
    Pause,
    Resume,
    Stop,
    Autopilot,
    Calibrate,
    DebugTemplates,
    DebugWindow,
    DebugStream,
    DevRectSelect,
    DevRectScreenshot,
    ResetCapturer,
    DetectRequest,
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
    const std::string& screen_name() const;
    const std::string& focused_name() const;
    bool match(const std::string& state) const;
    std::vector<std::string> splitPath() const;
    bool valid {false};
    GuiFocus guiFocus {GuiFocus::None};
    const widget::Screen* screen {nullptr};
    const widget::BaseDialog* widget {nullptr};
    const widget::Widget* focused {nullptr};
    bool autopilot {false};
    friend std::ostream& operator<<(std::ostream& os, const UIState& obj);
    std::string to_string() const;
};

struct CompassInfo {
    CompassInfo() : hemisphere(-1), has_nav_target(false) {}
    float targetPitch;
    float targetYaw;
    float targetRoll;
    short hemisphere; // +1: front, -1: back, 0: invalid
    bool has_nav_target;
    dist_t nav_target_dist;
};
struct DetectRequest {
    DetectLevel level;
    UIState* uiState;
    ResolvedEnv* rEnv;
    CompassInfo* compass;
    cv::Mat* colorImage;
    cv::Mat* grayImage;
};

struct CommodityMatch {
    const Commodity* commodity;
    int ocr_conf;
    int fuzzy_conf;
};

class Master {
public:
    static const wchar_t ED_WINDOW_NAME[];
    static const wchar_t ED_WINDOW_CLASS[];
    static const wchar_t ED_WINDOW_EXE[];

    static Master& getInstance();

    int initialize(int argc, char* argv[]);
    void loop();
    bool isGameForeground();
    bool setGameForeground();
    bool setGameMouseCapture();
    bool setDetectStream(DetectLevel);
    bool detectEDState(DetectLevel);
    const UIState& lastEDState() { return mLastUIState; }
    const json5pp::value& getTaskActions(const std::string& action);
    cv::Rect resolveWidgetReferenceRect(const std::string& name) const;
    ai::AIManager* getAIManager() const { return mAIManager; }
    int getDefaultKeyHoldTime() const { return Cfg.defaultKeyHoldTime; }
    int getDefaultKeyAfterTime() const { return Cfg.defaultKeyAfterTime; }
    int getSearchRegionExtent() const { return Cfg.searchRegionExtent; }

    int canSell(Commodity* commodity) const;
    static const Commodity* getLabelCommodity(ResolvedEnv& rEnv, const cv::Mat& grayImage, const std::string& lbl_name);
    static bool approximateListOfCommodities(ResolvedEnv& rEnv, const cv::Mat& grayImage, const std::string& lst_name, const std::vector<Commodity*>& table, std::vector<CommodityMatch>* verify = nullptr);

    void pushCommand(Command cmd);
    void pushDetectRequest(std::promise<bool>&& p, DetectRequest&& req);
    void pushDevRectScreenshotCommand(cv::Rect rect);

    static const Commodity* ocrMarketRowCommodity(ResolvedEnv& rEnv, const cv::Mat& grayImage, ClassifiedRect* cr);

    const widget::Widget* getCfgItem(std::string state) const;
private:
    friend class Configuration;
    friend class UIState;

    Master();
    ~Master();

    void initializeInternal(std::string ocr_dir);

    static void tradingKbHook(int code, int scancode, int flags, const std::string& name);

    typedef std::unique_ptr<CommandEntry> pCommand;

    void pushCommand(CommandEntry* cmd);
    void popCommand(pCommand& cmd);

    Capturer* getCapturer();
    void resetCapturer();

    bool preInitTask();
    bool startCalibration();
//    bool startTrade();
    bool pauseAITask();
    bool resumeAITask();
    bool stopAITask();
    bool autopilotAITask();

    bool captureWindow(ClassifyEnv& env);
    widget::Widget* matchWithSubItems(widget::Widget* item);
    void processDetectRequest(pCommand& cmd);
    bool matchItem(widget::Widget* item);
    widget::Widget* debugTemplates(widget::Widget* item, ClassifyEnv* env);
    bool debugMatchItem(widget::Widget* item, ClassifyEnv& env);
    bool debugRectScreenshot(pCommand& cmd);
    bool debugWindow();
    bool debugWindowUpdate();

    std::unique_ptr<widget::Root> mScreensRoot;
    std::map<std::string,json5pp::value> mActions;
    HWND hWndED;

    cv::UMat mCapturedED;
    Capturer* mCapturer {nullptr};
    //cv::Rect mCaptureRect; // in captured image coordinated
    //cv::Rect mMonitorRect; // in virtual desktop coordinated
    UIState mLastUIState;
    ClassifyEnv mClassifyEnv;
    bool mDuplicateToDebugWindow {false};
    DetectLevel mDetectLevelStream {DetectLevel::None};
    std::deque<std::chrono::time_point<std::chrono::steady_clock>> mStreamFramePoints;
    std::chrono::milliseconds mLoopWakeup;
    std::unique_ptr<detect::CompassDetector> mCompassDetector;
    ai::AIManager* mAIManager {nullptr};

    std::queue<pCommand> mCommandQueue;
    std::mutex mCommandMutex;
    std::condition_variable mCommandCond;

    int mSellChunk {1};
};

extern Master& Mgr;

#endif //EDROBOT_MASTER_H
