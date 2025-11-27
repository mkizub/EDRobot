#include <utility>

//
// Created by mkizub on 04.06.2025.
//

#pragma once

#ifndef EDROBOT_CONFIGURATION_H
#define EDROBOT_CONFIGURATION_H

enum class Command;

class CReadDirectoryChanges;
class Configuration;
typedef struct XMLNode XMLNode;

class CommodityCategory {
    friend class Configuration;
public:
    int intId; // from market filter
    std::string nameId;
    std::string name;   // current localization
    std::wstring wide;  // same as 'name'
    std::array<std::string,2> translation;
};

struct MarketLine {
    int buyPrice;
    int sellPrice;
    int meanPrice;
    int stock;
    int demand;
    uint8_t stockBracket;
    uint8_t demandBracket;
    bool isConsumer;
    bool isProducer;
};

struct Commodity {
    friend class Configuration;
public:
    int intId;
    std::string nameId;
    CommodityCategory* category;
    std::string name;   // current localization
    std::wstring wide;  // same as 'name'
    std::wstring wocr;  // same as 'wide' but with OCR chars
    std::array<std::string,2> translation;
    int carrierSortingOrder[2];

    bool rare;

    struct {
        Timestamp timestamp;
        int count;
        int stolen;
    } ship;

    struct {
        Timestamp timestamp;
        int count;
    } fc;
};

struct Market {
    Timestamp timestamp;
    int64_t marketId;
    std::string stationName;
    std::string stationType;
    std::string starSystem;
    std::unordered_map<Commodity*,MarketLine> items;
};

struct ShipCargo {
    Timestamp timestamp;
    std::string vessel;
    int count {0};
    std::vector<Commodity*> inventory;
};

struct NavRoute {
    struct Entry {
        std::string starSystem;
        int64_t systemAddress;
        cv::Point3d starpos;
        std::string starClass;
    };
    Timestamp timestamp;
    std::vector<Entry> route;
};

struct GameEvent {
    GameEvent(json5pp::value&& data);
    json5pp::value data;
    Timestamp timestamp;
    std::string event;
};

typedef std::shared_ptr<Market> spMarket;
typedef std::shared_ptr<ShipCargo> spShipCargo;
typedef std::shared_ptr<NavRoute> spNavRoute;
typedef std::shared_ptr<GameEvent> spGameEvent;

struct GameKey {
    enum Device { Void, Keyboard, Mouse, vJoy };
    Device device {Void};
    std::string key;
    int code {0}; // scancode or mouse button
    std::vector<GameKey> modifiers;
    friend std::ostream& operator<<(std::ostream& os, const GameKey& obj);
};
struct KeyBindings {
    enum Mode { Hold, Toggle, Axis, AxisInv };
    std::string action;
    Mode mode {Hold};
    GameKey primary;
    GameKey secondary;
};

class Configuration {
    Configuration();
    ~Configuration();
public:
    enum class FullScreenMode : int { Window, FullScreen, Borderless };

    static Configuration& getInstance();

    bool load();
    void setCalibrationResult(const std::array<cv::Vec3b,4>& buttonBGR, const std::array<cv::Vec3b,4>& lstRowBGR);
    bool saveCalibration() const;
    bool checkResolutionSupported(cv::Size gameSize, std::string& error);
    bool checkNeedColorCalibration() const;
    bool isCapturerWin32Disabled() const { return capturerWin32Disabled; }
    bool isCapturerWinRTDisabled() const { return capturerWinRTDisabled; }
    bool isCapturerDXGIDisabled() const { return capturerDXGIDisabled; }
    int getUiScalePercents() const { return mUiScalePercents; }
    int getDefaultKeyHoldTime() const { return defaultKeyHoldTime; }
    int getDefaultKeyAfterTime() const { return defaultKeyAfterTime; }
    int getSearchRegionExtent() const { return searchRegionExtent; }
    std::string getShortcutFor(Command cmd) const;
    CommodityCategory* getCommodityCategoryByName(const std::string& name);
    Commodity* getCommodityById(const std::string& name);
    Commodity* getCommodityByName(const std::string& name, bool fuzzy_ocr);
    Commodity* getCommodityByName(const std::wstring& name, bool fuzzy_ocr);

    bool loadMarket(Timestamp timestamp);
    bool loadCargo(Timestamp timestamp);
    bool loadNavRoute(Timestamp timestamp);
    const char* makeTesseractWordsFile();

    std::vector<Commodity*> getMarketInSellOrder();
    std::vector<Commodity*> getMarketInBuyOrder();
    std::vector<Commodity*> getAllKnownCommodities();

    const KeyBindings& getGameKeyBindings(const std::string& name) const;

    uchar getButtonGrayColor(WState ws) const {
        if (ws == WState::Unknown)
            return {};
        if (mUseCalibratedColors)
            return mCalibratedButtonGray[int(ws)];
        return mCalcButtonGray[int(ws)];
    }
    uchar getLstRowGrayColor(WState ws) const {
        if (ws == WState::Unknown)
            return {};
        if (mUseCalibratedColors)
            return mCalibratedLstRowGray[int(ws)];
        return mCalcLstRowGray[int(ws)];
    }
    const std::array<cv::Vec3b, 4>& getButtonHsvColors() const {
        if (mUseCalibratedColors)
            return mCalibratedButtonHsv;
        return mCalcButtonHsv;
    }
    const std::array<cv::Vec3b, 4>& getLstRowHsvColors() const {
        if (mUseCalibratedColors)
            return mCalibratedLstRowHsv;
        return mCalcLstRowHsv;
    }

