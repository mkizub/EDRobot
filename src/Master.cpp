//
// Created by mkizub on 23.05.2025.
//

#include "pch.h"

#include "ui/UIManager.h"
#include "ai/AIManager.h"
#include "detect/Detector.h"
#include "detect/Lines.h"
#include "detect/NavPanel.h"
#include "Keyboard.h"
#include "Capturer.h"
#include "FuzzyMatch.h"
#include "EDWidget.h"
#include "OCR.h"
#include "Galaxy.h"
#include <fstream>
#include <memory>
#include <string>
#include <iterator>
#include <ranges>
#include "opencv2/core/utils/logger.hpp"
#include <CLI11/CLI11.hpp>
#include <magic_enum/magic_enum.hpp>

#ifndef NDEBUG
#include <cpptrace/cpptrace.hpp>
#include "cpptrace/from_current.hpp"
#endif

#ifdef CPPTRACE_TRY
# define TRY CPPTRACE_TRY
# define CATCH(param) CPPTRACE_CATCH(param)
# define GET_EXCEPTION_STACK_TRACE cpptrace::from_current_exception().to_string()
#else
# define TRY try
# define CATCH(param) catch(param)
# include <stacktrace>
# define GET_EXCEPTION_STACK_TRACE std::stacktrace::current()
#endif


Master& Mgr = Master::getInstance();

const wchar_t Master::ED_WINDOW_NAME[] = L"Elite - Dangerous (CLIENT)";
const wchar_t Master::ED_WINDOW_CLASS[] = L"FrontierDevelopmentsAppWinClass";
const wchar_t Master::ED_WINDOW_EXE[] = L"EliteDangerous64.exe";

using namespace widget;

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
    valid = false;
    guiFocus = GuiFocus::None;
    screen = nullptr;
    widget = nullptr;
    focused = nullptr;
    autopilot = false;
}

const std::string& UIState::path() const {
    static std::string empty;
    if (widget)
        return widget->path;
    return empty;
}

const std::string& UIState::screen_name() const {
    static std::string empty;
    if (!screen)
        return empty;
    return screen->name;
}
const std::string& UIState::focused_name() const {
    static std::string empty;
    if (!focused)
        return empty;
    return focused->name;
}

std::ostream& operator<<(std::ostream& os, const UIState& obj) {
    bool add_path = false;
    switch (obj.guiFocus) {
    default:
        os << "Unknown::";
        break;
    case GuiFocus::None:
        os << "Cockpit::";
        if (obj.autopilot)
            os << "autopilot";
        break;
    case GuiFocus::Right:
        os << "RightPanel::";
        add_path = true;
        break;
    case GuiFocus::Left:
        os << "LeftPanel::";
        add_path = true;
        break;
    case GuiFocus::Chat:
        os << "Chat::";
        break;
    case GuiFocus::Role:
        os << "LowPanel::";
        break;
    case GuiFocus::Services:
        os << "Services::";
        add_path = true;
        break;
    case GuiFocus::GalaxyMap:
        os << "GalaxyMap::";
        break;
    case GuiFocus::SystemMap:
        os << "SystemMap::";
        break;
    case GuiFocus::Orrery:
        os << "SystemMap::";
        break;
    case GuiFocus::FSS:
        os << "SystemMap::";
        break;
    case GuiFocus::SAA:
        os << "SystemMap::";
        break;
    case GuiFocus::Codex:
        os << "Codex::";
        break;
    }
    if (add_path && obj.widget) {
        os << obj.widget->path;
        if (obj.focused)
            os << "@" << obj.focused->name;
    }
    return os;
}

std::string UIState::to_string() const {
    if (!this->widget)
        return "unknown";
    if (this->focused)
        return widget->path+"@"+this->focused->name;
    return widget->path;
}

