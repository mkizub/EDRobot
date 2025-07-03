//
// Created by mkizub on 04.06.2025.
//

#include "pch.h"

#include "Configuration.h"
#include "Keyboard.h"
#include "FuzzyMatch.h"
#include "EDWidget.h"

#include <dirlistener/ReadDirectoryChanges.h>
#ifdef DEBUG
# undef DEBUG
#endif


#define XML_H_IMPLEMENTATION
#include <xml/xml.h>
#include <filesystem>


static cv::Vec3b color_from_json(const json::value& v);
static void from_json(const json::value& j, cv::Rect& r);

static void from_json(const json5pp::value& j, detect::Detector*& o);
static void from_json(const json5pp::value& j, cv::Rect& r);
static widget::Widget* from_json(const json5pp::value& j, widget::Widget* parent);

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

    cv::Vec3b RED = {0, 0, 255};
    cv::Vec3f lBgr = sBgr2lBgr(RED);
    unsigned gray = sBgr2sGray(RED);
    cv::Vec3b sBgrFromGray = sGray2sBgr(gray);
    cv::Vec3b luv = sBgr2Luv(RED);
    cv::Vec3b hsv = sBgr2Hsv(RED);
    cv::Vec3b sBgrFromLuv = luv2sBgr(luv);
    cv::Vec3b sBgrFromHsv = hsv2sBgr(hsv);


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
    if (hShutdownChangDirListenerEvent) {
        SetEvent(hShutdownChangDirListenerEvent);
        CloseHandle(hShutdownChangDirListenerEvent);
        if (changeDirThread.joinable())
            changeDirThread.join();
    }
}


