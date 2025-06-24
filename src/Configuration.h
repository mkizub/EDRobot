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

enum GuiFocus { NoFocus=0, Right=1, Left=2, Chat=3, Role=4, Services=5, GalaxyMap=6, SystemMap=7, Orrery=8, FSS=9, SAA=10, Codex=11 };

class CReadDirectoryChanges;
class Configuration;
typedef struct XMLNode XMLNode;

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

struct ShipStatus {
    std::chrono::time_point<std::chrono::utc_clock> timestamp;
    union {
        struct {
            bool docked : 1;
            bool landed : 1;
            bool landing_gear_down : 1;
            bool shields_up : 1;
            bool cruise : 1;
            bool fa_off : 1;
            bool weapon_on : 1;
            bool in_wing : 1;
            bool lights_on : 1;
            bool cargo_scoop_on : 1;
            bool silent_run : 1;
            bool fuel_scooping : 1;
            bool srv_handbrake : 1;
            bool srv_turret_view : 1;
            bool srv_turret_retructed : 1;
            bool srv_drive_assist : 1;
            bool fsd_masslocked : 1;
            bool fsd_charging : 1;
            bool fsd_cooldown : 1;
            bool fuel_low : 1;
            bool overheating : 1;
            bool has_lat_lon : 1;
            bool in_danger : 1;
            bool in_interdiction : 1;
            bool in_ship : 1;
            bool in_fighter : 1;
            bool in_srv : 1;
            bool hud_analysis : 1;
            bool night_vision : 1;
            bool alt_from_avr_radius : 1;
            bool fsd_jump : 1;
            bool srv_high_beam : 1;
        };
        uint32_t all {0};
    } flags;
    union {
        struct {
            bool on_foot : 1;
            bool in_taxy : 1;
            bool in_multicrew : 1;
            bool on_foot_in_station : 1;
            bool on_foot_on_planet : 1;
            bool aim_down_sight : 1;
            bool low_oxygen : 1;
            bool low_health : 1;
            bool cold : 1;
            bool hot : 1;
            bool very_cold : 1;
            bool very_hot : 1;
            bool glide_mode : 1;
            bool on_foot_in_hangar : 1;
            bool on_foot_social_space : 1;
            bool on_foot_exterior : 1;
            bool breathable_atmosphere : 1;
            bool telepresence_multicrew : 1;
            bool physical_multicrew : 1;
            bool fsd_hyperdrive_charging : 1;
        };
        uint32_t all {0};
    } flags2;
    uint8_t  pips[3];
    uint8_t fireGroup {0};
    GuiFocus guiFocus {GuiFocus::NoFocus};
    float fuelMain;
    float fuelReservoir;
    float cargo;
    uint64_t balance;
    std::string legalState; // Clean,IllegalCargo,Speeding,Wanted,Hostile,PassengerWanted,Warrant,Allied,Thargoid
    // if on or near a planet
    float latitude;
    float altitude;
    float longitude;
    float heading;
    std::string bodyName;
    float planetRadius;
    // "Destination":{ "System":22659307939297, "Body":0, "Name":"Col 285 Sector IK-K b23-10" } before system jump, or
    // "Destination":{ "System":22659307939297, "Body":17, "Name":"Giraud Prospect" } when in system
    std::string destination;
};

typedef std::shared_ptr<ShipStatus> spShipStatus;

struct GameKey {
    enum Device { Void, Keyboard, Mouse};
    Device device {Void};
    std::string key;
    int code {0}; // scancode or mouse button
    std::vector<GameKey> modifiers;
    friend std::ostream& operator<<(std::ostream& os, const GameKey& obj);
};
struct KeyBindings {
    std::string action;
    GameKey primary;
    GameKey secondary;
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
    bool isCapturerWin32Disabled() const { return capturerWin32Disabled; }
    bool isCapturerWinRTDisabled() const { return capturerWinRTDisabled; }
    bool isCapturerDXGIDisabled() const { return capturerDXGIDisabled; }
    int getDefaultKeyHoldTime() const { return defaultKeyHoldTime; }
    int getDefaultKeyAfterTime() const { return defaultKeyAfterTime; }
    int getSearchRegionExtent() const { return searchRegionExtent; }
    std::string getShortcutFor(Command cmd) const;
    CommodityCategory* getCommodityCategoryByName(const std::string& name);
    Commodity* getCommodityByName(const std::string& name, bool fuzzy);
    Commodity* getCommodityByName(const std::wstring& name, bool fuzzy);

