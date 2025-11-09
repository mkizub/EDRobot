//
// Created by mkizub on 04.06.2025.
//

#include "pch.h"

#include "Configuration.h"
#include "Keyboard.h"
#include "FuzzyMatch.h"
#include "EDWidget.h"
#include "Capturer.h"
#include "ShipStats.h"
#include "Galaxy.h"
#include "detect/Detector.h"
#include "detect/Lines.h"
#include "detect/NavPanel.h"

#include <dirlistener/ReadDirectoryChanges.h>
#ifdef DEBUG
# undef DEBUG
#endif


#define XML_H_IMPLEMENTATION
#include <xml/xml.h>
#include <filesystem>


#ifdef EDROBOT_USE_OPENCL
bool g_DisableOpenCL;
#endif

Configuration& Cfg = Configuration::getInstance();

Configuration& Configuration::getInstance() {
    static Configuration cfg;
    return cfg;
}

static cv::Vec3b color_from_json(const json5pp::value& v);
static detect::Detector* detector_from_json(const json5pp::value& j, widget::Widget& widget);
static widget::Widget* widget_from_json(const json5pp::value& j, widget::Widget* parent);

Configuration::Configuration()
{
    mOrigButtonBGR[int(WState::Normal)]  = { 0, 15, 34};
    mOrigButtonBGR[int(WState::Focused)] = { 0,111,255};
    mOrigButtonBGR[int(WState::Active)]  = { 0, 34, 77};
    mOrigButtonBGR[int(WState::Disabled)]= {25, 25, 25};

    mOrigLstRowBGR[int(WState::Normal)]  = { 6, 20, 39};
    mOrigLstRowBGR[int(WState::Focused)] = { 0,111,255};
    mOrigLstRowBGR[int(WState::Active)]  = { 6, 28, 57};
    mOrigLstRowBGR[int(WState::Disabled)]= {25, 25, 25};

    mUseCalibratedColors = false;
    for (int i=0; i < 4; i++) {
        mCalcButtonBGR[i] = mOrigButtonBGR[i];
        mCalcLstRowBGR[i] = mOrigLstRowBGR[i];

        mCalcButtonHsv[i] = sBgr2Hsv(mOrigButtonBGR[i]);
        mCalcLstRowHsv[i] = sBgr2Hsv(mOrigLstRowBGR[i]);

        mCalcButtonLuv[i] = sBgr2Luv(mOrigButtonBGR[i]);
        mCalcLstRowLuv[i] = sBgr2Luv(mOrigLstRowBGR[i]);

        mCalcButtonGray[i] = sBgr2sGray(mOrigButtonBGR[i]);
        mCalcLstRowGray[i] = sBgr2sGray(mOrigLstRowBGR[i]);
    }
}

Configuration::~Configuration() {
    if (hShutdownEvent) {
        SetEvent(hShutdownEvent);
        CloseHandle(hShutdownEvent);
        if (changeDirThread.joinable())
            changeDirThread.join();
    }
}


bool Configuration::load() {

    // initialize default keymapping
    keyMapping = {
            {{"esc",         0},                      Command::Stop},
            {{"printscreen", 0},                      Command::Start},
            {{"scrolllock",  0},                      Command::Resume},
            {{"pause",       0},                      Command::Pause},
            {{"printscreen", kbd::LCTRL | kbd::LALT}, Command::DebugTemplates},
            {{"a",           kbd::LALT},              Command::Autopilot},
            {{"r",           kbd::LCTRL | kbd::LALT}, Command::DevRectSelect},
            {{"[",           kbd::LCTRL | kbd::LALT}, Command::DebugWindow},
            {{"\\",          kbd::LCTRL | kbd::LALT}, Command::DebugStream},
            {{"]",           kbd::LCTRL | kbd::LALT}, Command::ResetCapturer},
    };

    {
        wchar_t buffer[MAX_PATH];

        std::wstring dirUserProfile, dirAppDataLocal;
        if (GetEnvironmentVariableW(L"UserProfile", buffer, MAX_PATH)) {
            dirUserProfile = buffer;
            mEDLogsPath = dirUserProfile + LR"(\Saved Games\Frontier Developments\Elite Dangerous)";
        }
        if (GetEnvironmentVariableW(L"LocalAppData", buffer, MAX_PATH)) {
            dirAppDataLocal = buffer;
            mEDSettingsPath = dirAppDataLocal + LR"(\Frontier Developments\Elite Dangerous)";
        }
        else if (!dirUserProfile.empty()) {
            mEDSettingsPath = dirUserProfile + LR"(\AppData\Local\Frontier Developments\Elite Dangerous)";
        }
        std::ifstream ifs_config("configuration.json5");
        json5pp::value j_config = json5pp::parse5(ifs_config);
        if (auto& tm = j_config.at("default-key-hold-time"); tm.is_integer())
            defaultKeyHoldTime = tm.as_integer();
        if (auto& tm = j_config.at("default-key-after-time"); tm.is_integer())
            defaultKeyAfterTime = tm.as_integer();
        if (auto& tm = j_config.at("search-region-extent"); tm.is_integer())
            searchRegionExtent = tm.as_integer();
        if (auto& tm = j_config.at("auto-pause"); tm.is_boolean())
            autoPause = tm.as_boolean();
        if (j_config.at("shortcuts").is_object()) {
            auto& obj = j_config.at("shortcuts");
            parseShortcutConfig(Command::Start, "start", obj);
            parseShortcutConfig(Command::Pause, "pause", obj);
            parseShortcutConfig(Command::Resume, "resume", obj);
            parseShortcutConfig(Command::Stop,  "stop",  obj);
            parseShortcutConfig(Command::DebugTemplates,  "debug-templates",  obj);
            parseShortcutConfig(Command::DebugWindow,     "debug-window",  obj);
            parseShortcutConfig(Command::DevRectSelect,   "dev-rect-select",  obj);
            parseShortcutConfig(Command::Shutdown,  "shutdown",  obj);
        }
        if (auto tm = j_config.at("elite-dangerous-settings-path"); tm.is_string())
            mEDSettingsPath = toUtf16(tm.as_string());
        if (auto tm = j_config.at("elite-dangerous-logs-path"); tm.is_string())
            mEDLogsPath = toUtf16(tm.as_string());
        if (auto tm = j_config.at("tesseract-data-path"); tm.is_string())
            mTesseractDataPath = tm.as_string();
        if (auto& tm = j_config.at("capturer-Win32-disabled"); tm.is_boolean())
            capturerWin32Disabled = tm.as_boolean();
        if (auto& tm = j_config.at("capturer-WinRT-disabled"); tm.is_boolean())
            capturerWinRTDisabled = tm.as_boolean();
        if (auto& tm = j_config.at("capturer-DXGI-disabled"); tm.is_boolean())
            capturerDXGIDisabled = tm.as_boolean();
        if (auto& tm = j_config.at("vjoy-device-id"); tm.is_integer())
            vJoyDeviceID = (uint8_t)tm.as_integer();
#ifdef EDROBOT_USE_OPENCL
        if (auto& tm = j_config.at("opencl-disabled"); tm.is_boolean())
            openclDisabled = tm.as_boolean();
        g_DisableOpenCL = openclDisabled;
        if (auto& tm = j_config.at("opencl-cache-dir"); tm.is_string()) {
            std::string dir = tm.as_string();
            _putenv_s("OPENCV_OPENCL_CACHE_DIR", dir.c_str());
        } else {
            _putenv_s("OPENCV_OPENCL_CACHE_DIR", "cache");
        }
#else
        openclDisabled = true;
#endif
        std::filesystem::create_directories("cache/systems");

        LOG(INFO) << "Initializing D3D device";
        Capturer::InitD3DDevice();

        eddb::loadEDDB();
        preloadGameJournal(); // game language & version
        loadGameSettings(true);
        loadPlayerOptions();
        loadInputBindings();

        loadCommodityDatabase(); // initialization depends on game language
        //dumpCommodityDatabase();
        mCommodityDatabaseUpdated = false;

        loadGameStatus();
        loadCalibration();

        LOG(INFO) << "Setting journal directory listener";
        if (!changeDirListener) {
            changeDirListener = std::make_unique<CReadDirectoryChanges>(100);
            changeDirListener->Init();
            DWORD dirNotificationFlags = FILE_NOTIFY_CHANGE_SIZE | FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_CREATION;
            changeDirListener->AddDirectory(mEDSettingsPath + LR"(\Options\Graphics\)", false, dirNotificationFlags);
            changeDirListener->AddDirectory(mEDSettingsPath + LR"(\Options\Bindings\)", false, dirNotificationFlags);
            changeDirListener->AddDirectory(mEDSettingsPath + LR"(\Options\Player\)", false, dirNotificationFlags);
            changeDirListener->AddDirectory(mEDLogsPath, false, dirNotificationFlags);
            hShutdownEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);
            changeDirThread = std::thread(&Configuration::changeDirThreadLoop, this);
        }
    }

    {
        LOG(INFO) << "Setting screens.json5";
        widget::Root* screensRoot = Master::getInstance().mScreensRoot.get();
        std::ifstream ifs_config("screens.json5");
        auto j_screens = json5pp::parse5(ifs_config).as_array();
        for (json5pp::value& s: j_screens) {
            screensRoot->addSubItem(widget_from_json(s, screensRoot));
        }
    }

    return true;
}

void Configuration::parseShortcutConfig(Command command, const std::string& name, json5pp::value cfg) {
    if (cfg.as_object().contains((name))) {
        for (auto it = keyMapping.begin(); it != keyMapping.end();)  {
            if (it->second == command)
                it = keyMapping.erase(it);
            else
                ++it;
        }
        json5pp::value jcmd = cfg.at(name);
        if (jcmd.is_string())
            keyMapping[decodeShortcut(jcmd.as_string())] = command;
        if (jcmd.is_array()) {
            for (auto& jc : jcmd.as_array()) {
                if (jc.is_string())
                    keyMapping[decodeShortcut(jc.as_string())] = command;
            }
        }

    }
}

std::string Configuration::filenameFromPreset(std::string base, std::string preset, const char* ext) {
    std::string filename = base + trim(preset);
    if (st::client.isOdyssey) {
        if (std::filesystem::exists(filename + ".4.2." + ext))
            filename = filename + ".4.2." + ext;
        else
            filename = filename + ".4.1." + ext;
    } else {
        filename = filename + ".4.0." + ext;
    }
    return filename;
}

const KeyBindings& Configuration::getGameKeyBindings(const std::string& name) const {
    static KeyBindings undefined;
    auto it = mKeyBindingsMap.find(name);
    if (it != mKeyBindingsMap.end())
        return it->second;
    return undefined;
}

