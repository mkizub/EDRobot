//
// Created by mkizub on 04.06.2025.
//

#include "pch.h"

#include "Configuration.h"
#include "CargoManager.h"
#include "Keyboard.h"
#include "FuzzyMatch.h"
#include "widget/EDWidget.h"
#include "Capturer.h"
#include "ShipStats.h"
#include "Galaxy.h"
#include "OCR.h"
#include "ui/UIManager.h"
#include "net/NetUtils.h"
#include "net/RavenColonial.h"
#include "net/EDDN.h"

#include <cpr/cpr.h>

#include <zlib.h>
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

Configuration::Configuration()
    : mCommodityDatabaseUpdated(true)
{
}

Configuration::~Configuration() {
    shutdown();
}

void Configuration::shutdown() {
    if (hShutdownEvent) {
        SetEvent(hShutdownEvent);
        CloseHandle(hShutdownEvent);
        if (changeDirThread.joinable())
            changeDirThread.join();
        hShutdownEvent = nullptr;
    }
}

static void debugNavPanel();

bool Configuration::load() {

    // initialize default keymapping
    keyMapping = {
            {{"printscreen", 0},                      Command::Start},
            {{"pause",       0},                      Command::PauseResume},
            {{"esc",         0},                      Command::Stop},
            {{"cancel",      0},                      Command::Stop}, // CtrlBreak
            {{"a",           kbd::LALT},              Command::Autopilot},
            {{"printscreen", kbd::LCTRL | kbd::LALT}, Command::DebugTemplates},
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
        } else if (!dirUserProfile.empty()) {
            mEDSettingsPath = dirUserProfile + LR"(\AppData\Local\Frontier Developments\Elite Dangerous)";
        }
        js::value j_config;
        try {
            std::ifstream ifs_config("configuration.json5");
            j_config = js::parse5(ifs_config);
        } catch (const js::syntax_error& ex) {
            LOG(ERROR) << ex.what();
        }
        if (!j_config)
            LOG(ERROR) << "Error loading configuration.json5";
        if (auto& tm = j_config.at("ui-scale-percents"); tm.is_int()) {
            if (tm.as_int() >= 25 && tm.as_int() <= 400)
                mUiScalePercents = tm.as_int();
        }
        if (auto& tm = j_config.at("default-key-hold-time"); tm.is_int()) {
            defaultKeyHoldTime = tm.as_int();
            LOG(INFO) << "default-key-hold-time: " << defaultKeyHoldTime;
        }
        if (auto& tm = j_config.at("default-key-after-time"); tm.is_int()) {
            defaultKeyAfterTime = tm.as_int();
            LOG(INFO) << "default-key-after-time: " << defaultKeyAfterTime;
        }
        if (auto& tm = j_config.at("search-region-extent"); tm.is_int()) {
            searchRegionExtent = tm.as_int();
            LOG(INFO) << "search-region-extent: " << searchRegionExtent;
        }
        if (j_config.at("shortcuts").is_object()) {
            auto &obj = j_config.at("shortcuts");
            parseShortcutConfig(Command::Start, "start", obj);
            parseShortcutConfig(Command::PauseResume, "pause", obj);
            parseShortcutConfig(Command::Stop, "stop", obj);
            parseShortcutConfig(Command::DebugTemplates, "debug-templates", obj);
            parseShortcutConfig(Command::DebugWindow, "debug-window", obj);
            parseShortcutConfig(Command::DebugStream, "debug-stream", obj);
            parseShortcutConfig(Command::ResetCapturer, "reset-capturer", obj);
            parseShortcutConfig(Command::DevRectSelect, "dev-rect-select", obj);
        }
        if (auto& tm = j_config.at("elite-dangerous-settings-path"); tm.is_string())
            mEDSettingsPath = toUtf16(tm.as_string());
        if (auto& tm = j_config.at("elite-dangerous-logs-path"); tm.is_string())
            mEDLogsPath = toUtf16(tm.as_string());
        if (auto& tm = j_config.at("tesseract-data-path"); tm.is_string()) {
            mTesseractDataPath = tm.as_string();
        } else {
            mTesseractDataPath = "tessdata";
        }
        if (auto& tm = j_config.at("force-dxgi-device")) {
            if (tm.is_string())
                forceDXGIDevice = tm.as_string();
            if (tm.is_int())
                forceDXGIDeviceId = tm.as_int();
        }
        if (auto& tm = j_config.at("capturer-Win32-disabled"); tm.is_bool()) {
            capturerWin32Disabled = tm.as_bool();
            LOG(INFO) << "capturer-Win32-disabled: " << capturerWin32Disabled;
        }
        if (auto& tm = j_config.at("capturer-WinRT-disabled"); tm.is_bool()) {
            capturerWinRTDisabled = tm.as_bool();
            LOG(INFO) << "capturer-WinRT-disabled: " << capturerWinRTDisabled;
        }
        if (auto& tm = j_config.at("capturer-DXGI-disabled"); tm.is_bool()) {
            capturerDXGIDisabled = tm.as_bool();
            LOG(INFO) << "capturer-DXGI-disabled: " << capturerDXGIDisabled;
        }
        if (auto& tm = j_config.at("curl-insecure"); tm.is_bool())
            mCurlInsecure = tm.as_bool();
        if (auto& tm = j_config.at("curl-proxy"); tm.is_string()){
            mCurlProxyUrl = tm.as_string();
            LOG(INFO) << "CURL proxy: " << mCurlProxyUrl;
        }
        if (auto& tm = j_config.at("vjoy-device-id"); tm.is_int())
            vJoyDeviceID = (uint8_t) tm.as_int();
        if (auto& tm = j_config.at("ravencolonial-enabled"); tm.is_bool()) {
            mRavenColonialEnabled = tm.as_bool();
            LOG(INFO) << "ravencolonial-enabled: " << mRavenColonialEnabled;
        }
        if (auto& tm = j_config.at("ravencolonial-keys"); tm.is_object()) {
            for (auto [cmdr,rcc_key] : tm.key_value()) {
                if (rcc_key.is_string() && !rcc_key.empty())
                    mRavenColonialKeys[std::string(cmdr)] = rcc_key.as_string();
            }
            LOG(INFO) << "ravencolonial-enabled: " << mRavenColonialEnabled;
        }

#ifdef EDROBOT_USE_OPENCL
        if (auto& tm = j_config.at("opencl-disabled"); tm.is_bool()) {
            openclDisabled = tm.as_bool();
            LOG(INFO) << "opencl-disabled: " << openclDisabled;
        }
        if (auto& tm = j_config.at("opencl-d3d11-interop"); tm.is_bool()) {
            openclD3dInterop = tm.as_bool();
            LOG(INFO) << "opencl-d3d11-interop: " << openclD3dInterop;
        }
        g_DisableOpenCL = openclDisabled;
        if (auto& tm = j_config.at("opencl-cache-dir"); tm.is_string()) {
            _putenv_s("OPENCV_OPENCL_CACHE_DIR", tm.as_string().c_str());
        } else {
            _putenv_s("OPENCV_OPENCL_CACHE_DIR", "cache");
        }
#else
        openclDisabled = true;
#endif
        std::filesystem::create_directories("cache/systems");
        std::filesystem::create_directories("cache/markets");
        std::filesystem::create_directories("cache/carriers");

        LOG(INFO) << "ED log files path: " << mEDLogsPath;
        LOG(INFO) << "ED settings  path: " << mEDSettingsPath;

        LOG(INFO) << "Initializing D3D device";
        if (!Capturer::InitD3DDevice())
            errorMessage = _gt("Error initializing DirectX");
    }

    {
        if (!eddb::loadEDDB())
            errorMessage = _gt("Failed to load ship database");
        if (!loadGameSettings(true))
            errorMessage = _gt("Failed to load game settings");
        if (!loadPlayerOptions(true))
            errorMessage = _gt("Failed to load game player options");
        if (!loadInputBindings())
            errorMessage = _gt("Failed to load all required key bindings");
    }
    {
        LOG(INFO) << "Setting screens.json5";
        widget::Root* screensRoot = Master::getInstance().mScreensRoot.get();
        std::ifstream ifs_config("screens.json5");
        const auto j_screens = js::parse5(ifs_config).as_array();
        for (auto& s: j_screens) {
            screensRoot->addSubItem(widget_from_json(s, screensRoot, nullptr));
        }
#ifndef NDEBUG
        widget::debugNavPanel();
#endif
    }
    {
        if (!loadCommodityDatabase())// initialization depends on game language
            errorMessage = _gt("Failed to load commodity database");
        //dumpCommodityDatabase();
        mCommodityDatabaseUpdated = false;
        if (st::cmdr.fleetCarrierId)
            CM.loadCarrierCargo();

        cpr::GlobalThreadPool::GetInstance()->SetMaxThreadNum(1);

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

    LOG(INFO) << "Loading preferences";
    try {
        std::ifstream prefsFile("prefs.json5", std::ifstream::in);
        if (prefsFile.fail()) {
            LOG(ERROR) << "Cannot read file: prefs.json5";
        } else {
            jprefs = js::parse5(prefsFile);
        }
    } catch (...) {
        LOG(ERROR) << "Failed to read/parse prefs.json5";
    }
    mRavenColonialEnabled = Cfg.jprefs["raven"]["enabled"].as_bool_or(mRavenColonialEnabled);
    mRavenColonialReportCarrierCargo = Cfg.jprefs["raven"]["carrier"].as_bool_or(mRavenColonialEnabled);
    mRavenColonialReportShipCargo = Cfg.jprefs["raven"]["ship"].as_bool_or(mRavenColonialEnabled);
    mEddnSystemsEnabled = Cfg.jprefs["eddn"]["systems"].as_bool_or();
    mEddnMarketsEnabled = Cfg.jprefs["eddn"]["markets"].as_bool_or();


    return true;
}