static std::vector<std::string> parseState(const std::string& state) {
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

std::vector<std::string> UIState::splitPath() const {
    if (!widget)
        return {};
    return parseState(widget->path);
}

bool UIState::match(const std::string& state) const {
    if (!widget) {
        return state.empty();
    }
    if (widget->path == state)
        return true;
    auto names1 = parseState(widget->path);
    auto names2 = parseState(state);
    if (names1.size() != names2.size()) {
        LOG(DEBUG) << "States '" << names1 << "' and '" << names2 << "' do not match";
        return false;
    }
    if (names1.empty())
        return true;
    Master& master = Master::getInstance();
    Widget* item = master.mScreensRoot.get();
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


Master& Master::getInstance() {
    static Master master;
    return master;
}

int Master::initialize(int argc, char* argv[]) {
    CLI::App options;
    options.allow_windows_style_options();

    bool kwd = false;
    std::string ocr_dir;
    std::string lang;
    options.add_flag("--kwd,--keep-working-dir", kwd, "Keep working directory (do not change on start)");
    options.add_option("--ocr-dir,--tesseract-dir", ocr_dir, "Tesseract OCR data directory");
    options.add_option("--lang,--language", lang, "Language (ru for russian)");

    CLI11_PARSE(options, argc, argv)
    if (!kwd) {
        wchar_t buffer[MAX_PATH] = {0};
        GetModuleFileName(nullptr, buffer, MAX_PATH);
        std::wstring fullPath(buffer);
        size_t lastSlash = fullPath.find_last_of(L'\\');
        if (lastSlash != std::string::npos) {
            std::wstring cwd = fullPath.substr(0, lastSlash);
            if (cwd.ends_with(L"\\bin"))
                cwd = cwd.substr(0,cwd.size()-4);
            SetCurrentDirectory(cwd.c_str());
            LOG(INFO) << "Working Directory: " << cwd;
        }
    }

    SetConsoleOutputCP(CP_UTF8);
    setlocale(LC_ALL, "");
    if (lang.empty() && GetUserDefaultUILanguage() == 0x0419) // ru-RU
        lang = "ru";

    if (equalsIgnoreCase(lang,"ru")) {
        SetConsoleOutputCP(CP_UTF8);
        setlocale(LC_ALL, "");
        setlocale(LC_MESSAGES, "ru-RU.UTF-8");
        _putenv_s("LC_MESSAGES", "ru");
        bindtextdomain("EDRobot", "locales");
        bind_textdomain_codeset("EDRobot", "UTF-8");
        textdomain("EDRobot");
        //LOG(INFO) << _("Hello world!");
    }

    TRY {
        initializeInternal(ocr_dir);
    } CATCH(const std::exception& e) {
        LOG(ERROR) << "Exception in initialization: " << e.what() << std::endl << GET_EXCEPTION_STACK_TRACE;
        return 1;
    }

    LOG(INFO) << lc_format("Press '{0}' to popup EDRobot", Cfg.getShortcutFor(Command::Start));
    LOG(INFO) << lc_format("Press '{0}' to pause/stop", Cfg.getShortcutFor(Command::Stop));


    return 0;
}
void Master::initializeInternal(std::string ocr_dir) {

    //cv::utils::logging::internal::replaceWriteLogMessage(writeOpenCVLogMessageFunc);
    //cv::utils::logging::internal::replaceWriteLogMessageEx(writeOpenCVLogMessageFuncEx);

    LOG(INFO) << "Loading configuration";
    Cfg.load();

    ai::init();

    if (ocr_dir.empty())
        ocr_dir = Cfg.mTesseractDataPath;
    if (ocr_dir.empty())
        ocr_dir = "tessdata-fast";
    ocr::init(ocr_dir);

    LOG(INFO) << "Initializing compass detector";
    mCompassDetector = std::make_unique<detect::CompassDetector>();

    LOG(INFO) << "Setup keyboard hooks";
    kbd::acquire_vJoy();
    std::vector<std::string> keys;
    for (auto& m : Cfg.keyMapping)
        keys.push_back(m.first.first);
    kbd::intercept(keys);
    kbd::start(tradingKbHook);
}

Master::Master() {
    mScreensRoot = std::make_unique<widget::Root>();
    mLoopWakeup = std::chrono::milliseconds(500);
}

Master::~Master() {
}

void Master::shutdown() {
    kbd::stop();
    kbd::release_vJoy();
    ai::shutdown();
    mCompassDetector.reset();
    mClassifyEnv.clear();
    if (mCapturer) {
        mCapturer->stop();
        mCapturer = nullptr;
        Capturer::shutdown();
    }
    ocr::shutdown();
    Cfg.shutdown();
}

void Master::loop() {
    TRY {
        LOG(INFO) << "Initializing screen capturers";
        Capturer::InitCapturers();

        LOG(INFO) << "Starting EDRobot task task_loop";
        bool shutdown = false;
        while (!shutdown) {
            pCommand cmd;
            popCommand(cmd);
            switch (cmd->command) {
            case Command::NoOp:
                if (mDetectLevelStream != DetectLevel::None)
                    detectEDState(mDetectLevelStream);
                else
                    debugWindowUpdate();
                break;
            case Command::TaskFinished:
                //clearCurrentTask();
                break;
            case Command::Start:
                UIManager::showMainDialog();
                break;
            case Command::Pause:
                stopAITask();
                break;
            case Command::Resume:
                resumeAITask();
                break;
            case Command::Stop:
                stopAITask();
                break;
            case Command::Autopilot:
                autopilotAITask();
                break;
            case Command::DebugTemplates:
                debugTemplates(nullptr, nullptr);
                break;
            case Command::DebugWindow:
                debugWindow();
                break;
            case Command::DebugStream:
                if (mDetectLevelStream == DetectLevel::None)
                    setDetectStream(DetectLevel::Buttons);
                else
                    setDetectStream(DetectLevel::None);
                break;
            case Command::DevRectScreenshot:
                debugRectScreenshot(cmd);
                break;
            case Command::DevRectSelect:
                UIManager::askSelectRectWindow();
                break;
            case Command::ResetCapturer:
                resetCapturer();
                break;
            case Command::DetectRequest:
                processDetectRequest(cmd);
                break;
            case Command::Shutdown:
                //clearCurrentTask();
                shutdown = true;
                break;
            }
        }
    }
    CATCH(const std::exception& e) {
        LOG(ERROR) << "Exception in main task_loop: " << e.what() << std::endl << GET_EXCEPTION_STACK_TRACE;
        //clearCurrentTask();
    }
}

bool Master::isGameForeground() {
    return hWndED && hWndED == GetForegroundWindow();
}

bool Master::setGameForeground() {
    if (!hWndED)
        Master::getCapturer();
    if (!hWndED)
        return false;
    if (hWndED == GetForegroundWindow())
        return true;
    SetForegroundWindow(hWndED);
    Sleep(500); // wait for switching to foreground
    return isGameForeground();
}

bool Master::setGameMouseCapture() {
    if (!hWndED)
        Master::getCapturer();
    if (!hWndED)
        return false;
    if (hWndED != GetForegroundWindow()) {
        SetForegroundWindow(hWndED);
        Sleep(500); // wait for switching to foreground
    }
    HWND hWndCapt = GetCapture();
    if (hWndCapt != hWndED) {
        SetCapture(hWndED);
        Sleep(100); // wait for switching to foreground
    }
    return GetCapture() == hWndED;
}


void Master::tradingKbHook(int code, int scancode, int flags, const std::string& name) {
    (void)code;
    (void)scancode;
    LOG(DEBUG) << "Key '"+encodeShortcut(name,flags)+"' pressed";
    Master& self = getInstance();
    auto keyMapping = Cfg.keyMapping;
    auto cmd = keyMapping.find(std::make_pair(toLower(name),flags));
    if (cmd != keyMapping.end()) {
        LOG(INFO) << "Command " << enum_name(cmd->second) << " by key '"+encodeShortcut(name,flags)+"' pressed";
        if (cmd->second == Command::Pause) {
            ai::toggleDebugPause();
            if (ai::isDebugPause())
                UIManager::showMainDialog();
        }
        else
            self.pushCommand(cmd->second);
    } else {
        if (Cfg.autoPause && ai::active()) {
            LOG(INFO) << "Auto-pausing AI task because '"+encodeShortcut(name,flags)+"' pressed";
            self.pushCommand(Command::Pause);
        }
    }
}

void Master::pushCommand(Command cmd) {
    std::unique_lock<std::mutex> lock(mCommandMutex);
    mCommandQueue.emplace(new CommandEntry(cmd));
    mCommandCond.notify_one();
}

void Master::pushCommand(CommandEntry* cmd) {
    std::unique_lock<std::mutex> lock(mCommandMutex);
    mCommandQueue.emplace(cmd);
    mCommandCond.notify_one();
}

struct CommandDetectRequest : public CommandEntry {
    CommandDetectRequest(std::promise<bool>&& p, DetectRequest&& req)
            : CommandEntry(Command::DetectRequest)
            , promise(std::move(p))
            , request(std::move(req))
    {}
    ~CommandDetectRequest() override = default;
    std::promise<bool> promise;
    DetectRequest request;
};

void Master::pushDetectRequest(std::promise<bool>&& p, DetectRequest&& req) {
    pushCommand(new CommandDetectRequest(std::move(p), std::move(req)));
}

struct CommandDevRestScreenshot : public CommandEntry {
    CommandDevRestScreenshot(cv::Rect rect)
            : CommandEntry(Command::DevRectScreenshot)
            , rect(rect)
    {}
    ~CommandDevRestScreenshot() override = default;
    cv::Rect rect;
};

void Master::pushDevRectScreenshotCommand(cv::Rect rect) {
    pushCommand(new CommandDevRestScreenshot(rect));
}

void Master::popCommand(pCommand& cmd) {
    std::unique_lock<std::mutex> lock(mCommandMutex);
    mCommandCond.wait_for(lock, mLoopWakeup, [this]() { return !mCommandQueue.empty(); });
    if (!mCommandQueue.empty()) {
        std::swap(cmd, mCommandQueue.front());
        mCommandQueue.pop();
    } else {
        pCommand nop = std::make_unique<CommandEntry>(Command::NoOp);
        std::swap(cmd, nop);
    }
}

const Commodity* Master::getLabelCommodity(ResolvedEnv& rEnv, const cv::Mat& grayImage, const std::string& lbl_name) {
    const Widget* widget = getInstance().getCfgItem(lbl_name);
    if (!widget) {
        LOG(ERROR) << "Widget '" << lbl_name << "' not found";
        return nullptr;
    }
    if (widget->tp != WidgetType::Label) {
        LOG(ERROR) << "Widget '" << lbl_name << "' is not a label";
        return nullptr;
    }
    Label* lbl = (Label*)widget;
    ClassifiedRect* cr = nullptr;
    for (auto& it : rEnv.classified) {
        if (it.cdt == ClsDetType::Widget && it.u.widg.widget == widget) {
            cr = &it;
            break;
        }
    }
    if (!cr) {
        LOG(ERROR) << "Label '" << lbl_name << "' was not detected on screen";
        return nullptr;
    }

    cv::Rect rect = rEnv.cvtReferenceToCaptured(cr->detectedRect);

    std::string text;
    int ocr_conf = ocr::ocrMarketLblText(grayImage, rEnv, *cr, text);
    const Commodity* commodity = nullptr;
    if (ocr_conf > 30) {
        LOG(DEBUG) << "Label text OCR: '" << text << "' conf=" << ocr_conf << "%";
        commodity = Cfg.getCommodityByName(text, true);
    }
    LOG_IF(!commodity,ERROR) << "Commodity '" << text << "' not found";
    return commodity;
}

int Master::canSell(Commodity* commodity) const {
    if (!commodity)
        return 0;
    if (commodity->ship.count <= commodity->ship.stolen)
        return 0;
    spMarket market = st::currentMarket;
    if (!market || market->items.empty())
        return 0;
    if (market->stationType == "FleetCarrier") {
        auto it = market->items.find(commodity);
        if (it == market->items.end())
            return 0;
        MarketLine& ml = it->second;
        return std::min(commodity->ship.count, ml.demand);
    }
    return commodity->ship.count - commodity->ship.stolen;
}

int Master::canBuy(Commodity* commodity) const {
    if (!commodity)
        return 0;
    spMarket market = st::currentMarket;
    if (!market || market->items.empty())
        return 0;
    auto it = market->items.find(commodity);
    if (it == market->items.end())
        return 0;
    MarketLine& ml = it->second;
    if (ml.stock <= 0)
        return 0;
    int free = st::shipStats.cargoCapacity - st::shipStats.cargo;
    if (free <= 0)
        return 0;
    return std::min(free, ml.stock);
}

const Commodity* Master::ocrMarketRowCommodity(ResolvedEnv& rEnv, const cv::Mat& grayImage, ClassifiedRect* cr, int min_conf) {
    if (cr->cdt != ClsDetType::ListRow)
        return nullptr;
    if (cr->u.lrow.commodity)
        return cr->u.lrow.commodity;
    if (cr->text.empty()) {
        int conf = ocr::ocrRowText(ocr::GENERIC, grayImage, rEnv, *cr, 1, cr->text);
        cr->u.lrow.text_confidence = conf;
        if (conf >= min_conf) {
            cr->u.lrow.commodity = Cfg.getCommodityByName(cr->text, true);
        } else {
            cr->text.clear();
        }
    }
    else if (cr->u.lrow.text_confidence >= min_conf) {
        cr->u.lrow.commodity = Cfg.getCommodityByName(cr->text, true);
    }
    return cr->u.lrow.commodity;
}

bool Master::approximateListOfCommodities(ResolvedEnv& rEnv, const cv::Mat& grayImage,
                                          const std::string& lst_name, const std::vector<Commodity*>& table,
                                          std::vector<CommodityMatch>* verify)
{
    std::vector<ClassifiedRect*> rows;
    for (auto& cr : rEnv.classified) {
        if (cr.cdt != ClsDetType::ListRow || cr.u.lrow.list->name != lst_name)
            continue;
        rows.push_back(&cr);
    }
    if (rows.empty())
        return false;
    ClassifiedRect* first = nullptr;
    ClassifiedRect* last = nullptr;
    for (auto lr : rows) {
        if (lr->u.lrow.commodity == nullptr && !lr->text.empty())
            lr->u.lrow.commodity = Cfg.getCommodityByName(lr->text, true);
        if (lr->u.lrow.commodity && !first)
            first = lr;
        if (lr->u.lrow.commodity)
            last = lr;
    }
    for (auto lr : rows) {
        if (lr->u.lrow.commodity == nullptr && !lr->text.empty())
            lr->u.lrow.commodity = Cfg.getCommodityByName(lr->text, true);
    }
    if (!first) {
        for (auto lr: rows) {
            if (ocrMarketRowCommodity(rEnv, grayImage, lr, 80)) {
                first = lr;
                break;
            }
        }
        for (auto lr : rows | std::views::reverse) {
            if (ocrMarketRowCommodity(rEnv, grayImage, lr, 80)) {
                last = lr;
                break;
            }
        }
    }
     if (!first || !last)
        return false;
    // check first and last match with table
    auto it_table_first = std::find_if(table.begin(), table.end(), [first](Commodity* c) { return c == first->u.lrow.commodity; });
    if (it_table_first == table.end())
        return false;
    auto it_table_last = std::find_if(table.begin(), table.end(), [last](Commodity* c) { return c == last->u.lrow.commodity; });
    if (it_table_last == table.end())
        return false;
    auto it_row_first = std::find(rows.begin(), rows.end(), first);
    if (it_row_first == rows.end())
        return false;
    auto it_row_last = std::find(rows.begin(), rows.end(), last);
    if (it_row_last == rows.end())
        return false;

    int table_dist = it_table_last - it_table_first;
    int list_dist = it_row_last - it_row_first;
    if (table_dist != list_dist) {
        LOG(ERROR) << "Approximated commodity table does not match with OCR-recognized entries. Do not use market filters!";
        LOG(ERROR) << "Recognized first row: '" << first->u.lrow.commodity->name << "' at row " << (it_row_first - rows.begin())
                   << " expected table position " << (it_table_first - table.begin());
        LOG(ERROR) << "Recognized last  row: '" << last->u.lrow.commodity->name << "' at row " << (it_row_last - rows.begin())
                   << " expected table position " << (it_table_last - table.begin());
        return false;
    }
    {
        auto it_table = it_table_first;
        auto it_rows = it_row_first;
        for (; it_rows >= rows.begin(); --it_rows, --it_table) {
            if ((*it_rows)->u.lrow.commodity && (*it_rows)->u.lrow.commodity != *it_table)
                return false;
            (*it_rows)->u.lrow.commodity = *it_table;
            if (it_rows == rows.begin())
                break;
        }
        it_table = it_table_first;
        it_rows = it_row_first;
        for (; it_rows != rows.end(); ++it_rows, ++it_table) {
            if ((*it_rows)->u.lrow.commodity && (*it_rows)->u.lrow.commodity != *it_table)
                return false;
            (*it_rows)->u.lrow.commodity = *it_table;
        }
    }

    if (verify) {
        FuzzyMatch matcher;
        for (auto lr: rows) {
            int idx = std::find(rows.begin(), rows.end(), lr) - rows.begin();
            if (!lr->u.lrow.commodity) {
                LOG(INFO) << "Commodity for row # " << std::to_string(idx) << " not detected and not deduced";
                verify->emplace_back(nullptr, 0, 0);
                continue;
            }
            std::string text;
            int ocr_conf = ocr::ocrRowText(ocr::GENERIC, grayImage, rEnv, *lr, 1, text);
            lr->u.lrow.text_confidence = ocr_conf;
            int fuzzy_conf = 0;
            if (ocr_conf > 30) {
                std::string match;
                if (text == lr->u.lrow.commodity->name) {
                    match = "EXACT";
                    fuzzy_conf = 100;
                } else {
                    fuzzy_conf = (int)matcher.ratio(toUtf16(text), lr->u.lrow.commodity->wocr);
                    match = std::to_string(fuzzy_conf) + "%";
                }
                LOG(INFO) << "OCR for row # " << std::to_string(idx) << " found: '" << text
                          << "' have to be '" << lr->u.lrow.commodity->name << "' match: " << match;
            }
            verify->emplace_back(lr->u.lrow.commodity, ocr_conf, fuzzy_conf);
        }
    }

    return true;
}

bool Master::pauseAITask() {
    ai::interrupt();
    return true;
}

bool Master::resumeAITask() {
    ai::resume();
    return true;
}

bool Master::stopAITask() {
    ai::interrupt();
    return true;
}

bool Master::autopilotAITask() {
    return ai::autopilot();
}

cv::Rect Master::resolveWidgetReferenceRect(const std::string& name) const {
    const Widget* item = getCfgItem(name);
    if (!item) {
        LOG(ERROR) << "Widget '" << name << "' not found";
        return {};
    }
    auto r = mClassifyEnv.calcReferenceRect(item->rect);
    if (r.empty()) {
        LOG(ERROR) << "Widget has no rect";
        return {};
    }
    return r;
}

const Widget* Master::getCfgItem(std::string state) const {
    if (state.empty())
        return nullptr;
    auto names = parseState(state);
    if (names.size() == 1 && !state.starts_with("scr-"))
        names = parseState(mLastUIState.path() + ":" + state);
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
                Widget* res = matchWithSubItems(m);
                if (res)
                    return res;
            }
        }
        return nullptr;
    }
    if (matchItem(item)) {
        if (item->tp == WidgetType::Screen) {
            bool savedWarpMode = mClassifyEnv.isWarpMode();
            widget::Screen *screen = static_cast<widget::Screen *>(item);
            if (screen->transform) {
                mClassifyEnv.warpPerspective(screen->transform);
                mClassifyEnv.setWarpMode(screen->transform->valid);
            }
            for (Widget* i : item->have) {
                Widget* res = matchWithSubItems(i);
                if (res) {
                    item = res;
                    break;
                }
            }
            mClassifyEnv.setWarpMode(savedWarpMode);
            return item;
        } else {
            for (Widget *i: item->have) {
                Widget *res = matchWithSubItems(i);
                if (res)
                    return res;
            }
        }
        return item;
    }
    return nullptr;
}