GameKey Configuration::parseGameKey(XMLNode *keyNode, bool has_modifiers, bool axis) {
    auto device = xml_node_attr(keyNode, "Device");
    auto key = xml_node_attr(keyNode, "Key");
    if (!device || !key)
        return {};
    GameKey gk;
    if (strcmp(device,"Keyboard") == 0) {
        gk.device = GameKey::Keyboard;
        gk.key = key;
        if (gk.key.starts_with("Key_"))
            gk.code = kbd::getScanCode(key + 4);
        if (!gk.code)
            gk.device = GameKey::Void;
    }
    else if (strcmp(device,"Mouse") == 0) {
        gk.device = GameKey::Mouse;
        gk.key = key;
        if (gk.key.starts_with("Mouse_"))
            gk.code = atoi(key+6);
        if (!gk.code)
            gk.device = GameKey::Void;
    }
    else if (strcmp(device,"vJoy") == 0) {
        gk.device = GameKey::vJoy;
        gk.key = key;
        if (!axis) {
            if (gk.key.starts_with("Joy_"))
                gk.code = atoi(key + 4);
            if (!gk.code)
                gk.device = GameKey::Void;
        }
    }
    else
        return {};

    if (gk.device != GameKey::Void && gk.code && has_modifiers) {
        auto mod = xml_node_find_tag(keyNode, "Modifier", true);
        if (keyNode->children) {
            for (size_t i = 0; i < keyNode->children->len; i++) {
                auto child = xml_node_child_at(keyNode, i);
                gk.modifiers.push_back(parseGameKey(child, false, false));
                if (gk.modifiers.back().device == GameKey::Void)
                    gk.device = GameKey::Void;
            }
        }
    }

    return gk;
}

std::ostream& operator<<(std::ostream& os, const GameKey& obj) {
    for (auto& m : obj.modifiers)
        os << m << "+";
    os << obj.key;
    return os;
}

static bool getBoolNodeValue(XMLNode* root, const char* tag) {
    if (auto node = xml_node_find_tag(root, tag, true)) {
        if (auto val = xml_node_attr(node, "Value"))
            return (val[0] != '0');
    }
    return false;
}

static int64_t getIntNodeValue(XMLNode* root, const char* tag) {
    if (auto node = xml_node_find_tag(root, tag, true)) {
        if (auto val = xml_node_attr(node, "Value"))
            return std::stoll(val);
    }
    return 0;
}

bool Configuration::parseKeyBindings(XMLNode *rootNode, std::unordered_map<std::string,KeyBindings>& map, const char* tag) {
    auto node = xml_node_find_tag(rootNode, tag, true);
    if (!node) {
        LOG(ERROR) << "Key binding for <" << tag << "> not found";
        return false;
    }
    KeyBindings kb;
    kb.action = tag;
    if (getBoolNodeValue(node, "ToggleOn"))
        kb.mode = KeyBindings::Toggle;
    if (auto primary = xml_node_find_tag(node, "Primary", true))
        kb.primary = parseGameKey(primary, true, false);
    if (auto secondary = xml_node_find_tag(node, "Secondary", true))
        kb.secondary = parseGameKey(secondary, true, false);
    if (auto binding = xml_node_find_tag(node, "Binding", true)) {
        kb.primary = parseGameKey(binding, false, true);
        if (getBoolNodeValue(node, "Inverted"))
            kb.mode = KeyBindings::AxisInv;
        else
            kb.mode = KeyBindings::Axis;
    }
    if (kb.primary.key.empty() && kb.secondary.key.empty()) {
        LOG(ERROR) << "Key binding for <" << tag << "> not found";
        return false;
    }
    map[tag] = kb;
    return true;
}

bool Configuration::loadGameSettings(bool initial) {
    LOG(INFO) << "Loading game settings";
    bool ok = true;
    bool needCapturerReset = false;
    std::string filename;

    //
    // Options/Graphics/DisplaySettings.xml
    //
    {
        filename = toUtf8(mEDSettingsPath) + R"(\Options\Graphics\DisplaySettings.xml)";
        XMLNode *rootNode = xml_parse_file(filename.c_str());
        if (rootNode) {
            if (auto node = xml_node_find_tag(rootNode, "ScreenWidth", true); node && node->text) {
                int width = atol(node->text);
                if (!initial && width != configScreenWidth)
                    needCapturerReset = true;
                configScreenWidth = width;
            }
            if (auto node = xml_node_find_tag(rootNode, "ScreenHeight", true); node && node->text) {
                int height = atol(node->text);
                if (!initial && height != configScreenHeight)
                    needCapturerReset = true;
                configScreenHeight = height;
            }
            if (auto node = xml_node_find_tag(rootNode, "FullScreen", true); node && node->text) {
                FullScreenMode mode = (FullScreenMode) atoi(node->text);
                if (!initial && mode != configFullScreen)
                    needCapturerReset = true;
                configFullScreen = mode;
            }
            xml_node_free(rootNode);
            rootNode = nullptr;
        } else {
            ok = false;
            LOG(ERROR) << "Cannot parse " << filename;
        }
    }

    //
    // Options/Graphics/Settings.xml
    //
    {
        filename = toUtf8(mEDSettingsPath) + R"(\Options\Graphics\Settings.xml)";
        XMLNode *rootNode = xml_parse_file(filename.c_str());
        if (rootNode) {
            if (auto node = xml_node_find_tag(rootNode, "FOV", true); node && node->text)
                configFOV = atof(node->text);
            if (auto node = xml_node_find_tag(rootNode, "GammaOffset", true); node && node->text)
                configGammaOffset = atof(node->text);
            xml_node_free(rootNode);
            rootNode = nullptr;
            // update colors
            for (int i=0; i < 4; i++) {
                double gp = 1.0 / (1.0 + configGammaOffset*0.5);
                for (int c=0; c < 3; c++) {
                    mCalcButtonBGR[i][c] = (uchar)std::clamp(255 * std::pow(mOrigButtonBGR[i][c]/255., gp), 0., 255.);
                    mCalcLstRowBGR[i][c] = (uchar)std::clamp(255 * std::pow(mOrigLstRowBGR[i][c]/255., gp), 0., 255.);
                }
                mCalcButtonHsv[i] = sBgr2Hsv(mCalcButtonBGR[i]);
                mCalcLstRowHsv[i] = sBgr2Hsv(mCalcLstRowBGR[i]);
                mCalcButtonLuv[i] = sBgr2Luv(mCalcButtonBGR[i]);
                mCalcLstRowLuv[i] = sBgr2Luv(mCalcLstRowBGR[i]);
                mCalcButtonGray[i] = sBgr2sGray(mCalcButtonBGR[i]);
                mCalcLstRowGray[i] = sBgr2sGray(mCalcLstRowBGR[i]);
            }
        } else {
            ok = false;
            LOG(ERROR) << "Cannot parse " << filename;
        }
        mUseCalibratedColors = checkNeedColorCalibration();
    }

    if (needCapturerReset)
        Master::getInstance().pushCommand(Command::ResetCapturer);

    return ok;
}

//
// Options/Player/... preset
//
bool Configuration::loadPlayerOptions() {
    LOG(INFO) << "Loading player options";
    std::string filename = toUtf8(mEDSettingsPath) + R"(\Options\Player\StartPreset.start)";
    std::ifstream ifs(filename, std::ifstream::in);
    if (!ifs.is_open()) {
        LOG(ERROR) << "Cannot parse " << filename;
        return false;
    }

    std::string preset;
    std::getline(ifs, preset);
    filename = filenameFromPreset(toUtf8(mEDSettingsPath) + R"(\Options\Player\)", preset, "misc");

    XMLNode *rootNode = xml_parse_file(filename.c_str());
    if (rootNode) {
        if (auto node = xml_node_find_tag(rootNode, "DashboardGUIBrightness", true)) {
            if (auto val = xml_node_attr(node, "Value"))
                configDashboardGUIBrightness = atof(val);
        }
        if (auto filters = xml_node_find_tag(rootNode, "LocationPanelFilters", true)) {
            st::NavPanelFilters npf;
            npf.star = getBoolNodeValue(filters, "Star");
            npf.asteroidCluster = getBoolNodeValue(filters, "AsteroidCluster");
            npf.planetOrMoon = getBoolNodeValue(filters, "PlanetOrMoon");
            npf.landablePlanetOrMoon = getBoolNodeValue(filters, "LandablePlanetOrMoon");
            npf.settlement = getBoolNodeValue(filters, "Settlement");
            npf.station = getBoolNodeValue(filters, "station");
            npf.fleetCarrier = getBoolNodeValue(filters, "fleetCarrier");
            npf.pointOfInterest = getBoolNodeValue(filters, "PointOfInterest");
            npf.signalSource = getBoolNodeValue(filters, "SignalSource");
            npf.system = getBoolNodeValue(filters, "System");
            st::navFilters = npf;
        }
        //	<RouteStartSystem Value="2381282543995" />
        //	<RouteDestinationSystem Value="0" />
        //	<RouteDestinationBody Value="0" />
        //	<RouteDestinationMarketID Value="18446744073709551615" />
        //	<RouteDestinationBodysiteID Value="18446744073709551615" />
        //	<RouteDestinationIsCluster Value="0" />
        //	<RouteDestinationIsSurfaceSettlement Value="0" />
        //		<categories>
        //			<Minerals Value="1" />
        //			<Weapons Value="1" />
        //			<ConsumerItems Value="1" />
        //			<Foods Value="1" />
        //			<Textiles Value="1" />
        //			<Metals Value="1" />
        //			<Narcotics Value="1" />
        //			<Medicines Value="1" />
        //			<IndustrialMaterials Value="1" />
        //			<Technology Value="1" />
        //			<Chemicals Value="1" />
        //			<Machinery Value="1" />
        //			<Other Value="1" />
        //		</categories>

        // 	<MarketFilter_inCargo Value="1" />
        //	<MarketFilter_requiredForMission Value="1" />
        //	<MarketFilter_highDemand Value="1" />
        //	<MarketFilter_rareGoods Value="1" />
        //	<MarketFilter_commodTypeFlags Value="4294967295" />

        marketShowInCargo = getBoolNodeValue(rootNode, "MarketFilter_inCargo");
        marketShowHighDemand = getBoolNodeValue(rootNode, "MarketFilter_highDemand");
        marketShowRareGoods = getBoolNodeValue(rootNode, "MarketFilter_rareGoods");
        marketShowRequiredForMission = getBoolNodeValue(rootNode, "MarketFilter_requiredForMission");
        marketCommodityFilter = getIntNodeValue(rootNode, "MarketFilter_commodTypeFlags");

        xml_node_free(rootNode);
        rootNode = nullptr;
        return true;
    } else {
        configDashboardGUIBrightness = 0.5;
        LOG(ERROR) << "Cannot parse " << filename;
        return false;
    }
}

