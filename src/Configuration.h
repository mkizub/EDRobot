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

struct RavenCmdrInfo {
    Timestamp timestamp; // contribution timestamp
};
struct Market {
    Timestamp timestamp;
    int64_t marketId;
    std::string stationName;
    std::string stationType;
    std::string starSystem;
    struct RavenCmdrInfo {
        Timestamp timestamp; // contribution timestamp
        int deliveries; // number of deliveries
        int contributed; // total cargo contributed
    };
    struct RavenProjInfo {
        std::string buildId;
        std::string status;
        Timestamp timestamp; // project timestamp
        std::map<std::string,RavenCmdrInfo> commanders;
    } raven;
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
    GameEvent(js::value&& data);
    const js::value data;
    Timestamp timestamp;
    std::string event;
};

typedef std::shared_ptr<Market> spMarket;
typedef std::shared_ptr<ShipCargo> spShipCargo;
typedef std::shared_ptr<NavRoute> spNavRoute;
typedef std::shared_ptr<GameEvent> spGameEvent;

class Configuration {
    Configuration();
    ~Configuration();
public:
    enum class GameScreenMode : int { Window, FullScreen, Borderless };

    static Configuration& getInstance();

    bool load();
    void shutdown();
    std::string getErrorMessage() const { return errorMessage; }
    std::string getForcedDXGIDeviceName() const { return forceDXGIDevice; }
    int getForcedDXGIDeviceId() const { return forceDXGIDeviceId; }
    bool isCapturerWin32Disabled() const { return capturerWin32Disabled; }
    bool isCapturerWinRTDisabled() const { return capturerWinRTDisabled; }
    bool isCapturerDXGIDisabled() const { return capturerDXGIDisabled; }
    bool useOpenclD3dInterop() const { return openclD3dInterop; }
    GameScreenMode getGameScreenMode() const { return configScreenMode; };
    int getUiScalePercents() const { return mUiScalePercents; }
    int getDefaultKeyHoldTime() const { return defaultKeyHoldTime; }
    int getDefaultKeyAfterTime() const { return defaultKeyAfterTime; }
    int getSearchRegionExtent() const { return searchRegionExtent; }
    std::string getShortcutFor(Command cmd) const;
    CommodityCategory* getCommodityCategoryById(int id);
    CommodityCategory* getCommodityCategoryByName(const std::string& name);
    Commodity* getCommodityById(std::string_view name);
    Commodity* getCommodityById(const std::string& name);
    Commodity* getCommodityByName(const std::string& name, bool fuzzy_ocr);
    Commodity* getCommodityByName(const std::wstring& name, bool fuzzy_ocr);

    bool loadMarket(spGameEvent ge);
    bool loadShipCargo(spGameEvent ge);
    bool loadCarrierCargo();
    bool saveCarrierCargo(Timestamp timestamp, const std::map<Commodity*,int>& patch);
    bool loadNavRoute(Timestamp timestamp);
    const char* makeTesseractWordsFile();

    std::vector<Commodity*> getMarketInSellOrder();
    std::vector<Commodity*> getMarketInBuyOrder();
    std::vector<Commodity*> getAllKnownCommodities();

    const KeyBindings& getGameKeyBindings(const std::string& name) const;

    double getConfigFOV() const { return configFOV; }
    cv::Size getConfigDisplaySize() const { return {configScreenWidth, configScreenHeight}; }
    cv::Size getCaptureDisplaySize() const { return {scaledScreenWidth, scaledScreenHeight}; }
    cv::Rect getCroppedDisplayRect() const { return croppedScreenRect; }
    unsigned getVJoyDeviceID() const { return vJoyDeviceID; }
    bool isRavenColonialEnabled() const { return mRavenColonialEnabled; }
    bool getCurlInsecure() const { return mCurlInsecure; }
    const std::string& getCurlProxyURL() const { return mCurlProxyUrl; }

    bool isHeadlookSmoothing() const { return configHeadlookSmoothing; }

    spGameEvent dockingEvent;
    spGameEvent marketEvent;

private:
    friend class Master;

    void parseShortcutConfig(Command command, const std::string& name, js::value cfg);
    GameKey parseGameKey(XMLNode *rootNode, bool has_modifiers, bool axis);
    bool parseKeyBindings(XMLNode *rootNode, std::unordered_map<std::string,KeyBindings>& map, const char* tag);
    bool loadGameSettings(bool initial);
    bool loadPlayerOptions(bool initial);
    bool loadInputBindings();
    bool findLatestJournalFile();
    bool preloadGameJournal();
    bool loadCommodityDatabase();
    bool dumpCommodityDatabase();
    bool loadGameStatus();
    CommodityCategory& getOrAddCommodityCategory(CommodityCategory&& cc);
    Commodity& getOrAddCommodity(Commodity&& c);
    void changeDirThreadLoop();

    void preloadOldEventsComplete();
    void readJournalChanges(std::ifstream& journalStream, std::string& journalLine);
    spGameEvent parseEvent(const std::string& line);

    std::string errorMessage;

    std::unique_ptr<CReadDirectoryChanges> changeDirListener;
    HANDLE hShutdownEvent {};
    std::thread changeDirThread;

    int defaultKeyHoldTime = 35;
    int defaultKeyAfterTime = 65;
    int searchRegionExtent = 10;
    std::string forceDXGIDevice;
    int forceDXGIDeviceId = -1;
    bool capturerWin32Disabled = false;
    bool capturerWinRTDisabled = false;
    bool capturerDXGIDisabled = false;
    bool openclDisabled = false;
    bool openclD3dInterop = true;
    uint8_t vJoyDeviceID = 1;
    bool mRavenColonialEnabled = false;
    bool mCurlInsecure = true;
    std::string mCurlProxyUrl;
    std::string mTesseractDataPath;
    std::wstring mEDSettingsPath;
    std::wstring mEDLogsPath;
    std::wstring mEDCurrentJournalFile;
    std::string mEDCurrentPlayerOptionsFile;
    unsigned mDisplaySettingsCRC32 {0};
    unsigned mSettingsCRC32 {0};

    int mUiScalePercents = 100;
    std::map<std::pair<std::string,unsigned>, Command> keyMapping;

    double configDashboardGUIBrightness = 0; // Options/Player/Custom.?.misc: <DashboardGUIBrightness Value="0.49999991" />
    double configGammaOffset = 0; // Options/Graphics/Settings.xml: <GammaOffset>1.200000</GammaOffset>
    double configFOV = 56.249001; // Options/Graphics/Settings.xml: <FOV>1.200000</FOV>
    int configScreenWidth = 0;    // Options\Graphics\DisplaySettings.xml: <ScreenWidth>1920</ScreenWidth>
    int configScreenHeight = 0;   // Options\Graphics\DisplaySettings.xml: <ScreenHeight>1080</ScreenHeight>
    int scaledScreenWidth = 0;    // downscaled configScreenWidth
    int scaledScreenHeight = 0;   // downscaled configScreenHeight
    cv::Rect croppedScreenRect;   // cropped to 16:9 scaledScreenWidth/scaledScreenHeight
    GameScreenMode configScreenMode = GameScreenMode::Window; // Options\Graphics\DisplaySettings.xml: <FullScreen>0</FullScreen>
    int configMonitorID = 0;       // Options\Graphics\DisplaySettings.xml: <Monitor>2</Monitor>
    bool configHeadlookSmoothing = true;
    std::unordered_map<std::string,KeyBindings> mKeyBindingsMap;

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