bool Configuration::load() {

    // initialize default keymapping
    keyMapping = {
            {{"esc",0}, Command::Stop},
            {{"printscreen",0}, Command::Start},
            {{"scrolllock",0}, Command::Resume},
            {{"printscreen",keyboard::CTRL|keyboard::ALT}, Command::DebugTemplates},
            {{"printscreen",keyboard::CTRL|keyboard::WIN}, Command::DebugButtons},
            {{"c",keyboard::CTRL|keyboard::ALT}, Command::DebugCompass},
            {{"c",keyboard::CTRL|keyboard::ALT|keyboard::SHIFT}, Command::DebugCompass},
            {{"r",keyboard::CTRL|keyboard::ALT}, Command::DevRectSelect},
            {{"[",keyboard::CTRL|keyboard::ALT}, Command::DebugWindow},
            {{"]",keyboard::CTRL|keyboard::ALT}, Command::ResetCapturer},
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
            parseShortcutConfig(Command::DebugButtons,    "debug-buttons",  obj);
            parseShortcutConfig(Command::DebugCompass,    "debug-compass",  obj);
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


        preloadGameJournal(); // game language & version
        loadGameSettings(true);
        //loadGameJournal(L""); // may change game language

        loadCommodityDatabase(); // initialization depends on game language
        //dumpCommodityDatabase();
        mCommodityDatabaseUpdated = false;

        loadMarket();
        if (!loadCargo())
            currentCargo = std::make_shared<ShipCargo>();
        if (!loadGameStatus())
            currentStatus = std::make_shared<ShipStatus>();
        loadCalibration();

        LOG(INFO) << "Setting journal directory listener";
        if (!changeDirListener) {
            changeDirListener = std::make_unique<CReadDirectoryChanges>(100);
            changeDirListener->Init();
            DWORD dirNotificationFlags = FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_CREATION;
            std::wstring dirname = mEDSettingsPath + LR"(\Options\Graphics\)";
            changeDirListener->AddDirectory(dirname, false, dirNotificationFlags);
            changeDirListener->AddDirectory(mEDLogsPath, false, dirNotificationFlags);
            hShutdownChangDirListenerEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);
            changeDirThread = std::thread(&Configuration::changeDirThreadLoop, this);
        }
    }

    LOG(INFO) << "Setting actions.json5 and screens.json5";
    Master& master = Master::getInstance();
    {
        std::ifstream ifs_config("actions.json5");
        auto j_actions = json5pp::parse5(ifs_config).as_object();
        for (auto& act: j_actions) {
            master.mActions[act.first] = act.second;
        }
    }
    {
        widget::Root* screensRoot = master.mScreensRoot.get();
        std::ifstream ifs_config("screens.json5");
        auto j_screens = json5pp::parse5(ifs_config).as_array();
        for (json5pp::value& s: j_screens) {
            screensRoot->addSubItem(from_json(s, screensRoot));
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
    if (isOdyssey) {
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
    const std::unordered_map<std::string,KeyBindings>* map = nullptr;
    switch (guiFocus) {
    case None:
        if (currentStatus->flags.docked)
            map = &keyBindingsGeneric;
        else
            map = &keyBindingsGeneric;
        break;
    case Right:
    case Left:
    case Chat:
    case Role:
        map = &keyBindingsGeneric;
        break;
    case FSS:
    case SAA:
        map = &keyBindingsEmpty;
        break;
    case Services:
    case GalaxyMap:
    case SystemMap:
    case Orrery:
    case Codex:
        map = &keyBindingsGeneric;
        break;
    }
    if (map) {
        auto it = map->find(name);
        if (it != map->end())
            return it->second;
    }
    return undefined;
}

GameKey Configuration::parseGameKey(XMLNode *keyNode, bool has_modifiers) {
    auto device = xml_node_attr(keyNode, "Device");
    auto key = xml_node_attr(keyNode, "Key");
    if (!device || !key || !(strcmp(device,"Keyboard") == 0 || strcmp(device,"Mouse") == 0))
        return {};
    GameKey gk;
    if (strcmp(device,"Keyboard") == 0) {
        gk.device = GameKey::Keyboard;
        gk.key = key;
        if (gk.key.starts_with("Key_"))
            gk.code = keyboard::getScanCode(key+4);
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

    if (gk.device != GameKey::Void && gk.code && has_modifiers) {
        auto mod = xml_node_find_tag(keyNode, "Modifier", true);
        if (keyNode->children) {
            for (size_t i = 0; i < keyNode->children->len; i++) {
                auto child = xml_node_child_at(keyNode, i);
                gk.modifiers.push_back(parseGameKey(child, false));
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


bool Configuration::parseKeyBindings(XMLNode *rootNode, std::unordered_map<std::string,KeyBindings>& map, const char* tag) {
    auto node = xml_node_find_tag(rootNode, tag, true);
    if (!node) {
        LOG(ERROR) << "Key binding for <" << tag << "> not found";
        return false;
    }
    KeyBindings kb;
    kb.action = tag;
    if (auto primary = xml_node_find_tag(node, "Primary", true))
        kb.primary = parseGameKey(primary, true);
    if (auto secondary = xml_node_find_tag(node, "Secondary", true))
        kb.secondary = parseGameKey(secondary, true);
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
                if (!initial)
                    Master::getInstance().initButtonStateDetector();
            }
        } else {
            ok = false;
            LOG(ERROR) << "Cannot parse " << filename;
        }
        mUseCalibratedColors = checkNeedColorCalibration();
    }

    //
    // Options/Player/... preset
    //
    {
        filename = toUtf8(mEDSettingsPath) + R"(\Options\Player\StartPreset.start)";
        std::ifstream ifs(filename, std::ifstream::in);
        if (ifs.is_open()) {
            std::string preset;
            std::getline(ifs, preset);
            filename = filenameFromPreset(toUtf8(mEDSettingsPath) + R"(\Options\Player\)", preset, "misc");
            XMLNode *rootNode = xml_parse_file(filename.c_str());
            if (rootNode) {
                if (auto node = xml_node_find_tag(rootNode, "DashboardGUIBrightness", true)) {
                    if (auto val = xml_node_attr(node, "Value"))
                        configDashboardGUIBrightness = atof(val);
                }
                xml_node_free(rootNode);
                rootNode = nullptr;
            } else {
                ok = false;
                configDashboardGUIBrightness = 0.5;
                LOG(ERROR) << "Cannot parse " << filename;
            }
        } else {
            ok = false;
            LOG(ERROR) << "Cannot parse " << filename;
        }
    }

    //
    // Options/Bindings/... preset
    //
    {
        keyBindingsGeneric.clear();
        keyBindingsShip.clear();
        filename = toUtf8(mEDSettingsPath) + R"(\Options\Bindings\StartPreset.4.start)";
        std::ifstream ifs(filename, std::ifstream::in);
        if (ifs.is_open()) {
            XMLNode * rootNode = nullptr;
            std::string preset;
            std::getline(ifs, preset);
            if (preset == "KeyboardMouseOnlyYaw") filename = "KeyboardMouseOnlyYaw.binds";
            else if (preset == "KeyboardMouseOnly") filename = "KeyboardMouseOnly.binds";
            else if (preset == "ClassicKeyboardOnly") filename = "ClassicKeyboardOnly.binds";
            else if (preset == "Empty") filename = "Empty.binds";
            else
                filename = filenameFromPreset(toUtf8(mEDSettingsPath) + R"(\Options\Bindings\)", preset, "binds");
            rootNode = xml_parse_file(filename.c_str());
            if (rootNode) {
                parseKeyBindings(rootNode, keyBindingsGeneric, "UI_Up");
                parseKeyBindings(rootNode, keyBindingsGeneric, "UI_Down");
                parseKeyBindings(rootNode, keyBindingsGeneric, "UI_Left");
                parseKeyBindings(rootNode, keyBindingsGeneric, "UI_Right");
                parseKeyBindings(rootNode, keyBindingsGeneric, "UI_Select");
                parseKeyBindings(rootNode, keyBindingsGeneric, "UI_Back");
                parseKeyBindings(rootNode, keyBindingsGeneric, "UI_Toggle");
                parseKeyBindings(rootNode, keyBindingsGeneric, "CycleNextPanel");
                parseKeyBindings(rootNode, keyBindingsGeneric, "CyclePreviousPanel");
                parseKeyBindings(rootNode, keyBindingsGeneric, "CycleNextPage");
                parseKeyBindings(rootNode, keyBindingsGeneric, "CyclePreviousPage");
                xml_node_free(rootNode);
                rootNode = nullptr;
            } else {
                ok = false;
                LOG(ERROR) << "Cannot parse " << filename;
            }

            std::getline(ifs, preset);
            if (preset == "KeyboardMouseOnlyYaw") filename = "KeyboardMouseOnlyYaw.binds";
            else if (preset == "KeyboardMouseOnly") filename = "KeyboardMouseOnly.binds";
            else if (preset == "ClassicKeyboardOnly") filename = "ClassicKeyboardOnly.binds";
            else if (preset == "Empty") filename = "Empty.binds";
            else
                filename = filenameFromPreset(toUtf8(mEDSettingsPath) + R"(\Options\Bindings\)", preset, "binds");
            rootNode = xml_parse_file(filename.c_str());
            if (rootNode) {
                parseKeyBindings(rootNode, keyBindingsGeneric, "RollLeftButton");
                parseKeyBindings(rootNode, keyBindingsGeneric, "RollRightButton");
                parseKeyBindings(rootNode, keyBindingsGeneric, "PitchUpButton");
                parseKeyBindings(rootNode, keyBindingsGeneric, "PitchDownButton");
                parseKeyBindings(rootNode, keyBindingsGeneric, "YawLeftButton");
                parseKeyBindings(rootNode, keyBindingsGeneric, "YawRightButton");
                parseKeyBindings(rootNode, keyBindingsGeneric, "LeftThrustButton");
                parseKeyBindings(rootNode, keyBindingsGeneric, "RightThrustButton");
                parseKeyBindings(rootNode, keyBindingsGeneric, "UpThrustButton");
                parseKeyBindings(rootNode, keyBindingsGeneric, "DownThrustButton");
                parseKeyBindings(rootNode, keyBindingsGeneric, "ForwardThrustButton");
                parseKeyBindings(rootNode, keyBindingsGeneric, "BackwardThrustButton");
                parseKeyBindings(rootNode, keyBindingsGeneric, "ForwardKey");
                parseKeyBindings(rootNode, keyBindingsGeneric, "BackwardKey");
                parseKeyBindings(rootNode, keyBindingsGeneric, "SetSpeedMinus100");
                parseKeyBindings(rootNode, keyBindingsGeneric, "SetSpeedMinus75");
                parseKeyBindings(rootNode, keyBindingsGeneric, "SetSpeedMinus50");
                parseKeyBindings(rootNode, keyBindingsGeneric, "SetSpeedMinus25");
                parseKeyBindings(rootNode, keyBindingsGeneric, "SetSpeedZero");
                parseKeyBindings(rootNode, keyBindingsGeneric, "SetSpeed25");
                parseKeyBindings(rootNode, keyBindingsGeneric, "SetSpeed50");
                parseKeyBindings(rootNode, keyBindingsGeneric, "SetSpeed75");
                parseKeyBindings(rootNode, keyBindingsGeneric, "SetSpeed100");
                parseKeyBindings(rootNode, keyBindingsGeneric, "HyperSuperCombination");
                parseKeyBindings(rootNode, keyBindingsGeneric, "Supercruise");
                parseKeyBindings(rootNode, keyBindingsGeneric, "Hyperspace");
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
    }

    if (needCapturerReset)
        Master::getInstance().pushCommand(Command::ResetCapturer);

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

        if (!parseEvent(line, event, timestamp))
            return false;
        if (start) {
            start = false;
            if (event != "Fileheader") {
                LOG(ERROR) << "Corrupted journal file, expecting 'Fileheader': " << line;
                return false;
            }
        }
        else if (event == "Shutdown" || event == "Loadout")
            return true;
    }
}

bool Configuration::loadGameJournal(std::wstring journalFilename) {
    LOG(INFO) << "Loading game journal";
    if (journalFilename.empty()) {
        if (!findLatestJournalFile())
            return false;
        journalFilename = mEDCurrentJournalFile;
    }
    std::ifstream ifs(journalFilename, std::ifstream::in);
    if (!ifs.is_open()) {
        LOG(ERROR) << "Cannot open journal file: " << journalFilename;
        return false;
    }

    bool start = true;
    for (;;) {
        std::string line;
        getline(ifs, line);

        std::string event;
        Timestamp timestamp;

        if (!parseEvent(line, event, timestamp))
            return false;
        if (start) {
            start = false;
            if (event != "Fileheader") {
                LOG(ERROR) << "Corrupted journal file, expecting 'Fileheader': " << line;
                return false;
            }
        }
        else if (event == "Shutdown" || event == "Loadout")
            return true;
    }

    return true;
}

bool Configuration::loadGameStatus() {
    LOG(INFO) << "Loading Status.json";
    std::string filename = toUtf8(mEDLogsPath) + "/Status.json";
    std::ifstream ifs(filename);
    if (!ifs)
        return false;
    std::stringstream buffer;
    buffer << ifs.rdbuf();
    std::string error;
    std::optional<json::value> res = json::parse5(buffer.str(), &error);
    if (!res.has_value()) {
        LOG(ERROR) << "Error loading Status.json: " << error;
        return false;
    }
    json::value j_status = res.value();
    if (!j_status.is_object())
        return false;
    json::object j = j_status.as_object();
    //{ "Cargo":29.000000, "LegalState":"Allied", "Balance":8269738711 }
    if (j["event"] != "Status")
        return false;
    spShipStatus status = std::make_shared<ShipStatus>();
    if (!parseTimestamp(j.at("timestamp"), status->timestamp))
        return false;
    if (j.contains("Flags"))
        status->flags.all = j["Flags"].as_unsigned();
    if (j.contains("Flags2"))
        status->flags2.all = j["Flags2"].as_unsigned();
    if (j.contains("FireGroup"))
        status->fireGroup = j["FireGroup"].as_unsigned();
    if (j.contains("GuiFocus")) {
        auto gf = enum_cast<GuiFocus>(j.at("GuiFocus").as_integer());
        status->guiFocus = gf.has_value() ? gf.value() : GuiFocus::None;
    }
    if (j.contains("Pips")) {
        json::array j_pips = j.at("Pips").as_array();
        status->pips[0] = j_pips[0].as_unsigned();
        status->pips[1] = j_pips[1].as_unsigned();
        status->pips[2] = j_pips[2].as_unsigned();
    }
    if (j.contains("Fuel")) {
        json::object j_fuel = j.at("Fuel").as_object();
        status->fuelMain = j_fuel.at("FuelMain").as_float();
        status->fuelReservoir = j_fuel.at("FuelReservoir").as_float();
    }
    if (j.contains("Cargo"))
        status->cargo = j.at("Cargo").as_float();
    if (j.contains("Balance"))
        status->balance = j.at("Balance").as_unsigned_long_long();
    if (j.contains("LegalState"))
        status->legalState = j.at("LegalState").as_string();

    guiFocus = status->guiFocus;
    currentStatus.swap(status);
    LOG(INFO) << "Ship status: " << *currentStatus.get();
    return true;
}

std::ostream& operator<<(std::ostream& os, const ShipStatus& st) {
    os << "{";
    os << "gui-focus:" << enum_name<GuiFocus>(st.guiFocus)<<",";
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
    std::ifstream ifs("calibration.json5");
    if (!ifs)
        return false;
    std::stringstream buffer;
    buffer << ifs.rdbuf();
    std::string error;
    auto res = json::parse5(buffer.str(), &error);
    if (!res.has_value()) {
        LOG(ERROR) << "Error loading calibration.json5: " << error;
        return false;
    }
    auto j = res.value();
    if (j.at("dashboardGUIBrightness").is_number())
        calibrationDashboardGUIBrightness = j.at("dashboardGUIBrightness").as_double();
    if (j.at("gammaOffset").is_number())
        calibrationGammaOffset = j.at("gammaOffset").as_double();
    if (j.at("screenWidth").is_number())
        calibrationScreenWidth = j.at("screenWidth").as_integer();
    if (j.at("screenHeight").is_number())
        calibrationScreenHeight = j.at("screenHeight").as_integer();
    std::optional<FullScreenMode> fullScreenMode;
    if (j.contains("fullScreen")) {
        if (j.at("fullScreen").is_string())
            fullScreenMode = enum_cast<FullScreenMode>(j.at("fullScreen").as_string());
        else if (j.at("fullScreen").is_number())
            fullScreenMode = enum_cast<FullScreenMode>(j.at("fullScreen").as_integer());
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
    typedef json::value j;
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
    if (lng != XX)
        cc.name = cc.translation[lng];
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
            if (c.translation[i].empty() && !c_add.translation[i].empty()) {
                c.translation[i] = c_add.translation[i];
                mCommodityDatabaseUpdated = true;
            }
        }
        return c;
    }
    allKnownCommodities.emplace_back(c_add);
    Commodity& c = allKnownCommodities.back();
    if (lng != XX)
        c.name = c.translation[lng];
    else
        c.name = c.nameId;
    c.wide = toUtf16(c.name);
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

Commodity* Configuration::getCommodityByName(const std::string& name, bool fuzzy) {
    if (name.empty())
        return nullptr;
    auto it = commodityMap.find(name);
    if (it != commodityMap.end())
        return it->second;
    for (auto& c : allKnownCommodities) {
        if (name == c.name)
            return &c;
    }
    if (!fuzzy)
        return nullptr;
    return getCommodityByName(toUtf16(name), true);
}
Commodity* Configuration::getCommodityByName(const std::wstring& name, bool fuzzy) {
    if (name.empty())
        return nullptr;
    for (auto& c : allKnownCommodities) {
        if (name == c.wide)
            return &c;
    }
    if (!fuzzy)
        return nullptr;

    double bestScore = -1;
    int bestScoreIndex = -1;
    FuzzyMatch matcher;
    for (int i=0; i < allKnownCommodities.size(); i++) {
        double score = matcher.ratio(name, allKnownCommodities[i].wide);
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
    if (lng == XX)
        return out;
    spMarket market = currentMarket;
    if (!market)
        return out;
    bool isFC = (market->stationType == "FleetCarrier");
    // add everything we can sell, then sort according to market order
    for (auto& c : allKnownCommodities) {
        if (!market->items.contains(&c))
            continue;
        if (isFC) {
            if (c.market.demand > 0)
                out.push_back(&c);
        } else {
            if (c.market.isConsumer || c.ship.count > c.ship.stolen)
                out.push_back(&c);
        }
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
    if (lng == XX)
        return out;
    spMarket market = currentMarket;
    if (!market)
        return out;
    // add everything we can buy, then sort according to market order
    for (auto& c : allKnownCommodities) {
        if (!market->items.contains(&c))
            continue;
        if (c.market.stock > 0)
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

bool Configuration::loadMarket() {
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
    std::istringstream iss(j_market.at("timestamp").as_string());
    iss >> std::chrono::parse("%Y-%m-%dT%H:%M:%SZ", timestamp);
    if (iss.fail()) {
        LOG(ERROR) << "Timestamp parse failed, Market.json file corrupted?";
        return false;
    }

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
        if (lng == EN)
            translation = {item.at("Category_Localised").as_string(),""};
        if (lng == RU)
            translation = {"",item.at("Category_Localised").as_string()};
        CommodityCategory& cc = getOrAddCommodityCategory({
                .nameId = item.at("Category").as_string(),
                .translation = translation
        });
        if (lng == EN)
            translation = {item.at("Name_Localised").as_string(),""};
        if (lng == RU)
            translation = {"",item.at("Name_Localised").as_string()};
        Commodity& commodity = getOrAddCommodity({
                .intId = item.at("id").as_integer(),
                .nameId = item.at("Name").as_string(),
                .category = &cc,
                .translation = translation,
                .rare = item.at("Rare").as_boolean()
        });
        MarketLine& ml = commodity.market;
        ml.timestamp = timestamp;
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

    currentMarket.swap(market);
    Timestamp zero_time;
    for (auto& c : allKnownCommodities) {
        if (c.market.timestamp > zero_time && c.market.timestamp < timestamp) {
            c.market = {};
        }
    }
    return true;
}

bool Configuration::loadCargo() {
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
    std::istringstream iss(j_cargo.at("timestamp").as_string());
    iss >> std::chrono::parse("%Y-%m-%dT%H:%M:%SZ", timestamp);
    if (iss.fail()) {
        LOG(ERROR) << "Timestamp parse failed, Cargo.json file corrupted?";
        return false;
    }
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
            LOG(ERROR) << "Unknown cargo item name: " << name;
            continue;
        }
        c->ship.timestamp = timestamp;
        c->ship.count = item.at("Count").as_integer();
        c->ship.stolen = item.at("Stolen").as_integer();
        cargo->inventory.push_back(c);
    }
    currentCargo.swap(cargo);
    Timestamp zero_time;
    for (auto& c : allKnownCommodities) {
        if (c.ship.timestamp > zero_time && c.ship.timestamp < timestamp) {
            c.ship = {};
        }
    }
    return true;
}

bool Configuration::loadCommodityDatabase() {
    LOG(INFO) << "Loading commodity database";
    std::ifstream dbf("commodity-database.json5");
    if (!dbf)
        return false;
    std::stringstream buffer;
    buffer << dbf.rdbuf();
    std::string error;
    auto j = json::parse5(buffer.str(), &error);
    if (!j.has_value()) {
        LOG(ERROR) << "Error loading commodity-database.json5: " << error;
        return false;
    }
    for (auto& jcc : j.value().as_object()) {
        if (jcc.first.contains("-order-"))
            continue;
        CommodityCategory cc_add;
        cc_add.nameId = jcc.first;
        auto& jv = jcc.second;
        cc_add.translation[EN] = jv["en"].as_string();
        cc_add.translation[RU] = jv["ru"].as_string();
        CommodityCategory& cc = getOrAddCommodityCategory(std::move(cc_add));
        for (auto& jc : jv["items"].as_object()) {
            auto& jv = jc.second;
            Commodity c_add{
                    .intId = jv["id"].as_long(),
                    .nameId = jc.first,
                    .category = &cc,
                    .translation = {jv["en"].as_string(), jv["ru"].as_string()}
            };
            getOrAddCommodity(std::move(c_add));
        }
    }
    for (auto& jcc : j.value().as_object()) {
        if (!jcc.first.contains("-order-"))
            continue;
        Lang l = jcc.first.ends_with("-en") ? EN : RU;
        int64_t commodityOrder = 1;
        for (auto& jn : jcc.second.as_array()) {
            if (!jn.is_string())
                continue;
            auto c = getCommodityByName(jn.as_string(), false);
            if (c)
                c->carrierSortingOrder[l] = commodityOrder;
            commodityOrder += 1;
        }
    }
    mCommodityDatabaseUpdated = false;
    return true;
}

bool Configuration::dumpCommodityDatabase() {
    typedef json::value j;
    std::ofstream wf("commodity-database.json5", std::ios::trunc | std::ios::binary);
    wf << "{" << std::endl;
    for (auto& ccit : commodityCategoryMap) {
        auto& cc = *ccit.second;
        wf << "  '" << cc.nameId << "': {" << std::endl;
        wf << "    en: " << j(cc.translation[EN]) << "," << std::endl;
        wf << "    ru: " << j(cc.translation[RU]) << "," << std::endl;
        wf << "    items: {" << std::endl;
        for (auto& cit : commodityMap) {
            auto& c = *cit.second;
            if (c.category != &cc) continue;
            wf << "      " << c.nameId << ": {" << std::endl;
            wf << "        id: " << c.intId << "," << std::endl;
            wf << "        en: " << j(c.translation[EN]) << "," << std::endl;
            wf << "        ru: " << j(c.translation[RU]) << "," << std::endl;
            wf << "      }," << std::endl;
        }
        wf << "    }," << std::endl;
        wf << "  }," << std::endl;
    }
    for (int l=0; l < 2; l++) {
        std::string suffix = l==EN ? "-en" : "-ru";
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
            wf << "    " << j(c->nameId) << ", // " << c->translation[0] << " | " << c->translation[1] << std::endl;
        }
        wf << "  ]," << std::endl;
    }
    wf << "}" << std::endl;
    wf.close();
    mCommodityDatabaseUpdated = false;
    return true;
}

const char* Configuration::makeTesseractWordsFile() {
    std::set<std::wstring> allWords;
    for (auto& c : allKnownCommodities) {
        std::wistringstream iss(c.wide);
        std::wstring word;
        while (iss >> word) {
            allWords.insert(trimWithPunktuation(word));
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

    const HANDLE handles[] = {hShutdownChangDirListenerEvent, changeDirListener->GetWaitHandle()};

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
        bool needReloadMarket = false;
        bool needReloadCargo = false;
        bool needReloadStatus = false;
        bool needOpenNewLog = false;
        std::wstring newLogFilenameW;


        DWORD action;
        std::wstring filenameW;
        while (changeDirListener->Pop(action, filenameW)) {
            LOG(DEBUG) << "File changes: " << ExplainAction(action) << " for file " << toUtf8(filenameW);
            if (filenameW.ends_with(L"Settings.xml"))
                needReloadSettings = true;
            if (filenameW.ends_with(L"Market.json"))
                needReloadMarket = true;
            if (filenameW.ends_with(L"Cargo.json"))
                needReloadCargo = true;
            if (filenameW.ends_with(L"Status.json"))
                needReloadStatus = true;
            if (action == FILE_ACTION_ADDED && filenameW.starts_with(L"Journal.") && filenameW.ends_with(L".log")) {
                needOpenNewLog = true;
                newLogFilenameW = filenameW;
            }
        }

        Sleep(500);

        if (needReloadSettings)
            loadGameSettings(false);
        if (needOpenNewLog)
            loadGameJournal(newLogFilenameW);
        if (needReloadMarket)
            loadMarket();
        if (needReloadCargo)
            loadCargo();
        if (needReloadStatus)
            loadGameStatus();
    }
}

#include "detect/Detector.h"

using namespace widget;
using namespace detect;

static cv::Vec3b color_from_json(const json::value& v) {
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

static cv::Vec3b color_from_json(const json5pp::value& v) {
    unsigned bgr = 0;
    if (v.is_integer())
        bgr = v.as_integer();
    else if (v.is_array()) {
        unsigned r = v.as_array().at(0).as_integer();
        unsigned g = v.as_array().at(1).as_integer();
        unsigned b = v.as_array().at(2).as_integer();
        bgr = (r&0xFF) | ((g&0xFF)<<8) | ((b&0xFF)<<16);
    }
    else if (v.is_string()) {
        auto& s = v.as_string();
        if (s.size() == 7 && s[0] == '#')
            bgr = std::stol(s.substr(1), nullptr, 16);
    }
    return encodeBGR(bgr);
}

static cv::Rect rect_from_json(const json::value& v) {
    cv::Rect rect;
    rect.x = v[0].as_integer();
    rect.y = v[1].as_integer();
    rect.width = v[2].as_integer();
    rect.height = v[3].as_integer();
    return rect;
}

static cv::Rect rect_from_json(const json5pp::value& v) {
    cv::Rect rect;
    rect.x = v[0].as_integer();
    rect.y = v[1].as_integer();
    rect.width = v[2].as_integer();
    rect.height = v[3].as_integer();
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

        extendLT = {extL, extT};
        extendRB = {extR, extB};
    }
}

static void from_json(const json5pp::value& jf, std::unique_ptr<detect::ImageFilter>& f) {
    if (!jf.is_object())
        return;
    if (jf["gauss"].is_object()) {
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
    if (jf["laplacian"].is_object()) {
        int kern = 3;
        if (jf["laplacian"]["kern"].is_integer())
            kern = jf["laplacian"]["kern"].as_integer();
        double scale = 1;
        if (jf["laplacian"]["scale"].is_number())
            scale = jf["laplacian"]["scale"].as_number();
        f.reset(new LaplacianFilter(kern, scale));
        return;
    }
    if (!jf["hsv_crop"].is_null()) {
        HsvColorCropFilter* filter = new HsvColorCropFilter();
        std::vector<json5pp::value> jarr;
        if (jf["hsv_crop"].is_array())
            jarr = jf["hsv_crop"].as_array();
        else if (jf["hsv_crop"].is_object())
            jarr.push_back(jf["hsv_crop"]);

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
            filter->ranges.push_back(std::make_pair(min,max));
        }
        if (filter->ranges.empty())
            delete filter;
        else
            f.reset(filter);
        return;
    }
}

static void from_json(const json5pp::value& j, Detector*& o) {
    o = nullptr;
    if (j.is_null())
        return;
    if (j.is_object()) {
        if (j.as_object().contains("img")) {
            std::string filename = "templates/"+j.at("img").as_string();
            cv::Rect rect = rect_from_json(j["rect"]);

            ImageTemplate* templ = new ImageTemplate(filename, rect);
            o = templ;

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
        }
        if (j.as_object().contains("line")) {
            Detector* anchor = nullptr;

            if (!j["anchor"].is_object() || !j["anchor"]["img"].is_string())
                throw std::runtime_error("Anchor image template required for line detector");
            from_json(j["anchor"], anchor);

            cv::Point p0 {j["p0"][0].as_integer(), j["p0"][1].as_integer()};
            cv::Point p1 {j["p1"][0].as_integer(), j["p1"][1].as_integer()};

            LineDetector* ldet = new LineDetector(dynamic_cast<ImageTemplate*>(anchor), p0, p1);
            ldet->name = j["line"].as_string();

            o = ldet;

            if (j.at("scale")) {
                double scaleX = 1;
                double scaleY = 1;
                if (j["scale"].is_number())
                    scaleX = scaleY = j["scale"].as_number();
                else if (j["scale"].is_array()) {
                    scaleX = j["scale"][0].as_number();
                    scaleY = j["scale"][1].as_number();
                }
                ldet->imageScaleX = scaleX;
                ldet->imageScaleY = scaleY;
            }
            ext_from_json(j["ext"], ldet->extendLT, ldet->extendRB);

            if (j.at("thr"))
                ldet->binaryThreshold = j["thr"].as_number();

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
            o = tiles;

            minmax_from_json(j["t"], tiles->threshold_min, tiles->threshold_max);
            if (j["hud"].is_boolean())
                tiles->hudTryHard = j["hud"].as_boolean();
        }
        else if (j.as_object().contains("best")) {
            std::vector<std::unique_ptr<Detector>> oracles;
            for (auto& jo : j["best"].as_array()) {
                Detector *oracle = nullptr;
                from_json(jo, oracle);
                if (!oracle) {
                    oracles.clear();
                    break;
                }
                oracles.emplace_back(oracle);
            }
            o = new BestOf(std::move(oracles));
        }
        return;
    }
    if (j.is_array()) {
        std::vector<std::unique_ptr<Detector>> oracles;
        for (auto& jo : j.as_array()) {
            Detector *oracle = nullptr;
            from_json(jo, oracle);
            if (!oracle) {
                oracles.clear();
                break;
            }
            oracles.emplace_back(oracle);
        }
        if (!oracles.empty())
            o = new Sequence(std::move(oracles));
        return;
    }
}

static void from_json(const json::value& j, cv::Rect& r) {
    if (j.is_null()) {
        LOG(WARNING) << "No rect provided";
        return;
    }
    if (!j.is_array() || j.as_array().size() != 4) {
        LOG(ERROR) << "rect must be an array of [x,y,width,height] int numbers";
        return;
    }
    auto arr = j.as_array();
    r.x = arr[0].as_integer();
    r.y = arr[1].as_integer();
    r.width = arr[2].as_integer();
    r.height = arr[3].as_integer();
}

static void from_json(const json5pp::value& j, cv::Rect& r) {
    if (j.is_null()) {
        LOG(WARNING) << "No rect provided";
        return;
    }
    if (!j.is_array() || j.as_array().size() != 4) {
        LOG(ERROR) << "rect must be an array of [x,y,width,height] int numbers";
        return;
    }
    auto arr = j.as_array();
    r.x = arr[0].as_integer();
    r.y = arr[1].as_integer();
    r.width = arr[2].as_integer();
    r.height = arr[3].as_integer();
}

static Widget* from_json(const json5pp::value& j, Widget* parent) {
    if (j.is_null()) {
        return nullptr;
    }
    Widget* child = nullptr;
    auto& jo = j.as_object();
    auto name = jo.at("name").as_string();
    if (name.starts_with("scr-")) {
        json::value status;
        if (jo.contains("status")) {
            std::string s = json5pp::stringify5(jo.at("status"), json5pp::rule::no_indent());
            status = json::parse5(s).value();
        }
        auto scr = new Screen(name, parent, status);
        if (jo.contains("transform")) {
            // transform: { tl: [212,256], tr: [1276,242], br: [1296,800], bl: [270,912] }
            // transform: { from: "lpline", tl: [0,-50], tr: [0,-50], ratio: 1.77777777 }
            auto& jt = jo.at("transform");
            cv::Point2f tl{(float) jt["tl"][0].as_number(), (float) jt["tl"][1].as_number()};
            cv::Point2f tr{(float) jt["tr"][0].as_number(), (float) jt["tr"][1].as_number()};
            cv::Point2f br{(float) jt["br"][0].as_number(), (float) jt["br"][1].as_number()};
            cv::Point2f bl{(float) jt["bl"][0].as_number(), (float) jt["bl"][1].as_number()};
            if (jt["line"]) {
                std::vector<std::string> lines;
                if (jt["line"].is_array()) {
                    for (auto& l : jt["line"].as_array())
                        lines.push_back(l.as_string());
                } else {
                    lines.push_back(jt["line"].as_string());
                }
                scr->transform = spEvalTransform(new LineTransform(lines, tl, tr, br, bl));
            }
            else {
                scr->transform = spEvalTransform(new ConstTransform(tl, tr, br, bl));
            }
        }
        child = scr;
    }
    else if (name.starts_with("dlg-")) {
        auto dlg = new Dialog(name, parent);
        child = dlg;
    }
    else if (name.starts_with("mod-")) {
        auto mode = new Mode(name, parent);
        child = mode;
    }
    else if (name.starts_with("btn-")) {
        auto btn = new Button(name, parent);
        child = btn;
        if (jo.contains("rect"))
            child->setRect(jo.at("rect"));
    }
    else if (name.starts_with("til-")) {
        std::string icon;
        int row = -1;
        int col = -1;
        if (jo.contains("icon"))
            icon = jo.at("icon").as_string();
        if (jo.contains("row"))
            row = jo.at("row").as_integer();
        if (jo.contains("col"))
            col = jo.at("col").as_integer();
        auto btn = new TileBtn(name, parent, icon, row, col);
        child = btn;
    }
    else if (name.starts_with("spn-")) {
        auto btn = new Spinner(name, parent);
        child = btn;
        child->setRect(jo.at("rect"));
    }
    else if (name.starts_with("lbl-")) {
        auto lbl = new Label(name, parent);
        child = lbl;
        child->setRect(jo.at("rect"));
        if (jo.contains("row") && jo.at("row").is_integer())
            lbl->row_height = jo.at("row").as_integer();
        if (jo.contains("invert") && jo.at("invert").is_boolean())
            lbl->invert = jo.at("invert").as_boolean();
    }
    else if (name.starts_with("lst-")) {
        auto lst = new List(name, parent);
        child = lst;
        child->setRect(jo.at("rect"));
        if (jo.at("row").is_integer())
            lst->row_height = jo.at("row").as_integer();
        if (jo.at("gap").is_integer())
            lst->row_gap = jo.at("gap").as_integer();
        if (jo.at("ocr").is_boolean())
            lst->ocr = jo.at("ocr").as_boolean();
    }
    else {
        LOG(ERROR) << "Unknown widget type: " << name;
        return nullptr;
    }
    if (jo.contains("have") && jo.at("have").is_array()) {
        for (auto &h: jo.at("have").as_array()) {
            child->addSubItem(from_json(h, child));
        }
    }
    if (jo.contains("detect")) {
        Detector* oracle = nullptr;
        from_json(jo.at("detect"), oracle);
        child->oracle.reset(oracle);
    }
    return child;
}

