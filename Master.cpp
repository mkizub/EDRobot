//
// Created by mkizub on 23.05.2025.
//

#include "pch.h"

#include "UI.h"
#include "Keyboard.h"
#include "Task.h"
#include "Template.h"
#include "Capturer.h"
#include <memory>
#include <opencv2/core/utils/logger.hpp>
#include "CLI11.hpp"

#ifndef NDEBUG
#include <cpptrace/cpptrace.hpp>
#include <cpptrace/from_current.hpp>
#endif

#ifdef CPPTRACE_TRY
# define TRY CPPTRACE_TRY
# define CATCH(param) CPPTRACE_CATCH(param)
# define TRYZ CPPTRACE_TRYZ
# define CATCHZ(param) CPPTRACE_CATCHZ(param)
# define CATCH_ALT(param) CPPTRACE_CATCH_ALT(param)
# define GET_STACK_TRACE std::stacktrace::current().to_string()
# define GET_EXCEPTION_STACK_TRACE cpptrace::from_current_exception().to_string()
#else
# define TRY try
# define CATCH(param) catch(param)
# define TRYZ try
# define CATCHZ(param) catch(param)
# define CATCH_ALT(param) catch(param)
# ifdef _GLIBCXX_HAVE_STACKTRACE
#  include <stacktrace>
#  define GET_EXCEPTION_STACK_TRACE std::stacktrace::current()
# else
#  define GET_EXCEPTION_STACK_TRACE "(stack trace unavailable)"
# endif
#endif


const char ED_WINDOW_NAME[] = "Elite - Dangerous (CLIENT)";
const char ED_WINDOW_CLASS[] = "FrontierDevelopmentsAppWinClass";
const char ED_WINDOW_EXE[] = "EliteDangerous64.exe";

using namespace widget;

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

void Master::saveCalibration() const {
    const Calibrarion* c = mCalibration.get();
    if (!c)
        return;
    // key values
    double dashboardGUIBrightness; // Options/Player/Custom.?.misc: <DashboardGUIBrightness Value="0.49999991" />
    double gammaOffset; // Options/Graphics/Settings.xml: <GammaOffset>1.200000</GammaOffset>
    int ScreenWidth;    // Options\Graphics\DisplaySettings.xml: <ScreenWidth>1920</ScreenWidth>
    int ScreenHeight;   // Options\Graphics\DisplaySettings.xml: <ScreenHeight>1080</ScreenHeight>
    // end of key values

    cv::Vec3b normalButtonLuv;
    cv::Vec3b focusedButtonLuv;
    cv::Vec3b disabledButtonLuv;
    cv::Vec3b activatedToggleLuv;

    json5pp::value x = json5pp::object(
            {
                {"dashboardGUIBrightness", c->dashboardGUIBrightness},
                {"gammaOffset", c->gammaOffset},
                {"screenWidth", c->ScreenWidth},
                {"screenHeight", c->ScreenHeight},
                {"normalButton", std::format("#{:06x}", decodeRGB(luv2rgb(c->normalButtonLuv)))},
                {"focusedButton", std::format("#{:06x}", decodeRGB(luv2rgb(c->focusedButtonLuv)))},
                {"disabledButton", std::format("#{:06x}", decodeRGB(luv2rgb(c->disabledButtonLuv)))},
                {"activatedToggle", std::format("#{:06x}", decodeRGB(luv2rgb(c->activatedToggleLuv)))},
            });
    std::ofstream calibrationFile;
    calibrationFile.open("calibration.json5");
    calibrationFile << json5pp::rule::json5() << json5pp::rule::space_indent<>() << x;
    calibrationFile.close();
    LOG(INFO) << "Calibration data saved to 'calibration.json5'";
}

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