//
// Options/Bindings/... preset
//
bool Configuration::loadInputBindings() {
    LOG(INFO) << "Loading input bindings";
    bool ok = true;
    mKeyBindingsMap.clear();
    std::string filename = toUtf8(mEDSettingsPath) + R"(\Options\Bindings\StartPreset.4.start)";
    std::ifstream ifs(filename, std::ifstream::in);
    if (ifs.is_open()) {
        XMLNode * rootNode = nullptr;
        std::string preset;
        std::getline(ifs, preset);
        if (preset == "KeyboardMouseOnlyYaw") filename = "ControlSchemes\\KeyboardMouseOnlyYaw.binds";
        else if (preset == "KeyboardMouseOnly") filename = "ControlSchemes\\KeyboardMouseOnly.binds";
        else if (preset == "ClassicKeyboardOnly") filename = "ControlSchemes\\ClassicKeyboardOnly.binds";
        else if (preset == "Empty") filename = "ControlSchemes\\Empty.binds";
        else
            filename = filenameFromPreset(toUtf8(mEDSettingsPath) + R"(\Options\Bindings\)", preset, "binds");
        rootNode = xml_parse_file(filename.c_str());
        if (rootNode) {
            parseKeyBindings(rootNode, mKeyBindingsMap, "Pause");
            parseKeyBindings(rootNode, mKeyBindingsMap, "FocusLeftPanel");
            parseKeyBindings(rootNode, mKeyBindingsMap, "UI_Up");
            parseKeyBindings(rootNode, mKeyBindingsMap, "UI_Down");
            parseKeyBindings(rootNode, mKeyBindingsMap, "UI_Left");
            parseKeyBindings(rootNode, mKeyBindingsMap, "UI_Right");
            parseKeyBindings(rootNode, mKeyBindingsMap, "UI_Select");
            parseKeyBindings(rootNode, mKeyBindingsMap, "UI_Back");
            parseKeyBindings(rootNode, mKeyBindingsMap, "UI_Toggle");
            parseKeyBindings(rootNode, mKeyBindingsMap, "CycleNextPanel");
            parseKeyBindings(rootNode, mKeyBindingsMap, "CyclePreviousPanel");
            parseKeyBindings(rootNode, mKeyBindingsMap, "CycleNextPage");
            parseKeyBindings(rootNode, mKeyBindingsMap, "CyclePreviousPage");
            xml_node_free(rootNode);
            rootNode = nullptr;
        } else {
            ok = false;
            LOG(ERROR) << "Cannot parse " << filename;
        }

        std::getline(ifs, preset);
        if (preset == "KeyboardMouseOnlyYaw") filename = "ControlSchemes\\KeyboardMouseOnlyYaw.binds";
        else if (preset == "KeyboardMouseOnly") filename = "ControlSchemes\\KeyboardMouseOnly.binds";
        else if (preset == "ClassicKeyboardOnly") filename = "ControlSchemes\\ClassicKeyboardOnly.binds";
        else if (preset == "Empty") filename = "ControlSchemes\\Empty.binds";
        else
            filename = filenameFromPreset(toUtf8(mEDSettingsPath) + R"(\Options\Bindings\)", preset, "binds");
        rootNode = xml_parse_file(filename.c_str());
        if (rootNode) {
            parseKeyBindings(rootNode, mKeyBindingsMap, "Pause");
            parseKeyBindings(rootNode, mKeyBindingsMap, "FocusLeftPanel");
            parseKeyBindings(rootNode, mKeyBindingsMap, "RollLeftButton");
            parseKeyBindings(rootNode, mKeyBindingsMap, "RollRightButton");
            parseKeyBindings(rootNode, mKeyBindingsMap, "PitchUpButton");
            parseKeyBindings(rootNode, mKeyBindingsMap, "PitchDownButton");
            parseKeyBindings(rootNode, mKeyBindingsMap, "YawLeftButton");
            parseKeyBindings(rootNode, mKeyBindingsMap, "YawRightButton");
            //parseKeyBindings(rootNode, mKeyBindingsMap, "LeftThrustButton");
            //parseKeyBindings(rootNode, mKeyBindingsMap, "RightThrustButton");
            parseKeyBindings(rootNode, mKeyBindingsMap, "UpThrustButton");
            parseKeyBindings(rootNode, mKeyBindingsMap, "DownThrustButton");
            //parseKeyBindings(rootNode, mKeyBindingsMap, "ForwardThrustButton");
            //parseKeyBindings(rootNode, mKeyBindingsMap, "BackwardThrustButton");
            //parseKeyBindings(rootNode, mKeyBindingsMap, "ForwardKey");
            //parseKeyBindings(rootNode, mKeyBindingsMap, "BackwardKey");
            parseKeyBindings(rootNode, mKeyBindingsMap, "SetSpeedMinus100");
            parseKeyBindings(rootNode, mKeyBindingsMap, "SetSpeedMinus75");
            parseKeyBindings(rootNode, mKeyBindingsMap, "SetSpeedMinus50");
            parseKeyBindings(rootNode, mKeyBindingsMap, "SetSpeedMinus25");
            parseKeyBindings(rootNode, mKeyBindingsMap, "SetSpeedZero");
            parseKeyBindings(rootNode, mKeyBindingsMap, "SetSpeed25");
            parseKeyBindings(rootNode, mKeyBindingsMap, "SetSpeed50");
            parseKeyBindings(rootNode, mKeyBindingsMap, "SetSpeed75");
            parseKeyBindings(rootNode, mKeyBindingsMap, "SetSpeed100");
            parseKeyBindings(rootNode, mKeyBindingsMap, "HyperSuperCombination");       // vJoy_1
            parseKeyBindings(rootNode, mKeyBindingsMap, "Supercruise");                 // vJoy_2
            parseKeyBindings(rootNode, mKeyBindingsMap, "Hyperspace");                  // vJoy_3
            parseKeyBindings(rootNode, mKeyBindingsMap, "GalaxyMapOpen");               // vJoy_4
            parseKeyBindings(rootNode, mKeyBindingsMap, "ToggleCargoScoop");
            parseKeyBindings(rootNode, mKeyBindingsMap, "DeployHardpointToggle");
            parseKeyBindings(rootNode, mKeyBindingsMap, "LandingGearToggle");
            parseKeyBindings(rootNode, mKeyBindingsMap, "TargetNextRouteSystem");       // vJoy_5
            parseKeyBindings(rootNode, mKeyBindingsMap, "MouseReset");                  // vJoy_6
            parseKeyBindings(rootNode, mKeyBindingsMap, "YawAxisRaw");
            parseKeyBindings(rootNode, mKeyBindingsMap, "PitchAxisRaw");
            parseKeyBindings(rootNode, mKeyBindingsMap, "RollAxisRaw");
            xml_node_free(rootNode);
            rootNode = nullptr;
        } else {
            ok = false;
            LOG(ERROR) << "Cannot parse " << filename;
        }
    } else {
        ok = false;
        LOG(ERROR) << "Cannot parse " << filename;
    }
    return ok;
}

bool Configuration::findLatestJournalFile() {
    std::filesystem::path latestJournalFile;
    auto newestTime = std::chrono::file_clock::time_point::min();
    for (const auto &entry: std::filesystem::directory_iterator(mEDLogsPath)) {
        if (!entry.is_regular_file())
            continue;
        auto &ep = entry.path();
        if (ep.filename().string().starts_with("Journal.") && ep.extension() == ".log") {
            auto lastWriteTime = std::filesystem::last_write_time(entry);
            if (lastWriteTime > newestTime) {
                newestTime = lastWriteTime;
                latestJournalFile = ep;
            }
        }
    }
    if (latestJournalFile.empty()) {
        LOG(ERROR) << "Cannot find journal file in " << mEDLogsPath;
        return false;
    }
    mEDCurrentJournalFile = latestJournalFile;
    return true;
}

bool Configuration::preloadGameJournal() {
    LOG(INFO) << "Pre-Loading game journal";
    if (!findLatestJournalFile())
        return false;
    std::ifstream ifs(mEDCurrentJournalFile, std::ifstream::in);
    if (!ifs.is_open()) {
        LOG(ERROR) << "Cannot open journal file: " << mEDCurrentJournalFile;
        return false;
    }

    bool start = true;
    for (;;) {
        std::string line;
        getline(ifs, line);

        std::string event;
        Timestamp timestamp;

        auto ge = parseEvent(line);
        if (!ge)
            return false;
        if (start) {
            start = false;
            if (ge->event != "Fileheader") {
                LOG(ERROR) << "Corrupted journal file, expecting 'Fileheader': " << line;
                return false;
            }
        }
        else if (ge->event == "Shutdown" || ge->event == "Loadout")
            return true;
    }
}

std::ostream& operator<<(std::ostream& os, const st::ShipStatus& st) {
    os << "{";
    os << "gui-focus:" << enum_name<GuiFocus>(::st::guiFocus)<<",";
    if (st.flags.docked) os << "docked,";
    if (st.flags.landed) os << "landed,";
    if (st.flags.landing_gear_down) os << "landing-gear,";
    if (st.flags.shields_up) os << "shields,";
    if (st.flags.cruise) os << "cruise,";
    if (st.flags.fa_off) os << "fa-off,";
    if (st.flags.weapon_on) os << "weapon,";
    if (st.flags.in_wing) os << "wing,";
    if (st.flags.lights_on) os << "lights,";
    if (st.flags.cargo_scoop_on) os << "cargo-scoop,";
    if (st.flags.silent_run) os << "silent,";
    if (st.flags.fuel_scooping) os << "fuel-scooping,";
    if (st.flags.fsd_masslocked) os << "fsd-masslocked,";
    if (st.flags.fsd_charging) os << "fsd-charging,";
    if (st.flags2.fsd_hyperdrive_charging) os << "fsd-hyperdrive-charging,";
    if (st.flags.fsd_cooldown) os << "fsd-сooldown,";
    if (st.flags.fsd_jump) os << "fsd-jump,";
    if (st.flags.fuel_low) os << "fuel-low,";
    if (st.flags.overheating) os << "overheating,";
    if (st.flags.in_danger) os << "in-danger,";
    if (st.flags.in_interdiction) os << "in-interdiction,";
    if (st.flags.hud_in_analysis) os << "hud-in-analysis,";
    if (st.flags.night_vision) os << "night-vision,";
    os << "pips:[" << int(st.pips[0]) << "," << int(st.pips[1]) << "," << int(st.pips[2]) << "]";
    os << "}";
    return os;
}