bool Master::matchItem(Widget* item) {
    if (!item || !item->oracle)
        return false;
    return item->oracle->match(mClassifyEnv) >= 0.5;
}

bool Master::debugMatchItem(Widget* item, ClassifyEnv& env) {
    if (!item)
        return false;
    if (!item->oracle) {
        for (Widget* m : item->have) {
            if (m->tp == WidgetType::Mode && m->oracle) {
                if (m->oracle->match(env) >= 0.5)
                    return true;
            }
        }
        return false;
    }
    return item->oracle->match(env) >= 0.5;
}

widget::Widget* Master::debugTemplates(Widget* item, ClassifyEnv* env) {
    if (!env) {
        ClassifyEnv debugEnv;
        if (!captureWindow(debugEnv)) {
            LOG(ERROR) << "Cannot capture screen for debug match";
            return nullptr;
        }
        debugEnv.isDebugMatch_ = true;
        Widget* foundWidget = nullptr;
        for (auto widget: mScreensRoot->have) {
            if (widget->tp != WidgetType::Screen)
                continue;
            widget::Screen* screen = static_cast<widget::Screen*>(widget);
            if (!screen || !screen->checkStatus())
                continue;
            auto w = debugTemplates(screen, &debugEnv);
            if (st::guiFocus == GuiFocus::None)
                mCompassDetector->match(debugEnv);
            else
                mCompassDetector->clear();
            el::Loggers::flushAll();
            if (w && !foundWidget) {
                foundWidget = w;
                if (screen->transform) {
                    debugEnv.warpPerspective(screen->transform);
                    if (screen->transform->valid) {
                        cv::imwrite("warped-screen-color.png", debugEnv.getWarpedColorImage());
                    }
                }
                UIState uiState;
                uiState.valid = true;
                uiState.guiFocus = st::guiFocus;
                uiState.screen = screen;
                Widget::DetectParams params {debugEnv, uiState, *this, DetectLevel::ListRows};
                screen->detect(params);
            }
            //std::string fname = "debug-match-"+screen->name+".png";
            //cv::imwrite(fname, env.debugImage);
            //cv::imshow(fname, debugEnv.getDebugImage());
            //cv::waitKey();
        }
        //cv::destroyAllWindows();
        //mDuplicateToDebugWindow = UIManager::showDebugWindow();
        if (mDuplicateToDebugWindow) {
            if (!UIManager::postToDebugWindow(debugEnv.getColorImage(), debugEnv.getDebugImage()))
                mDuplicateToDebugWindow = false;
        }
        debugEnv.clear();
        el::Loggers::flushAll();
        return foundWidget;
    } else {
        if (debugMatchItem(item, *env)) {
            Widget* foundWidget = nullptr;
            if (item->tp == WidgetType::Screen) {
                bool savedWarpMode = env->isWarpMode();
                widget::Screen *screen = static_cast<widget::Screen *>(item);
                if (screen->transform) {
                    env->warpPerspective(screen->transform);
                    env->setWarpMode(screen->transform->valid);
                }
                for (Widget* i : item->have) {
                    Widget* res = debugTemplates(i, env);
                    if (res && !foundWidget)
                        foundWidget = res;
                }
                env->setWarpMode(savedWarpMode);
                return item;
            } else {
                for (Widget *i: item->have) {
                    Widget *res = debugTemplates(i, env);
                    if (res && !foundWidget)
                        foundWidget = res;
                }
            }
            return foundWidget ? foundWidget : item;
        }
        return nullptr;
    }
}