void Configuration::savePrefs() {
    LOG(INFO) << "Saving preferences";

    Cfg.jprefs["raven"]["enabled"] = mRavenColonialEnabled;
    Cfg.jprefs["raven"]["carrier"] = mRavenColonialReportCarrierCargo;
    Cfg.jprefs["raven"]["ship"] = mRavenColonialReportShipCargo;
    Cfg.jprefs["eddn"]["systems"] = mEddnSystemsEnabled;
    Cfg.jprefs["eddn"]["markets"] = mEddnMarketsEnabled;

    std::ofstream ofs("prefs.json5", std::ios::trunc | std::ios::binary);
    ofs << js::rule::json5() << js::rule::no_object_nulls() << js::rule::space_indent<1>() << jprefs;
    ofs.close();
}


void Configuration::parseShortcutConfig(Command command, const std::string& name, js::value cfg) {
    if (cfg.has_key(name)) {
        for (auto it = keyMapping.begin(); it != keyMapping.end();)  {
            if (it->second == command)
                it = keyMapping.erase(it);
            else
                ++it;
        }
        js::value jcmd = cfg.at(name);
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

static std::string filenameFromPreset(const std::string& dir, const std::string& preset, const char* ext) {
    namespace fs = std::filesystem;

    fs::path latestFilePath;
    auto latestWriteTime = fs::file_time_type::min(); // Initialize with the earliest possible time

    for (const auto& entry : fs::directory_iterator(dir)) {
        if (!fs::is_regular_file(entry.status()))
            continue;
        auto nm = entry.path().filename();
        if (!nm.string().starts_with(preset) && nm.extension().string() == ext)
            continue;
        try {
            auto currentWriteTime = fs::last_write_time(entry.path());
            if (currentWriteTime > latestWriteTime) {
                latestWriteTime = currentWriteTime;
                latestFilePath = entry.path();
            }
        } catch (const fs::filesystem_error& e) {
            std::cerr << "Error getting last write time for " << entry.path() << ": " << e.what() << std::endl;
        }
    }

    return latestFilePath.string();
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

struct XmlBuffer {
    std::string path;
    size_t size {};
    char *buffer {};
    unsigned crc32 {};
    ~XmlBuffer() {
        if (buffer)
            free(buffer);
    }
};
bool load_file(const std::string& path, XmlBuffer& b) {
    b = {path};
    FILE *file = fopen(path.c_str(), "rb");
    if (!file) return false;
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    b.buffer = (char *)malloc(file_size + 1);
    if (!b.buffer) {
        fclose(file);
        return false;
    }
    b.size = fread(b.buffer, 1, file_size, file);
    if (b.size != file_size) {
        fclose(file);
        return false;
    }
    b.buffer[file_size] = '\0';
    fclose(file);
    b.crc32 = crc32(0U, (uchar*)b.buffer, file_size);
    return true;
}

bool Configuration::loadGameSettings(bool initial) {
    XmlBuffer settingsBuffer, displaySettingsBuffer;
    if (!load_file(toUtf8(mEDSettingsPath) + R"(\Options\Graphics\DisplaySettings.xml)", displaySettingsBuffer)) {
        LOG(ERROR) << "Filed to load " << displaySettingsBuffer.path;
        return false;
    }
    if (!load_file(toUtf8(mEDSettingsPath) + R"(\Options\Graphics\Settings.xml)", settingsBuffer)) {
        LOG(ERROR) << "Filed to load " << settingsBuffer.path;
        return false;
    }
    if (!initial) {
        if (displaySettingsBuffer.crc32 == mDisplaySettingsCRC32 && settingsBuffer.crc32 == mSettingsCRC32) {
            LOG(DEBUG) << "Settings not changed";
            return true;
        }
    }
    LOG(INFO) << "Loading game settings";
    LOG(INFO) << "Loaded " << displaySettingsBuffer.path;
    LOG(INFO) << "Loaded " << settingsBuffer.path;
    bool ok = true;
    bool needCapturerReset = false;

    //
    // Options/Graphics/DisplaySettings.xml
    //
    {
        XMLNode *rootNode = xml_parse_string(displaySettingsBuffer.buffer);
        if (rootNode) {
            if (auto node = xml_node_find_tag(rootNode, "ScreenWidth", true); node && node->text) {
                int width = atol(node->text);
                if (!initial && width != configScreenWidth)
                    needCapturerReset = true;
                configScreenWidth = width;
                scaledScreenWidth = width;
            }
            if (auto node = xml_node_find_tag(rootNode, "ScreenHeight", true); node && node->text) {
                int height = atol(node->text);
                if (!initial && height != configScreenHeight)
                    needCapturerReset = true;
                configScreenHeight = height;
                scaledScreenHeight = height;
            }
            if (auto node = xml_node_find_tag(rootNode, "FullScreen", true); node && node->text) {
                auto mode = (GameScreenMode) atoi(node->text);
                if (!initial && mode != configScreenMode)
                    needCapturerReset = true;
                configScreenMode = mode;
            }
            if (auto node = xml_node_find_tag(rootNode, "Monitor", true); node && node->text) {
                int monitorId = atoi(node->text);
                if (!initial && monitorId != configMonitorID)
                    needCapturerReset = true;
                configMonitorID = monitorId;
            }
            xml_node_free(rootNode);
            rootNode = nullptr;

            // downscale if resolution is 2048x1536 or 2560x1440 or above
            if (/*(configScreenWidth > 2560 && configScreenHeight > 1536) ||*/ configScreenWidth*configScreenHeight > 4050*1024) {
                double x_scale = ReferenceScreenSize.width / double(configScreenWidth);
                double y_scale = ReferenceScreenSize.height / double(configScreenHeight);
                if (x_scale >= 1)
                    x_scale = 0;
                if (y_scale >= 1)
                    y_scale = 0;
                double scale = std::max(x_scale, y_scale);
                if (scale == 0)
                    scale = 0.75;
                scaledScreenWidth = (int)std::round(configScreenWidth*scale);
                scaledScreenHeight = (int)std::round(configScreenHeight*scale);
                while ((scaledScreenWidth & 3) || (scaledScreenHeight & 3)) {
                    scaledScreenWidth &= ~3;
                    scaledScreenHeight &= ~3;
                    x_scale = double(scaledScreenWidth) / ReferenceScreenSize.width;
                    y_scale = double(scaledScreenHeight) / ReferenceScreenSize.height;
                    scale = std::min(x_scale, y_scale);
                    scaledScreenWidth = (int)std::round(configScreenWidth*scale);
                    scaledScreenHeight = (int)std::round(configScreenHeight*scale);
                }
            }
            {
                double x_scale = double(scaledScreenWidth) / ReferenceScreenSize.width;
                double y_scale = double(scaledScreenHeight) / ReferenceScreenSize.height;
                double scale = std::min(x_scale, y_scale);
                cv::Point frameCenter {scaledScreenWidth / 2, scaledScreenHeight / 2};
                cv::Point tl = cv::Point() - cv::Point(ReferenceScreenCenter);
                tl *= scale;
                tl += frameCenter;
                cv::Point br = cv::Point(ReferenceScreenSize) - cv::Point(ReferenceScreenCenter);
                br *= scale;
                br += frameCenter;
                croppedScreenRect = {tl, br};
            }
            if (initial || needCapturerReset)
                LOG(INFO) << std::format("Screen: config {}x{}; scaled to {}x{}; cropped to [{}:{},{}x{}]",
                                         configScreenWidth, configScreenHeight,
                                         scaledScreenWidth, scaledScreenHeight,
                                         croppedScreenRect.x, croppedScreenRect.y,
                                         croppedScreenRect.width, croppedScreenRect.height);
            LOG(INFO) << "Game screen mode: " << enum_name<GameScreenMode>(configScreenMode);
            LOG(INFO) << "Game monitor id: " << configMonitorID;
        } else {
            ok = false;
            LOG(ERROR) << "Cannot parse " << displaySettingsBuffer.path;
        }
    }

    //
    // Options/Graphics/Settings.xml
    //
    {
        XMLNode *rootNode = xml_parse_string(settingsBuffer.buffer);
        if (rootNode) {
            if (auto node = xml_node_find_tag(rootNode, "FOV", true); node && node->text) {
                configFOV = atof(node->text);
                LOG_IF(initial,INFO) << std::format("FOV: {:.4f}°", configFOV);
            }
            if (auto node = xml_node_find_tag(rootNode, "GammaOffset", true); node && node->text) {
                configGammaOffset = atof(node->text);
                LOG_IF(initial,INFO) << std::format("Gamma offset: {:.4f}", configGammaOffset);
            }
            xml_node_free(rootNode);
            rootNode = nullptr;
        } else {
            ok = false;
            LOG(ERROR) << "Cannot parse " << settingsBuffer.path;
        }
    }

    if (needCapturerReset)
        Master::getInstance().pushCommand(Command::ResetCapturer);

    mDisplaySettingsCRC32 = displaySettingsBuffer.crc32;
    mSettingsCRC32 = settingsBuffer.crc32;
    return ok;
}

//
// Options/Player/... preset
//
bool Configuration::loadPlayerOptions(bool initial) {
    LOG(INFO) << "Loading player options";
    if (initial) {
        std::string filename = toUtf8(mEDSettingsPath) + R"(\Options\Player\StartPreset.start)";
        std::ifstream ifs(filename, std::ifstream::in);
        if (!ifs.is_open()) {
            LOG(ERROR) << "Cannot parse " << filename;
            return false;
        }

        std::string preset;
        std::getline(ifs, preset);
        filename = filenameFromPreset(toUtf8(mEDSettingsPath) + R"(\Options\Player\)", preset, "misc");
        mEDCurrentPlayerOptionsFile = filename;
    }

    XMLNode *rootNode = xml_parse_file(mEDCurrentPlayerOptionsFile.c_str());
    if (rootNode) {
        if (auto node = xml_node_find_tag(rootNode, "DashboardGUIBrightness", true)) {
            if (auto val = xml_node_attr(node, "Value")) {
                configDashboardGUIBrightness = atof(val);
                LOG_IF(initial,INFO) << std::format("UI Brightness: {:.4f}", configDashboardGUIBrightness);
            }
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
            LOG_IF(st::navFilters != npf, INFO) << "NavList Filters: " << npf;
            st::navFilters = npf;
        }

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
        LOG(ERROR) << "Cannot parse " << mEDCurrentPlayerOptionsFile;
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
            ok &= parseKeyBindings(rootNode, mKeyBindingsMap, "UI_Up");
            ok &= parseKeyBindings(rootNode, mKeyBindingsMap, "UI_Down");
            ok &= parseKeyBindings(rootNode, mKeyBindingsMap, "UI_Left");
            ok &= parseKeyBindings(rootNode, mKeyBindingsMap, "UI_Right");
            ok &= parseKeyBindings(rootNode, mKeyBindingsMap, "UI_Select");
            ok &= parseKeyBindings(rootNode, mKeyBindingsMap, "UI_Back");
            ok &= parseKeyBindings(rootNode, mKeyBindingsMap, "UI_Toggle");
            ok &= parseKeyBindings(rootNode, mKeyBindingsMap, "CycleNextPanel");
            ok &= parseKeyBindings(rootNode, mKeyBindingsMap, "CyclePreviousPanel");
            ok &= parseKeyBindings(rootNode, mKeyBindingsMap, "CycleNextPage");
            ok &= parseKeyBindings(rootNode, mKeyBindingsMap, "CyclePreviousPage");
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
            ok &= parseKeyBindings(rootNode, mKeyBindingsMap, "Pause");
            ok &= parseKeyBindings(rootNode, mKeyBindingsMap, "FocusLeftPanel");
            ok &= parseKeyBindings(rootNode, mKeyBindingsMap, "FocusRightPanel");
            ok &= parseKeyBindings(rootNode, mKeyBindingsMap, "RollLeftButton");
            ok &= parseKeyBindings(rootNode, mKeyBindingsMap, "RollRightButton");
            ok &= parseKeyBindings(rootNode, mKeyBindingsMap, "PitchUpButton");
            ok &= parseKeyBindings(rootNode, mKeyBindingsMap, "PitchDownButton");
            ok &= parseKeyBindings(rootNode, mKeyBindingsMap, "YawLeftButton");
            ok &= parseKeyBindings(rootNode, mKeyBindingsMap, "YawRightButton");
            //ok &= parseKeyBindings(rootNode, mKeyBindingsMap, "LeftThrustButton");
            //ok &= parseKeyBindings(rootNode, mKeyBindingsMap, "RightThrustButton");
            ok &= parseKeyBindings(rootNode, mKeyBindingsMap, "UpThrustButton");
            ok &= parseKeyBindings(rootNode, mKeyBindingsMap, "DownThrustButton");
            //ok &= parseKeyBindings(rootNode, mKeyBindingsMap, "ForwardThrustButton");
            //ok &= parseKeyBindings(rootNode, mKeyBindingsMap, "BackwardThrustButton");
            //ok &= parseKeyBindings(rootNode, mKeyBindingsMap, "ForwardKey");
            //ok &= parseKeyBindings(rootNode, mKeyBindingsMap, "BackwardKey");
            ok &= parseKeyBindings(rootNode, mKeyBindingsMap, "UseBoostJuice");
            ok &= parseKeyBindings(rootNode, mKeyBindingsMap, "SetSpeedMinus100");
            ok &= parseKeyBindings(rootNode, mKeyBindingsMap, "SetSpeedMinus75");
            ok &= parseKeyBindings(rootNode, mKeyBindingsMap, "SetSpeedMinus50");
            ok &= parseKeyBindings(rootNode, mKeyBindingsMap, "SetSpeedMinus25");
            ok &= parseKeyBindings(rootNode, mKeyBindingsMap, "SetSpeedZero");
            ok &= parseKeyBindings(rootNode, mKeyBindingsMap, "SetSpeed25");
            ok &= parseKeyBindings(rootNode, mKeyBindingsMap, "SetSpeed50");
            ok &= parseKeyBindings(rootNode, mKeyBindingsMap, "SetSpeed75");
            ok &= parseKeyBindings(rootNode, mKeyBindingsMap, "SetSpeed100");
            ok &= parseKeyBindings(rootNode, mKeyBindingsMap, "HyperSuperCombination");       // vJoy_1
            ok &= parseKeyBindings(rootNode, mKeyBindingsMap, "Supercruise");                 // vJoy_2
            ok &= parseKeyBindings(rootNode, mKeyBindingsMap, "Hyperspace");                  // vJoy_3
            ok &= parseKeyBindings(rootNode, mKeyBindingsMap, "GalaxyMapOpen");               // vJoy_4
            ok &= parseKeyBindings(rootNode, mKeyBindingsMap, "ToggleCargoScoop");
            ok &= parseKeyBindings(rootNode, mKeyBindingsMap, "DeployHardpointToggle");
            ok &= parseKeyBindings(rootNode, mKeyBindingsMap, "LandingGearToggle");
            ok &= parseKeyBindings(rootNode, mKeyBindingsMap, "TargetNextRouteSystem");       // vJoy_5
            ok &= parseKeyBindings(rootNode, mKeyBindingsMap, "MouseReset");                  // vJoy_6
            ok &= parseKeyBindings(rootNode, mKeyBindingsMap, "YawAxisRaw");
            ok &= parseKeyBindings(rootNode, mKeyBindingsMap, "PitchAxisRaw");
            ok &= parseKeyBindings(rootNode, mKeyBindingsMap, "RollAxisRaw");
            configHeadlookSmoothing = getBoolNodeValue(rootNode, "HeadlookSmoothing");
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
    LOG(INFO) << "Journal file: " << mEDCurrentJournalFile;
    return true;
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
            else if (!c.category)
                c.category = c_add.category;
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

CommodityCategory* Configuration::getCommodityCategoryById(int id) {
    for (auto& cc : allKnownCommodityCategories) {
        if (id == cc.intId)
            return &cc;
    }
    return nullptr;
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

Commodity* Configuration::getCommodityById(std::string_view name) {
    return getCommodityById(std::string(name));
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
        if (c.category->intId <= 0 || c.category->intId >= 16)
            continue;
        if (isFC) {
            if (ml.demand <= 0)
                continue;
        } else {
            if ((!ml.isConsumer && ml.demand <= 0) || ml.isProducer) {
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

bool Configuration::loadMarket(spGameEvent ge) {
    auto& je = ge->data;

    int64_t marketId = je["MarketID"].as_int_or(0);
    spMarket oldMarket = gal::getMarket(marketId);
    if (oldMarket && oldMarket->timestamp >= ge->timestamp)
        return false;

    LOG(INFO) << "Loading Market.json";
    js::value j_market;
    try {
        std::ifstream marketFile(mEDLogsPath + L"/Market.json", std::ifstream::in);
        if (marketFile.fail()) {
            LOG(ERROR) << "Cannot read file: " << (mEDLogsPath + L"/Market.json");
            return false;
        }
        j_market = js::parse5(marketFile);
        marketFile.close();
    } catch (...) {
        LOG(ERROR) << "Failed to read/parse Market.json";
        return false;
    }
    if (!j_market)
        return false;
    Timestamp timestamp;
    if (!parseTimestamp(j_market, timestamp))
        return false;

    spMarket market = std::shared_ptr<Market>(new Market{
            .timestamp = timestamp,
            .marketId = j_market.at("MarketID").as_int(),
            .stationName = j_market.at("StationName").as_string(),
            .stationType = j_market.at("StationType").as_string(),
            .starSystem = j_market.at("StarSystem").as_string(),
    });
    if (oldMarket)
        market->raven = oldMarket->raven;
    auto items = j_market.at("Items").as_array();
    for (auto& item : items) {
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
                .intId = (int)item["id"].as_int(),
                .nameId = item["Name"].as_string(),
                .category = &cc,
                .translation = translation,
                .rare = item["Rare"].as_bool_or()
        });
        MarketLine ml {};
        ml.buyPrice = item.at("BuyPrice").as_int_or();
        ml.sellPrice = item.at("SellPrice").as_int_or();
        ml.meanPrice = item.at("MeanPrice").as_int_or();
        ml.stock = item.at("Stock").as_int_or();
        ml.demand = item.at("Demand").as_int_or();
        ml.stockBracket = (uint8_t)item["StockBracket"].as_int_or();
        ml.demandBracket = (uint8_t)item.at("DemandBracket").as_int_or();
        ml.isConsumer = item["Consumer"].as_bool_or();
        ml.isProducer = item["Producer"].as_bool_or();
        market->items.emplace(&commodity, ml);
    }
    if (mCommodityDatabaseUpdated)
        dumpCommodityDatabase();

    st::currentMarket.swap(market);
    gal::setMarketData(st::currentMarket);

    if (!ge->expired) {
        spGameEvent ge_loaded(new GameEvent{std::move(j_market), timestamp, "Market", false});
        EDDN::getInstance()->event_Market(ge_loaded);
    }
    return true;
}

bool Configuration::loadNavRoute(spGameEvent& ge) {
    js::value j_route;
    try {
        std::ifstream routeFile(mEDLogsPath + L"/NavRoute.json", std::ifstream::in);
        if (routeFile.fail()) {
            LOG(ERROR) << "Cannot read file: " << (mEDLogsPath + L"/NavRoute.json");
            return false;
        }
        j_route = js::parse5(routeFile);
        routeFile.close();
    } catch (...) {
        LOG(ERROR) << "Failed to read/parse NavRoute.json";
        return false;
    }
    if (!j_route)
        return false;
    Timestamp timestamp;
    if (!parseTimestamp(j_route, timestamp) || ge->timestamp < timestamp)
        return false;

    std::vector<NavRoute::Entry> entries;
    auto j_entries = j_route.at("Route").as_array();
    for (auto& je : j_entries) {
        std::string starSystem = je["StarSystem"].as_string();
        int64_t systemAddress = je["SystemAddress"].as_int();
        cv::Point3d pos;
        std::string starClass = je["StarClass"].as_string();
        if (je["StarPos"].is_array()) {
            auto& jp = je.at("StarPos");
            pos = {jp[0].as_real(), jp[1].as_real(), jp[2].as_real()};
        }
        entries.emplace_back(starSystem, systemAddress, pos, starClass);
    }
    spNavRoute route = std::make_shared<NavRoute>(timestamp,entries);
    st::currentNavRoute.swap(route);

    if (!ge->expired) {
        const_cast<js::value&>(ge->data) = j_route;
        EDDN::getInstance()->event_NavRoute(ge);
    }
    return true;
}

void Configuration::updateLanguage(Lang lng) {
    if (lng == st::lng)
        return;
    st::lng = lng;
    for (auto& cc : allKnownCommodityCategories) {
        if (lng == Lang::XX)
            cc.name = cc.nameId;
        else
            cc.name = cc.translation[int(lng)];
        cc.wide = toUtf16(cc.name);
    }
    FuzzyMatch fm;
    for (auto& c : allKnownCommodities) {
        if (lng == Lang::XX)
            c.name = c.nameId;
        else
            c.name = c.translation[int(lng)];
        c.wide = toUtf16(c.name);
        c.wocr = fm.toOCR(c.wide);
    }
    ocr::shutdown();
    ocr::init(mTesseractDataPath);
}

bool Configuration::loadCommodityDatabase() {
    LOG(INFO) << "Loading commodity database";
    js::value j;
    try {
        std::ifstream dbf("commodity-database.json5");
        if (!dbf)
            return false;
        std::stringstream buffer;
        buffer << dbf.rdbuf();
        j = js::parse5(buffer.str());
    } catch (const js::syntax_error& ex) {
        LOG(ERROR) << "Error loading commodity-database.json5: " << ex.what();
        return false;
    }
    for (auto [cc_nameId, jcc] : j.key_value()) {
        if (cc_nameId.contains("-order-"))
            continue;
        CommodityCategory cc_add;
        cc_add.nameId = cc_nameId;
        cc_add.intId = jcc["id"].as_int();
        cc_add.translation[int(Lang::EN)] = jcc["en"].as_string();
        cc_add.translation[int(Lang::RU)] = jcc["ru"].as_string();
        CommodityCategory& cc = getOrAddCommodityCategory(std::move(cc_add));
        for (auto [c_nameId,j_c] : jcc["items"].key_value()) {
            Commodity c_add{
                    .intId = (int)j_c["id"].as_int(),
                    .nameId = std::string(c_nameId),
                    .category = &cc,
                    .translation = {j_c["en"].as_string(), j_c["ru"].as_string()},
                    .rare = j_c["rare"].as_bool_or(),
            };
            getOrAddCommodity(std::move(c_add));
        }
    }
    for (auto [order_lng,j_order] : j.key_value()) {
        if (!order_lng.contains("-order-"))
            continue;
        Lang l = order_lng.ends_with("-en") ? Lang::EN : Lang::RU;
        int commodityOrder = 1;
        for (auto& jn : j_order.as_array()) {
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
        wf << "    en: " << js::value(cc.translation[int(Lang::EN)]) << "," << std::endl;
        wf << "    ru: " << js::value(cc.translation[int(Lang::RU)]) << "," << std::endl;
        wf << "    items: {" << std::endl;
        for (auto& cit : commodityMap) {
            auto& c = *cit.second;
            if (c.category != &cc) continue;
            wf << "      " << c.nameId << ": {" << std::endl;
            wf << "        id: " << c.intId << "," << std::endl;
            if (c.rare)
                wf << "        rare: true," << std::endl;
            wf << "        en: " << js::value(c.translation[int(Lang::EN)]) << "," << std::endl;
            wf << "        ru: " << js::value(c.translation[int(Lang::RU)]) << "," << std::endl;
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
            wf << "    " << js::value(c->nameId) << ", // " << c->translation[0] << " | " << c->translation[1] << std::endl;
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

void Configuration::writeLogTimestamp(std::ofstream& fs, Timestamp timestamp) {
    if (!fs.good())
        fs.close();
    if (!fs.is_open())
        fs.open("cache/journal.json5", std::fstream::out | std::fstream::trunc | std::fstream::binary);

    fs.seekp(0, std::ios::beg);
    int64_t unix_ts = std::chrono::duration_cast<std::chrono::seconds>(timestamp.time_since_epoch()).count();
    std::filesystem::path p(mEDCurrentJournalFile);
    fs << js::rule::json5() << js::object({{"ts", unix_ts},{"file", p.filename().string()}}) << std::flush;
}

void Configuration::changeDirThreadLoop() {
    SetThreadDescription(GetCurrentThread(), L"Directory listener");

    const HANDLE handles[2] = {hShutdownEvent, changeDirListener->GetWaitHandle()};

    findLatestJournalFile();

    Timestamp latest_log_timestamp {};
    Timestamp dumped_log_timestamp {};
    try {
        std::ifstream ifs_latest_log("cache/journal.json5");
        js::value j_log = js::parse5(ifs_latest_log);
        std::filesystem::path log_path = std::filesystem::path(mEDLogsPath) / toUtf16(j_log["file"].as_string_or());
        if (log_path == mEDCurrentJournalFile) {
            latest_log_timestamp = Timestamp(std::chrono::seconds(j_log["ts"].as_int_or()));
            dumped_log_timestamp = latest_log_timestamp;
        }
    } catch (const js::syntax_error& ex) {
        //LOG(ERROR) << ex.what();
    }

    std::ofstream ofs_latest_log;
    std::string journalLine;
    std::ifstream journalStream(mEDCurrentJournalFile, std::ifstream::in);
    // read all events from journal
    readJournalChanges(journalStream, latest_log_timestamp, journalLine);
    // read last ship status after all events
    loadGameStatus();

    for(;;) {
        DWORD rc = ::MsgWaitForMultipleObjectsEx(
                        _countof(handles),
                        handles,
                        5000, // wakeup every 5 seconds
                        QS_ALLINPUT,
                        MWMO_INPUTAVAILABLE | MWMO_ALERTABLE);
        if (rc == WAIT_OBJECT_0) // shutdown
            break;
        if (!(rc == WAIT_TIMEOUT || rc == (WAIT_OBJECT_0+1))) // timeout or dir listener
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
            //LOG(DEBUG) << "File changes: " << ExplainAction(action) << " for file " << toUtf8(filenameW);
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
            loadPlayerOptions(false);
        if (needReloadBindings)
            loadInputBindings();
        if (needReloadStatus)
            loadGameStatus();

        // real all events from journal
        readJournalChanges(journalStream, latest_log_timestamp, journalLine);

        if (!journalFilenameW.empty() && journalFilenameW != mEDCurrentJournalFile) {
            mEDCurrentJournalFile = journalFilenameW;
            LOG(INFO) << "Journal file: " << mEDCurrentJournalFile;
            if (journalStream.is_open())
                journalStream.close();
            if (ofs_latest_log.is_open())
                ofs_latest_log.close();
            latest_log_timestamp = {};
            writeLogTimestamp(ofs_latest_log, Timestamp());
            journalStream.open(mEDCurrentJournalFile, std::ifstream::in);
            // real all events from journal
            readJournalChanges(journalStream, latest_log_timestamp, journalLine);
        }

        if (latest_log_timestamp > dumped_log_timestamp+4s) {
            writeLogTimestamp(ofs_latest_log, latest_log_timestamp);
            dumped_log_timestamp = latest_log_timestamp;
        }
    }
    writeLogTimestamp(ofs_latest_log, latest_log_timestamp);
    ofs_latest_log.close();
}