bool Configuration::loadCalibration() {
    LOG(INFO) << "Loading calibration.json5";
    json5pp::value j;
    try {
        std::ifstream ifs("calibration.json5");
        if (!ifs)
            return false;
        std::stringstream buffer;
        buffer << ifs.rdbuf();
        j = json5pp::parse5(buffer.str());
    } catch (const json5pp::syntax_error& ex) {
        LOG(ERROR) << "Error loading calibration.json5: " << ex.what();
        return false;
    }
    if (j.at("dashboardGUIBrightness").is_number())
        calibrationDashboardGUIBrightness = j.at("dashboardGUIBrightness").as_number();
    if (j.at("gammaOffset").is_number())
        calibrationGammaOffset = j.at("gammaOffset").as_number();
    if (j.at("screenWidth").is_number())
        calibrationScreenWidth = j.at("screenWidth").as_integer();
    if (j.at("screenHeight").is_number())
        calibrationScreenHeight = j.at("screenHeight").as_integer();
    std::optional<FullScreenMode> fullScreenMode;
    if (auto& jfs = j["fullScreen"]; jfs) {
        if (jfs.is_string())
            fullScreenMode = enum_cast<FullScreenMode>(jfs.as_string());
        else if (jfs.is_number())
            fullScreenMode = enum_cast<FullScreenMode>(jfs.as_integer());
        if (fullScreenMode.has_value())
            calibrationFullScreen = fullScreenMode.value();
    }

    mCalibratedButtonBGR[int(WState::Normal)] = color_from_json(j.at("normalButton"));
    mCalibratedButtonBGR[int(WState::Focused)] = color_from_json(j.at("focusedButton"));
    mCalibratedButtonBGR[int(WState::Active)] = color_from_json(j.at("activeToggle"));
    mCalibratedButtonBGR[int(WState::Disabled)] = color_from_json(j.at("disabledButton"));
    mCalibratedLstRowBGR[int(WState::Normal)] = color_from_json(j.at("normalRow"));
    mCalibratedLstRowBGR[int(WState::Focused)] = color_from_json(j.at("focusedRow"));
    mCalibratedLstRowBGR[int(WState::Active)] = color_from_json(j.at("activeRow"));
    mCalibratedLstRowBGR[int(WState::Disabled)] = color_from_json(j.at("disabledRow"));

    for (int i=0; i < 4; i++) {
        mCalibratedButtonHsv[i] = sBgr2Hsv(mCalibratedButtonBGR[i]);
        mCalibratedButtonLuv[i] = sBgr2Luv(mCalibratedButtonBGR[i]);
        mCalibratedButtonGray[i] = sBgr2sGray(mCalibratedButtonBGR[i]);
        mCalibratedLstRowHsv[i] = sBgr2Hsv(mCalibratedLstRowBGR[i]);
        mCalibratedLstRowLuv[i] = sBgr2Luv(mCalibratedLstRowBGR[i]);
        mCalibratedLstRowGray[i] = sBgr2sGray(mCalibratedLstRowBGR[i]);
    }
    mUseCalibratedColors = checkNeedColorCalibration();
    LOG(INFO) << "Calibration data loaded from 'calibration.json5'";
    return true;
}

void Configuration::setCalibrationResult(const std::array<cv::Vec3b,4>& buttonBGR, const std::array<cv::Vec3b,4>& lstRowBGR)
{
    mUseCalibratedColors = true;
    for (int i=0; i < 4; i++) {
        if (buttonBGR[i] == cv::Vec3b::zeros())
            mCalibratedButtonBGR[i] = mCalcButtonBGR[i];
        else
            mCalibratedButtonBGR[i] = buttonBGR[i];
        mCalibratedButtonHsv[i] = sBgr2Hsv(mCalibratedButtonBGR[i]);
        mCalibratedButtonLuv[i] = sBgr2Luv(mCalibratedButtonBGR[i]);
        mCalibratedButtonGray[i] = sBgr2sGray(mCalibratedButtonBGR[i]);
    }
    for (int i=0; i < 4; i++) {
        if (lstRowBGR[i] == cv::Vec3b::zeros())
            mCalibratedLstRowBGR[i] = mCalcLstRowBGR[i];
        else
            mCalibratedLstRowBGR[i] = lstRowBGR[i];
        mCalibratedLstRowHsv[i] = sBgr2Hsv(mCalibratedLstRowBGR[i]);
        mCalibratedLstRowLuv[i] = sBgr2Luv(mCalibratedLstRowBGR[i]);
        mCalibratedLstRowGray[i] = sBgr2sGray(mCalibratedLstRowBGR[i]);
    }

    calibrationDashboardGUIBrightness = configDashboardGUIBrightness;
    calibrationGammaOffset = configGammaOffset;
    calibrationScreenWidth = configScreenWidth;
    calibrationScreenHeight = configScreenHeight;
    calibrationFullScreen = configFullScreen;

    LOG(INFO) << "Set color calibration result:";
    for (int i=0; i < 4; i++) {
        WState ws = enum_cast<WState>(i).value();
        LOG(INFO) << "Button " << enum_name<WState>(ws) << " calc-to-calibrated distance:"
                  << " bgr " << distanceBGR(mCalcButtonBGR[i], mCalibratedButtonBGR[i])
                  << " hsv " << distanceHsv(mCalcButtonHsv[i], mCalibratedButtonHsv[i])
                  << " luv " << distanceLuv(mCalcButtonLuv[i], mCalibratedButtonLuv[i])
                  << " calc bgr: " << mCalcButtonBGR[i] << " calibrated bgr: " << mCalibratedButtonBGR[i];
    }
    int minDistLuv = 1000;
    int minLuvI = -1;
    int minLuvJ = -1;
    int minDistHsv = 1000;
    int minHsvI = -1;
    int minHsvJ = -1;
    int minDistBgr = 1000;
    int minBgrI = -1;
    int minBgrJ = -1;
    for (int i=0; i < 4; i++) {
        for (int j=0; j < 4; j++) {
            if (j == i)
                continue;
            int distLuv = distanceLuv(mCalibratedButtonLuv[i], mCalibratedButtonLuv[j]);
            if (distLuv < minDistLuv) { minDistLuv = distLuv; minLuvI = i; minLuvJ = j; }
            int distHsv = distanceHsv(mCalibratedButtonHsv[i], mCalibratedButtonHsv[j]);
            if (distHsv < minDistHsv) { minDistHsv = distHsv; minHsvI = i; minHsvJ = j; }
            int distBgr = distanceBGR(mCalibratedButtonBGR[i], mCalibratedButtonBGR[j]);
            if (distBgr < minDistBgr) { minDistBgr = distBgr; minBgrI = i; minBgrJ = j; }
        }
    }
    LOG(INFO) << "Button Luv min distance: " << minDistLuv
              << " between " << enum_name<WState>((WState)minLuvI) << " and " << enum_name<WState>((WState)minLuvJ);
    LOG(INFO) << "Button Hsv min distance: " << minDistHsv
              << " between " << enum_name<WState>((WState)minHsvI) << " and " << enum_name<WState>((WState)minHsvJ);
    LOG(INFO) << "Button BGR min distance: " << minDistBgr
              << " between " << enum_name<WState>((WState)minBgrI) << " and " << enum_name<WState>((WState)minBgrJ);

    for (int i=0; i < 4; i++) {
        WState ws = enum_cast<WState>(i).value();
        LOG(INFO) << "LstRow " << enum_name<WState>(ws) << " calc-to-calibrated distance:"
                  << " bgr " << distanceBGR(mCalcLstRowBGR[i], mCalibratedLstRowBGR[i])
                  << " hsv " << distanceHsv(mCalcLstRowHsv[i], mCalibratedLstRowHsv[i])
                  << " luv " << distanceLuv(mCalcLstRowLuv[i], mCalibratedLstRowLuv[i])
                  << " calc bgr: " << mCalcLstRowBGR[i] << " calibrated bgr: " << mCalibratedLstRowBGR[i];
    }
    minDistLuv = 1000;
    minLuvI = -1;
    minLuvJ = -1;
    minDistHsv = 1000;
    minHsvI = -1;
    minHsvJ = -1;
    minDistBgr = 1000;
    minBgrI = -1;
    minBgrJ = -1;
    for (int i=0; i < 4; i++) {
        for (int j=0; j < 4; j++) {
            if (j == i)
                continue;
            int distLuv = distanceLuv(mCalibratedLstRowLuv[i], mCalibratedLstRowLuv[j]);
            if (distLuv < minDistLuv) { minDistLuv = distLuv; minLuvI = i; minLuvJ = j; }
            int distHsv = distanceHsv(mCalibratedLstRowHsv[i], mCalibratedLstRowHsv[j]);
            if (distHsv < minDistHsv) { minDistHsv = distHsv; minHsvI = i; minHsvJ = j; }
            int distBgr = distanceBGR(mCalibratedLstRowBGR[i], mCalibratedLstRowBGR[j]);
            if (distBgr < minDistBgr) { minDistBgr = distBgr; minBgrI = i; minBgrJ = j; }
        }
    }
    LOG(INFO) << "LstRow Luv min distance: " << minDistLuv
              << " between " << enum_name<WState>((WState)minLuvI) << " and " << enum_name<WState>((WState)minLuvJ);
    LOG(INFO) << "LstRow Hsv min distance: " << minDistHsv
              << " between " << enum_name<WState>((WState)minHsvI) << " and " << enum_name<WState>((WState)minHsvJ);
    LOG(INFO) << "LstRow BGR min distance: " << minDistBgr
              << " between " << enum_name<WState>((WState)minBgrI) << " and " << enum_name<WState>((WState)minBgrJ);
}

bool Configuration::saveCalibration() const {
    std::ofstream wf("calibration.json5", std::ios::trunc | std::ios::binary);
    wf << "{" << std::endl;
    wf << "  dashboardGUIBrightness: " << configDashboardGUIBrightness << "," << std::endl;
    wf << "  gammaOffset:   " << configGammaOffset << "," << std::endl;
    wf << "  screenWidth:   " << configScreenWidth << ","  << std::endl;
    wf << "  screenHeight:  " << configScreenHeight << ","  << std::endl;
    wf << "  fullScreen:    " << enum_underlying<FullScreenMode>(configFullScreen) << ", // '" << enum_name<FullScreenMode>(configFullScreen) << "'"  << std::endl;
    wf << "  // button colors " << std::endl;
    wf << "  normalButton:  " << std::format("'#{:06x}',", decodeBGR(mCalibratedButtonBGR[int(WState::Normal)])) << std::endl;
    wf << "  focusedButton: " << std::format("'#{:06x}',", decodeBGR(mCalibratedButtonBGR[int(WState::Focused)])) << std::endl;
    wf << "  activeToggle:  " << std::format("'#{:06x}',", decodeBGR(mCalibratedButtonBGR[int(WState::Active)])) << std::endl;
    wf << "  disabledButton:" << std::format("'#{:06x}',", decodeBGR(mCalibratedButtonBGR[int(WState::Disabled)])) << std::endl;
    wf << "  // list row colors " << std::endl;
    wf << "  normalRow:     " << std::format("'#{:06x}',", decodeBGR(mCalibratedLstRowBGR[int(WState::Normal)])) << std::endl;
    wf << "  focusedRow:    " << std::format("'#{:06x}',", decodeBGR(mCalibratedLstRowBGR[int(WState::Focused)])) << std::endl;
    wf << "  activeRow:     " << std::format("'#{:06x}',", decodeBGR(mCalibratedLstRowBGR[int(WState::Active)])) << std::endl;
    wf << "  disabledRow:   " << std::format("'#{:06x}',", decodeBGR(mCalibratedLstRowBGR[int(WState::Disabled)])) << std::endl;
    wf << "}" << std::endl;
    wf.close();
    LOG(INFO) << "Calibration data saved to 'calibration.json5'";
    return true;
}

bool Configuration::checkResolutionSupported(cv::Size gameSize, std::string& error) {
    if (configFullScreen == FullScreenMode::Window)
        return true;
    cv::Size displaySize(configScreenWidth, configScreenHeight);
    double cmp = gameSize.aspectRatio() - displaySize.aspectRatio();
    if (std::abs(cmp) > 0.01) {
        std::string msg = std_format(
                _("In FullScreen/Borderless mode aspect ratio must match, but {:.3f} != {:.3f} for {}x{} and {}x{}"),
                gameSize.aspectRatio(), displaySize.aspectRatio(),
                gameSize.width, gameSize.height, configScreenWidth, configScreenHeight);
        LOG(ERROR) << msg;
        return false;
    }
    return true;
}