    bool loadMarket();
    bool loadCargo();
    const char* makeTesseractWordsFile();

    spMarket getCurrentMarket() const { return currentMarket; }
    spShipCargo getCurrentCargo() const { return currentCargo; }
    spShipStatus getCurrentStatus() const { return currentStatus; }
    std::vector<Commodity*> getMarketInSellOrder();
    std::vector<Commodity*> getMarketInBuyOrder();
    std::vector<Commodity*> getAllKnownCommodities();

    GuiFocus getGuiFocus() { return guiFocus; }

    const KeyBindings& getGameKeyBindings(const std::string& name) const;

    const Lang lng {XX};
    const bool isOdyssey {false};

private:
    friend class Master;

    void parseShortcutConfig(Command command, const std::string& name, json5pp::value cfg);
    bool loadCalibration();
    std::string filenameFromPreset(std::string base, std::string preset, const char* ext);
    GameKey parseGameKey(XMLNode *rootNode, bool has_modifiers);
    bool parseKeyBindings(XMLNode *rootNode, std::unordered_map<std::string,KeyBindings>& map, const char* tag);
    bool loadGameSettings(bool initial);
    bool findLatestJournalFile();
    bool preloadGameJournal();
    bool loadGameJournal(std::wstring journalFilename);
    bool loadCommodityDatabase();
    bool dumpCommodityDatabase();
    bool loadGameStatus();
    CommodityCategory& getOrAddCommodityCategory(CommodityCategory&& cc);
    Commodity& getOrAddCommodity(Commodity&& c);
    void changeDirThreadLoop();

    bool parseTimestamp(const json::value&, std::chrono::time_point<std::chrono::utc_clock>&);
    bool parseTimestamp(const std::string&, std::chrono::time_point<std::chrono::utc_clock>&);

    std::unique_ptr<CReadDirectoryChanges> changeDirListener;
    HANDLE hShutdownChangDirListenerEvent {};
    std::thread changeDirThread;

    int defaultKeyHoldTime = 35;
    int defaultKeyAfterTime = 50;
    int searchRegionExtent = 10;
    bool capturerWin32Disabled = false;
    bool capturerWinRTDisabled = false;
    bool capturerDXGIDisabled = false;
    std::string mTesseractDataPath;
    std::wstring mEDSettingsPath;
    std::wstring mEDLogsPath;
    std::wstring mEDCurrentJournalFile;

    bool autoPause = true;
    std::map<std::pair<std::string,unsigned>, Command> keyMapping;

    double configDashboardGUIBrightness = 0; // Options/Player/Custom.?.misc: <DashboardGUIBrightness Value="0.49999991" />
    double configGammaOffset = 0; // Options/Graphics/Settings.xml: <GammaOffset>1.200000</GammaOffset>
    double configFOV = 56.249001; // Options/Graphics/Settings.xml: <FOV>1.200000</FOV>
    int configScreenWidth = 0;    // Options\Graphics\DisplaySettings.xml: <ScreenWidth>1920</ScreenWidth>
    int configScreenHeight = 0;   // Options\Graphics\DisplaySettings.xml: <ScreenHeight>1080</ScreenHeight>
    FullScreenMode configFullScreen = FullScreenMode::Window; // Options\Graphics\DisplaySettings.xml: <FullScreen>0</FullScreen>
    std::unordered_map<std::string,KeyBindings> keyBindingsGeneric;
    std::unordered_map<std::string,KeyBindings> keyBindingsShip;

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

    GuiFocus guiFocus {GuiFocus::NoFocus};
    std::atomic<spMarket> currentMarket;
    std::atomic<spShipCargo> currentCargo;
    std::atomic<spShipStatus> currentStatus;

};


#endif //EDROBOT_CONFIGURATION_H
