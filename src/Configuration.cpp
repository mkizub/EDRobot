//
// Created by mkizub on 04.06.2025.
//

#include "pch.h"
#include "Configuration.h"
#include "Keyboard.h"
#include "FuzzyMatch.h"
#include <dirlistener/ReadDirectoryChanges.h>

#define XML_H_IMPLEMENTATION
#include <xml/xml.h>
#include <filesystem>


static cv::Vec3b color_from_json(const json::value& v);
static void from_json(const json::value& j, cv::Rect& r);

static void from_json(const json5pp::value& j, Template*& o);
static void from_json(const json5pp::value& j, cv::Rect& r);
static widget::Widget* from_json(const json5pp::value& j, widget::Widget* parent);

Configuration::Configuration() {
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
            {{"printscreen",keyboard::CTRL|keyboard::ALT}, Command::DebugTemplates},
            {{"printscreen",keyboard::CTRL|keyboard::WIN}, Command::DebugButtons},
            {{"r",keyboard::CTRL|keyboard::ALT}, Command::DevRectSelect}
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
        if (j_config.at("shortcuts").is_object()) {
            auto& obj = j_config.at("shortcuts");
            parseShortcutConfig(Command::Start, "start", obj);
            parseShortcutConfig(Command::Pause, "pause", obj);
            parseShortcutConfig(Command::Stop,  "stop",  obj);
            parseShortcutConfig(Command::DebugTemplates,  "debug-templates",  obj);
            parseShortcutConfig(Command::DebugButtons,    "debug-buttons",  obj);
            parseShortcutConfig(Command::DevRectSelect,   "dev-rect-select",  obj);
            parseShortcutConfig(Command::Shutdown,  "shutdown",  obj);
        }
        if (auto tm = j_config.at("elite-dangerous-settings-path"); tm.is_string())
            mEDSettingsPath = toUtf16(tm.as_string());
        if (auto tm = j_config.at("elite-dangerous-logs-path"); tm.is_string())
            mEDLogsPath = toUtf16(tm.as_string());
        if (auto tm = j_config.at("tesseract-data-path"); tm.is_string())
            mTesseractDataPath = tm.as_string();

        loadGameSettings();
        loadGameJournal(L""); // may change game language

        loadCommodityDatabase(); // initialization depends on game language
        //dumpCommodityDatabase();
        mCommodityDatabaseUpdated = false;

        loadMarket();
        loadCargo();
        loadCalibration();

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

bool Configuration::loadGameSettings() {
    LOG(INFO) << "Loading game settings";
    bool ok = true;
    std::string filename;
    filename = toUtf8(mEDSettingsPath) + R"(\Options\Graphics\DisplaySettings.xml)";
    XMLNode* rootNode = xml_parse_file(filename.c_str());
    if (rootNode) {
        if (auto node = xml_node_find_tag(rootNode, "ScreenWidth", true); node && node->text)
            configScreenWidth = atol(node->text);
        if (auto node = xml_node_find_tag(rootNode, "ScreenHeight", true); node && node->text)
            configScreenHeight = atol(node->text);
        if (auto node = xml_node_find_tag(rootNode, "FullScreen", true); node && node->text)
            configFullScreen = (FullScreenMode)atoi(node->text);
        xml_node_free(rootNode);
        rootNode = nullptr;
    } else {
        ok = false;
        LOG(ERROR) << "Cannot parse " << filename;
    }
    filename = toUtf8(mEDSettingsPath) + R"(\Options\Graphics\Settings.xml)";
    rootNode = xml_parse_file(filename.c_str());
    if (rootNode) {
        if (auto node = xml_node_find_tag(rootNode, "FOV", true); node && node->text)
            configFOV = atof(node->text);
        if (auto node = xml_node_find_tag(rootNode, "GammaOffset", true); node && node->text)
            configGammaOffset = atof(node->text);
        xml_node_free(rootNode);
        rootNode = nullptr;
    } else {
        ok = false;
        LOG(ERROR) << "Cannot parse " << filename;
    }
    filename = toUtf8(mEDSettingsPath) + R"(\Options\Player\StartPreset.start)";
    std::ifstream ifs(filename, std::ifstream::in);
    if (ifs.is_open()) {
        std::string startPreset{std::istreambuf_iterator<char>(ifs), std::istreambuf_iterator<char>()};
        filename = toUtf8(mEDSettingsPath) + R"(\Options\Player\)" + trim(startPreset) + ".4.1.misc";
        rootNode = xml_parse_file(filename.c_str());
        if (rootNode) {
            if (auto node = xml_node_find_tag(rootNode, "DashboardGUIBrightness", true)) {
                if (auto val = xml_node_attr(node, "Value"))
                    configDashboardGUIBrightness = atof(val);
            }
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

bool Configuration::loadGameJournal(std::wstring journalFilename) {
    if (journalFilename.empty()) {
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
        journalFilename = latestJournalFile.wstring();
    }
    if (journalFilename.empty()) {
        LOG(ERROR) << "Cannot find journal file in " << mEDLogsPath;
        return false;
    }

    std::ifstream ifs(journalFilename, std::ifstream::in);
    if (!ifs.is_open()) {
        LOG(ERROR) << "Cannot open journal file" << mEDLogsPath;
        return false;
    }
    mEDCurrentJournalFile = journalFilename;

    // TODO: implement log file reading for events via a separate thread + poll
    // But currently I only extract client language

    std::string line;
    getline(ifs, line);

    std::string error;
    auto jres = json::parse5(line, &error);
    if (!jres.has_value()) {
        LOG(ERROR) << "Error parsing journal file: " << error;
        return false;
    }
    auto& j = jres.value();
    if (!j.is_object() || !j.contains("event") || j["event"] != "Fileheader") {
        LOG(ERROR) << "Corrupted journal file, expecting 'Fileheader': " << j;
        return false;
    }

    auto gameLang = j["language"].as_string();
    if (gameLang == "Russian/RU" || gameLang.ends_with("/RU"))
        const_cast<Lang&>(this->lng) = RU;
    else if (gameLang == "English/EN" || gameLang.ends_with("/EN"))
        const_cast<Lang&>(this->lng) = EN;
    else if (gameLang == "English/UK" || gameLang.ends_with("/UK"))
        const_cast<Lang&>(this->lng) = EN;
    else {
        LOG(ERROR) << "Unsupported game language: " << gameLang;
        const_cast<Lang&>(this->lng) = XX;
    }

    return true;
}

bool Configuration::loadCalibration() {
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

    mButtonLuv[int(WState::Normal)] = rgb2luv(color_from_json(j.at("normalButton")));
    mButtonLuv[int(WState::Focused)] = rgb2luv(color_from_json(j.at("focusedButton")));
    mButtonLuv[int(WState::Active)] = rgb2luv(color_from_json(j.at("activeToggle")));
    mButtonLuv[int(WState::Disabled)] = rgb2luv(color_from_json(j.at("disabledButton")));
    mLstRowLuv[int(WState::Normal)] = rgb2luv(color_from_json(j.at("normalRow")));
    mLstRowLuv[int(WState::Focused)] = rgb2luv(color_from_json(j.at("focusedRow")));
    mLstRowLuv[int(WState::Active)] = rgb2luv(color_from_json(j.at("activeRow")));
    mLstRowLuv[int(WState::Disabled)] = rgb2luv(color_from_json(j.at("disabledRow")));
    LOG(INFO) << "Calibration data loaded from 'calibration.json5'";
    return true;
}

void Configuration::setCalibrationResult(const std::array<cv::Vec3b,4>& buttonLuv, const std::array<cv::Vec3b,4>& lstRowLuv)
{
    mButtonLuv = buttonLuv;
    mLstRowLuv = lstRowLuv;
    calibrationDashboardGUIBrightness = configDashboardGUIBrightness;
    calibrationGammaOffset = configGammaOffset;
    calibrationScreenWidth = configScreenWidth;
    calibrationScreenHeight = configScreenHeight;
    calibrationFullScreen = configFullScreen;
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
    wf << "  normalButton:  " << std::format("'#{:06x}',", decodeRGB(luv2rgb(mButtonLuv[int(WState::Normal)]))) << " // Luv " << mButtonLuv[int(WState::Normal)] << std::endl;
    wf << "  focusedButton: " << std::format("'#{:06x}',", decodeRGB(luv2rgb(mButtonLuv[int(WState::Focused)]))) << " // Luv " << mButtonLuv[int(WState::Focused)] << std::endl;
    wf << "  activeToggle:  " << std::format("'#{:06x}',", decodeRGB(luv2rgb(mButtonLuv[int(WState::Active)]))) << " // Luv " << mButtonLuv[int(WState::Active)] << std::endl;
    wf << "  disabledButton:" << std::format("'#{:06x}',", decodeRGB(luv2rgb(mButtonLuv[int(WState::Disabled)]))) << " // Luv " << mButtonLuv[int(WState::Disabled)] << std::endl;
    wf << "  // list row colors " << std::endl;
    wf << "  normalRow:     " << std::format("'#{:06x}',", decodeRGB(luv2rgb(mLstRowLuv[int(WState::Normal)]))) << " // Luv " << mLstRowLuv[int(WState::Normal)] << std::endl;
    wf << "  focusedRow:    " << std::format("'#{:06x}',", decodeRGB(luv2rgb(mLstRowLuv[int(WState::Focused)]))) << " // Luv " << mLstRowLuv[int(WState::Focused)] << std::endl;
    wf << "  activeRow:     " << std::format("'#{:06x}',", decodeRGB(luv2rgb(mLstRowLuv[int(WState::Active)]))) << " // Luv " << mLstRowLuv[int(WState::Active)] << std::endl;
    wf << "  disabledRow:   " << std::format("'#{:06x}',", decodeRGB(luv2rgb(mLstRowLuv[int(WState::Disabled)]))) << " // Luv " << mLstRowLuv[int(WState::Disabled)] << std::endl;
    wf << "}" << std::endl;
    wf.close();
    LOG(INFO) << "Calibration data saved to 'calibration.json5'";
    return true;
}

bool Configuration::checkResolutionSupported(cv::Size gameSize) {
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
        Master::getInstance().notifyError(_("Unsupported aspect ratio"), msg);
        return false;
    }
    return true;
}

bool Configuration::checkNeedColorCalibration() const {
    if (std::abs(configDashboardGUIBrightness - calibrationDashboardGUIBrightness) > 0.001)
        return true;
    if (std::abs(configGammaOffset - calibrationGammaOffset) > 0.001)
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
    if (bestScore < 70)
        return nullptr;
    return &allKnownCommodities[bestScoreIndex];
}

std::vector<Commodity*> Configuration::getMarketInSellOrder() {
    std::vector<Commodity*> out;
    if (lng != RU)
        return out;
    spMarket market = currentMarket.load();
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
    const int l = lng;
    std::sort(out.begin(), out.end(), [l](Commodity* a, Commodity* b) {
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
    std::chrono::time_point<std::chrono::utc_clock> timestamp;
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

    currentMarket.exchange(market);
    std::chrono::time_point<std::chrono::utc_clock> zero_time;
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
    std::chrono::time_point<std::chrono::utc_clock> timestamp;
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
    currentCargo.exchange(cargo);
    std::chrono::time_point<std::chrono::utc_clock> zero_time;
    for (auto& c : allKnownCommodities) {
        if (c.ship.timestamp > zero_time && c.ship.timestamp < timestamp) {
            c.ship = {};
        }
    }
    return true;
}

bool Configuration::loadCommodityDatabase() {
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
        for (auto& jn : jcc.second.as_array()) {
            int64_t commodityOrder = 1;
            for (auto& jc : jn.as_array()) {
                auto c = getCommodityByName(jc.as_string(), false);
                if (c)
                    c->carrierSortingOrder[l] = commodityOrder;
                commodityOrder += 1;
            }
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
        wf << "  " << cc.nameId << ": {" << std::endl;
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
        bool needOpenNewLog = false;
        std::wstring newLogFilenameW;


        DWORD action;
        std::wstring filenameW;
        while (changeDirListener->Pop(action, filenameW)) {
            LOG(TRACE) << "File changes: " << ExplainAction(action) << " for file " << toUtf8(filenameW);
            if (filenameW.ends_with(L"Settings.xml"))
                needReloadSettings = true;
            if (filenameW.ends_with(L"Market.json"))
                needReloadMarket = true;
            if (filenameW.ends_with(L"Cargo.json"))
                needReloadCargo = true;
            if (action == FILE_ACTION_ADDED && filenameW.starts_with(L"Journal.") && filenameW.ends_with(L".log")) {
                needOpenNewLog = true;
                newLogFilenameW = filenameW;
            }
        }

        Sleep(500);

        if (needReloadSettings)
            loadGameSettings();
        if (needOpenNewLog)
            loadGameJournal(newLogFilenameW);
        if (needReloadMarket)
            loadMarket();
        if (needReloadCargo)
            loadCargo();
    }
}

using namespace widget;

static cv::Vec3b color_from_json(const json::value& v) {
    unsigned rgb = 0;
    if (v.is_number())
        rgb = v.as_unsigned();
    else if (v.is_array()) {
        unsigned r = v.as_array().at(0).as_unsigned();
        unsigned g = v.as_array().at(1).as_unsigned();
        unsigned b = v.as_array().at(2).as_unsigned();
        rgb = (r&0xFF) | ((g&0xFF)<<8) | ((b&0xFF)<<16);
    }
    else if (v.is_string()) {
        auto s = v.as_string();
        if (s.size() == 7 && s[0] == '#')
            rgb = std::stol(s.substr(1), nullptr, 16);
    }
    return encodeRGB(rgb);
}

static cv::Vec3b color_from_json(const json5pp::value& v) {
    unsigned rgb = 0;
    if (v.is_integer())
        rgb = v.as_integer();
    else if (v.is_array()) {
        unsigned r = v.as_array().at(0).as_integer();
        unsigned g = v.as_array().at(1).as_integer();
        unsigned b = v.as_array().at(2).as_integer();
        rgb = (r&0xFF) | ((g&0xFF)<<8) | ((b&0xFF)<<16);
    }
    else if (v.is_string()) {
        auto& s = v.as_string();
        if (s.size() == 7 && s[0] == '#')
            rgb = std::stol(s.substr(1), nullptr, 16);
    }
    return encodeRGB(rgb);
}

static void from_json(const json5pp::value& j, Template*& o) {
    o = nullptr;
    if (j.is_null())
        return;
    if (j.is_object()) {
        Template* oracle = nullptr;
        if (j.as_object().contains("img")) {
            std::string filename = "templates/"+j.at("img").as_string();
            auto image = cv::imread(filename, cv::IMREAD_UNCHANGED);
            if (image.empty()) {
                LOG(ERROR) << "Template image " << filename << " not found";
                return;
            }
            json5pp::value atX = j.at("at").as_array()[0];
            json5pp::value atY = j.at("at").as_array()[1];
            spEvalRect screenRect = makeEvalRect(j.at("at"), image.cols, image.rows);
            int extL = 0;
            int extT = 0;
            int extR = 0;
            int extB = 0;
            if (j.at("ext")) {
                if (j.at("ext").is_number())
                    extL = extT = extR = extB = j.at("ext").as_integer();
                else if (j.at("ext").is_array()) {
                    auto& jext = j.at("ext").as_array();
                    if (!jext.empty())
                        extL = extT = extR = extB = jext[0].as_integer();
                    if (jext.size() > 1)
                        extT = extB = jext[1].as_integer();
                    if (jext.size() > 2)
                        extR = jext[2].as_integer();
                    if (jext.size() > 3)
                        extB = jext[3].as_integer();
                }
            }
            cv::Point extLT(extL, extT);
            cv::Point extRB(extR, extB);
            double tmin = 0.8;
            double tmax = 0.8;
            if (j.at("t")) {
                if (j.at("t").is_number())
                    tmax = tmin = j.at("t").as_number();
                else if (j.at("t").is_array()) {
                    auto& jt = j.at("t").as_array();
                    if (!jt.empty())
                        tmin = jt[0].as_number();
                    if (jt.size() > 1)
                        tmax = jt[1].as_number();
                    else
                        tmax = tmin;
                }
                if (tmax < tmin)
                    std::swap(tmin, tmax);
            }
            std::string name;
            if (j.at("name").is_string())
                name = j.at("name").as_string();
            o = new ImageTemplate(name, filename, image, screenRect, extLT, extRB, tmin, tmax);

            //t: [0.5, 0.9], rect:[500, 170], ext: [4,4,300,4]
        }
        return;
    }
    if (j.is_array()) {
        std::vector<std::unique_ptr<Template>> oracles;
        for (auto& jo : j.as_array()) {
            Template *oracle = nullptr;
            from_json(jo, oracle);
            if (!oracle) {
                oracles.clear();
                break;
            }
            oracles.emplace_back(oracle);
        }
        if (!oracles.empty())
            o = new SequenceTemplate(std::move(oracles));
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
        auto scr = new Screen(name, parent);
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
        child->setRect(jo.at("rect"));
    }
    else if (name.starts_with("spn-")) {
        auto btn = new Spinner(name, parent);
        child = btn;
        child->setRect(jo.at("rect"));
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
        Template* oracle = nullptr;
        from_json(jo.at("detect"), oracle);
        child->oracle.reset(oracle);
    }
    return child;
}