bool Configuration::checkNeedColorCalibration() const {
    //if (std::abs(configDashboardGUIBrightness - calibrationDashboardGUIBrightness) > 0.001)
    //    return true;
    if (std::abs(configGammaOffset - calibrationGammaOffset) > 0.1)
        return true;
    return false;
}

std::string Configuration::getShortcutFor(Command cmd) const {
    for (const auto& entry : keyMapping) {
        if (entry.second == cmd) {
            return encodeShortcut(entry.first.first, entry.first.second);
        }
    }
    return "";
}

CommodityCategory& Configuration::getOrAddCommodityCategory(CommodityCategory&& cc_add) {
    std::string& nameId = cc_add.nameId;
    if (nameId.starts_with("$MARKET_category_") && nameId.ends_with(";")) {
        nameId = cc_add.nameId.substr(17, nameId.size() - 17 - 1);
    }
    auto it = commodityCategoryMap.find(nameId);
    if (it != commodityCategoryMap.end()) {
        CommodityCategory& cc = *it->second;
        for (int i=0; i < 2; i++) {
            if (cc.translation[i].empty() && !cc_add.translation[i].empty()) {
                cc.translation[i] = cc_add.translation[i];
                mCommodityDatabaseUpdated = true;
            }
        }
        return cc;
    }
    allKnownCommodityCategories.emplace_back(cc_add);
    CommodityCategory& cc = allKnownCommodityCategories.back();
    if (st::lng != Lang::XX)
        cc.name = cc.translation[int(st::lng)];
    else
        cc.name = cc.nameId;
    cc.wide = toUtf16(cc.name);
    commodityCategoryMap[nameId] = &cc;
    mCommodityDatabaseUpdated = true;
    if (changeDirListener)
        LOG(ERROR) << "New CommodityCategory added: " << nameId;
    return cc;
}

Commodity& Configuration::getOrAddCommodity(Commodity&& c_add) {
    std::string& nameId = c_add.nameId;
    if (nameId.starts_with("$") && nameId.ends_with("_name;")) {
        nameId = c_add.nameId.substr(1, nameId.size() - 7);
    }
    auto it = commodityMap.find(nameId);
    if (it != commodityMap.end()) {
        Commodity& c = *it->second;
        for (int i=0; i < 2; i++) {
            if (c.intId == 0 && c_add.intId != 0) {
                c.intId = c_add.intId;
                mCommodityDatabaseUpdated = true;
            }
            if (c.category != c_add.category && !c_add.category->nameId.empty()) {
                c.category = c_add.category;
                mCommodityDatabaseUpdated = true;
            }
            if (c.translation[i].empty() && !c_add.translation[i].empty()) {
                c.translation[i] = c_add.translation[i];
                mCommodityDatabaseUpdated = true;
            }
            if (!c.rare && c_add.rare) {
                c.rare = true;
                mCommodityDatabaseUpdated = true;
            }
        }
        return c;
    }
    allKnownCommodities.emplace_back(c_add);
    Commodity& c = allKnownCommodities.back();
    if (st::lng != Lang::XX)
        c.name = c.translation[int(st::lng)];
    else
        c.name = c.nameId;
    c.wide = toUtf16(c.name);
    FuzzyMatch fm;
    c.wocr = fm.toOCR(c.wide);
    commodityMap[nameId] = &c;
    mCommodityDatabaseUpdated = true;
    if (changeDirListener)
        LOG(ERROR) << "New Commodity added: " << nameId;
    return c;
}

CommodityCategory* Configuration::getCommodityCategoryByName(const std::string& name) {
    auto it = commodityCategoryMap.find(name);
    if (it != commodityCategoryMap.end())
        return it->second;
    for (auto& cc : allKnownCommodityCategories) {
        if (name == cc.name)
            return &cc;
    }
    return nullptr;
}

Commodity* Configuration::getCommodityById(const std::string& id) {
    if (id.empty())
        return nullptr;
    auto it = commodityMap.find(id);
    if (it != commodityMap.end())
        return it->second;
    return nullptr;
}

Commodity* Configuration::getCommodityByName(const std::string& name, bool fuzzy_ocr) {
    if (name.empty())
        return nullptr;
    auto it = commodityMap.find(name);
    if (it != commodityMap.end())
        return it->second;
    if (!fuzzy_ocr) {
        for (auto &c: allKnownCommodities) {
            if (name == c.name)
                return &c;
        }
        return nullptr;
    }
    return getCommodityByName(toUtf16(name), true);
}
Commodity* Configuration::getCommodityByName(const std::wstring& name, bool fuzzy_ocr) {
    if (name.empty())
        return nullptr;
    if (!fuzzy_ocr) {
        for (auto &c: allKnownCommodities) {
            if (name == c.wide)
                return &c;
        }
        return nullptr;
    }

    FuzzyMatch matcher;
    std::wstring wocr = matcher.toOCR(name);
    double bestScore = -1;
    int bestScoreIndex = -1;
    for (int i=0; i < allKnownCommodities.size(); i++) {
        double score = matcher.ratio(wocr, allKnownCommodities[i].wocr);
        if (score > bestScore) {
            bestScore = score;
            bestScoreIndex = i;
        }
    }
    if (bestScore < 40) {
        LOG(WARNING) << "FuzzyMatch commodity score: '" << bestScore << "' for '"
                   << ((bestScoreIndex < 0) ? std::string() : allKnownCommodities[bestScoreIndex].name)
                   << "'";
        return nullptr;
    }
    return &allKnownCommodities[bestScoreIndex];
}

std::vector<Commodity*> Configuration::getMarketInSellOrder() {
    std::vector<Commodity*> out;
    if (st::lng == Lang::XX)
        return out;
    spMarket market = st::currentMarket;
    if (!market)
        return out;
    // check filters are supported
    if (marketCommodityFilter != marketCommodityFilterShowAll) {
        if (marketShowRequiredForMission || marketShowHighDemand)
            return out;
    }
    bool isFC = (market->stationType == "FleetCarrier");
    // add everything we can sell, then sort according to market order
    for (auto& c : allKnownCommodities) {
        auto it = market->items.find(&c);
        if (it == market->items.end())
            continue;
        MarketLine& ml = it->second;
        if (c.rare && c.ship.count <= c.ship.stolen)
            continue;
        if (isFC) {
            if (ml.demand <= 0)
                continue;
        } else {
            if (!ml.isConsumer || ml.demand <= 0) {
                if (c.ship.count <= c.ship.stolen)
                    continue;
            }
        }
        if (marketShowRareGoods && c.rare)
            out.push_back(&c);
        else if (marketShowInCargo)
            out.push_back(&c);
        else if ((marketCommodityFilter & (1 << c.category->intId)) != 0)
            out.push_back(&c);
        else if ((marketCommodityFilter == marketCommodityFilterShowNone) && !marketShowRareGoods && !marketShowInCargo)
            out.push_back(&c);
    }
    std::sort(out.begin(), out.end(), [](Commodity* a, Commodity* b) {
        int cmp = a->category->wide.compare(b->category->wide);
        if (cmp < 0)
            return true;
        if (cmp > 0)
            return false;
        return a->wide < b->wide;
    });
    return out;
}

std::vector<Commodity*> Configuration::getMarketInBuyOrder() {
    std::vector<Commodity*> out;
    if (st::lng == Lang::XX)
        return out;
    spMarket market = st::currentMarket;
    if (!market)
        return out;
    // check filters are supported
    if (marketCommodityFilter != marketCommodityFilterShowAll) {
        if (marketShowRequiredForMission || marketShowHighDemand)
            return out;
    }
    // add everything we can buy, then sort according to market order
    for (auto& c : allKnownCommodities) {
        auto it = market->items.find(&c);
        if (it == market->items.end())
            continue;
        MarketLine& ml = it->second;
        if (ml.stock <= 0)
            continue;
        if (marketShowRareGoods && c.rare)
            out.push_back(&c);
        else if (marketShowInCargo && c.ship.count > c.ship.stolen)
            out.push_back(&c);
        else if ((marketCommodityFilter & (1 << c.category->intId)) != 0)
            out.push_back(&c);
        else if ((marketCommodityFilter == marketCommodityFilterShowNone) && !marketShowRareGoods && !marketShowInCargo)
            out.push_back(&c);
    }
    std::sort(out.begin(), out.end(), [](Commodity* a, Commodity* b) {
        int cmp = a->category->wide.compare(b->category->wide);
        if (cmp < 0)
            return true;
        if (cmp > 0)
            return false;
        return a->wide < b->wide;
    });
    return out;
}

std::vector<Commodity*> Configuration::getAllKnownCommodities() {
    std::vector<Commodity*> out;
    // add everything, then sort according to market order
    for (auto& c : allKnownCommodities) {
        out.push_back(&c);
    }
    std::sort(out.begin(), out.end(), [](Commodity* a, Commodity* b) {
        int cmp = a->category->wide.compare(b->category->wide);
        if (cmp < 0)
            return true;
        if (cmp > 0)
            return false;
        return a->wide < b->wide;
    });
    return out;
}

bool Configuration::loadMarket(Timestamp event_timestamp) {
    LOG(INFO) << "Loading Market.json";
    json5pp::value j_market;
    try {
        std::ifstream marketFile(mEDLogsPath + L"/Market.json", std::ifstream::in);
        if (marketFile.fail()) {
            LOG(ERROR) << "Cannot read file: " << (mEDLogsPath + L"/Market.json");
            return false;
        }
        j_market = json5pp::parse5(marketFile);
        marketFile.close();
    } catch (...) {
        LOG(ERROR) << "Failed to read/parse Market.json";
        return false;
    }
    if (!j_market)
        return false;
    Timestamp timestamp;
    if (!parseTimestamp(j_market, timestamp) || event_timestamp < timestamp)
        return false;

    spMarket market = std::shared_ptr<Market>(new Market{
            .timestamp = timestamp,
            .marketId = j_market.at("MarketID").as_integer(),
            .stationName = j_market.at("StationName").as_string(),
            .stationType = j_market.at("StationType").as_string(),
            .starSystem = j_market.at("StarSystem").as_string(),
    });
    auto items = j_market.at("Items").as_array();
    for (auto& j_item : items) {
        auto item = j_item.as_object();
        std::array<std::string,2> translation;
        if (st::lng == Lang::EN)
            translation = {item.at("Category_Localised").as_string(),""};
        if (st::lng == Lang::RU)
            translation = {"",item.at("Category_Localised").as_string()};
        CommodityCategory& cc = getOrAddCommodityCategory({
                .nameId = item.at("Category").as_string(),
                .translation = translation
        });
        if (st::lng == Lang::EN)
            translation = {item.at("Name_Localised").as_string(),""};
        if (st::lng == Lang::RU)
            translation = {"",item.at("Name_Localised").as_string()};
        Commodity& commodity = getOrAddCommodity({
                .intId = item.at("id").as_integer(),
                .nameId = item.at("Name").as_string(),
                .category = &cc,
                .translation = translation,
                .rare = j_item.at("Rare",false).as_boolean()
        });
        MarketLine ml {};
        ml.buyPrice = item.at("BuyPrice").as_int32();
        ml.sellPrice = item.at("SellPrice").as_int32();
        ml.meanPrice = item.at("MeanPrice").as_int32();
        ml.stock = item.at("Stock").as_int32();
        ml.demand = item.at("Demand").as_int32();
        ml.stockBracket = (uint8_t)item.at("StockBracket").as_integer();
        ml.demandBracket = (uint8_t)item.at("DemandBracket").as_integer();
        ml.isConsumer = item.at("Consumer").as_boolean();
        ml.isProducer = item.at("Producer").as_boolean();
        market->items.emplace(&commodity, ml);
    }
    if (mCommodityDatabaseUpdated)
        dumpCommodityDatabase();

    st::currentMarket.swap(market);
    gal::setMarketData(st::currentMarket);
    return true;
}

