//
// Created by mkizub on 23.05.2025.
//

#include "Master.h"
#include "easylogging++.h"
#include "Keyboard.h"
#include "Task.h"
#include "Template.h"
#include "Capturer.h"
#include "Utils.h"
#include "UI.h"
#include <opencv2/opencv.hpp>
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
            int l = 4;
            int t = 4;
            int r = 4;
            int b = 4;
            if (jo.contains("ext")) {
                if (jo.at("ext").is_number())
                    l = t = r = b = jo.at("ext").as_integer();
                else if (jo.at("ext").is_array()) {
                    auto& jext = jo.at("ext").as_array();
                    if (!jext.empty())
                        l = t = r = b = jext[0].as_integer();
                    if (jext.size() > 1)
                        t = b = jext[1].as_integer();
                    if (jext.size() > 2)
                        r = jext[2].as_integer();
                    if (jext.size() > 3)
                        b = jext[3].as_integer();
                }
            }
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
            o = new ImageTemplate(filename, x, y, l, t, r, b, tmin, tmax);

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


Master::Master() {
    mSells = 3;
    mItems = 5;
    hWndED = FindWindow(ED_WINDOW_CLASS, ED_WINDOW_NAME);
    keyboard::start([this](int code, int scancode, int flags) { tradingKbHook(code, scancode, flags); });
    {
        std::ifstream ifs_config("configuration.json5");
        json5pp::value j_config = json5pp::parse5(ifs_config);
        if (auto tm = j_config.at("default-key-hold-time"); tm.is_integer())
            defaultKeyHoldTime = tm.as_integer();
        if (auto tm = j_config.at("default-key-after-time"); tm.is_integer())
            defaultKeyAfterTime = tm.as_integer();
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
}

Master::~Master() {
    keyboard::stop();
    stopTrade();
}


void Master::loop() {
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
            case Command::Shutdown:
                clearCurrentTask();
                return;
        }
    }
}

bool Master::isForeground() {
    return hWndED && hWndED == GetForegroundWindow();
}

