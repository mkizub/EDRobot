//
// Created by mkizub on 04.06.2025.
//

#include "pch.h"
#include "Configuration.h"
#include "Keyboard.h"
#include "dirlistener/ReadDirectoryChanges.h"
#include "magic_enum/magic_enum.hpp"

#define XML_H_IMPLEMENTATION
#include "xml.h"


static cv::Vec3b from_json(const json5pp::value& v);
static void from_json(const json5pp::value& j, Template*& o);
static void from_json(const json5pp::value& j, cv::Rect& r);
static widget::Widget* from_json(const json5pp::value& j, widget::Widget* parent);

Configuration::Configuration() {
}

Configuration::~Configuration() {
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
        if (auto tm = j_config.at("default-key-hold-time"); tm.is_integer())
            defaultKeyHoldTime = tm.as_integer();
        if (auto tm = j_config.at("default-key-after-time"); tm.is_integer())
            defaultKeyAfterTime = tm.as_integer();
        if (auto tm = j_config.at("search-region-extent"); tm.is_integer())
            searchRegionExtent = tm.as_integer();
        if (j_config.at("shortcuts").is_object()) {
            auto obj = j_config.at("shortcuts");
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


        if (!changeDirListener) {
            changeDirListener = std::make_unique<CReadDirectoryChanges>(10);
            changeDirListener->Init();
            DWORD dirNotificationFlags = FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_CREATION;
            std::wstring dirname = mEDSettingsPath + LR"(\Options\Graphics\)";
            changeDirListener->AddDirectory(dirname, false, dirNotificationFlags);
        }

        loadGameSettings();
        loadCalibration();
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
            configGammaOffset = atol(node->text);
        xml_node_free(rootNode);
        rootNode = nullptr;
    } else {
        ok = false;
        LOG(ERROR) << "Cannot parse " << filename;
    }
    filename = toUtf8(mEDSettingsPath) + R"(\Options\Player\StartPreset.start)";
    std::ifstream ifs(filename);
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

bool Configuration::loadCalibration() {
    std::ifstream calibrationFile("calibration.json5");
    if (calibrationFile.fail()) {
        LOG(ERROR) << "File calibration.json5 not exists";
        return false;
    }
    auto x = json5pp::parse5(calibrationFile).as_object();
    if (x.at("dashboardGUIBrightness").is_number())
        calibrationDashboardGUIBrightness = x.at("dashboardGUIBrightness").as_number();
    if (x.at("gammaOffset").is_number())
        calibrationGammaOffset = x.at("gammaOffset").as_number();
    if (x.at("screenWidth").is_integer())
        calibrationScreenWidth = x.at("screenWidth").as_integer();
    if (x.at("screenHeight").is_integer())
        calibrationScreenHeight = x.at("screenHeight").as_integer();
    mButtonLuv[int(ButtonState::Normal)] = rgb2luv(from_json(x.at("normalButton")));
    mButtonLuv[int(ButtonState::Focused)] = rgb2luv(from_json(x.at("focusedButton")));
    mButtonLuv[int(ButtonState::Disabled)] = rgb2luv(from_json(x.at("disabledButton")));
    mButtonLuv[int(ButtonState::Activated)] = rgb2luv(from_json(x.at("activatedToggle")));
    LOG(INFO) << "Calibration data loaded from 'calibration.json5'";
    return true;
}

void Configuration::setCalibrationResult(const std::array<cv::Vec3b,4>& luv)
{
    mButtonLuv = luv;
    calibrationDashboardGUIBrightness = configDashboardGUIBrightness;
    calibrationGammaOffset = configGammaOffset;
    calibrationScreenWidth = configScreenWidth;
    calibrationScreenHeight = configScreenHeight;
    calibrationFullScreen = configFullScreen;
}

bool Configuration::saveCalibration() const {
    json5pp::value x = json5pp::object(
            {
                    {"dashboardGUIBrightness", configDashboardGUIBrightness},
                    {"gammaOffset", configGammaOffset},
                    {"screenWidth", configScreenWidth},
                    {"screenHeight", configScreenHeight},
                    {"normalButton", std::format("#{:06x}", decodeRGB(luv2rgb(mButtonLuv[int(ButtonState::Normal)])))},
                    {"focusedButton", std::format("#{:06x}", decodeRGB(luv2rgb(mButtonLuv[int(ButtonState::Focused)])))},
                    {"disabledButton", std::format("#{:06x}", decodeRGB(luv2rgb(mButtonLuv[int(ButtonState::Disabled)])))},
                    {"activatedToggle", std::format("#{:06x}", decodeRGB(luv2rgb(mButtonLuv[int(ButtonState::Activated)])))},
            });
    std::ofstream calibrationFile;
    calibrationFile.open("calibration.json5");
    if (!calibrationFile.is_open()) {
        LOG(INFO) << "Cannot open 'calibration.json5' to save calibration";
        return false;
    }
    calibrationFile << json5pp::rule::json5() << json5pp::rule::space_indent<>() << x;
    calibrationFile.close();
    LOG(INFO) << "Calibration data saved to 'calibration.json5'";
    return true;
}

bool Configuration::checkReloadGameSettings() {
    if (changeDirListener) {
        bool needReload = false;
        DWORD action;
        std::wstring filename;
        while (changeDirListener->Pop(action, filename)) {
            if (filename.ends_with(L"Settings.xml"))
                needReload = true;
        }
        if (needReload)
            return loadGameSettings();
    }
    return true;
}

bool Configuration::checkResolutionSupported(cv::Size gameSize) {
    checkReloadGameSettings();
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

bool Configuration::checkNeedColorCalibration() {
    checkReloadGameSettings();
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

using namespace widget;

static cv::Vec3b from_json(const json5pp::value& v) {
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
        auto btn = new List(name, parent);
        child = btn;
        child->setRect(jo.at("rect"));
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