bool Master::debugWindow() {
    detectEDState(DetectLevel::Screen);
    if (UIManager::showDebugWindow()) {
        mDuplicateToDebugWindow = UIManager::postToDebugWindow(mClassifyEnv.getColorImage());
        return true;
    } else {
        mDuplicateToDebugWindow = false;
        return true;
    }
}

bool Master::debugWindowUpdate() {
    double streamFPS = -1;
    if (mStreamFramePoints.size() > 2) {
        auto startTime = mStreamFramePoints.front();
        auto endTime = mStreamFramePoints.back();
        auto elapsed = std::chrono::duration_cast<std::chrono::duration<double>>(endTime - startTime);
        streamFPS = mStreamFramePoints.size() / elapsed.count();
        static int fps_counter;
        fps_counter = (fps_counter + 1) % 60;
        if (fps_counter == 0) {
            std::string text;
            //if (streamFPS > 5)
            //    text = std::format("Stream FPS: {}", int(std::round(streamFPS)));
            //else
                text = std::format("Stream FPS: {:.1f}", streamFPS);
            LOG(INFO) << text;
        }
    }

    mDuplicateToDebugWindow = UIManager::hasDebugWindow();
    if (!mDuplicateToDebugWindow || !mLastUIState.valid)
        return false;

    ClassifyEnv& cEnv = mClassifyEnv;
    cv::Mat& debugImage = cEnv.getDebugImage();
    if (debugImage.empty())
        return UIManager::postToDebugWindow(cEnv.getColorImage(), debugImage);

    if (streamFPS > 0) {
        std::string text;
        if (streamFPS > 5)
            text = std::format("FPS: {}", int(std::round(streamFPS)));
        else
            text = std::format("FPS: {:.1f}", streamFPS);
        cv::putText(debugImage, text, {10,40}, cv::FONT_HERSHEY_PLAIN, 1.5, {254, 254, 254}, 2);
    }

    if (st::guiFocus == GuiFocus::None) {
        detect::CompassDetector& c = *mCompassDetector.get();
        detect::ImageTemplate& ct = *c.compassDetector.get();

        {
            cv::Rect r = cEnv.cvtReferenceToCaptured(ct.matchRect);
            cv::rectangle(debugImage, r, {96, 96, 96}, 2);
        }

        if (mCompassDetector->lastHemisphere) {
            cv::Point center = (ct.captureRect.tl() + ct.captureRect.br()) / 2;
            cv::Size size = ct.captureRect.size() / 2;
            cv::Scalar color {0, 255, 0};
            if (c.lastHemisphere < 0)
                color = {255, 0, 0};
            cv::ellipse(debugImage, center, size, 0, 0, 360, color, 2);
            if (c.lastHemisphere > 0)
                cv::rectangle(debugImage, c.dotCaptureRect, {0, 255, 0}, 2);
            else
                cv::rectangle(debugImage, c.dotCaptureRect, {255, 0, 0}, 2);

            std::string text = std::format("{}/{}/{}", int(c.lastTgtPitch), int(c.lastTgtYaw), int(c.lastTgtRoll));
            cv::Point orig = ct.captureRect.tl() + cv::Point(0, -10);
            cv::putText(debugImage, text, orig, cv::FONT_HERSHEY_PLAIN, 1.5, {0, 0, 0}, 3);
            cv::putText(debugImage, text, orig, cv::FONT_HERSHEY_PLAIN, 1.5, {254, 254, 254}, 2);
        }

        if (c.navTargetFound) {
            cv::Point center = cEnv.cvtReferenceToCaptured(cEnv.ReferenceScreenCenter + c.lastNavTargetOffset);
            int radius = c.navTargetReferenceRadius * cEnv.getScale();
            cv::circle(debugImage, center, radius, {255,255,0}, 2);
            if (c.lastNavDist.unit != dist_t::X) {
                std::string text = c.lastNavDist.to_string();
                cv::Point orig = center + cv::Point(radius, -radius);
                cv::putText(debugImage, text, orig, cv::FONT_HERSHEY_PLAIN, 1.5, {0, 0, 0}, 3);
                cv::putText(debugImage, text, orig, cv::FONT_HERSHEY_PLAIN, 1.5, {254, 254, 254}, 2);
            }
        }
    }

    if (mLastUIState.screen && mLastUIState.screen->transform && mLastUIState.screen->transform->valid) {
        auto* npd = dynamic_cast<detect::NavPanelDetector*>(mLastUIState.screen->oracle.get());
        if (npd) {
            detect::LineDetector *detect_angle = npd->getLineDetector("detect-angle");
            cv::rectangle(debugImage, detect_angle->lineMatchRect, {128,128,128});
            for (auto& dl : detect_angle->detectedLines) {
                cv::Line line = dl.line;
                line += detect_angle->lineMatchRect.tl();
                cv::line(debugImage, line.p0(), line.p1(), {128,128,128}, 1, cv::LINE_AA);
            }
            cv::Point2f points[4];
            npd->lastRotRect.points(points); // bottomLeft, topLeft, topRight, bottomRight
            for (int j = 0; j < 4; j++)
                cv::line(debugImage, points[j], points[(j+1) % 4], {160,160,160}, 1, cv::LINE_AA);
            auto& topLine = npd->lastTopLine;
            cv::line(debugImage, topLine.p0(), topLine.p1(), {200,200,200}, 1, cv::LINE_AA);
            cv::drawMarker(debugImage, topLine.p0(), {255,255,255}, 10);
            cv::drawMarker(debugImage, topLine.p1(), {255,255,255}, 10);
            auto& btmLine = npd->lastBottomLine;
            cv::line(debugImage, btmLine.p0(), btmLine.p1(), {200,200,200}, 1, cv::LINE_AA);
            cv::drawMarker(debugImage, btmLine.p0(), {255,255,255}, 10);
            cv::drawMarker(debugImage, btmLine.p1(), {255,255,255}, 10);
        }
        for (int p=0; p < 4; p++) {
            auto& p0 = mLastUIState.screen->transform->transformSrc[p];
            auto& p1 = mLastUIState.screen->transform->transformSrc[(p+1)%4];
            cv::line(debugImage, p0, p1, {200,200,200}, 1, cv::LINE_AA);
        }
    }

    for (auto& cr : cEnv.classified) {
//        if (cr.cdt == ClsDetType::Detected) {
//            if (cr.warped) {
//                auto points = cEnv.unWarp(cr.detectedRect);
//                for (int j = 0; j < 4; j++)
//                    cv::line(debugImage, points[j], points[(j+1) % 4], {96, 255, 96}, 1, cv::LINE_AA);
//            } else {
//                cv::Rect r = cEnv.cvtReferenceToCaptured(cr.detectedRect);
//                cv::rectangle(debugImage, r, {96, 255, 96}, 1);
//            }
//        }
        if (cr.cdt == ClsDetType::LineDetected) {
            cv::Rect r = cEnv.cvtReferenceToCaptured(cr.detectedRect);
            cv::rectangle(debugImage, r, {96, 255, 96}, 2);
            //std::string text = std::format("s:{:.4f} a:{:.4f} m:{:.4f}", cr.u.ldet.scale, cr.u.ldet.angle, cr.u.ldet.match);
            //cv::putText(debugImage, text, r.br(), cv::FONT_HERSHEY_PLAIN, 1.5, {254, 254, 254}, 2);
            cv::Line line = cEnv.cvtReferenceToCaptured(cr.u.ldet.referenceLine);
            cv::line(debugImage, line.p0(), line.p1(), {96, 255, 96}, 3, cv::LINE_AA);
        }
        if (cr.cdt == ClsDetType::Widget) {
            cv::Scalar color = {255, 96, 96};
            int thickness = 1;
            if (cr.u.widg.ws == WState::Focused) {
                color = {255, 255, 96};
                thickness = 2;
            }
            if (cr.warped) {
                auto points = cEnv.unWarp(cr.detectedRect);
                for (int j = 0; j < 4; j++)
                    cv::line(debugImage, points[j], points[(j+1) % 4], color, thickness, cv::LINE_AA);
            } else {
                cv::Rect r = cEnv.cvtReferenceToCaptured(cr.detectedRect);
                cv::rectangle(debugImage, r, color, thickness);
            }
        }
        if (cr.cdt == ClsDetType::ListRow) {
            cv::Scalar color = {255, 96, 96};
            int thickness = 1;
            if (cr.u.lrow.ws == WState::Focused) {
                color = {255, 255, 96};
                thickness = 2;
            }
            if (cr.warped) {
                auto points = cEnv.unWarp(cr.detectedRect);
                for (int j = 0; j < 4; j++)
                    cv::line(debugImage, points[j], points[(j+1) % 4], color, thickness, cv::LINE_AA);
            } else {
                cv::Rect r = cEnv.cvtReferenceToCaptured(cr.detectedRect);
                cv::rectangle(debugImage, r, color, thickness);
            }
        }
    }

    return UIManager::postToDebugWindow(cEnv.getColorImage(), debugImage);
}

