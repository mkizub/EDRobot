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

class CReadDirectoryChanges;

struct CommodityCategory {
    std::string name;
    std::string nameLocalized;
    unsigned marketOrder;   // N*1000
};

struct Commodity {
    int64_t commodityId;
    std::string name;
    std::string nameLocalized;
    std::wstring nameLocalizedW;
    std::string category;
    std::string categoryLocalized;
    std::wstring categoryLocalizedW;
    unsigned marketOrder;    // category.marketOrder + N
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

struct Market {
    Market& operator=(Market&& other) noexcept = default;
    std::chrono::time_point<std::chrono::utc_clock> timestamp;
    int64_t marketId;
    std::string stationName;
    std::string stationType;
    std::string starSystem;
    std::vector<MarketCommodity> items;
};

struct Cargo {
    Cargo& operator=(Cargo&& other) noexcept = default;
    std::chrono::time_point<std::chrono::utc_clock> timestamp;
    std::string vessel;
    int count {0};
    std::vector<CargoCommodity> inventory;
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
    bool checkNeedColorCalibration();
    std::string getShortcutFor(Command cmd) const;
    Commodity* getCommodityByName(const std::string& name);
    CargoCommodity* getCargoByName(const std::string& name, bool fuzzy);

    bool loadMarket();
    bool loadCargo();
    const char* makeTesseractWordsFile();

private:
    friend class Master;

    void parseShortcutConfig(Command command, const std::string& name, json5pp::value cfg);
    bool loadCalibration();
    bool loadGameSettings();
    bool checkReloadGameSettings();
    Commodity& getOrAddCommodity(Commodity&& c);

    std::unique_ptr<CReadDirectoryChanges> changeDirListener;

    int defaultKeyHoldTime = 35;
    int defaultKeyAfterTime = 50;
    int searchRegionExtent = 10;
    std::wstring mEDSettingsPath;
    std::wstring mEDLogsPath;
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
    std::deque<CommodityCategory> allKnownCommodityCategories;
    std::deque<Commodity> allKnownCommodities;
    std::unordered_map<uint64_t,Commodity*> commodityById;
    std::unordered_map<std::string,Commodity*> commodityByName;
    Market currentMarket;
    Cargo currentCargo;
};


#endif //EDROBOT_CONFIGURATION_H
