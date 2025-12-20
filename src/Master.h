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
    Start,
    PauseResume,
    Stop,
    Autopilot,
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
    Timestamp timestamp;
    const widget::Screen* screen {nullptr};
    const widget::BaseDialog* widget {nullptr};
    const widget::Widget* focused {nullptr};
    bool autopilot {false};
    friend std::ostream& operator<<(std::ostream& os, const UIState& obj);
    std::string to_string() const;
};

struct DetectRequest {
    DetectLevel level;
    UIState* uiState;
    ResolvedEnv* rEnv;
    CompassInfo* compass;
    cv::Mat* colorImage;
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

    bool initialize(int argc, char* argv[]);
    void shutdown();
    void loop();
    bool isGameForeground() const;
    bool setGameForeground();
    bool setGameMouseCapture();
    bool setDetectStream(DetectLevel);
    bool detectEDState(DetectLevel);
    const UIState& lastEDState() { return mLastUIState; }
    cv::Point cvtReferenceToDesktop(const cv::Point& point) const;
    cv::Rect resolveWidgetReferenceRect(const std::string& name, const ResolvedEnv& rEnv) const;
    int getDefaultKeyHoldTime() const { return Cfg.defaultKeyHoldTime; }
    int getDefaultKeyAfterTime() const { return Cfg.defaultKeyAfterTime; }
    int getSearchRegionExtent() const { return Cfg.searchRegionExtent; }

    int canSell(Commodity* commodity) const;
    int canBuy(Commodity* commodity) const;
    static const Commodity* getLabelCommodity(ResolvedEnv& rEnv, const cv::Mat& grayImage, const std::string& lbl_name);
    static bool approximateListOfCommodities(ResolvedEnv& rEnv, const cv::Mat& grayImage, const std::string& lst_name, const std::vector<Commodity*>& table, std::vector<CommodityMatch>* verify = nullptr);

    void pushCommand(Command cmd);
    void pushDetectRequest(std::promise<bool>&& p, DetectRequest&& req);
    void pushDevRectScreenshotCommand(cv::Rect rect);

    static const Commodity* ocrMarketRowCommodity(ResolvedEnv& rEnv, const cv::Mat& grayImage, ClassifiedRect* cr, int min_conf);

    const widget::Widget* getCfgItem(std::string state) const;
private:
    friend class Configuration;
    friend class UIState;

    Master();
    ~Master();

    std::string initializeInternal(std::string ocr_dir);

    static void tradingKbHook(int code, int scancode, int flags, const std::string& name);

    typedef std::unique_ptr<CommandEntry> pCommand;

    void pushCommand(CommandEntry* cmd);
    void popCommand(pCommand& cmd);

    Capturer* getCapturer();
    void resetCapturer();

    bool stopAITask();
    bool autopilotAITask();

    bool captureWindow(ClassifyEnv& env);
    void processDetectRequest(pCommand& cmd);
    void debugDetectEDState();
    bool debugRectScreenshot(pCommand& cmd);
    bool debugWindow();
    bool debugWindowUpdate(ClassifyEnv& cEnv, ClassifyEnv& wEnv);

    std::unique_ptr<widget::Root> mScreensRoot;
    HWND hWndED;

    Capturer* mCapturer {nullptr};
    UIState mLastUIState;
    ClassifyEnv mClassifyEnv;
    ClassifyEnv mWarpedEnv;
    bool mDuplicateToDebugWindow {false};
    DetectLevel mDetectLevelStream {DetectLevel::None};
    std::deque<std::chrono::time_point<std::chrono::steady_clock>> mStreamFramePoints;
    std::chrono::milliseconds mLoopWakeup;
    std::unique_ptr<detect::CompassDetector> mCompassDetector;

    std::queue<pCommand> mCommandQueue;
    std::mutex mCommandMutex;
    std::condition_variable mCommandCond;

    int mSellChunk {1};
};

extern Master& Mgr;

#endif //EDROBOT_MASTER_H