bool Master::loadCalibration() {
    std::ifstream calibrationFile("calibration.json5");
    if (calibrationFile.fail()) {
        LOG(ERROR) << "File calibration.json5 not exists";
        return false;
    }
    auto x = json5pp::parse5(calibrationFile).as_object();
    std::unique_ptr<Calibrarion> c = std::make_unique<Calibrarion>();
    if (x.at("dashboardGUIBrightness").is_number())
        c->dashboardGUIBrightness = x.at("dashboardGUIBrightness").as_number();
    if (x.at("gammaOffset").is_number())
        c->gammaOffset = x.at("gammaOffset").as_number();
    if (x.at("screenWidth").is_integer())
        c->ScreenWidth = x.at("screenWidth").as_integer();
    if (x.at("screenHeight").is_integer())
        c->ScreenHeight = x.at("screenHeight").as_integer();
    c->normalButtonLuv = rgb2luv(from_json(x.at("normalButton")));
    c->focusedButtonLuv = rgb2luv(from_json(x.at("focusedButton")));
    c->disabledButtonLuv = rgb2luv(from_json(x.at("disabledButton")));
    c->activatedToggleLuv = rgb2luv(from_json(x.at("activatedToggle")));
    LOG(INFO) << "Calibration data loaded from 'calibration.json5'";
    setCalibrationResult(c, false);
    return true;
}

namespace {
void writeOpenCVLogMessageFunc(cv::utils::logging::LogLevel cvLevel, const char* msg) {
    if (!msg || !*msg)
        return;
    static el::Logger* cvLogger = el::Loggers::getLogger("OpenCV");
    switch (cvLevel) {
        default: return;
        case cv::utils::logging::LOG_LEVEL_FATAL:
            cvLogger->fatal(msg);
            break;
        case cv::utils::logging::LOG_LEVEL_ERROR:
            cvLogger->error(msg);
            break;
        case cv::utils::logging::LOG_LEVEL_WARNING:
            if (cvLogger->enabled(el::Level::Warning))
                cvLogger->warn(msg); break;
            break;
        case cv::utils::logging::LOG_LEVEL_INFO:
            if (cvLogger->enabled(el::Level::Info))
                cvLogger->info(msg); break;
            break;
        case cv::utils::logging::LOG_LEVEL_DEBUG:
            if (cvLogger->enabled(el::Level::Debug))
                cvLogger->debug(msg);
            break;
        //case cv::utils::logging::LOG_LEVEL_VERBOSE: cvLogger->verbose(0, msg); break;
    }
}
void writeOpenCVLogMessageFuncEx(cv::utils::logging::LogLevel cvLevel, const char* tag, const char* file, int line, const char* func, const char* msg) {
    if (!msg || !*msg)
        return;
    el::Level elLevel = el::Level::Unknown;
    switch (cvLevel) {
        default: return;
        case cv::utils::logging::LOG_LEVEL_FATAL:   elLevel = el::Level::Fatal; break;
        case cv::utils::logging::LOG_LEVEL_ERROR:   elLevel = el::Level::Error; break;
        case cv::utils::logging::LOG_LEVEL_WARNING: elLevel = el::Level::Warning; break;
        case cv::utils::logging::LOG_LEVEL_INFO:    elLevel = el::Level::Info; break;
        case cv::utils::logging::LOG_LEVEL_DEBUG:   elLevel = el::Level::Debug; break;
        //case cv::utils::logging::LOG_LEVEL_VERBOSE:  elLevel = el::Level::Verbose; break;
    }
    static el::Logger* cvLogger = el::Loggers::getLogger("OpenCV");
    el::base::Writer(elLevel, file, line, func).construct(cvLogger) << msg;
}
}

void UIState::clear() {
    widget = nullptr;
    focused = nullptr;
    cEnv.clear();
}

const std::string& UIState::path() const {
    static std::string empty;
    if (widget)
        return widget->path;
    return empty;
}

std::ostream& operator<<(std::ostream& os, const UIState& obj) {
    if (!obj.widget) {
        os << "unknown";
        return os;
    }
    os << obj.widget->path;
    if (obj.focused) {
        os << "@" << obj.focused->name;
    }
    return os;
}

