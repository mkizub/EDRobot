#include <utility>

//
// Created by mkizub on 04.06.2025.
//

#pragma once

#ifndef EDROBOT_CONFIGURATION_H
#define EDROBOT_CONFIGURATION_H

#include <chrono>

enum class Command;

enum class WState : int { Unknown=-1, Normal=0, Focused=1, Active=2, Disabled=3 };

enum Lang { XX=-1, EN=0, RU=1 };

class CReadDirectoryChanges;
class Configuration;

class CommodityCategory {
    friend class Configuration;
public:
    std::string nameId;
    std::string name;   // current localization
    std::wstring wide;  // same as 'name'
    std::array<std::string,2> translation;
};

struct MarketLine {
    std::chrono::time_point<std::chrono::utc_clock> timestamp;
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
    CommodityCategory* const category;
    std::string name;   // current localization
    std::wstring wide;  // same as 'name'
    std::array<std::string,2> translation;
    int carrierSortingOrder[2];

    bool rare;

    struct {
        std::chrono::time_point<std::chrono::utc_clock> timestamp;
        int count;
        int stolen;
    } ship;

    struct {
        std::chrono::time_point<std::chrono::utc_clock> timestamp;
        int count;
    } fc;

    MarketLine market;
};

struct Market {
    std::chrono::time_point<std::chrono::utc_clock> timestamp;
    int64_t marketId;
    std::string stationName;
    std::string stationType;
    std::string starSystem;
    std::unordered_map<Commodity*,MarketLine> items;
};
typedef std::shared_ptr<Market> spMarket;

struct ShipCargo {
    std::chrono::time_point<std::chrono::utc_clock> timestamp;
    std::string vessel;
    int count {0};
    std::vector<Commodity*> inventory;
};
typedef std::shared_ptr<ShipCargo> spShipCargo;

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
    Commodity* getCommodityByName(const std::wstring& name, bool fuzzy);

    bool loadMarket();
    bool loadCargo();
    const char* makeTesseractWordsFile();

    spMarket getCurrentMarket() const { return currentMarket; }
    spShipCargo getCurrentCargo() const { return currentCargo; }
    std::vector<Commodity*> getMarketInSellOrder();

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

    std::atomic<spMarket> currentMarket;
    std::atomic<spShipCargo> currentCargo;

};


#endif //EDROBOT_CONFIGURATION_H