    double getConfigFOV() { return configFOV; }
    unsigned getVJoyDeviceID() { return vJoyDeviceID; }

    spGameEvent dockingEvent;
    spGameEvent marketEvent;

private:
    friend class Master;

    void parseShortcutConfig(Command command, const std::string& name, json5pp::value cfg);
    bool loadCalibration();
    std::string filenameFromPreset(std::string base, std::string preset, const char* ext);
    GameKey parseGameKey(XMLNode *rootNode, bool has_modifiers, bool axis);
    bool parseKeyBindings(XMLNode *rootNode, std::unordered_map<std::string,KeyBindings>& map, const char* tag);
    bool loadGameSettings(bool initial);
    bool loadPlayerOptions();
    bool loadInputBindings();
    bool findLatestJournalFile();
    bool preloadGameJournal();
    bool loadCommodityDatabase();
    bool dumpCommodityDatabase();
    bool loadGameStatus();
    CommodityCategory& getOrAddCommodityCategory(CommodityCategory&& cc);
    Commodity& getOrAddCommodity(Commodity&& c);
    void changeDirThreadLoop();

    void readJournalChanges(std::ifstream& journalStream, std::string& journalLine);
    spGameEvent parseEvent(const std::string& line);

    std::unique_ptr<CReadDirectoryChanges> changeDirListener;
    HANDLE hShutdownEvent {};
    std::thread changeDirThread;

    int defaultKeyHoldTime = 35;
    int defaultKeyAfterTime = 50;
    int searchRegionExtent = 10;
    bool capturerWin32Disabled = false;
    bool capturerWinRTDisabled = false;
    bool capturerDXGIDisabled = false;
    bool openclDisabled = false;
    uint8_t vJoyDeviceID = 1;
    std::string mTesseractDataPath;
    std::wstring mEDSettingsPath;
    std::wstring mEDLogsPath;
    std::wstring mEDCurrentJournalFile;

    int mUiScalePercents = 100;
    bool autoPause = true;
    std::map<std::pair<std::string,unsigned>, Command> keyMapping;

    double configDashboardGUIBrightness = 0; // Options/Player/Custom.?.misc: <DashboardGUIBrightness Value="0.49999991" />
    double configGammaOffset = 0; // Options/Graphics/Settings.xml: <GammaOffset>1.200000</GammaOffset>
    double configFOV = 56.249001; // Options/Graphics/Settings.xml: <FOV>1.200000</FOV>
    int configScreenWidth = 0;    // Options\Graphics\DisplaySettings.xml: <ScreenWidth>1920</ScreenWidth>
    int configScreenHeight = 0;   // Options\Graphics\DisplaySettings.xml: <ScreenHeight>1080</ScreenHeight>
    FullScreenMode configFullScreen = FullScreenMode::Window; // Options\Graphics\DisplaySettings.xml: <FullScreen>0</FullScreen>
    std::unordered_map<std::string,KeyBindings> mKeyBindingsMap;

    bool mUseCalibratedColors = false;
    double calibrationDashboardGUIBrightness = -1;
    double calibrationGammaOffset = 0;
    int calibrationScreenWidth = 0;
    int calibrationScreenHeight = 0;
    FullScreenMode calibrationFullScreen = FullScreenMode::Window;

    std::array<cv::Vec3b, 4> mOrigButtonBGR;
    std::array<cv::Vec3b, 4> mOrigLstRowBGR;

    std::array<cv::Vec3b, 4> mCalcButtonBGR;
    std::array<cv::Vec3b, 4> mCalcLstRowBGR;
    std::array<cv::Vec3b, 4> mCalibratedButtonBGR;
    std::array<cv::Vec3b, 4> mCalibratedLstRowBGR;

    std::array<cv::Vec3b, 4> mCalcButtonHsv;
    std::array<cv::Vec3b, 4> mCalcLstRowHsv;
    std::array<cv::Vec3b, 4> mCalibratedButtonHsv;
    std::array<cv::Vec3b, 4> mCalibratedLstRowHsv;

    std::array<cv::Vec3b, 4> mCalcButtonLuv;
    std::array<cv::Vec3b, 4> mCalcLstRowLuv;
    std::array<cv::Vec3b, 4> mCalibratedButtonLuv;
    std::array<cv::Vec3b, 4> mCalibratedLstRowLuv;

    std::array<uchar, 4> mCalcButtonGray;
    std::array<uchar, 4> mCalcLstRowGray;
    std::array<uchar, 4> mCalibratedButtonGray;
    std::array<uchar, 4> mCalibratedLstRowGray;

    bool mCommodityDatabaseUpdated;
    std::deque<CommodityCategory> allKnownCommodityCategories;
    std::deque<Commodity> allKnownCommodities;
    std::unordered_map<std::string,CommodityCategory*> commodityCategoryMap;
    std::unordered_map<std::string,Commodity*> commodityMap;

    const unsigned marketCommodityFilterShowAll {0xFFFFFFFFu};
    const unsigned marketCommodityFilterShowNone {0xFFFE0001u};
    bool marketShowInCargo {true};
    bool marketShowRequiredForMission {true};
    bool marketShowHighDemand {true};
    bool marketShowRareGoods {true};
    unsigned marketCommodityFilter {0xFFFFFFFFu};

};

extern Configuration& Cfg;

#endif //EDROBOT_CONFIGURATION_H
