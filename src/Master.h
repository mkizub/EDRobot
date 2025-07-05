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
    None, Screen, Buttons, ListRows, ListOcrFocusedRow
};
enum class Command {
    NoOp,
    TaskFinished,
    Start,
    Pause,
    Resume,
    Stop,
    Calibrate,
    DebugTemplates,
    DebugButtons,
    DebugFindAllCommodities,
    DebugCompass,
    DebugWindow,
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
    CompassInfo() : hemisphere(-1) {}
    short targetPitch;
    short targetYaw;
    short targetRoll;
    short hemisphere; // 0: front, 1: back, -1: invalid
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
    bool detectEDState(DetectLevel);
    const UIState& lastEDState() { return mLastUIState; }
    const json5pp::value& getTaskActions(const std::string& action);
    cv::Rect resolveWidgetReferenceRect(const std::string& name);
    Configuration* getConfiguration() const { return mConfiguration; }
    ai::AIManager* getAIManager() const { return mAIManager; }
    int getDefaultKeyHoldTime() const { return mConfiguration->defaultKeyHoldTime; }
    int getDefaultKeyAfterTime() const { return mConfiguration->defaultKeyAfterTime; }
    int getSearchRegionExtent() const { return mConfiguration->searchRegionExtent; }

    int canSell(Commodity* commodity) const;
    const Commodity* getLabelCommodity(const std::string& lbl_name);
    static bool approximateListOfCommodities(ResolvedEnv& rEnv, const cv::Mat& grayImage, const std::string& lst_name, const std::vector<Commodity*>& table, std::vector<CommodityMatch>* verify = nullptr);

    void pushCommand(Command cmd);
    void pushDetectRequest(std::promise<bool>&& p, DetectRequest&& req);
    void pushDevRectScreenshotCommand(cv::Rect rect);

    tesseract::TessBaseAPI* getTesseractApi() { return mTesseractApiForMarket.get(); }
    static int ocrMarketText(const cv::Mat& grayImage, cv::Rect, std::string& text, std::optional<bool> invert={});
    static const Commodity* ocrMarketRowCommodity(ResolvedEnv& rEnv, const cv::Mat& grayImage, ClassifiedRect* cr);

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
    bool startTrade();
    bool pauseAITask();
    bool resumeAITask();
    bool stopAITask();

    bool captureWindow(ClassifyEnv& env);
    widget::Widget* getCfgItem(std::string state);
    widget::Widget* matchWithSubItems(widget::Widget* item);
    void processDetectRequest(pCommand& cmd);
    bool matchItem(widget::Widget* item);
    widget::Widget* debugTemplates(widget::Widget* item, ClassifyEnv* env);
    bool debugMatchItem(widget::Widget* item, ClassifyEnv& env);
    void drawButton(widget::Widget* item);
    bool debugButtons();
    bool debugRectScreenshot(pCommand& cmd);
    bool debugFindAllCommodities();
    bool debugCompass();
    bool debugWindow();
    bool debugWindowUpdate(bool idle);

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
    DetectLevel mDetectLevelIdle {DetectLevel::Screen};
    std::chrono::milliseconds mLoopWakeup;
    std::unique_ptr<tesseract::TessBaseAPI> mTesseractApiForMarket;
    std::unique_ptr<detect::CompassDetector> mCompassDetector;
    Configuration* mConfiguration {nullptr};
    ai::AIManager* mAIManager {nullptr};

    std::queue<pCommand> mCommandQueue;
    std::mutex mCommandMutex;
    std::condition_variable mCommandCond;

    int mSellChunk {1};
};


#endif //EDROBOT_MASTER_H
