#include <utility>

//
// Created by mkizub on 04.06.2025.
//

#pragma once

#ifndef EDROBOT_CONFIGURATION_H
#define EDROBOT_CONFIGURATION_H

#include <chrono>

enum class Command;

enum class WState : int { Normal, Focused, Active, Disabled };

enum Lang { XX=-1, EN=0, RU=1 };

class CReadDirectoryChanges;

struct CommodityCategory {
    std::string nameId;
    std::string name;   // current localization
    std::wstring wide;  // same as 'name'
    std::array<std::string,2> translation;
    unsigned sortingOrder[2][2];
};

struct Commodity {
    int64_t intId;
    std::string nameId;
    std::string categoryId;
    std::string name;   // current localization
    std::wstring wide;  // same as 'name'
    std::array<std::string,2> translation;
    unsigned sortingOrder[2][2];
};

struct MarketCommodity {
    Commodity& commodity;
    int buyPrice;
    int sellPrice;
    int meanPrice;
    int stock;
    int demand;
    uint8_t stockBracket;
    uint8_t demandBracket;
    bool isConsumer;
    bool isProducer;
    bool isRare;
};

struct CargoCommodity {
    Commodity& commodity;
    int count;
    int stolen;
};
typedef std::shared_ptr<CargoCommodity> spCargoCommodity;

struct Market {
    Market& operator=(Market&& other) noexcept = default;
    std::chrono::time_point<std::chrono::utc_clock> timestamp;
    int64_t marketId;
    std::string stationName;
    std::string stationType;
    std::string starSystem;
    std::vector<std::shared_ptr<MarketCommodity>> items;
};

struct Cargo {
    Cargo& operator=(Cargo&& other) noexcept = default;
    std::chrono::time_point<std::chrono::utc_clock> timestamp;
    std::string vessel;
    int count {0};
    std::vector<std::shared_ptr<CargoCommodity>> inventory;
};

class Configuration {
public:
    enum class FullScreenMode : int { Window, FullScreen, Borderless };

    Configuration();
    ~Configuration();

    bool load();
    void setCalibrationResult(const std::array<cv::Vec3b,4>& buttonLuv, const std::array<cv::Vec3b,4>& lstRowLuv);
    bool saveCalibration() const;
    bool checkResolutionSupported(cv::Size gameSize);
    bool checkNeedColorCalibration() const;
    std::string getShortcutFor(Command cmd) const;
    CommodityCategory* getCommodityCategoryByName(const std::string& name);
    Commodity* getCommodityByName(const std::string& name, bool fuzzy);
    std::shared_ptr<CargoCommodity> getCargoByName(const std::string& name, bool fuzzy);

    bool loadMarket();
    bool loadCargo();
    const char* makeTesseractWordsFile();

    const Lang lng {XX};

private:
    friend class Master;

    void parseShortcutConfig(Command command, const std::string& name, json5pp::value cfg);
    bool loadCalibration();
    bool loadGameSettings();
    bool loadGameJournal(std::wstring journalFilename);
    bool loadCommodityDatabase();
    bool dumpCommodityDatabase();
    CommodityCategory& getOrAddCommodityCategory(CommodityCategory&& cc);
    Commodity& getOrAddCommodity(Commodity&& c);
    void changeDirThreadLoop();

    std::unique_ptr<CReadDirectoryChanges> changeDirListener;
    HANDLE hShutdownChangDirListenerEvent {};
    std::thread changeDirThread;

    int defaultKeyHoldTime = 35;
    int defaultKeyAfterTime = 50;
    int searchRegionExtent = 10;
    std::string mTesseractDataPath;
    std::wstring mEDSettingsPath;
    std::wstring mEDLogsPath;
    std::wstring mEDCurrentJournalFile;

    std::map<std::pair<std::string,unsigned>, Command> keyMapping;

    double configDashboardGUIBrightness = 0; // Options/Player/Custom.?.misc: <DashboardGUIBrightness Value="0.49999991" />
    double configGammaOffset = 0; // Options/Graphics/Settings.xml: <GammaOffset>1.200000</GammaOffset>
    double configFOV = 56.249001; // Options/Graphics/Settings.xml: <FOV>1.200000</FOV>
    int configScreenWidth = 0;    // Options\Graphics\DisplaySettings.xml: <ScreenWidth>1920</ScreenWidth>
    int configScreenHeight = 0;   // Options\Graphics\DisplaySettings.xml: <ScreenHeight>1080</ScreenHeight>
    FullScreenMode configFullScreen = FullScreenMode::Window; // Options\Graphics\DisplaySettings.xml: <FullScreen>0</FullScreen>

    double calibrationDashboardGUIBrightness = -1;
    double calibrationGammaOffset = 0;
    int calibrationScreenWidth = 0;
    int calibrationScreenHeight = 0;
    FullScreenMode calibrationFullScreen = FullScreenMode::Window;

    std::array<cv::Vec3b, 4> mButtonLuv;
    std::array<cv::Vec3b, 4> mLstRowLuv;

    bool mCommodityDatabaseUpdated;
    std::deque<CommodityCategory> allKnownCommodityCategories;
    std::deque<Commodity> allKnownCommodities;
    std::unordered_map<std::string,CommodityCategory*> commodityCategoryMap;
    std::unordered_map<std::string,Commodity*> commodityMap;

    std::mutex currentDataMutex;
    Market currentMarket;
    Cargo currentCargo;

};


#endif //EDROBOT_CONFIGURATION_H