bool Master::debugRectScreenshot(pCommand& cmd) {
    cv::Rect rect;
    {
        CommandDevRestScreenshot *c = dynamic_cast<CommandDevRestScreenshot *>(cmd.get());
        if (!c)
            return false;
        rect = c->rect;
        cmd.reset();
    }
    std::string clipboardText = std::format("[{},{},{},{}]", rect.x, rect.y, rect.width, rect.height);
    LOG(INFO) << "Selected rect: " << rect << ": " << clipboardText;
    pasteToClipboard(clipboardText);

    Sleep(200);
    if (!isGameForeground()) {
        SetForegroundWindow(hWndED);
        Sleep(200);
        if (!isGameForeground()) {
            LOG(ERROR) << "ED is not foreground";
            //return false;
        }
    }
    ClassifyEnv debugEnv;
    if (!captureWindow(debugEnv)) {
        LOG(ERROR) << "Cannot capture screen for screenshot";
        return false;
    }
    if ((rect & debugEnv.captureRect) != rect) {
        LOG(ERROR) << "Cannot make screenshot because dev rect is beyond of game client area";
        return false;
    }
    rect -= debugEnv.captureRect.tl();

    //cv::Vec4b color = debugEnv.getColorImage().at<cv::Vec4b>( (rect.tl() + rect.br())/2 );
    //LOG(INFO) << "Selected rect BGRA color (center dot): " << color;

    //cv::imwrite("debug-rect-gray.png", cv::Mat(debugEnv.getGrayImage(), rect));
    cv::imwrite("debug-rect-color.png", debugEnv.getColorImage()(rect));

//    std::string text;
//    if (Master::ocrMarketText(debugEnv.getGrayImage(), rect, text) > 50) {
//        Commodity *commodity = Cfg.getCommodityByName(text, true);
//        if (commodity) {
//            if (Cfg.lng != EN)  // they capitalize text
//                text = commodity->name;
//            std::string lng;
//            if (Cfg.lng == RU)
//                lng = "-rus";
//            else if (Cfg.lng == EN)
//                lng = "-eng";
//            else
//                lng = "-xxx";
//            cv::imwrite(commodity->nameId + lng + ".png", cv::Mat(debugEnv.getGrayImage(), rect));
//            std::ofstream wf(commodity->nameId + lng + ".gt.txt", std::ios::trunc | std::ios::binary);
//            if (wf.is_open()) {
//                wf << text;
//                wf.close();
//            }
//        }
//    }
    return true;
}

