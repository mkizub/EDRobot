//
// Created by mkizub on 23.05.2025.
//

#include "Master.h"
#include "easylogging++.h"
#include <cpptrace/from_current.hpp>
#include "Keyboard.h"
#include "Task.h"
#include "Template.h"
#include "Capturer.h"
#include "Utils.h"
#include "UI.h"
#include "CLI11.hpp"
#include <opencv2/opencv.hpp>
#include <opencv2/core/utils/logger.hpp>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
//#include <gdiplus.h>
//#pragma comment(lib, "Gdiplus.lib")

const char ED_WINDOW_NAME[] = "Elite - Dangerous (CLIENT)";
const char ED_WINDOW_CLASS[] = "FrontierDevelopmentsAppWinClass";
const char ED_WINDOW_EXE[] = "EliteDangerous64.exe";

//ImageTemplate ED_TEMPLATE_AT_MARKET({92, 57, "templates/at_market.png"});
//ImageTemplate ED_TEMPLATE_AT_MARKET_SELL({245, 172, "templates/at_market_sell.png"});
//ImageTemplate ED_TEMPLATE_AT_MARKET_BUY({245, 172, "templates/at_market_buy.png"});
//ImageTemplate ED_TEMPLATE_AT_MARKET_DIALOG({507, 714, 300, "templates/at_market_dialog.png"});
//ImageTemplate ED_TEMPLATE_AT_MARKET_SELL_DIALOG({500, 170, 470, "templates/at_market_sell_dialog.png"});
//ImageTemplate ED_TEMPLATE_AT_MARKET_BUY_DIALOG({500, 185, 470, "templates/at_market_buy_dialog.png"}); // 240x185


namespace cfg {

enum class ItemType {
    Keypress, Action, Button, Mode, Dialog, Screen
};

struct Item {
    Item(ItemType tp) : tp(tp), oracle(nullptr), parent(nullptr) {}

    ItemType tp;
    std::string name;