std::ostream& operator<<(std::ostream& os, const Calibrarion& obj) {
    os << std::format("Screen {}x{} gamma {:.3f} GUI brightness {:.2f}", obj.ScreenWidth, obj.ScreenHeight, obj.gammaOffset, obj.dashboardGUIBrightness)
       << "; Luv colors: focused=" << obj.focusedButtonLuv << ", normal=" << obj.normalButtonLuv;
    return os;
}

Master& Master::getInstance() {
    static Master master;
    return master;
}

int Master::initialize(int argc, char* argv[]) {
    CLI::App options;
    options.allow_windows_style_options();

    bool kwd = false;
    options.add_flag("--kwd,--keep-working-dir", kwd, "Keep working directory (do not change on start)");

    CLI11_PARSE(options, argc, argv);

    TRY {
    if (!kwd) {
        char buffer[MAX_PATH] = {0};
        GetModuleFileName(nullptr, buffer, MAX_PATH);
        std::string fullPath(buffer);
        size_t lastSlash = fullPath.find_last_of("\\");
        if (lastSlash != std::string::npos) {
            std::string cwd = fullPath.substr(0, lastSlash);
            if (cwd.ends_with("\\bin"))
                cwd = cwd.substr(0,cwd.size()-4);
            SetCurrentDirectory(cwd.c_str());
            LOG(INFO) << "Working Directory: " << cwd;
        }
    }

    //cv::utils::logging::internal::replaceWriteLogMessage(writeOpenCVLogMessageFunc);
    //cv::utils::logging::internal::replaceWriteLogMessageEx(writeOpenCVLogMessageFuncEx);

    keyMapping = {
            {{"esc",0}, Command::Stop},
            {{"printscreen",0}, Command::Start},
            {{"printscreen",keyboard::CTRL|keyboard::ALT}, Command::DebugTemplates},
            {{"printscreen",keyboard::CTRL|keyboard::WIN}, Command::DebugButtons},
            {{"r",keyboard::CTRL|keyboard::ALT}, Command::DevRectSelect}
    };

    {
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
    }
    {
        std::ifstream ifs_config("actions.json5");
        auto j_actions = json5pp::parse5(ifs_config).as_object();
        for (auto& act: j_actions) {
            mActions[act.first] = act.second;
        }
    }
    {
        mScreensRoot = std::make_unique<Root>();
        std::ifstream ifs_config("screens.json5");
        auto j_screens = json5pp::parse5(ifs_config).as_array();
        for (json5pp::value& s: j_screens) {
            mScreensRoot->addSubItem(from_json(s, mScreensRoot.get()));
        }
    }

    loadCalibration();
    } CATCH(const std::exception& e) {
    LOG(ERROR) << "Exception in initializaton: " << GET_EXCEPTION_STACK_TRACE;
    return 1;
    }

    std::vector<std::string> keys;
    for (auto& m : keyMapping)
        keys.push_back(m.first.first);
    keyboard::intercept(std::move(keys));
    keyboard::start(tradingKbHook);
    return 0;
}

void Master::setCalibrationResult(std::unique_ptr<Calibrarion>& calibration, bool save) {
    mCalibration.swap(calibration);
    if (!mCalibration.get()) {
        LOG(INFO) << "setCalibrationResult: reset";
        mFocusedButtonDetector.reset();
    } else {
        LOG(INFO) << "setCalibrationResult: " << mCalibration.get();
        mFocusedDetectorRect = std::make_shared<ConstRect>(cv::Rect());
        mFocusedButtonDetector.reset(new HistogramTemplate(mCalibration->focusedButtonLuv, mFocusedDetectorRect));
        LOG(INFO) << "Button state detector installed";
        if (save)
            saveCalibration();
    }
}