bool Configuration::loadCargo(Timestamp event_timestamp) {
    json5pp::value j_cargo;
    try {
        std::ifstream cargoFile(mEDLogsPath + L"/Cargo.json", std::ifstream::in);
        if (cargoFile.fail()) {
            LOG(ERROR) << "Cannot read file: " << (mEDLogsPath + L"/Cargo.json");
            return false;
        }
        j_cargo = json5pp::parse5(cargoFile);
        cargoFile.close();
    } catch (...) {
        LOG(ERROR) << "Failed to read/parse Cargo.json";
        return false;
    }
    if (!j_cargo)
        return false;
    Timestamp timestamp;
    if (!parseTimestamp(j_cargo, timestamp) || event_timestamp < timestamp)
        return false;

    spShipCargo cargo = std::shared_ptr<ShipCargo>(new ShipCargo({
            .timestamp = timestamp,
            .vessel = j_cargo.at("Vessel").as_string(),
            .count = j_cargo.at("Count").as_integer(),
    }));
    auto items = j_cargo.at("Inventory").as_array();
    for (auto& j_item : items) {
        auto item = j_item.as_object();
        auto name = item.at("Name").as_string();
        if (name.empty()) {
            LOG(ERROR) << "Bad cargo item name: " << name;
            continue;
        }
        Commodity* c = getCommodityByName(name, false);
        if (!c) {
            LOG(ERROR) << "Unknown cargo item name: " << name << ", adding to dummy category";
            std::array<std::string,2> translation;
            CommodityCategory* cc = getCommodityCategoryByName("");
            if (st::lng == Lang::EN)
                translation = {item.at("Name_Localised").as_string(),""};
            if (st::lng == Lang::RU)
                translation = {"",item.at("Name_Localised").as_string()};
            c = &getOrAddCommodity({.intId = 0, .nameId = name, .category = cc, .translation = translation, .rare = false});
        }
        c->ship.timestamp = timestamp;
        c->ship.count = item.at("Count").as_integer();
        c->ship.stolen = item.at("Stolen").as_integer();
        cargo->inventory.push_back(c);
    }
    st::currentCargo.swap(cargo);
    Timestamp zero_time;
    for (auto& c : allKnownCommodities) {
        if (c.ship.timestamp > zero_time && c.ship.timestamp < timestamp) {
            c.ship = {};
        }
    }
    return true;
}

bool Configuration::loadNavRoute(Timestamp event_timestamp) {
    json5pp::value j_route;
    try {
        std::ifstream routeFile(mEDLogsPath + L"/NavRoute.json", std::ifstream::in);
        if (routeFile.fail()) {
            LOG(ERROR) << "Cannot read file: " << (mEDLogsPath + L"/NavRoute.json");
            return false;
        }
        j_route = json5pp::parse5(routeFile);
        routeFile.close();
    } catch (...) {
        LOG(ERROR) << "Failed to read/parse NavRoute.json";
        return false;
    }
    if (!j_route)
        return false;
    Timestamp timestamp;
    if (!parseTimestamp(j_route, timestamp) || event_timestamp < timestamp)
        return false;

    spNavRoute route = std::make_shared<NavRoute>();
    route->timestamp = timestamp;
    auto entries = j_route.at("Route").as_array();
    for (auto& je : entries) {
        std::string starSystem = je["StarSystem"].as_string();
        int64_t systemAddress = je["SystemAddress"].as_int64();
        cv::Point3d pos;
        std::string starClass = je["StarClass"].as_string();
        if (je["StarPos"].is_array()) {
            auto& jp = je["StarPos"];
            pos = {jp[0].as_number(), jp[1].as_number(), jp[2].as_number()};
        }
        route->route.emplace_back(starSystem, systemAddress, pos, starClass);
    }
    st::currentNavRoute.swap(route);
    return true;
}

bool Configuration::loadCommodityDatabase() {
    LOG(INFO) << "Loading commodity database";
    json5pp::value j;
    try {
        std::ifstream dbf("commodity-database.json5");
        if (!dbf)
            return false;
        std::stringstream buffer;
        buffer << dbf.rdbuf();
        j = json5pp::parse5(buffer.str());
    } catch (const json5pp::syntax_error& ex) {
        LOG(ERROR) << "Error loading commodity-database.json5: " << ex.what();
        return false;
    }
    for (auto& jcc_it : j.as_object()) {
        if (jcc_it.first.contains("-order-"))
            continue;
        CommodityCategory cc_add;
        cc_add.nameId = jcc_it.first;
        auto& jcc = jcc_it.second;
        cc_add.intId = jcc["id"].as_integer();
        cc_add.translation[int(Lang::EN)] = jcc["en"].as_string();
        cc_add.translation[int(Lang::RU)] = jcc["ru"].as_string();
        CommodityCategory& cc = getOrAddCommodityCategory(std::move(cc_add));
        for (auto& jc_it : jcc["items"].as_object()) {
            auto& jc = jc_it.second;
            Commodity c_add{
                    .intId = jc["id"].as_integer(),
                    .nameId = jc_it.first,
                    .category = &cc,
                    .translation = {jc["en"].as_string(), jc["ru"].as_string()},
                    .rare = jc.at("rare",false).as_boolean(),
            };
            getOrAddCommodity(std::move(c_add));
        }
    }
    for (auto& jcc_it : j.as_object()) {
        if (!jcc_it.first.contains("-order-"))
            continue;
        Lang l = jcc_it.first.ends_with("-en") ? Lang::EN : Lang::RU;
        int64_t commodityOrder = 1;
        for (auto& jn : jcc_it.second.as_array()) {
            if (!jn.is_string())
                continue;
            auto c = getCommodityByName(jn.as_string(), false);
            if (c)
                c->carrierSortingOrder[int(l)] = commodityOrder;
            commodityOrder += 1;
        }
    }
    mCommodityDatabaseUpdated = false;
    return true;
}

bool Configuration::dumpCommodityDatabase() {
    std::ofstream wf("commodity-database.json5", std::ios::trunc | std::ios::binary);
    wf << "{" << std::endl;
    for (auto& ccit : commodityCategoryMap) {
        auto& cc = *ccit.second;
        wf << "  '" << cc.nameId << "': {" << std::endl;
        wf << "    id: " << cc.intId << "," << std::endl;
        wf << "    en: " << json5pp::value(cc.translation[int(Lang::EN)]) << "," << std::endl;
        wf << "    ru: " << json5pp::value(cc.translation[int(Lang::RU)]) << "," << std::endl;
        wf << "    items: {" << std::endl;
        for (auto& cit : commodityMap) {
            auto& c = *cit.second;
            if (c.category != &cc) continue;
            wf << "      " << c.nameId << ": {" << std::endl;
            wf << "        id: " << c.intId << "," << std::endl;
            if (c.rare)
                wf << "        rare: true," << std::endl;
            wf << "        en: " << json5pp::value(c.translation[int(Lang::EN)]) << "," << std::endl;
            wf << "        ru: " << json5pp::value(c.translation[int(Lang::RU)]) << "," << std::endl;
            wf << "      }," << std::endl;
        }
        wf << "    }," << std::endl;
        wf << "  }," << std::endl;
    }
    for (int l=0; l < 2; l++) {
        std::string suffix = l==int(Lang::EN) ? "-en" : "-ru";
        std::vector<Commodity *> cv;
        for (auto &c: allKnownCommodities)
            cv.push_back(&c);
        std::sort(cv.begin(), cv.end(), [l](Commodity *a, Commodity *b) {
            int64_t ao = a->carrierSortingOrder[l];
            int64_t bo = b->carrierSortingOrder[l];
            if (ao == 0) ao = a->intId;
            if (bo == 0) bo = b->intId;
            return ao < bo;
        });
        wf << "  'carrier-order" << suffix << "': [" << std::endl;
        for (auto &c: cv) {
            wf << "    " << json5pp::value(c->nameId) << ", // " << c->translation[0] << " | " << c->translation[1] << std::endl;
        }
        wf << "  ]," << std::endl;
    }
    wf << "}" << std::endl;
    wf.close();
    mCommodityDatabaseUpdated = false;
    return true;
}

const char* Configuration::makeTesseractWordsFile() {
    FuzzyMatch fuzzyMatch;
    std::set<std::wstring> allWords;
    for (auto& c : allKnownCommodities) {
        std::wistringstream iss(c.wide);
        std::wstring word;
        while (iss >> word) {
            std::wstring ocr = fuzzyMatch.toOCR(trimWithPunktuation(word));
            allWords.insert(ocr);
        }
    }
    std::ofstream wf("tesseract-words.txt", std::ios::out | std::ios::trunc | std::ios::binary);
    for (auto& w : allWords) {
        wf << toUtf8(w) << '\n';
    }
    wf.close();
    return "tesseract-words.txt";
}

static const char* ExplainAction(DWORD dwAction)
{
    switch (dwAction)
    {
    case FILE_ACTION_ADDED:
        return "Added";
    case FILE_ACTION_REMOVED:
        return "Deleted";
    case FILE_ACTION_MODIFIED:
        return "Modified";
    case FILE_ACTION_RENAMED_OLD_NAME:
        return "Renamed From";
    case FILE_ACTION_RENAMED_NEW_NAME:
        return "Renamed To";
    default:
        return "BAD DATA";
    }
}