    Item* parent;
    std::unique_ptr<Template> oracle;
    std::vector<Item*> have;
    std::string state;
};

struct Button : public Item {
    Button() : Item(ItemType::Button) {}
    std::vector<int> rect;
};

struct Mode : public Item {
    Mode() : Item(ItemType::Mode) {}
};

struct Dialog : public Item {
    Dialog() : Item(ItemType::Dialog) {}
    std::vector<int> rect;
};

struct Screen : public Item {
    Screen() : Item(ItemType::Screen) {}
};

struct Tree {
    cfg::Item* parent;
    cfg::Item* child;
};

static void from_json(const json5pp::value& j, Template*& o) {
    o = nullptr;
    if (j.is_null())
        return;
    if (j.is_object()) {
        Template* oracle = nullptr;
        if (j.as_object().contains("img")) {
            auto& jo = j.as_object();
            std::string filename = "templates/"+j["img"].as_string();
            int x = jo.at("at").as_array()[0].as_integer();
            int y = jo.at("at").as_array()[1].as_integer();
            cv::Point screenLT(x, y);
            int extL = 0;
            int extT = 0;
            int extR = 0;
            int extB = 0;
            if (jo.contains("ext")) {
                if (jo.at("ext").is_number())
                    extL = extT = extR = extB = jo.at("ext").as_integer();
                else if (jo.at("ext").is_array()) {
                    auto& jext = jo.at("ext").as_array();
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
            if (jo.contains("t")) {
                if (jo.at("t").is_number())
                    tmax = tmin = jo.at("t").as_number();
                else if (jo.at("t").is_array()) {
                    auto& jt = jo.at("t").as_array();
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
            o = new ImageTemplate(filename, screenLT, extLT, extRB, tmin, tmax);

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

static void from_json(const json5pp::value& j, Tree& t) {
    if (j.is_null()) {
        t.child = nullptr;
        return;
    } else {
        auto& jo = j.as_object();
        auto name = jo.at("name").as_string();
        if (name.starts_with("scr-")) {
            auto scr = new cfg::Screen();
            t.child = scr;
        }
        else if (name.starts_with("dlg-")) {
            auto dlg = new cfg::Dialog();
            t.child = dlg;
            if (jo.contains("rect") && jo.at("rect").is_array()) {
                for (auto &v: jo.at("rect").as_array())
                    dlg->rect.push_back(v);
            }
        }
        else if (name.starts_with("mode-")) {
            auto mode = new cfg::Mode();
            t.child = mode;
        }
        else if (name.starts_with("btn-")) {
            auto btn = new cfg::Button();
            t.child = btn;
            if (jo.contains("rect") && jo.at("rect").is_array()) {
                for (auto &v: jo.at("rect").as_array())
                    btn->rect.push_back(v);
            }
        }
        else {
            LOG(ERROR) << "Unknown widget type: " << name;
            t.child = nullptr;
            return;
        }
        t.child->name = name;
        if (t.parent)
            t.child->state = t.parent->state + ":" + name;
        else
            t.child->state = name;
        if (jo.contains("have") && jo.at("have").is_array()) {
            for (auto &h: jo.at("have").as_array()) {
                Tree tt{t.child, nullptr};
                from_json(h, tt);
                if (tt.child)
                    t.child->have.emplace_back(tt.child);
            }
        }
        if (jo.contains("detect")) {
            Template* oracle = nullptr;
            from_json(jo.at("detect"), oracle);
            t.child->oracle.reset(oracle);
        }
    }
}

} // namespace cfg

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

    if (!kwd) {
        char buffer[MAX_PATH] = {0};
        GetModuleFileName(nullptr, buffer, MAX_PATH);
        std::string fullPath(buffer);
        size_t lastSlash = fullPath.find_last_of("\\");
        if (lastSlash != std::string::npos) {
            std::string cwd = fullPath.substr(0, lastSlash);
            SetCurrentDirectory(cwd.c_str());
            LOG(INFO) << "Working Directory: " << cwd;
        }
    }

    cv::utils::logging::internal::replaceWriteLogMessage(writeOpenCVLogMessageFunc);
    cv::utils::logging::internal::replaceWriteLogMessageEx(writeOpenCVLogMessageFuncEx);

    keyMapping = {
            {{"esc",0}, Command::Stop},
            {{"printscreen",0}, Command::Start},
            {{"printscreen",keyboard::CTRL|keyboard::ALT}, Command::DebugTemplates},
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
            parseShortcutConfig(Command::Shutdown,  "shutdown",  obj);
        }
    }
    {
        std::ifstream ifs_config("actions.json5");
        auto j_actions = json5pp::parse5(ifs_config).as_array();
        for (json5pp::value& s: j_actions) {
            mActions[s.at("name").as_string()] = s;
        }
    }
    {
        std::ifstream ifs_config("screens.json5");
        auto j_screens = json5pp::parse5(ifs_config).as_array();
        for (json5pp::value& s: j_screens) {
            cfg::Tree t{nullptr, nullptr};
            cfg::from_json(s, t);
            if (t.child && t.child->tp == cfg::ItemType::Screen)
                mScreens.emplace_back((cfg::Screen *) t.child);
        }
    }

    keyboard::start(tradingKbHook);
    return 0;
}

static std::pair<std::string,unsigned> decodeShortcut(std::string key) {
    unsigned flags = 0;
    for (;;) {
        size_t pos = key.find_first_of('+');
        if (pos == std::string::npos)
            break;
        std::string mod = key.substr(0, pos);
        std::transform(mod.begin(), mod.end(), mod.begin(),[](unsigned char c){ return std::tolower(c); });
        if (mod == "ctrl")
            flags |= keyboard::CTRL;
        else if (mod == "alt")
            flags |= keyboard::ALT;
        else if (mod == "shift")
            flags |= keyboard::SHIFT;
        else if (mod == "win" || mod == "meta")
            flags |= keyboard::WIN;
        else if (mod == "ext")
            flags |= keyboard::EXT;
        else
            LOG(ERROR) << "Unknown key modifier " << mod;
        key = key.substr(pos+1);
    }
    std::transform(key.begin(), key.end(), key.begin(),[](unsigned char c){ return std::tolower(c); });
    return std::make_pair(key, flags);
}

static std::string encodeShortcut(const std::string& name, unsigned flags) {
    std::string res;
    if (flags & keyboard::CTRL) res += "Ctrl+";
    if (flags & keyboard::ALT) res += "Alt+";
    if (flags & keyboard::SHIFT) res += "Shift+";
    if (flags & keyboard::WIN) res += "Win+";
    if (flags & keyboard::EXT) res += "Ext+";
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
    CPPTRACE_TRY {
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
                case Command::DebugTemplates:
                    debugTemplates(nullptr, cv::Mat());
                    break;
                case Command::Shutdown:
                    clearCurrentTask();
                    return;
            }
        }
    }
    CPPTRACE_CATCH(const std::exception& e) {
        LOG(ERROR) << "Exception in main loop: " << cpptrace::from_current_exception();
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
    auto cmd = self.keyMapping.find(std::make_pair(name,flags));
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

bool Master::startTrade() {
    LOG(INFO) << "Start trading";
    if (currentTask) {
        LOG(ERROR) << "Exiting current task";
        clearCurrentTask();
    }

    int sells = mSells;
    int items = mItems;
    int res = UI::askSellInput(sells, items);
    if (res == UI::RES_EXIT) {
        pushCommand(Command::Shutdown);
        return false;
    }
    if (sells <= 0 || items <= 0)
        res = UI::RES_CANCEL;
    if (res == UI::RES_CANCEL)
        return false;
    mSells = sells;
    mItems = items;

    Sleep(200); // wait for dialog to dissappear
    hWndED = FindWindow(ED_WINDOW_CLASS, ED_WINDOW_NAME);
    if (!hWndED) {
        LOG(ERROR) << "Window [class " << ED_WINDOW_CLASS << "; name " << ED_WINDOW_NAME << "] not found" ;
        return false;
    }
    if (!isForeground()) {
        SetForegroundWindow(hWndED);
        Sleep(200); // wait for switching to foreground
        if (!isForeground()) {
            LOG(ERROR) << "ED is not foreground";
            return false;
        }
    }
    LOG(ERROR) << "Staring new trade task";
    currentTask.reset(new TaskSell(*this, mSells, mItems));
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
    CPPTRACE_TRY {
        bool ok = self.currentTask->run();
        self.pushCommand(Command::TaskFinished);
    } CPPTRACE_CATCH (const std::exception& e) {
        LOG(ERROR) << "Exception in current task: " << cpptrace::from_current_exception();
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

std::string trim(const std::string & source) {
    std::string s(source);
    s.erase(0,s.find_first_not_of(" \n\r\t"));
    s.erase(s.find_last_not_of(" \n\r\t")+1);
    return s;
}

const json5pp::value& Master::getAction(const std::string& action) {
    auto it = mActions.find(action);
    if (it == mActions.end()) {
        static json5pp::value nullAction;
        return nullAction;
    }
    return it->second;
}

cfg::Item* Master::getCfgItem(std::string state) {
    if (state.empty())
        return nullptr;
    size_t start = 0;
    size_t end = state.find(':');
    std::string name;
    if (end == std::string::npos) {
        name = state;
    } else {
        name = state.substr(start, end - start);
        start = end + 1;
        end = state.find(':', start);
    }
    name = trim(name);
    cfg::Item* item = nullptr;
    for (auto& screen : mScreens) {
        if (screen->name == name) {
            item = screen.get();
            break;
        }
    }
    if (!item)
        return nullptr;
    std::vector<cfg::Item*> items = item->have;
    while (end != std::string::npos) {
        name = state.substr(start, end - start);
        name = trim(name);
        start = end + 1;
        cfg::Item* found = nullptr;
        for (cfg::Item* i : items = item->have) {
            if (i->name == name) {
                found = i;
                break;
            }
        }
        if (!found)
            return nullptr;
        item = found;
        end = state.find(':', start);
    }
    return item;
}

cfg::Item* Master::matchWithSubItems(cfg::Item* item) {
    if (!item || !item->oracle)
        return nullptr;
    if (matchItem(item)) {
        for (cfg::Item* i : item->have) {
            cfg::Item* res = matchWithSubItems(i);
            if (res)
                return res;
        }
        return item;
    }
    return nullptr;
}

bool Master::matchItem(cfg::Item* item) {
    if (!item || !item->oracle)
        return false;
    return item->oracle->classify() >= 0.8;
}

bool Master::debugMatchItem(cfg::Item* item, cv::Mat debugImage) {
    if (!item || !item->oracle)
        return false;
    return item->oracle->debugMatch(debugImage) >= 0.8;
}

bool Master::debugTemplates(cfg::Item* item, cv::Mat debugImage) {
    if (item == nullptr && debugImage.empty()) {
        if (!captureWindow()) {
            LOG(ERROR) << "Cannot capture screen for debug match";
            return false;
        }
        for (auto &screen: mScreens) {
            if (screen->oracle) {
                debugImage = grayED.clone();
                debugTemplates(screen.get(), debugImage);
                el::Loggers::flushAll();
                std::string fname = "debug-match-"+screen->name+".png";
                cv::imwrite(fname, debugImage);
                cv::imshow(fname, debugImage);
                cv::waitKey();
            }
        }
        cv::destroyAllWindows();
        debugImage.release();
        el::Loggers::flushAll();
        return true;
    } else {
        if (item->oracle && debugMatchItem(item, debugImage)) {
            bool ok = false;
            for (cfg::Item* i : item->have) {
                ok |= debugTemplates(i, debugImage);
            }
            return ok;
        }
        return false;
    }
}


const std::string& Master::getEDState(const std::string& expectedState) {
    static std::string unknownState;
    // make screenshot
    if (!captureWindow()) {
        LOG(ERROR) << "Cannot capture screen";
        return unknownState;
    }

    cfg::Item* item = getCfgItem(expectedState);
    while (item) {
        if (item->oracle) {
            cfg::Item* subItem = matchWithSubItems(item);
            if (subItem)
                return subItem->state;
            item = item->parent;
        }
    }
    // not found, lookup from top, iterating all screens
    for (auto& screen : mScreens) {
        cfg::Item* subItem = matchWithSubItems(screen.get());
        if (subItem)
            return subItem->state;
    }
    LOG(ERROR) << "Unknown state";
    return unknownState; // nothing found
}

bool Master::captureWindow() {
    if (!isForeground()) {
        LOG(ERROR) << "Cannot capture screen - not foreground; hWndED=0x" << std::hex << std::setw(16) << std::setfill('0') << hWndED;
        return false;
    }
    Capturer *capturer = Capturer::getEDCapturer(hWndED);
    if (!capturer)
        return false;
    if (!capturer->captureDisplay())
        return false;
    grayED = capturer->getGrayImage();
    //cv::imwrite("captured-window-gray.png", grayED);
    return true;
}

cv::Mat Master::getGrayScreenshot() {
    return grayED;
}
