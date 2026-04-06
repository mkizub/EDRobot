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

class Configuration {
    Configuration();
    ~Configuration();
public:
    enum class GameScreenMode : int { Window, FullScreen, Borderless };

    static Configuration& getInstance();

    bool load();
    void shutdown();
    void savePrefs();
    void saveBookmarks();
    spBookmark addBookmark(int idx, std::string system, std::string dock);
    void delBookmark(int idx);
    void delBookmark(std::string system, std::string dock);
    void setBookmarks(std::vector<spBookmark> bookmarks);
    bool isBookmarked(const std::string& system, const std::string& dock) const;
    std::string getErrorMessage() const { return errorMessage; }
    std::string getTesseractDataPath() const { return mTesseractDataPath; }
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
    bool loadNavRoute(spGameEvent& ge);
    const char* makeTesseractWordsFile();

    std::vector<Commodity*> getMarketInSellOrder();
    std::vector<Commodity*> getMarketInBuyOrder();
    std::vector<Commodity*> getAllKnownCommodities();

    const KeyBindings& getGameKeyBindings(const std::string& name) const;

    std::vector<spBookmark> getBookmarks() const { return mBookmarks; }

    double getConfigFOV() const { return configFOV; }
    cv::Size getConfigDisplaySize() const { return {configScreenWidth, configScreenHeight}; }
    cv::Size getCaptureDisplaySize() const { return {scaledScreenWidth, scaledScreenHeight}; }
    cv::Rect getCroppedDisplayRect() const { return croppedScreenRect; }
    unsigned getVJoyDeviceID() const { return vJoyDeviceID; }
    bool isRavenColonialEnabled() const { return mRavenColonialEnabled; }
    bool isRavenColonialReportCarrierCargo() const { return mRavenColonialReportCarrierCargo && st::cmdr.fleetCarrierId != 0; }
    bool isRavenColonialReportShipCargo() const { return mRavenColonialReportShipCargo && !st::cmdr.ravenKey.empty(); }
    void setRavenColonialEnabled(bool on) { mRavenColonialEnabled = on; }
    void setRavenColonialReportCarrierCargo(bool on) { mRavenColonialReportCarrierCargo = on; }
    void setRavenColonialReportShipCargo(bool on) { mRavenColonialReportShipCargo = on; }
    std::string getRavenColonialKey(const std::string& cmdr) const {
        if (mRavenColonialKeys.contains(cmdr))
            return mRavenColonialKeys.at(cmdr);
        return {};
    }
    bool isEddnSystemsEnabled() const { return mEddnSystemsEnabled; }
    bool isEddnMarketsEnabled() const { return mEddnMarketsEnabled; }
    void setEddnSystemsEnabled(bool on) { mEddnSystemsEnabled = on; }
    void setEddnMarketsEnabled(bool on) { mEddnMarketsEnabled = on; }
    bool getCurlInsecure() const { return mCurlInsecure; }
    const std::string& getCurlProxyURL() const { return mCurlProxyUrl; }

    bool isHeadlookSmoothing() const { return configHeadlookSmoothing; }

    spGameEvent dockingEvent;
    spGameEvent marketEvent;

    js::value jprefs;

private:
    friend class Master;
    friend class CargoManager;
    friend void parseEvent_Fileheader(spGameEvent& ge); // for updateLanguage
    friend void parseEvent_LoadGame(spGameEvent& ge); // for updateLanguage

    void parseShortcutConfig(Command command, const std::string& name, js::value cfg);
    GameKey parseGameKey(XMLNode *rootNode, bool has_modifiers, bool axis);
    bool parseKeyBindings(XMLNode *rootNode, std::unordered_map<std::string,KeyBindings>& map, const char* tag);
    bool loadGameSettings(bool initial);
    bool loadPlayerOptions(bool initial);
    bool loadInputBindings();
    void updateLanguage(Lang lng);
    bool findLatestJournalFile();
    bool loadCommodityDatabase();
    bool dumpCommodityDatabase();
    bool loadGameStatus();
    CommodityCategory& getOrAddCommodityCategory(CommodityCategory&& cc);
    Commodity& getOrAddCommodity(Commodity&& c);
    void writeLogTimestamp(std::ofstream& fs, Timestamp timestamp);
    void changeDirThreadLoop();

    void readJournalChanges(std::ifstream& journalStream, Timestamp& latest_log_timestamp, std::string& journalLine);
    spGameEvent parseEvent(Timestamp& latest_log_timestamp, const std::string& line);

    void debugStaticTests();

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
    bool mRavenColonialReportCarrierCargo = false;
    bool mRavenColonialReportShipCargo = false;
    bool mEddnSystemsEnabled = false;
    bool mEddnMarketsEnabled = false;
    bool mCurlInsecure = true;
    std::map<std::string,std::string> mRavenColonialKeys;
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

    std::vector<spBookmark> mBookmarks;

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