void Configuration::changeDirThreadLoop() {
    SetThreadDescription(GetCurrentThread(), L"Directory listener");

    const HANDLE handles[] = {hShutdownEvent, changeDirListener->GetWaitHandle()};

    std::string journalLine;
    std::ifstream journalStream(mEDCurrentJournalFile, std::ifstream::in);
    // read all events from journal
    readJournalChanges(journalStream, journalLine);
    // read last ship status after all events
    loadGameStatus();

    for(;;) {
        DWORD rc = ::MsgWaitForMultipleObjectsEx(
                        _countof(handles),
                        handles,
                        INFINITE,
                        QS_ALLINPUT,
                        MWMO_INPUTAVAILABLE | MWMO_ALERTABLE);
        if (rc == WAIT_OBJECT_0)
            break;
        if (rc != (WAIT_OBJECT_0+1))
            continue;

        // We've received a notification in the queue.
        bool needReloadSettings = false;
        bool needReloadOptions = false;
        bool needReloadBindings = false;
        bool needReloadStatus = false;
        std::wstring journalFilenameW;

        DWORD action;
        std::wstring filenameW;
        Sleep(100); // let ED finish writes
        while (changeDirListener->Pop(action, filenameW)) {
            LOG(DEBUG) << "File changes: " << ExplainAction(action) << " for file " << toUtf8(filenameW);
            if (filenameW.ends_with(L".log") && filenameW.contains(L"\\Journal."))
                journalFilenameW = filenameW;
            else if (filenameW.ends_with(L"\\Status.json"))
                needReloadStatus = true;
            else if (filenameW.ends_with(LR"(\Options\Graphics\DisplaySettings.xml)"))
                needReloadSettings = true;
            else if (filenameW.ends_with(LR"(\Options\Graphics\Settings.xml)"))
                needReloadSettings = true;
            else if (filenameW.contains(LR"(\Options\Player\)"))
                needReloadOptions = true;
            else if (filenameW.contains(LR"(\Options\Bindings\)"))
                needReloadBindings = true;
        }

        if (needReloadSettings)
            loadGameSettings(false);
        if (needReloadOptions)
            loadPlayerOptions();
        if (needReloadBindings)
            loadInputBindings();
        if (needReloadStatus)
            loadGameStatus();

        // real all events from journal
        readJournalChanges(journalStream, journalLine);

        if (!journalFilenameW.empty() && journalFilenameW != mEDCurrentJournalFile) {
            mEDCurrentJournalFile = journalFilenameW;
            if (journalStream.is_open())
                journalStream.close();
            journalStream.open(mEDCurrentJournalFile, std::ifstream::in);
            // real all events from journal
            readJournalChanges(journalStream, journalLine);
        }
    }
}

void Configuration::readJournalChanges(std::ifstream& journalStream, std::string& journalLine) {
    if (!journalStream.is_open())
        return;
    for (;;) {
        journalStream.clear();
        char buffer[1024];
        journalStream.getline(buffer, sizeof(buffer));
        int count = journalStream.gcount();
        if (count == 0) {
            if (journalStream.eof())
                return;
            if (journalStream.fail()) {
                LOG(ERROR) << "Journal read error: " << strerror(errno);
                return;
            }
        } else {
            int len = strlen(buffer);
            journalLine.append(buffer, len);
            if (len == count)
                continue; // no '\n' was extracted from stream
            auto ge = parseEvent(journalLine);
            if (ge && ge->event == "Shutdown")
                journalStream.close();
            journalLine.clear();
        }
    }
}


#include "detect/Detector.h"

using namespace widget;
using namespace detect;

static cv::Vec3b color_from_json(const json5pp::value& v) {
    unsigned bgr = 0;
    if (v.is_number())
        bgr = v.as_unsigned();
    else if (v.is_array()) {
        unsigned r = v.as_array().at(0).as_unsigned();
        unsigned g = v.as_array().at(1).as_unsigned();
        unsigned b = v.as_array().at(2).as_unsigned();
        bgr = (r&0xFF) | ((g&0xFF)<<8) | ((b&0xFF)<<16);
    }
    else if (v.is_string()) {
        auto s = v.as_string();
        if (s.size() == 7 && s[0] == '#')
            bgr = std::stol(s.substr(1), nullptr, 16);
    }
    return encodeBGR(bgr);
}

static cv::Point point_from_json(const json5pp::value& v) {
    cv::Point p;
    p.x = v[0].as_integer();
    p.y = v[1].as_integer();
    return p;
}

static cv::Rect rect_from_json(const json5pp::value& v) {
    cv::Rect rect;
    rect.x = v.at(0,0).as_integer();
    rect.y = v.at(1,0).as_integer();
    rect.width = v.at(2,0).as_integer();
    rect.height = v.at(3,0).as_integer();
    return rect;
}

template<class Tp>
static void minmax_from_json(const json5pp::value& v, Tp& vmin, Tp& vmax) {
    Tp tmin = vmin;
    Tp tmax = vmax;
    if (v.is_number())
        tmin = tmax = v.as_number();
    else if (v.is_array()) {
        auto& jt = v.as_array();
        if (!jt.empty())
            tmin = jt[0].as_number();
        if (jt.size() > 1)
            tmax = jt[1].as_number();
        else
            tmax = tmin;
    }
    if (tmax < tmin)
        std::swap(tmin, tmax);
    vmin = tmin;
    vmax = tmax;
}

static void ext_from_json(const json5pp::value& v, cv::Point& extendLT, cv::Point& extendRB) {
    if (!v)
        return;
    int extL = 0;
    int extT = 0;
    int extR = 0;
    int extB = 0;
    if (v.is_number())
        extL = extT = extR = extB = v.as_integer();
    else if (v.is_array()) {
        auto& jext = v.as_array();
        if (!jext.empty())
            extL = extT = extR = extB = jext[0].as_integer();
        if (jext.size() > 1)
            extT = extB = jext[1].as_integer();
        if (jext.size() > 2)
            extR = jext[2].as_integer();
        if (jext.size() > 3)
            extB = jext[3].as_integer();
    }
    extendLT = {extL, extT};
    extendRB = {extR, extB};
}

static void from_json(const json5pp::value& jf, std::unique_ptr<detect::ImageFilter>& f) {
    if (!jf.is_object())
        return;
    auto& jo = jf.as_object();
    if (jo.contains("threshold") && jf["threshold"].is_number()) {
        double thr = jf["threshold"].as_number();
        double max = thr < 1 ? 0.5 : 255.0;
        if (jf["max"].is_number())
            max = jf["max"].as_number();
        f.reset(new ThresholdFilter(thr, max));
        return;
    }
    if (jo.contains("channel") && jf["channel"].is_string()) {
        std::string chn = toLower(jf["channel"].as_string());
        ChannelFilter::Channel channel = enum_cast<ChannelFilter::Channel>(chn).value();
        f.reset(new ChannelFilter(channel));
        return;
    }
    if (jo.contains("gauss") && jf["gauss"].is_object()) {
        int kernX = 3;
        int kernY = 3;
        if (jf["gauss"]["kern"].is_array()) {
            kernX = jf["gauss"]["kern"][0].as_integer();
            kernY = jf["gauss"]["kern"][1].as_integer();
        } else {
            kernX = jf["gauss"]["kern"].as_integer();
            kernY = kernX;
        }
        kernX = (kernX & ~1) + 1;
        kernY = (kernY & ~1) + 1;
        f.reset(new GaussFilter(kernX, kernY));
        return;
    }
    if (jo.contains("laplacian") && jf["laplacian"].is_object()) {
        int kern = 3;
        if (jf["laplacian"]["kern"].is_integer())
            kern = jf["laplacian"]["kern"].as_integer();
        double scale = 1;
        if (jf["laplacian"]["scale"].is_number())
            scale = jf["laplacian"]["scale"].as_number();
        f.reset(new LaplacianFilter(kern, scale));
        return;
    }
    if (jo.contains("scharr") && jf["scharr"].is_object()) {
        double scale = 1;
        if (jf["scharr"]["scale"].is_number())
            scale = jf["scharr"]["scale"].as_number();
        f.reset(new ScharrFilter(scale));
        return;
    }
    if (jo.contains("edge_box") && jf["edge_box"].is_object()) {
        int kern = 5;
        double scale = 2.0;
        double thr = 0;
        if (jf["edge_box"]["kern"].is_integer())
            kern = jf["edge_box"]["kern"].as_integer();
        if (jf["edge_box"]["scale"].is_number())
            scale = jf["edge_box"]["scale"].as_number();
        if (jf["edge_box"]["thr"].is_number())
            thr = jf["edge_box"]["thr"].as_number();
        f.reset(new EdgeByBoxFilter(kern, scale, thr));
        return;
    }
    if (jo.contains("gain") && jf["gain"].is_number()) {
        double gain = jf["gain"].as_number();
        double bias = 0;
        if (jf["bias"].is_number())
            bias = jf["bias"].as_number();
        f.reset(new GainBiasFilter(gain, bias));
        return;
    }
    if (jo.contains("dilate")) {
        int kernX = 3;
        int kernY = 3;
        int iter = 1;
        if (jf["dilate"].is_integer())
            kernX = kernY = jf["dilate"].as_integer();
        else if (jf["dilate"].is_array()) {
            kernX = jf["dilate"][0].as_integer();
            kernY = jf["dilate"][1].as_integer();
        }
        if (jf["iter"].is_integer())
            iter = jf["iter"].as_integer();
        f.reset(new DilateFilter(kernX, kernY, iter));
        return;
    }
    if (jo.contains("erode")) {
        int kernX = 3;
        int kernY = 3;
        int iter = 1;
        if (jf["erode"].is_integer())
            kernX = kernY = jf["erode"].as_integer();
        else if (jf["erode"].is_array()) {
            kernX = jf["erode"][0].as_integer();
            kernY = jf["erode"][1].as_integer();
        }
        if (jf["iter"].is_integer())
            iter = jf["iter"].as_integer();
        f.reset(new ErodeFilter(kernX, kernY, iter));
        return;
    }
    bool has_hsv_crop = jo.contains("hsv_crop");
    bool has_hsv_gray = jo.contains("hsv_gray");
    bool has_hsv_mask = jo.contains("hsv_mask");
    if (has_hsv_crop || has_hsv_gray || has_hsv_mask) {
        json5pp::value jhsv;
        HsvMaskFilter* filter;
        if (has_hsv_crop) {
            jhsv = jf["hsv_crop"];
            filter = new HsvColorCropFilter();
        }
        else if (has_hsv_gray) {
            jhsv = jf["hsv_gray"];
            filter = new HsvGrayCropFilter();
        }
        else {
            jhsv = jf["hsv_mask"];
            filter = new HsvMaskFilter();
        }
        std::vector<json5pp::value> jarr;
        if (jhsv.is_array())
            jarr = jhsv.as_array();
        else if (jhsv.is_object())
            jarr.push_back(jhsv);

        for (auto& jv_ : jarr) {
            cv::Vec3b min = {0,0,0};
            cv::Vec3b max = {255,255,255};
            auto& jv = jv_.as_object();
            if (jv.contains("h")) {
                min[0] = jv["h"][0].as_integer();
                max[0] = jv["h"][1].as_integer();
            }
            if (jv.contains("s")) {
                min[1] = jv["s"][0].as_integer();
                max[1] = jv["s"][1].as_integer();
            }
            if (jv.contains("v")) {
                min[2] = jv["v"][0].as_integer();
                max[2] = jv["v"][1].as_integer();
            }
            filter->rangesU.push_back(std::make_pair(min,max));
        }
        if (filter->rangesU.empty())
            delete filter;
        else
            f.reset(filter);
        return;
    }
}

