#include <utility>

//
// Created by mkizub on 04.06.2025.
//

#pragma once

#ifndef EDROBOT_CONFIGURATION_H
#define EDROBOT_CONFIGURATION_H

enum class Command;

enum class WState : int { Unknown=-1, Normal=0, Focused=1, Active=2, Disabled=3 };

enum Lang { XX=-1, EN=0, RU=1 };

enum GuiFocus { None=0, Right=1, Left=2, Chat=3, Role=4, Services=5, GalaxyMap=6, SystemMap=7, Orrery=8, FSS=9, SAA=10, Codex=11 };

class CReadDirectoryChanges;
class Configuration;
typedef struct XMLNode XMLNode;

typedef std::chrono::time_point<std::chrono::utc_clock> Timestamp;

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
    Timestamp timestamp;
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

    MarketLine market;
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

struct ShipStatus {
    Timestamp timestamp;
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
            bool hud_in_analysis : 1;
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
    GuiFocus guiFocus {GuiFocus::None};
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
    int64 destinationSystem {};
    int destinationBody {};
    std::string destinationName;

    friend std::ostream& operator<<(std::ostream& os, const ShipStatus& obj);
};

struct GameEvent {
    GameEvent(json::value&& data);
    json::object data;
    Timestamp timestamp;
    std::string event;
};

struct StarSystem {
    int64_t address;
    std::string name;
    cv::Point3d pos;
    std::map<std::string,std::shared_ptr<GameEvent>> fssSignalDiscovered;
};

typedef std::shared_ptr<Market> spMarket;
typedef std::shared_ptr<ShipCargo> spShipCargo;
typedef std::shared_ptr<ShipStatus> spShipStatus;
typedef std::shared_ptr<ShipCargo> spShipCargo;
typedef std::shared_ptr<GameEvent> spGameEvent;
typedef std::shared_ptr<StarSystem> spStarSystem;

union LocationPanelFilters {
    LocationPanelFilters() : mask(0) {}
    struct {
        bool pointOfInterest: 1;
        bool star: 1;
        bool settlement: 1;
        bool station: 1;
        bool signalSource: 1;
        bool asteroidCluster: 1;
        bool landablePlanetOrMoon: 1;
        bool system: 1;
        bool fleetCarrier: 1;
        bool planetOrMoon: 1;
    } bits;
    int mask;
    bool operator==(const LocationPanelFilters& other) const { return this->mask == other.mask; }
    bool operator!=(const LocationPanelFilters& other) const { return this->mask != other.mask; }
};

struct DockStation {
    uint32_t marketId;
    std::string name;
    std::string stationType;
};

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
    int getDefaultKeyHoldTime() const { return defaultKeyHoldTime; }
    int getDefaultKeyAfterTime() const { return defaultKeyAfterTime; }
    int getSearchRegionExtent() const { return searchRegionExtent; }
    std::string getShortcutFor(Command cmd) const;
    CommodityCategory* getCommodityCategoryByName(const std::string& name);
    Commodity* getCommodityById(const std::string& name);
    Commodity* getCommodityByName(const std::string& name, bool fuzzy_ocr);
    Commodity* getCommodityByName(const std::wstring& name, bool fuzzy_ocr);

    bool loadMarket();
    bool loadCargo();
    const char* makeTesseractWordsFile();

    const json5pp::value& getEDDBFull() const { return mEDDBFull; }
    const json5pp::value& getEDDBShip(const std::string& type) const { return mEDDBShips.at(toLower(type)); }
    const spMarket getCurrentMarket() const { return currentMarket; }
    const spShipCargo& getCurrentCargo() const { return currentCargo; }
    const spShipStatus& getCurrentStatus() const { return currentStatus; }
    const spStarSystem& getCurrentStarSystem() const { return currentStarSystem; }
    std::vector<Commodity*> getMarketInSellOrder();
    std::vector<Commodity*> getMarketInBuyOrder();
    std::vector<Commodity*> getAllKnownCommodities();

    GuiFocus getGuiFocus() { return guiFocus; }
    const std::string& getCmdrName() { return mCmdrName; }
    const std::string& getShipType() { return mShipType; }
    const std::string& getShipUserName() { return mShipUserName; }

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

    const Lang lng {XX};
    const bool isOdyssey {false};
    const LocationPanelFilters configLocationPanelFilters {};

    double getConfigFOV() { return configFOV; }
    unsigned getVJoyDeviceID() { return vJoyDeviceID; }

    spGameEvent dockingEvent;

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
    bool loadEDDB();
    bool loadGameStatus();
    CommodityCategory& getOrAddCommodityCategory(CommodityCategory&& cc);
    Commodity& getOrAddCommodity(Commodity&& c);
    void changeDirThreadLoop();

    void readJournalChanges(std::ifstream& journalStream, std::string& journalLine);
    spGameEvent parseEvent(const std::string& line);
    void parseEvent_Commander(spGameEvent& ge);
    void parseEvent_LoadGame(spGameEvent& ge);
    void parseEvent_CarrierLocation(spGameEvent& ge);
    void parseEvent_Location(spGameEvent& ge);
    void parseEvent_Loadout(spGameEvent& ge);
    void parseEvent_Cargo(spGameEvent& ge);
    void parseEvent_ShipyardSwap(spGameEvent& ge);
    void parseEvent_Docked(spGameEvent& ge);
    void parseEvent_Undocked(spGameEvent& ge);
    void parseEvent_Docking(spGameEvent& ge);
    void parseEvent_StartJump(spGameEvent& ge);
    void parseEvent_FSDJump(spGameEvent& ge);
    void parseEvent_FSSSignalDiscovered(spGameEvent& ge);

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

    json5pp::value mEDDBFull;
    std::unordered_map<std::string,const json5pp::value&> mEDDBShips;

    std::string mCmdrName;
    std::string mShipType;
    std::string mShipTypeLocalized;
    std::string mShipUserName;
    std::string mCurrentStarSystem;
    std::string mCurrentDockedStation;

    GuiFocus guiFocus {GuiFocus::None};
    spMarket currentMarket;
    spShipCargo currentCargo;
    spShipStatus currentStatus;
    spStarSystem currentStarSystem;
    DockStation currentDock;

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