//int Master::ocrNavText(const cv::Mat& grayImage, cv::Rect rect, std::string& text, std::optional<bool> invert) {
//    text.clear();
//    tesseract::TessBaseAPI* tesseractApi = Master::getInstance().mTesseractApiForNav.get();
//    if (!tesseractApi)
//        return 0;
//    cv::Mat rowImage(grayImage, rect);
//    int normConf = 0;
//    std::string normText;
//    if (!invert.has_value() || !invert.value()) {
//        tesseractApi->SetImage(rowImage.data, rowImage.cols, rowImage.rows, 1, rowImage.step);
//        char *outText = tesseractApi->GetUTF8Text();
//        normText = trim(outText);
//        normConf = tesseractApi->MeanTextConf();
//        delete[] outText;
//        if (normConf > 70) {
//            text = normText;
//            LOG(INFO) << "OCR Output: '" << normText << "' words conf=" << normConf << "%";
//            return normConf;
//        }
//    }
//
//    int invertedConf = 0;
//    std::string invertedText;
//    if (!invert.has_value() || invert.value()) {
//        int background = 127;
//        if (!invert.has_value()) {
//            // try hard - detect background, and if it's dark - threshold and invert the image
//            int histSize = 16;
//            float range[]{0, 256}; //the upper boundary is exclusive
//            const float *histRange[]{range};
//            cv::Mat hist;
//            cv::calcHist(&rowImage, 1, nullptr, cv::Mat(), hist, 1, &histSize, histRange);
//            int maxLoc[4]{};
//            cv::minMaxIdx(hist, nullptr, nullptr, nullptr, maxLoc);
//            background = (maxLoc[0]+1) * 16;
//            if (background > 127)
//                return 0;
//        }
//
//        cv::Mat invertedImage;
//        cv::bitwise_not(rowImage, invertedImage);
//        cv::Mat thrImage;
//        cv::threshold(invertedImage, thrImage, 255 - background, 255, cv::THRESH_BINARY);
//        tesseractApi->SetImage(thrImage.data, thrImage.cols, thrImage.rows, 1, thrImage.step);
//        char *outText = tesseractApi->GetUTF8Text();
//        invertedText = trim(outText);
//        invertedConf = tesseractApi->MeanTextConf();
//        delete[] outText;
//    }
//
//    if (normConf >= invertedConf) {
//        text = normText;
//        LOG(INFO) << "OCR Output: '" << normText << "' words conf=" << normConf << "% (as black-on-white)";
//        return normConf;
//    } else {
//        text = invertedText;
//        LOG(INFO) << "OCR Output: '" << invertedText << "' words conf=" << invertedConf << "% (as negative)";
//        return invertedConf;
//    }
//}