static Detector* detector_from_json(const json5pp::value& j, Widget& widget) {
    if (j.is_null())
        return nullptr;
    if (j.is_boolean()) {
        if (j.as_boolean())
            return new ConstDetector(1);
        return new ConstDetector(0);
    }
    if (j.is_number()) {
        double value = j.as_number();
        value = std::clamp(value, 0.0, 1.0);
        return new ConstDetector(value);
    }
    if (j.is_string()) {
        std::string referred = j.as_string();
        return new detect::ReferDetector(referred);
    }
    if (j.is_object()) {
        if (j.as_object().contains("img")) {
            std::string filename = "templates/"+j.at("img").as_string();
            spEvalRect rect = makeEvalRect(widget, "rect", j["rect"]);

            ImageTemplate* templ = new ImageTemplate(filename, rect);

            if (j.at("name").is_string()) {
                templ->name = j.at("name").as_string();
            }

            if (j.at("scale").is_number())
                templ->testScales.push_back(j["scale"].as_number());
            else if (j.at("scale").is_array()) {
                for (auto& scl : j.at("scale").as_array())
                    templ->testScales.push_back(scl.as_number());
            }

            if (j.at("angle").is_integer())
                templ->testAngles.push_back(j["angle"].as_integer());
            else if (j.at("angle").is_array()) {
                for (auto& angle : j.at("angle").as_array())
                    templ->testAngles.push_back(angle.as_integer());
            }

            ext_from_json(j["ext"], templ->extendLT, templ->extendRB);
            minmax_from_json(j["t"], templ->threshold_min, templ->threshold_max);

            if (j.at("filter")) {
                if (j.at("filter").is_object()) {
                    std::unique_ptr<ImageFilter> f;
                    from_json(j.at("filter"), f);
                    if (f)
                        templ->filters.push_back(std::move(f));
                }
                else if (j.at("filter").is_array()) {
                    for (auto& jf : j.at("filter").as_array()) {
                        std::unique_ptr<ImageFilter> f;
                        from_json(jf, f);
                        if (f)
                            templ->filters.push_back(std::move(f));
                    }
                }
            }
            return templ;
        }
        if (j.as_object().contains("line")) {
            Detector* anchor = nullptr;

            if (j["anchor"].is_object())
                anchor = detector_from_json(j["anchor"], widget);

            spEvalLine line = makeEvalLine(widget, "line", j["line"]);

            detect::LineDetector* ldet;
            if (anchor)
                ldet = new AnchoredLineDetector(dynamic_cast<ImageTemplate*>(anchor), line);
            else
                ldet = new SimpleLineDetector(line);

            if (j["name"].is_string())
                ldet->name = j["name"].as_string();

            minmax_from_json(j["ext"], ldet->extendAngleMin, ldet->extendAngleMax);
            if (j.at("votes"))
                ldet->houghThreshold = j["votes"].as_integer();
            if (j.at("prec"))
                ldet->angleStep = j["prec"].as_number();

            if (j.at("filter")) {
                if (j.at("filter").is_object()) {
                    std::unique_ptr<ImageFilter> f;
                    from_json(j.at("filter"), f);
                    if (f)
                        ldet->filters.push_back(std::move(f));
                }
                else if (j.at("filter").is_array()) {
                    for (auto& jf : j.at("filter").as_array()) {
                        std::unique_ptr<ImageFilter> f;
                        from_json(jf, f);
                        if (f)
                            ldet->filters.push_back(std::move(f));
                    }
                }
            }
            return ldet;
        }
        else if (j.as_object().contains("tiles")) {
            cv::Rect tilesRect = rect_from_json(j["tiles"]);
            cv::Rect iconsRect = rect_from_json(j["rect"]);

            std::string name = j["name"].as_string();
            int rows_min = 1;
            int rows_max = 1;
            minmax_from_json(j["rows"], rows_min, rows_max);
            int cols_min = 1;
            int cols_max = 1;
            minmax_from_json(j["cols"], cols_min, cols_max);

            int gap = j["gap"].as_integer();

            std::string icons = "templates/"+j["icons"].as_string();

            TilesDetector* tiles = new TilesDetector(name, tilesRect, icons, iconsRect,
                                                     rows_min, rows_max, cols_min, cols_max, gap);

            minmax_from_json(j["t"], tiles->threshold_min, tiles->threshold_max);
            ext_from_json(j["ext"], tiles->extendLT, tiles->extendRB);
            if (j["icon_align"].is_string()) {
                auto& align = j["icon_align"].as_string();
                if (toLower(align) == "center")
                    tiles->mIconAlign = TilesDetector::IconAlign::Center;
                else if (toLower(align) == "top-left")
                    tiles->mIconAlign = TilesDetector::IconAlign::TopLeft;
            }
            if (j["hud"].is_boolean())
                tiles->hudTryHard = j["hud"].as_boolean();
            return tiles;
        }
        else if (j.as_object().contains("best")) {
            std::vector<std::unique_ptr<Detector>> oracles;
            for (auto& jo : j["best"].as_array()) {
                Detector *oracle = detector_from_json(jo, widget);
                if (oracle)
                    oracles.emplace_back(oracle);
            }
            return new BestOf(std::move(oracles));
        }
        else if (j.as_object().contains("nav_panel")) {
            std::vector<std::unique_ptr<Detector>> oracles;
            for (auto& jo : j["nav_panel"].as_array()) {
                Detector *oracle = detector_from_json(jo, widget);
                if (oracle)
                    oracles.emplace_back(oracle);
            }
            return new NavPanelDetector(std::move(oracles));
        }
        return nullptr;
    }
    if (j.is_array()) {
        std::vector<std::unique_ptr<Detector>> oracles;
        for (auto& jo : j.as_array()) {
            Detector *oracle = detector_from_json(jo, widget);
            if (oracle)
                oracles.emplace_back(oracle);
        }
        return new Sequence(std::move(oracles));
    }
    return nullptr;
}

static void from_json(const json5pp::value& j, std::map<std::string,std::vector<BaseDialog::Vars>>& varSetMap) {
    for (auto& varSet_it : j.as_object()) {
        std::string varSetName = varSet_it.first;
        for (auto& vars_it : varSet_it.second.as_array()) {
            BaseDialog::Vars& vars = varSetMap[varSetName].emplace_back();
            if (vars_it.at("key").is_string())
                vars.keys.push_back(vars_it.at("key").as_string());
            else if (vars_it.at("key").is_array()) {
                for (auto& js : vars_it.at("key").as_array())
                    vars.keys.push_back(js.as_string());
            }
            for (auto& jv_it : vars_it.as_object()) {
                if (jv_it.first == "key")
                    continue;
                if (jv_it.second.is_array()) {
                    for (auto jv : jv_it.second.as_array())
                        vars.values[jv_it.first].push_back(jv.as_number());
                }
                else if (jv_it.second.is_number())
                    vars.values[jv_it.first].push_back(jv_it.second.as_number());
            }
        }
    }
}

static Widget* widget_from_json(const json5pp::value& j, Widget* parent) {
    if (j.is_null()) {
        return nullptr;
    }
    Widget* widget = nullptr;
    auto& jo = j.as_object();
    auto name = jo.at("name").as_string();
    if (name.starts_with("scr-")) {
        auto scr = new Screen(name, parent, j["status"]);
        if (auto& jvars = j["vars"]; jvars.is_object()) {
            from_json(jvars, scr->varSetMap);
        }
        if (auto& jt = j["transform"]; jt.is_object()) {
            // transform: { tl: [212,256], tr: [1276,242], br: [1296,800], bl: [270,912] }
            // transform: { line: "lpline", tl: [0,-50], tr: [0,-50], ratio: 1.77777777 }
            spEvalPoint tl = makeEvalPoint(*scr, "tl", jt["tl"]);
            spEvalPoint tr = makeEvalPoint(*scr, "tr", jt["tr"]);
            spEvalPoint br = makeEvalPoint(*scr, "br", jt["br"]);
            spEvalPoint bl = makeEvalPoint(*scr, "bl", jt["bl"]);
            cv::Size sz = point_from_json(jt["size"]);
            if (jt["line"]) {
                std::vector<std::string> lines;
                if (jt["line"].is_array()) {
                    for (auto& l : jt["line"].as_array())
                        lines.push_back(l.as_string());
                } else {
                    lines.push_back(jt["line"].as_string());
                }
                scr->transform = spEvalTransform(new LineTransform(lines, tl, tr, br, bl, sz));
            }
            else {
                scr->transform = spEvalTransform(new ConstTransform(tl, tr, br, bl, sz));
            }
        }
        widget = scr;
    }
    else if (name.starts_with("dlg-")) {
        auto dlg = new Dialog(name, parent);
        widget = dlg;
    }
    else if (name.starts_with("mod-")) {
        auto mode = new Mode(name, parent);
        widget = mode;
    }
    else if (name.starts_with("btn-")) {
        auto btn = new Button(name, parent);
        widget = btn;
        widget->setRect("rect", j);
        if (jo.contains("ext"))
            ext_from_json(jo.at("ext"), btn->extendLT, btn->extendRB);
        if (j["icon"].is_string())
            btn->icon = j["icon"].as_string();
    }
    else if (name.starts_with("spn-")) {
        auto btn = new Spinner(name, parent);
        widget = btn;
        widget->setRect("rect", j);
        if (jo.contains("ext"))
            ext_from_json(jo.at("ext"), btn->extendLT, btn->extendRB);
    }
    else if (name.starts_with("til-")) {
        std::string icon;
        if (jo.contains("icon"))
            icon = jo.at("icon").as_string();
        auto btn = new TileBtn(name, parent, icon);
        widget = btn;
    }
    else if (name.starts_with("lbl-")) {
        auto lbl = new Label(name, parent);
        widget = lbl;
        widget->setRect("rect", j);
        if (jo.contains("ocr_top") && jo.contains("ocr_bot")) {
            lbl->ocr_top = jo.at("ocr_top").as_integer();
            lbl->ocr_bot = jo.at("ocr_bot").as_integer();
        }
    }
    else if (name.starts_with("lst-")) {
        auto lst = new List(name, parent);
        widget = lst;
        widget->setRect("rect", j);
        lst->row_height = (float) j.at("row_height",0).as_number();
        lst->row_gap = (float) j.at("row_gap",0).as_number();
        lst->header = (float) j.at("header",0).as_number();
        if (j["row_test"].is_array()) {
            lst->row_test_bgn = j["row_test"][0].as_integer();
            lst->row_test_end = j["row_test"][1].as_integer();
        }
        if (jo.contains("tabs")) {
            auto& jtabs = jo.at("tabs").as_array();
            for (auto& jt : jtabs) {
                List::Tab tab;
                if (jt.at("name"))
                    tab.name = jt.at("name").as_string();
                tab.tab_left = jt.at("left").as_integer();
                tab.tab_right = jt.at("right").as_integer();
                if (jt.at("ocr_top") && jt.at("ocr_bot")) {
                    tab.ocr_top = jt.at("ocr_top").as_integer();
                    tab.ocr_bot = jt.at("ocr_bot").as_integer();
                }
                lst->tabs.push_back(tab);
            }
        }
    }
    else {
        LOG(ERROR) << "Unknown widget type: " << name;
        return nullptr;
    }
    if (jo.contains("have") && jo.at("have").is_array()) {
        for (auto &h: jo.at("have").as_array()) {
            widget->addSubItem(widget_from_json(h, widget));
        }
    }
    if (jo.contains("detect")) {
        Detector* oracle = detector_from_json(jo.at("detect"), *widget);
        widget->oracle.reset(oracle);
    }
    return widget;
}