static std::pair<std::string,unsigned> decodeShortcut(std::string key) {
    unsigned flags = 0;
    for (;;) {
        size_t pos = key.find_first_of('+');
        if (pos == std::string::npos)
            break;
        std::string mod = toLower(key.substr(0, pos));
        if (mod == "ctrl")
            flags |= keyboard::CTRL;
        else if (mod == "alt")
            flags |= keyboard::ALT;
        else if (mod == "shift")
            flags |= keyboard::SHIFT;
        else if (mod == "win" || mod == "meta")
            flags |= keyboard::WIN;
        else
            LOG(ERROR) << "Unknown key modifier " << mod;
        key = key.substr(pos+1);
    }
    return std::make_pair(toLower(key), flags);
}

static std::string encodeShortcut(const std::string& name, unsigned flags) {
    std::string res;
    if (flags & keyboard::CTRL) res += "Ctrl+";
    if (flags & keyboard::ALT) res += "Alt+";
    if (flags & keyboard::SHIFT) res += "Shift+";
    if (flags & keyboard::WIN) res += "Win+";
    res += name;
    return res;
}


void Master::parseShortcutConfig(Command command, const std::string& name, json5pp::value cfg) {
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

Master::Master() {
    mSells = 3;
    mItems = 5;
    hWndED = FindWindow(ED_WINDOW_CLASS, ED_WINDOW_NAME);
}

Master::~Master() {
    keyboard::stop();
    stopTrade();
}


void Master::loop() {
    TRY {
        while (true) {
            switch (popCommand()) {
            case Command::NoOp:
                break;
            case Command::TaskFinished:
                clearCurrentTask();
                break;
            case Command::Start:
                startTrade();
                break;
            case Command::Pause:
                stopTrade();
                break;
            case Command::Stop:
                stopTrade();
                break;
            case Command::Calibrate:
                startCalibration();
                break;
            case Command::DebugTemplates:
                {
                    ClassifyEnv env;
                    debugTemplates(nullptr, env);
                }
                break;
            case Command::DebugButtons:
                debugButtons();
                break;
            case Command::DevRectScreenshot:
                debugRectScreenshot("rect");
                break;
            case Command::DevRectSelect:
                UI::selectRectDialog(hWndED);
                break;
            case Command::Shutdown:
                clearCurrentTask();
                return;
            }
        }
    }
    CATCH(const std::exception& e) {
        LOG(ERROR) << "Exception in main loop: " << GET_EXCEPTION_STACK_TRACE;
        clearCurrentTask();
    }
}

bool Master::isForeground() {
    return hWndED && hWndED == GetForegroundWindow();
}

void Master::tradingKbHook(int code, int scancode, int flags, const std::string& name) {
    (void)code;
    (void)scancode;
    LOG(INFO) << "Key '"+encodeShortcut(name,flags)+"' pressed";
    Master& self = getInstance();
    auto cmd = self.keyMapping.find(std::make_pair(toLower(name),flags));
    if (cmd != self.keyMapping.end()) {
        LOG(INFO) << "Key '"+encodeShortcut(name,flags)+"' pressed";
        self.pushCommand(cmd->second);
    }
}

void Master::pushCommand(Command cmd) {
    std::unique_lock<std::mutex> lock(mCommandMutex);
    mCommandQueue.push(cmd);
    mCommandCond.notify_one();
}

Command Master::popCommand() {
    std::unique_lock<std::mutex> lock(mCommandMutex);
    mCommandCond.wait(lock, [this]() { return !mCommandQueue.empty(); });
    Command cmd = mCommandQueue.front();
    mCommandQueue.pop();
    return cmd;
}

bool Master::preInitTask() {
    if (currentTask) {
        LOG(ERROR) << "Exiting current task";
        clearCurrentTask();
    }

    hWndED = FindWindow(ED_WINDOW_CLASS, ED_WINDOW_NAME);
    if (!hWndED) {
        LOG(ERROR) << "Window [class " << ED_WINDOW_CLASS << "; name " << ED_WINDOW_NAME << "] not found" ;
        return false;
    }

    Sleep(200); // wait for dialog to dissappear
    if (!isForeground()) {
        SetForegroundWindow(hWndED);
        Sleep(200); // wait for switching to foreground
        if (!isForeground()) {
            LOG(ERROR) << "ED is not foreground";
            return false;
        }
    }
    return true;
}

bool Master::startCalibration() {
    if (!preInitTask())
        return false;
    LOG(INFO) << "Staring calibration task";
    currentTask = std::make_unique<TaskCalibrate>();
    currentTaskThread = std::thread(Master::runCurrentTask);
    return true;
}

bool Master::startTrade() {
    int sells = mSells;
    int items = mItems;
    bool res = UI::askSellInput(sells, items);
    if (!res || sells <= 0 || items <= 0)
        return false;
    mSells = sells;
    mItems = items;

    if (!preInitTask())
        return false;

    LOG(INFO) << "Staring new trade task";
    currentTask = std::make_unique<TaskSell>(mSells, mItems);
    currentTaskThread = std::thread(Master::runCurrentTask);
    return true;
}

bool Master::stopTrade() {
    LOG_IF(currentTask, INFO) << "Stop trading";
    clearCurrentTask();
    return false;
}

void Master::runCurrentTask() {
    Master& self = getInstance();
    TRY {
        bool ok = self.currentTask->run();
        self.pushCommand(Command::TaskFinished);
    } CATCH (const std::exception& e) {
        LOG(ERROR) << "Exception in current task: " << GET_EXCEPTION_STACK_TRACE;
        self.clearCurrentTask();
    }
}

void Master::clearCurrentTask() {
    if (currentTask) {
        currentTask->stop();
        if (currentTaskThread.joinable())
            currentTaskThread.join();
        currentTask.reset();
    }
    else if (currentTaskThread.joinable()) {
        currentTaskThread.join();
    }
}

const json5pp::value& Master::getTaskActions(const std::string& action) {
    auto it = mActions.find(action);
    if (it == mActions.end()) {
        static json5pp::value nullAction;
        return nullAction;
    }
    return it->second;
}

cv::Rect Master::resolveWidgetRect(const std::string& name) {
    Widget* item = getCfgItem(name);
    if (!item) {
        LOG(ERROR) << "Widget '" << name << "' not found";
        return {};
    }
    auto r = item->calcRect(mLastEDState.cEnv);
    if (r.empty()) {
        LOG(ERROR) << "Widget has no rect";
        return {};
    }
    return r;
}

std::vector<std::string> Master::parseState(const std::string& state) {
    std::vector<std::string> tokens;
    size_t start = 0;
    size_t end = state.find(':');

    while (end != std::string::npos) {
        tokens.push_back(state.substr(start, end - start));
        start = end + 1;
        end = state.find(':', start);
    }

    tokens.push_back(state.substr(start));
    return tokens;
}

static Widget* getItemMode(Widget* item, const std::string& mode) {
    for (Widget* child : item->have) {
        if (child->tp == WidgetType::Mode && (mode.empty() || mode == "*" || child->name == mode))
            return child;
    }
    return nullptr;
}
static Widget* getItemByName(const Widget* item, const std::string& name) {
    for (Widget* child : item->have) {
        if (child->name == name)
            return child;
    }
    return nullptr;
}

Widget* Master::getCfgItem(std::string state) {
    if (state.empty())
        return nullptr;
    auto names = parseState(state);
    int idx = 0;
    Widget* item = mScreensRoot.get();
    for (auto& name : names) {
        Widget* found = getItemByName(item, name);
        if (!found) {
            if (item->tp == WidgetType::Mode) {
                found = getItemByName(item->parent, name);
                item = found;
                continue;
            }
            return nullptr;
        }
        item = found;
    }
    return item;
}

Widget* Master::matchWithSubItems(Widget* item) {
    if (!item)
        return nullptr;
    if (!item->oracle) {
        for (Widget* m : item->have) {
            if (m->tp == WidgetType::Mode) {
                if (matchItem(m))
                    return m;
            }
        }
        return nullptr;
    }
    if (matchItem(item)) {
        for (Widget* i : item->have) {
            Widget* res = matchWithSubItems(i);
            if (res)
                return res;
        }
        return item;
    }
    return nullptr;
}

bool Master::matchItem(Widget* item) {
    if (!item || !item->oracle)
        return false;
    return item->oracle->classify(mLastEDState.cEnv);
}

bool Master::debugMatchItem(Widget* item, ClassifyEnv& env) {
    if (!item)
        return false;
    if (!item->oracle) {
        for (Widget* m : item->have) {
            if (m->tp == WidgetType::Mode && m->oracle) {
                if (m->oracle->debugMatch(env) >= 0.8)
                    return true;
            }
        }
        return false;
    }
    return item->oracle->debugMatch(env) >= 0.8;
}

bool Master::debugTemplates(Widget* item, ClassifyEnv& env) {
    if (item == nullptr && env.debugImage.empty()) {
        cv::Rect captureRect;
        cv::Mat imageColor, imageGray;
        if (!captureWindow(captureRect, imageColor, imageGray)) {
            LOG(ERROR) << "Cannot capture screen for debug match";
            return false;
        }
        env.init(captureRect, imageColor, imageGray);
        for (auto &screen: mScreensRoot->have) {
            if (screen->oracle) {
                env.imageColor.copyTo(env.debugImage);
                debugTemplates(screen, env);
                el::Loggers::flushAll();
                std::string fname = "debug-match-"+screen->name+".png";
                //cv::imwrite(fname, env.debugImage);
                cv::imshow(fname, env.debugImage);
                cv::waitKey();
                env.debugImage.release();
            }
        }
        cv::destroyAllWindows();
        env.clear();
        el::Loggers::flushAll();
        return true;
    } else {
        if (debugMatchItem(item, env)) {
            bool ok = false;
            for (Widget* i : item->have) {
                ok |= debugTemplates(i, env);
            }
            return ok;
        }
        return false;
    }
}

void Master::drawButton(widget::Widget* item) {
    if (!(item->tp == WidgetType::Button || item->tp == WidgetType::Spinner || item->tp == WidgetType::List))
        return;
    cv::Rect rect = item->calcRect(mLastEDState.cEnv);
    if (rect.empty())
        return;
    rect = mLastEDState.cEnv.cvtReferenceToCaptured(rect);
    cv::Mat& debugImage = mLastEDState.cEnv.debugImage;
    cv::Scalar color(200, 80, 80);
    int size = (item == mLastEDState.focused) ? 2 : 1;
    cv::rectangle(debugImage, rect.tl(), rect.br(), color, size);
    if ( item->tp == WidgetType::Button)
        return;
    if ( item->tp == WidgetType::Spinner) {
        cv::Point p1 = rect.tl();
        p1.x += rect.height;
        cv::Point p2 = p1;
        p2.y += rect.height;
        cv::line(debugImage, p1, p2, color, size);
        p1.x = rect.br().x - rect.height;
        p2.x = p1.x;
        cv::line(debugImage, p1, p2, color, size);
        return;
    }
//    ////////////////////////////////////////////////
//    unsigned buttonRedColor = 0x45;
//    if (mCalibration) {
//        buttonRedColor = luv2rgb(mCalibration->normalButtonLuv)[2];
//    }
//    cv::Mat redImage;
//    cv::extractChannel(colorED, redImage, 2);
//    cv::Mat listImage(redImage, rect);
//
//    cv::imshow("Red image: "+mLastEDState.path(), listImage);
//
//    cv::Mat erodeImage;
//    int erosion_size = 2;
//    cv::Mat kernel = cv::getStructuringElement(cv::MORPH_RECT,
//                                               cv::Size(2*erosion_size + 1, 2*erosion_size+1),
//                                               cv::Point(erosion_size, erosion_size));
//    cv::erode(listImage, erodeImage, kernel);
//    cv::imshow("Eroded image: "+mLastEDState.path(), erodeImage);
//
//    cv::Mat thresholdedImage;
//    cv::threshold(erodeImage, thresholdedImage, buttonRedColor*0.95, 255, cv::THRESH_BINARY);
//    //cv::adaptiveThreshold(erodeImage, thresholdedImage, 255, cv::ADAPTIVE_THRESH_MEAN_C, cv::THRESH_BINARY, 15, 10);
//
//    cv::imshow("Threshold image: "+mLastEDState.path(), thresholdedImage);
//    cv::waitKey();
//
//    std::vector<std::vector<cv::Point>> contours;
//    cv::findContours(thresholdedImage, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
//    for (const auto& contour : contours) {
//        std::vector<cv::Point> approx;
//        cv::approxPolyDP(contour, approx, cv::arcLength(contour, true) * 0.02, true);
//
//        if (approx.size() == 4 && cv::contourArea(approx) > 1000 && cv::isContourConvex(approx)) {
//            cv::Rect bbox = cv::boundingRect(approx);
//            cv::rectangle(debugImage, bbox+rect.tl(), cv::Scalar(0, 255, 0), 2);
//        }
//    }
//    cv::destroyAllWindows();
}
bool Master::debugButtons() {
    detectEDState();
    widget::Widget* widget = mLastEDState.widget;
    mLastEDState.cEnv.debugImage = colorED.clone();
    if (widget) {
        for (Widget* item : widget->have)
            drawButton(item);
        if (widget->tp == WidgetType::Mode) {
            for (Widget *item: widget->parent->have)
                drawButton(item);
        }
    }
    cv::imshow(mLastEDState.path(), mLastEDState.cEnv.debugImage);
    cv::waitKey();
    cv::destroyAllWindows();
    mLastEDState.cEnv.debugImage = cv::Mat();
    return true;
}

bool Master::debugRectScreenshot(std::string name) {
    Sleep(200);
    if (!isForeground()) {
        SetForegroundWindow(hWndED);
        Sleep(200);
        if (!isForeground()) {
            LOG(ERROR) << "ED is not foreground";
            return false;
        }
    }
    cv::Rect captureRect;
    cv::Mat imgColor, imgGray;
    if (!captureWindow(captureRect, imgColor, imgGray)) {
        LOG(ERROR) << "Cannot capture screen";
        return false;
    }
    cv::Rect rect = mDevScreenRect;
    if ((rect & captureRect) != rect) {
        LOG(ERROR) << "Cannot make screenshot because dev rect is beyond of game client area";
        return false;
    }
    rect -= captureRect.tl();
    cv::imwrite("debug-" + name + "-gray.png", cv::Mat(imgGray, rect));
    cv::imwrite("debug-" + name + "-color.png", cv::Mat(imgColor, rect));
    return true;
}

Widget* Master::detectFocused(const Widget* parent) {
    for (Widget* item : parent->have) {
        cv::Rect r = item->calcRect(mLastEDState.cEnv);
        if (r.empty())
            continue;
        if (item->tp == WidgetType::Button || item->tp == WidgetType::Spinner) {
            // check it's focused
            int x = r.x + r.width - 36;
            int y = r.y + r.height / 2 - 8;
            if (item->tp == WidgetType::Spinner)
                x -= r.height + 10;
            mFocusedDetectorRect->mRect = cv::Rect(cv::Point(x,y), cv::Size(16,16));
            double value = mFocusedButtonDetector->classify(mLastEDState.cEnv);
            if (value >= 0.96)
                return item;
        }
    }
    return nullptr;
}
const UIState& Master::detectEDState() {
    mLastEDState.clear();
    // make screenshot
    if (!captureWindow(mCaptureRect, colorED, grayED)) {
        LOG(ERROR) << "Cannot capture screen";
        return mLastEDState;
    }
    mLastEDState.cEnv.init(mCaptureRect, colorED, grayED);

    // detect screen and widget
    for (auto& screen : mScreensRoot->have) {
        Widget* subItem = matchWithSubItems(screen);
        if (subItem) {
            mLastEDState.widget = subItem;
            LOG(DEBUG) << "Detected UI state: " << subItem->path;
            break;
        }
    }

    if (!mLastEDState.widget) {
        LOG(ERROR) << "Unknown state";
        return mLastEDState;
    }

    if (mFocusedButtonDetector) {
        // detect focused button
        Widget *focused = detectFocused(mLastEDState.widget);
        if (!focused && mLastEDState.widget->tp == WidgetType::Mode)
            focused = detectFocused(mLastEDState.widget->parent);
        if (focused) {
            mLastEDState.focused = focused;
            LOG(DEBUG) << "Detected focused button: " << focused->name;
        } else {
            LOG(DEBUG) << "Focused button not detected";
        }
    } else {
        LOG(DEBUG) << "Colors not calibrated, cannot detect focused widget";
    }

    LOG(INFO) << "Detected UI state: " << mLastEDState;
    return mLastEDState;
}

bool Master::isEDStateMatch(const std::string& state) {
    if (mLastEDState.path() == state)
        return true;
    auto names1 = parseState(mLastEDState.path());
    auto names2 = parseState(state);
    if (names1.size() != names2.size()) {
        LOG(DEBUG) << "States '" << names1 << "' and '" << names2 << "' do not match";
        return false;
    }
    if (names1.empty())
        return true;
    Widget* item = mScreensRoot.get();
    for (size_t idx = 0; idx < names1.size(); idx++) {
        auto& name1 = names1[idx];
        auto& name2 = names2[idx];
        if (name1 == "*" && name2 == "*") {
            Widget* found = getItemMode(item, "");
            if (!found) {
                LOG(DEBUG) << "Widget '" << item->path << "' have no modes";
                return false;
            }
            item = found;
            continue;
        }
        else if (name1 == "*") {
            Widget* found = getItemMode(item, name2);
            if (!found) {
                LOG(DEBUG) << "Widget '" << item->path << "' have no mode '" << name2 << "'";
                return false;
            }
            item = found;
            continue;
        }
        else if (name2 == "*") {
            Widget* found = getItemMode(item, name1);
            if (!found) {
                LOG(DEBUG) << "Widget '" << item->path << "' have no mode '" << name1 << "'";
                return false;
            }
            item = found;
            continue;
        }
        else if (name1 == name2) {
            Widget* found = getItemByName(item, name2);
            if (!found) {
                LOG(DEBUG) << "Widget '" << item->path << "' have no item '" << name2 << "'";
                return false;
            }
            item = found;
            continue;
        }
        LOG(DEBUG) << "States '" << names1 << "' and '" << names2 << "' do not match";
        return false;
    }
    return true;
}

bool Master::captureWindow(cv::Rect& captureRect, cv::Mat& colorImg, cv::Mat& grayImg) {
    if (!isForeground()) {
        LOG(WARNING) << "Elite Dangerous is not foreground; hWndED=" << std::format("0x{}", (void*)hWndED);
        //return false;
    }
    Capturer *capturer = Capturer::getEDCapturer(hWndED);
    if (!capturer)
        return false;
    if (!capturer->captureDisplay())
        return false;
    captureRect = capturer->getCaptureRect();
    colorImg = capturer->getColorImage();
    //cv::imwrite("captured-window-color.png", colorImg);
    grayImg = capturer->getGrayImage();
    //cv::imwrite("captured-window-gray.png", grayImg);
    return true;
}