void Master::tradingKbHook(int code, int scancode, int flags) {
    (void)scancode;
    (void)flags;

    switch(code) {
        case VK_ESCAPE:
            LOG(INFO) << "ESC pressed";
            pushCommand(Command::Stop);
            return;
        case VK_SNAPSHOT:
            LOG(INFO) << "PrintScreen pressed";
            pushCommand(Command::Start);
            return;
        case VK_PAUSE:
            LOG(INFO) << "Pause pressed";
            pushCommand(Command::Pause);
            return;
        default:
            LOG(DEBUG) << "Unknown command";
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
        LOG(INFO) << "Window [class " << ED_WINDOW_CLASS << "; name " << ED_WINDOW_NAME << "] not found" ;
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
    currentTaskThread = std::thread([this](){
        bool ok = currentTask->run();
        pushCommand(Command::TaskFinished);
    });
    return true;
}

bool Master::stopTrade() {
    LOG(INFO) << "Stop trading";
    clearCurrentTask();
    return false;
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
    return item->oracle->match(this) >= 0.8;
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

//    cfg::Screen* screen;
//    switch (expectedState) {
//        case EDState::Unknown: break;           // full detection
//        case EDState::DockedStationL: break;    // not implemented
//        case EDState::DockedStationM: break;    // not implemented
//        case EDState::MarketBuy:
//            screen = getScreen("market:buy");
//            if (match(screen))
//                return EDState::MarketBuy;
//            if (ED_TEMPLATE_AT_MARKET_BUY.match(this) &&
//                !(ED_TEMPLATE_AT_MARKET_DIALOG.match(this) || ED_TEMPLATE_AT_MARKET_BUY_DIALOG.match(this)))
//                return EDState::MarketBuy;
//            break;
//        case EDState::MarketBuyDialog:
//            if (ED_TEMPLATE_AT_MARKET_BUY_DIALOG.match(this))
//                return EDState::MarketBuyDialog;
//            break;
//        case EDState::MarketSell:
//            if (ED_TEMPLATE_AT_MARKET_SELL.match(this) &&
//                !(ED_TEMPLATE_AT_MARKET_DIALOG.match(this) || ED_TEMPLATE_AT_MARKET_SELL_DIALOG.match(this)))
//                return EDState::MarketSell;
//            break;
//        case EDState::MarketSellDialog:
//            if (ED_TEMPLATE_AT_MARKET_SELL_DIALOG.match(this))
//                return EDState::MarketSellDialog;
//            break;
//    }
//    // check market
//    for (auto& screen : mScreens) {
//        if (match_)
//    }
//    if (ED_TEMPLATE_AT_MARKET.match(this)) {
//        LOG(ERROR) << "At market";
//        if (ED_TEMPLATE_AT_MARKET_SELL.match(this)) {
//            LOG(ERROR) << "At market sell";
//            if (ED_TEMPLATE_AT_MARKET_SELL_DIALOG.match(this) || ED_TEMPLATE_AT_MARKET_DIALOG.match(this)) {
//                LOG(ERROR) << "At market sell dialog";
//                return EDState::MarketSellDialog;
//            } else {
//                return EDState::MarketSell;
//            }
//        }
//        if (ED_TEMPLATE_AT_MARKET_BUY.match(this)) {
//            LOG(ERROR) << "At market buy";
//            if (ED_TEMPLATE_AT_MARKET_BUY_DIALOG.match(this) || ED_TEMPLATE_AT_MARKET_DIALOG.match(this)) {
//                LOG(ERROR) << "At market buy dialog";
//                return EDState::MarketBuyDialog;
//            } else {
//                return EDState::MarketBuy;
//            }
//        }
//    } else {
//        LOG(ERROR) << "Unknown screen";
//    }
//
//    return EDState::Unknown;
}

bool Master::captureWindow() {
    if (!isForeground())
        return false;
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

//#include <FL/Fl.H>
//#include <FL/Fl_Window.H>
//#include <FL/Fl_Button.H>
//#include <FL/Fl_Input.H>
//#include <FL/Fl_Int_Input.H>
//#include <FL/Fl_Output.H>
//#include <FL/Fl_Text_Buffer.H>
//#include <FL/Fl_Text_Display.H>
//#include <FL/fl_message.H>
//
//void get_input_cb(Fl_Widget* btn, void* data) {
//    Fl_Output* output = (Fl_Output*)data;
//    const char* input_value = fl_input("Enter some text:", "");
//
//    if (input_value != nullptr) {
//        output->value(input_value);
//    }
//}
//
//static int dialogResult;
//struct CloseData {
//    Fl_Window* window;
//    int code;
//};
//
//static void close_window_cb(Fl_Widget* widget, void* data) {
//    CloseData* cd = (CloseData*)data;
//    dialogResult = cd->code;
//    cd->window->hide();
//}
//
//void Master::showStartDialog(int argc, char *argv[]) {
//    Fl_Window window(250, 130, "ED Robot");
//    CloseData cd{ &window, 0 };
//    Fl_Text_Buffer*  buff = new Fl_Text_Buffer();
//    Fl_Text_Display* disp = new Fl_Text_Display(10, 10, 230, 80);
//    Fl_Button*       btn  = new Fl_Button      (20, 90,  60, 30, "Ok");
//    btn->callback(close_window_cb, &cd);
//    btn->shortcut(FL_Enter);
//    disp->buffer(buff);
//    buff->text("Press 'Print Screen' key to start selling\nPress 'Esc' to stop");
//    window.end();
//    window.show(argc, argv);
//    Fl::run();
//}
//
//bool Master::askSellInput() {
//    dialogResult = 1;
//    Fl_Window window(250, 160, "Sell Items");
//    Fl_Int_Input* in_times = new Fl_Int_Input(100,  10, 100, 30, "Sell times: ");
//    in_times->value(mSells);
//    in_times->insert_position(0, in_times->value() ? strlen(in_times->value()) : 0);
//    in_times->selection_color(0xDDDDDD00);
//    Fl_Int_Input* in_items = new Fl_Int_Input(100,  50, 100, 30, "By N items: ");
//    in_items->value(mItems);
//    in_items->insert_position(0, in_items->value() ? strlen(in_items->value()) : 0);
//    in_times->selection_color(0xDDDDDD00);
//    Fl_Button* btn_ok      = new Fl_Button   (20,   80,  60, 30, "Ok");
//    Fl_Button* btn_cancel  = new Fl_Button   (100,  80, 100, 30, "Cancel");
//    Fl_Button* btn_exit    = new Fl_Button   (20,  120, 100, 30, "Exit EDRobot");
//    CloseData cd_ok{ &window, 0 };
//    CloseData cd_cancel{ &window, 1 };
//    CloseData cd_exit{ &window, 2 };
//    btn_ok->callback(close_window_cb, &cd_ok);
//    btn_cancel->callback(close_window_cb, &cd_cancel);
//    btn_exit->callback(close_window_cb, &cd_exit);
//    btn_ok->shortcut(FL_Enter);
//    btn_cancel->shortcut(FL_Escape);
//    window.end();
//    window.position(1800, 200);
//    window.show();
//    while (Fl::check()) {
//        // check exit
//    }
//    if (dialogResult == 1)
//        return false;
//    if (dialogResult == 2) {
//        pushCommand(Command::Shutdown);
//        return false;
//    }
//
//    int n_times = in_times->ivalue();
//    if (n_times <= 0)
//        return false;
//    mSells = n_times;
//
//    int n_items = in_items->ivalue();
//    if (n_items <= 0)
//        return false;
//    mItems = n_items;
//
//    return true;
//}