bool Master::setDetectStream(DetectLevel level) {
    mDetectLevelStream = level;
    mStreamFramePoints.clear();
    if (level != DetectLevel::None) {
        LOG(INFO) << "Detect stream started";
        mLoopWakeup = std::chrono::milliseconds(5);
        pushCommand(Command::NoOp);
    } else {
        LOG(INFO) << "Detect stream stopped";
        mLoopWakeup = std::chrono::milliseconds(500);
    }
    return true;
}
bool Master::detectEDState(DetectLevel level) {
    auto startTime = std::chrono::high_resolution_clock::now();

    mLastUIState.clear();
    // make screenshot
    if (!captureWindow(mClassifyEnv)) {
        LOG(ERROR) << "Cannot capture screen";
        mLastUIState.valid = false;
        return false;
    }
    //auto utc_now = std::chrono::utc_clock::now();
    //auto time_delta = std::chrono::duration_cast<std::chrono::milliseconds>(utc_now - mClassifyEnv.mFrame->timestamp);
    //LOG(INFO) << std::format("Captured frame delta: {}ms", time_delta.count());
    mLastUIState.timestamp = mClassifyEnv.mFrame->timestamp;
    mLastUIState.guiFocus = st::guiFocus;
    mLastUIState.valid = true;
    if (level == DetectLevel::None) {
        auto elapsedTime = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - startTime);
        LOG(DEBUG) << "Detected UI state: " << mLastUIState << " (took " << elapsedTime.count() << "us)";
        debugWindowUpdate();
        return true;
    }

    // detect screen and widget
    Widget::DetectParams params {mClassifyEnv, mLastUIState, *this, level};
    mScreensRoot->detect(params);
    if (!st::ship.flags.docked && !st::ship.flags.fsd_jump && st::guiFocus == GuiFocus::None) {
        // detect autopilot
        for (auto& cr : mClassifyEnv.classified) {
            if (cr.cdt == ClsDetType::Detected && cr.text == "auto-pilot") {
                mLastUIState.autopilot = !cr.detectedRect.empty();
                break;
            }
        }
        // detect compass
        if (!st::destination.name.empty()) {
            mCompassDetector->match(mClassifyEnv);
            CompassInfo compass {};
            compass.timestamp = mClassifyEnv.mFrame->timestamp;
            compass.hemisphere = mCompassDetector->lastHemisphere;
            compass.targetPitch = (float) mCompassDetector->lastTgtPitch;
            compass.targetYaw = (float) mCompassDetector->lastTgtYaw;
            compass.targetRoll = (float) mCompassDetector->lastTgtRoll;
            compass.targetAngle = (float) mCompassDetector->lastTgtAngle;
            compass.has_nav_target = mCompassDetector->navTargetFound;
            if (mCompassDetector->navTargetFound)
                compass.nav_target_dist = mCompassDetector->lastNavDist;
            st::compass = compass;

            ai::reportCompassDetect(compass);
        } else {
            mCompassDetector->clear();
            st::compass = {};
        }
    }

    auto elapsedTime = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - startTime);
    LOG(DEBUG) << "Detected UI state: " << mLastUIState << " (took " << elapsedTime.count() << "us)";
    debugWindowUpdate();
    return true;
}

cv::Point Master::cvtReferenceToDesktop(const cv::Point& point) const {
    return mClassifyEnv.cvtReferenceToDesktop(point);
}

Capturer* Master::getCapturer() {
    if (!hWndED) {
        HWND hWnd = FindWindow(ED_WINDOW_CLASS, ED_WINDOW_NAME);
        if (hWnd) {
            resetCapturer();
            hWndED = hWnd;
        }
    }
    if (!mCapturer) {
        mCapturer = Capturer::getEDCapturer(hWndED);
        if (!mCapturer)
            return nullptr;
        mCapturer->start();
    }
//    std::string error;
//    bool ok = Cfg.checkResolutionSupported(capturer->getCaptureRect().size(), error);
//    if (!ok) {
//        UIManager::showToast(_("Unsupported aspect ratio"), error);
//        return false;
//    }
    return mCapturer;
}

void Master::resetCapturer() {
    hWndED = nullptr;
    if (mCapturer) {
        mCapturer->stop();
        mCapturer = nullptr;
    }
}


bool Master::captureWindow(ClassifyEnv& env) {
    auto capturer = getCapturer();
    if (!capturer)
        return false;

    //if (!isForeground()) {
    //    LOG(WARNING) << "Elite Dangerous is not foreground; hWndED=" << std::format("{}", (void*)hWndED);
    //    //return false;
    //}

    upFrame recycle;
    env.mFrame.swap(recycle);
    recycle = capturer->capture(std::move(recycle));
    if (!recycle)
        return false;
    cv::Rect captureRect = capturer->getCaptureRect();
    cv::Rect monitorRect = capturer->getMonitorVirtualRect();
    env.init(monitorRect, captureRect, std::move(recycle));

    int fps_millis = mStreamFramePoints.size() < 10 ? 3000 : 1000;
    auto now = std::chrono::steady_clock::now();
    auto expired = now - std::chrono::milliseconds(fps_millis);
    while (!mStreamFramePoints.empty() && mStreamFramePoints.front() < expired)
        mStreamFramePoints.pop_front();
    mStreamFramePoints.push_back(now);

    return env.mFrame && env.mFrame->valid();
}

void Master::processDetectRequest(pCommand &cmd) {
    CommandDetectRequest *c = dynamic_cast<CommandDetectRequest *>(cmd.get());
    if (!c)
        return;
    try {
        bool ok = detectEDState(c->request.level);
        if (ok) {
            if (c->request.uiState)
                *c->request.uiState = mLastUIState;
            if (c->request.rEnv)
                *c->request.rEnv = mClassifyEnv;
            if (c->request.compass) {
                c->request.compass->timestamp = mLastUIState.timestamp;;
                c->request.compass->hemisphere = mCompassDetector->lastHemisphere;
                c->request.compass->targetPitch = (float) mCompassDetector->lastTgtPitch;
                c->request.compass->targetYaw = (float) mCompassDetector->lastTgtYaw;
                c->request.compass->targetRoll = (float) mCompassDetector->lastTgtRoll;
                c->request.compass->targetAngle = (float) mCompassDetector->lastTgtAngle;
                c->request.compass->has_nav_target = mCompassDetector->navTargetFound;
                if (mCompassDetector->navTargetFound)
                    c->request.compass->nav_target_dist = mCompassDetector->lastNavDist;
                else
                    c->request.compass->nav_target_dist = {};
            }
            if (c->request.colorImage) {
                if (mClassifyEnv.mWarpTransform && mClassifyEnv.mWarpTransform->valid)
                    *c->request.colorImage = toMat(mClassifyEnv.getWarpedColorImage()).clone();
                else
                    *c->request.colorImage = toMat(mClassifyEnv.getColorImage()).clone();
            }
            if (c->request.grayImage) {
                if (mClassifyEnv.mWarpTransform && mClassifyEnv.mWarpTransform->valid)
                    *c->request.grayImage = toMat(mClassifyEnv.getWarpedGrayImage()).clone();
                else
                    *c->request.grayImage = toMat(mClassifyEnv.getGrayImage()).clone();
            }
        }
        c->promise.set_value(ok);
    } catch (const std::exception& ex) {
        LOG(ERROR) << "Exception in processDetectRequest: " << ex.what() << std::endl << GET_EXCEPTION_STACK_TRACE;
        c->promise.set_value(false);
    }
}
