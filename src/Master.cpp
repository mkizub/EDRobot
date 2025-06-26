//
// Created by mkizub on 23.05.2025.
//

#include "pch.h"

#include "ui/UIManager.h"
#include "ai/AIManager.h"
#include "Keyboard.h"
#include "Template.h"
#include "Capturer.h"
#include "FuzzyMatch.h"
#include <memory>
#include <fstream>
#include <string>
#include <iterator>
#include "opencv2/core/utils/logger.hpp"
#include <CLI11/CLI11.hpp>
#include <magic_enum/magic_enum.hpp>

#include <tesseract/baseapi.h>
#include <leptonica/allheaders.h>

#ifndef NDEBUG
//#include <cpptrace/cpptrace.hpp>
#include "cpptrace/from_current.hpp"
#include "Master.h"

#endif

#ifdef CPPTRACE_TRY
# define TRY CPPTRACE_TRY
# define CATCH(param) CPPTRACE_CATCH(param)
# define GET_EXCEPTION_STACK_TRACE cpptrace::from_current_exception().to_string()
#else
# define TRY try
# define CATCH(param) catch(param)
# ifdef _GLIBCXX_HAVE_STACKTRACE
#  include <stacktrace>
#  define GET_EXCEPTION_STACK_TRACE std::stacktrace::current()
# else
#  define GET_EXCEPTION_STACK_TRACE "(stack trace unavailable)"
# endif
#endif


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
    widget = nullptr;
    focused = nullptr;
}

const std::string& UIState::path() const {
    static std::string empty;
    if (widget)
        return widget->path;
    return empty;
}

std::ostream& operator<<(std::ostream& os, const UIState& obj) {
    switch (obj.guiFocus) {
    default:
        os << "Unknown::";
        break;
    case GuiFocus::None:
        os << "Cockpit::";
        break;
    case GuiFocus::Right:
        os << "RightPanel::";
        break;
    case GuiFocus::Left:
        os << "LeftPanel::";
        break;
    case GuiFocus::Chat:
        os << "Chat::";
        break;
    case GuiFocus::Role:
        os << "LowPanel::";
        break;
    case GuiFocus::Services:
        os << "Services::";
        if (!obj.widget) {
            os << "unknown";
            return os;
        }
        os << obj.widget->path;
        if (obj.focused) {
            os << "@" << obj.focused->name;
        }
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
    options.add_flag("--kwd,--keep-working-dir", kwd, "Keep working directory (do not change on start)");
    options.add_option("--ocr-dir,--tesseract-dir", ocr_dir, "Tesseract OCR data directory");

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

    TRY {
        initializeInternal(ocr_dir);
    } CATCH(const std::exception& e) {
        LOG(ERROR) << "Exception in initialization: " << GET_EXCEPTION_STACK_TRACE;
        return 1;
    }

    return 0;
}
void Master::initializeInternal(std::string ocr_dir) {

    //cv::utils::logging::internal::replaceWriteLogMessage(writeOpenCVLogMessageFunc);
    //cv::utils::logging::internal::replaceWriteLogMessageEx(writeOpenCVLogMessageFuncEx);

    LOG(INFO) << "Initializing UI";
    UIManager::initialize();
    LOG(INFO) << "Initializing OpenCV";

    mConfiguration = new Configuration();
    mConfiguration->load();
    initButtonStateDetector();

    mAIManager = new ai::AIManager();

    if (ocr_dir.empty())
        ocr_dir = mConfiguration->mTesseractDataPath;
    if (ocr_dir.empty())
        ocr_dir = "tessdata-fast";

    {
        LOG(INFO) << "Initializing Tesseract OCR for commodities";
        const char* tesseractLang = "eng";
        if (mConfiguration->lng == RU)
            tesseractLang = "edo";
        mTesseractApiForMarket = std::make_unique<tesseract::TessBaseAPI>();
        const std::vector<std::string> vars_vec{"user_words_file"};
        const std::vector<std::string> vars_values{mConfiguration->makeTesseractWordsFile()};
        // "eng" for English language
        if (mTesseractApiForMarket->Init(ocr_dir.c_str(), tesseractLang, tesseract::OEM_LSTM_ONLY,
                                         nullptr, 0, &vars_vec, &vars_values, true)) {
            LOG(ERROR) << "Error: Could not initialize tesseract.";
            mTesseractApiForMarket.reset();
        } else {
            mTesseractApiForMarket->SetPageSegMode(tesseract::PSM_SINGLE_LINE);
        }
    }

    LOG(INFO) << "Initializing compass detector";
    cv::Mat compassImage;
    spEvalRect compassRect = std::make_shared<ConstRect>(cv::Rect(679,803,71,71));
    mCompassDetector = std::make_unique<CompassDetector>(compassImage, compassRect);

    LOG(INFO) << "Setup keyboard hooks";
    std::vector<std::string> keys;
    for (auto& m : mConfiguration->keyMapping)
        keys.push_back(m.first.first);
    keyboard::intercept(keys);
    keyboard::start(tradingKbHook);
}


void Master::setCalibrationResult(const std::array<cv::Vec3b,4>& buttonBGR, const std::array<cv::Vec3b,4>& lstRowBGR) {
    mConfiguration->setCalibrationResult(buttonBGR, lstRowBGR);
    initButtonStateDetector();
}
void Master::initButtonStateDetector() {
    //std::vector<cv::Vec3b> buttonColors(mConfiguration->mCalcButtonLuv.begin(), mConfiguration->mCalcButtonLuv.end());
    std::vector<cv::Vec3b> buttonColors(mConfiguration->mCalibratedButtonLuv.begin(), mConfiguration->mCalibratedButtonLuv.end());
    mButtonStateDetector.reset(new HistogramTemplate(HistogramTemplate::CompareMode::Luv, cv::Rect(), buttonColors));
    LOG(INFO) << "Button state detector installed";
    //std::vector<cv::Vec3b> lstRowColors(mConfiguration->mCalcLstRowLuv.begin(), mConfiguration->mCalcLstRowLuv.end());
    std::vector<cv::Vec3b> lstRowColors(mConfiguration->mCalibratedLstRowLuv.begin(), mConfiguration->mCalibratedLstRowLuv.end());
    mLstRowStateDetector.reset(new HistogramTemplate(HistogramTemplate::CompareMode::Luv, cv::Rect(), lstRowColors));
    LOG(INFO) << "List row state detector installed";
}


Master::Master() {
    mScreensRoot = std::make_unique<widget::Root>();
    mLoopWakeup = std::chrono::milliseconds(1000);
}

Master::~Master() {
    keyboard::stop();
    delete mAIManager;
    delete mConfiguration;
    if (mCapturer) {
        mCapturer->stop();
        mCapturer = nullptr;
    }
    if (mTesseractApiForMarket) {
        mTesseractApiForMarket->End();
        mTesseractApiForMarket.reset();
    }
}


void Master::loop() {
    TRY {
        LOG(INFO) << "Starting EDRobot task loop";
        bool shutdown = false;
        while (!shutdown) {
            pCommand cmd;
            popCommand(cmd);
            switch (cmd->command) {
            case Command::NoOp:
                debugWindowUpdate();
                break;
            case Command::TaskFinished:
                //clearCurrentTask();
                break;
            case Command::Start:
                startTrade();
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
            case Command::UserNotify:
                showNotification(cmd);
                break;
            case Command::Calibrate:
                startCalibration();
                break;
            case Command::DebugTemplates:
                debugTemplates(nullptr, nullptr);
                break;
            case Command::DebugButtons:
                debugButtons();
                break;
            case Command::DebugFindAllCommodities:
                debugFindAllCommodities();
                break;
            case Command::DebugCompass:
                debugCompass();
                break;
            case Command::DebugWindow:
                debugWindow();
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
        LOG(ERROR) << "Exception in main loop: " << GET_EXCEPTION_STACK_TRACE;
        //clearCurrentTask();
    }
}

bool Master::isForeground() {
    return hWndED && hWndED == GetForegroundWindow();
}

void Master::tradingKbHook(int code, int scancode, int flags, const std::string& name) {
    (void)code;
    (void)scancode;
    LOG(DEBUG) << "Key '"+encodeShortcut(name,flags)+"' pressed";
    Master& self = getInstance();
    auto keyMapping = self.mConfiguration->keyMapping;
    auto cmd = keyMapping.find(std::make_pair(toLower(name),flags));
    if (cmd != keyMapping.end()) {
        LOG(INFO) << "Command " << enum_name(cmd->second) << " by key '"+encodeShortcut(name,flags)+"' pressed";
        self.pushCommand(cmd->second);
    } else {
        if (self.mConfiguration->autoPause && self.mAIManager->active()) {
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

struct CommandNotify : public CommandEntry {
    CommandNotify(bool error, int timeout, const std::string& title, const std::string& text)
        : CommandEntry(Command::UserNotify)
        , error(error)
        , timeout(timeout)
        , title(title)
        , text(text)
        {}
    ~CommandNotify() override = default;
    const bool error;
    const int timeout;
    const std::string title;
    const std::string text;
};

void Master::notifyProgress(const std::string& title, const std::string& text) {
    LOG(INFO) << title << ": " << text;
    UIManager::showToast(title, text);
}
void Master::notifyError(const std::string& title, const std::string& text) {
    LOG(ERROR) << title << ": " << text;
    UIManager::showToast(title, text);
}

struct CommandDetectRequest : public CommandEntry {
    CommandDetectRequest(std::promise<UIState>&& p, DetectLevel level)
            : CommandEntry(Command::DetectRequest)
            , promise(std::move(p))
            , level(level)
    {}
    ~CommandDetectRequest() override = default;
    std::promise<UIState> promise;
    DetectLevel level;
};

void Master::pushDetectRequest(std::promise<UIState>&& p, DetectLevel level) {
    pushCommand(new CommandDetectRequest(std::move(p), level));
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

void Master::showNotification(pCommand& cmd) {
    CommandNotify* c = dynamic_cast<CommandNotify*>(cmd.get());
    if (c)
        UIManager::showToast(c->title, c->text);
}

bool Master::preInitTask(bool checkCalibration) {
    auto capturer = getCapturer();
    if (!capturer)
        return false;
    bool ok = mConfiguration->checkResolutionSupported(capturer->getCaptureRect().size());
    if (!ok)
        return false;
    if (checkCalibration && mConfiguration->checkNeedColorCalibration()) {
        bool agree = UIManager::askCalibrationDialog(_("Color calibration required"));
        if (agree) {
            pushCommand(Command::Calibrate);
            return false;
        }
    }

    Sleep(200); // wait for dialog to dissappear
    SetForegroundWindow(hWndED);
    Sleep(200); // wait for switching to foreground
    if (!isForeground()) {
        LOG(ERROR) << "ED is not foreground";
        return false;
    }
    return true;
}

bool Master::startCalibration() {
    if (!preInitTask(false))
        return false;
    LOG(INFO) << "Staring calibration task";
    mAIManager->new_task(std::make_unique<ai::TaskCalibrate>(*mAIManager));
    return true;
}


const Commodity* Master::getLabelCommodity(const std::string& lbl_name) {
    Widget* widget = getCfgItem(lbl_name);
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
    for (auto& it : mClassifyEnv.classified) {
        if (it.cdt == ClsDetType::Widget && it.u.widg.widget == widget) {
            cr = &it;
            break;
        }
    }
    if (!cr) {
        LOG(ERROR) << "Label '" << lbl_name << "' was not detected on screen";
        return nullptr;
    }


    cv::Rect rect = mClassifyEnv.cvtReferenceToCaptured(cr->detectedRect);

    std::string text;
    int ocr_conf = 0;
    if (!lbl->row_height.has_value() || lbl->row_height.value() <= 0) {
        ocr_conf = ocrMarketText(mClassifyEnv.getGrayImage(), rect, text, lbl->invert);
    } else {
        int row_height = mClassifyEnv.scaleToCaptured(cv::Size(0,lbl->row_height.value())).height;
        int lines = (int)std::round(double(rect.height) / double(row_height));
        row_height = (int)std::round(rect.height / lines);
        int ocr_conf_sum = 0;
        for (int l=0; l < lines; l++) {
            cv::Rect r (rect.x, rect.y+l*row_height, rect.width, row_height);
            std::string line;
            ocr_conf_sum += ocrMarketText(mClassifyEnv.getGrayImage(), r, line, lbl->invert);
            if (l > 0)
                text += " ";
            text += line;
        }
        ocr_conf = ocr_conf_sum / lines;
    }
    const Commodity* commodity = nullptr;
    if (ocr_conf > 30) {
        LOG(DEBUG) << "Label text OCR: '" << text << "' conf=" << ocr_conf << "%";
        commodity = mConfiguration->getCommodityByName(text, true);
    }
    LOG_IF(!commodity,ERROR) << "Commodity '" << text << "' not found";
    return commodity;
}

const ClassifiedRect* Master::getFocusedRow(const std::string& lst_name) {
    for (auto& it : mClassifyEnv.classified) {
        if (it.cdt != ClsDetType::ListRow)
            continue;
        if (it.u.lrow.ws == WState::Focused && it.u.lrow.list && it.u.lrow.list->name == lst_name)
            return &it;
    }
    return nullptr;
}

int Master::canSell(Commodity* commodity) const {
    if (!commodity)
        return 0;
    if (commodity->ship.count <= commodity->ship.stolen)
        return 0;
    spMarket market = mConfiguration->currentMarket.load();
    if (!market)
        return 0;
    if (market->stationType == "FleetCarrier") {
        return std::min(commodity->ship.count, commodity->market.demand);
    }
    return commodity->ship.count - commodity->ship.stolen;
}

const Commodity* Master::ocrMarketRowCommodity(ClassifiedRect* cr) {
    if (cr->cdt != ClsDetType::ListRow)
        return nullptr;
    if (cr->u.lrow.commodity)
        return cr->u.lrow.commodity;
    if (cr->text.empty()) {
        cv::Rect rect = mClassifyEnv.cvtReferenceToCaptured(cr->detectedRect);
        if (ocrMarketText(mClassifyEnv.getGrayImage(), rect, cr->text) > 50) {
            cr->u.lrow.commodity = mConfiguration->getCommodityByName(cr->text, true);
        } else {
            cr->text.clear();
        }
    } else {
        cr->u.lrow.commodity = mConfiguration->getCommodityByName(cr->text, true);
    }
    return cr->u.lrow.commodity;
}

bool Master::approximateListOfCommodities(const std::string& lst_name, const std::vector<Commodity*>& table, std::vector<CommodityMatch>* verify) {
    std::vector<ClassifiedRect*> rows;
    for (auto& cr : mClassifyEnv.classified) {
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
            lr->u.lrow.commodity = mConfiguration->getCommodityByName(lr->text, true);
        if (lr->u.lrow.commodity && !first)
            first = lr;
        if (lr->u.lrow.commodity)
            last = lr;
    }
    for (auto lr : rows) {
        if (lr->u.lrow.commodity == nullptr && !lr->text.empty())
            lr->u.lrow.commodity = mConfiguration->getCommodityByName(lr->text, true);
    }
    if (!first) {
        for (auto lr: rows) {
            if (ocrMarketRowCommodity(lr)) {
                first = lr;
                break;
            }
        }
        for (auto lr : rows | std::views::reverse) {
            if (ocrMarketRowCommodity(lr)) {
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

    if (it_table_last - it_table_first != it_row_last - it_row_first) {
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
            cv::Rect rect = mClassifyEnv.cvtReferenceToCaptured(lr->detectedRect);
            std::string text;
            int ocr_conf = ocrMarketText(mClassifyEnv.getGrayImage(), rect, text);
            int fuzzy_conf = 0;
            if (ocr_conf > 30) {
                std::string match;
                if (text == lr->u.lrow.commodity->name) {
                    match = "EXACT";
                    fuzzy_conf = 100;
                } else {
                    fuzzy_conf = (int)matcher.ratio(toUtf16(text), lr->u.lrow.commodity->wide);
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

bool Master::startTrade() {
//    if (!mConfiguration->loadMarket()) {
//        UIManager::showToast(_("Cannot sell"), _("Cannot load market"));
//        return false;
//    }


//    std::string cargo_name;
//    detectEDState(DetectLevel::ListOcrFocusedRow);
//    if (isEDStateMatch("scr-market:mod-sell")) {
//        auto row = getFocusedRow("lst-goods");
//        if (row) {
//            Commodity* c = mConfiguration->getCommodityByName(row->text, true);
//            if (c)
//                cargo_name = c->name;
//        }
//    }

    int total = 0;
    int chunk = mSellChunk;
    Commodity* commodity = nullptr;
    bool res = UIManager::askSellInput(total, chunk, commodity);
    if (!res || total <= 0 || chunk <= 0)
        return false;
    mSellChunk = chunk;

//    spShipCargo shipCargo = mConfiguration->getCurrentCargo();
//    if (!shipCargo || shipCargo->count <= 0) {
//        UIManager::showToast(_("Cannot sell"), _("Ship cargo empty"));
//        return false;
//    }

    if (!preInitTask())
        return false;

//    if (!cargo_name.empty()) {
//        commodity = mConfiguration->getCommodityByName(cargo_name, false);
//        LOG(INFO) << "Internal error, cannot find cargo for: " << cargo_name;
//        return false;
//    }

    LOG(INFO) << "Staring new trade task";
    if (commodity) {
        mAIManager->new_task(std::make_unique<ai::TaskSell>(nullptr, *mAIManager, commodity, total, chunk));
    } else {
        mAIManager->new_task(std::make_unique<ai::TaskSellAll>(nullptr, *mAIManager, chunk));
    }
    return true;
}

bool Master::pauseAITask() {
    mAIManager->interrupt();
    return true;
}

bool Master::resumeAITask() {
    mAIManager->resume();
    return true;
}

bool Master::stopAITask() {
    mAIManager->interrupt();
    return true;
}

bool Master::debugFindAllCommodities() {
    if (!preInitTask())
        return false;
    LOG(INFO) << "Staring new debug task";
    mAIManager->new_task(std::make_unique<ai::TaskDebugFindAllCommodities>(*mAIManager));
    return true;
}

const json5pp::value& Master::getTaskActions(const std::string& action) {
    auto it = mActions.find(action);
    if (it == mActions.end()) {
        static json5pp::value nullAction;
        return nullAction;
    }
    return it->second;
}

cv::Rect Master::resolveWidgetReferenceRect(const std::string& name) {
    Widget* item = getCfgItem(name);
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

Widget* Master::getCfgItem(std::string state) {
    if (state.empty())
        return nullptr;
    auto names = parseState(state);
    if (names.size() == 1 && !state.starts_with("scr-"))
        names = parseState(mLastEDState.path() + ":" + state);
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
    return item->oracle->classify(mClassifyEnv);
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

bool Master::debugTemplates(Widget* item, ClassifyEnv* env) {
    if (!env) {
        ClassifyEnv debugEnv;
        if (!captureWindow(debugEnv)) {
            LOG(ERROR) << "Cannot capture screen for debug match";
            return false;
        }
        GuiFocus guiFocus = mConfiguration->guiFocus;
        for (auto widget: mScreensRoot->have) {
            if (widget->tp != WidgetType::Screen)
                continue;
            widget::Screen* screen = static_cast<widget::Screen*>(widget);
            if (!screen || (screen->gui.has_value() && screen->gui != guiFocus))
                continue;
            debugTemplates(screen, &debugEnv);
            el::Loggers::flushAll();
            //std::string fname = "debug-match-"+screen->name+".png";
            //cv::imwrite(fname, env.debugImage);
            //cv::imshow(fname, debugEnv.getDebugImage());
            //cv::waitKey();
        }
        //cv::destroyAllWindows();
        mDuplicateToDebugWindow = UIManager::showDebugWindow();
        if (mDuplicateToDebugWindow) {
            if (!UIManager::postToDebugWindow(debugEnv.getColorImage(), debugEnv.getDebugImage()))
                mDuplicateToDebugWindow = false;
        }
        debugEnv.clear();
        el::Loggers::flushAll();
        return true;
    } else {
        if (debugMatchItem(item, *env)) {
            bool ok = false;
            for (Widget* i : item->have) {
                ok |= debugTemplates(i, env);
            }
            return ok;
        }
        return false;
    }
}

static const int USE_EROSION = 0;

void Master::drawButton(widget::Widget* item) {
    if (!(item->tp == WidgetType::Button || item->tp == WidgetType::TileBtn ||
          item->tp == WidgetType::Spinner || item->tp == WidgetType::Label ||
          item->tp == WidgetType::List))
        return;
    cv::Rect rect = mClassifyEnv.calcCapturedRect(item->rect);
    if (rect.empty())
        return;
    cv::Mat& debugImage = mClassifyEnv.getDebugImage();
    cv::Scalar color(200, 80, 80);
    int size = (item == mLastEDState.focused) ? 2 : 1;
    cv::rectangle(debugImage, rect.tl(), rect.br(), color, size);
    if (item->tp == WidgetType::Button || item->tp == WidgetType::TileBtn)
        return;
    if (item->tp == WidgetType::Spinner) {
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
    if (item->tp == WidgetType::Label) {
        Label* lbl = (Label*)item;
        std::string text;
        int ocr_conf = 0;
        if (!lbl->row_height.has_value() || lbl->row_height.value() <= 0) {
            ocr_conf = ocrMarketText(mClassifyEnv.getGrayImage(), rect, text, lbl->invert);
        } else {
            int row_height = mClassifyEnv.scaleToCaptured(cv::Size(0,lbl->row_height.value())).height;
            int lines = (int)std::round(double(rect.height) / double(row_height));
            row_height = (int)std::round(rect.height / lines);
            int ocr_conf_sum = 0;
            for (int l=0; l < lines; l++) {
                cv::Rect r (rect.x, rect.y+l*row_height, rect.width, row_height);
                std::string line;
                ocr_conf_sum += ocrMarketText(mClassifyEnv.getGrayImage(), r, line, lbl->invert);
                if (l > 0)
                    text += " ";
                text += line;
                cv::line(debugImage, cv::Point(r.x, r.y+r.height), r.br(), color, size);
            }
            ocr_conf = ocr_conf_sum / lines;
        }
        //std::wstring wide = toUtf16(text);
        //if (ocr_conf > 30) {
            LOG(ERROR) << "Label text OCR: '" << text << "' conf=" << ocr_conf << "%";
            auto commodity = mConfiguration->getCommodityByName(text, true);
            LOG(ERROR) << "Label commodity: " << (commodity ? commodity->name : "(not found)");
        //} else {
        //    LOG(ERROR) << "Label text not recognized, ocr: '" << text << "' conf=" << ocr_conf << "%";
        //}
        return;
    }
    if (item->tp == WidgetType::List) {
        List* lst = static_cast<List*>(item);
        ////////////////////////////////////////////////
        unsigned buttonGrayColor = mConfiguration->getLstRowGrayColor(WState::Normal);
        cv::Mat listImage(mClassifyEnv.getGrayImage(), rect);

        //cv::imshow("List image: " + mLastEDState.path(), listImage);

        cv::Mat thrImage;
        cv::Mat erodedImage;
        cv::threshold(listImage, thrImage, buttonGrayColor - 2, 255, cv::THRESH_BINARY);
        //cv::imshow("Threshold image: " + mLastEDState.path(), thrImage);
        if (USE_EROSION > 0) {
            cv::Mat kernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(3, 3));
            cv::erode(thrImage, erodedImage, kernel, cv::Point(-1, -1), USE_EROSION, cv::BORDER_CONSTANT, cv::Scalar::all(0));
            //cv::imshow("Eroded image: " + mLastEDState.path(), erodedImage);
        } else {
            erodedImage = thrImage;
        }

        auto expected_row_height = mClassifyEnv.scaleToCaptured(cv::Size(0, lst->row_height)).height;
        double minArea =  rect.width * expected_row_height * 0.75;

        std::vector<cv::Rect> detectedRows;
        std::vector<std::vector<cv::Point>> contours;
        cv::findContoursLinkRuns(erodedImage, contours);
        for (const auto &contour: contours) {
            std::vector<cv::Point> convex;
            cv::convexHull(contour, convex);
            if (convex.size() >= 4) {
                std::vector<cv::Point> approx;
                cv::approxPolyN(convex, approx, 4, 5, true);
                if (cv::contourArea(approx) > minArea) {
                    cv::Rect bbox = cv::boundingRect(approx);
                    if (bbox.height < expected_row_height * 1.5) {
                        detectedRows.push_back(bbox);
                        cv::rectangle(debugImage, bbox + rect.tl(), cv::Scalar(0, 255, 0), 1);
                    } else {
                        int count = (int)std::floor(0.1 + double(bbox.height) / double(expected_row_height));
                        int split_height = bbox.height / count;
                        for (int i=0; i < count; i++) {
                            cv::Rect bb(bbox.x, bbox.y+(i*split_height), bbox.width, split_height-2);
                            detectedRows.push_back(bb);
                            cv::rectangle(debugImage, bb + rect.tl(), cv::Scalar(0, 255, 0), 1);
                        }
                    }
                }
            }
        }

        //cv::imshow("Contour image: " + mLastEDState.path(), debugImage);

        //cv::waitKey();
        //cv::destroyAllWindows();
    }
}

bool Master::debugButtons() {
    detectEDState(DetectLevel::ListRows);
    const widget::Widget* widget = mLastEDState.widget;
    if (widget) {
        for (Widget* item : widget->have)
            drawButton(item);
        if (widget->tp == WidgetType::Mode) {
            for (Widget *item: widget->parent->have)
                drawButton(item);
        }
    }
    mDuplicateToDebugWindow = UIManager::showDebugWindow();
    if (mDuplicateToDebugWindow) {
        if (!UIManager::postToDebugWindow(mClassifyEnv.getColorImage(), mClassifyEnv.getDebugImage()))
            mDuplicateToDebugWindow = false;
    }
    return true;
}

bool Master::debugCompass() {
    ClassifyEnv debugEnv;
    if (!captureWindow(debugEnv)) {
        LOG(ERROR) << "Cannot capture screen to debug compass";
        return false;
    }
    mCompassDetector->debugMatch(debugEnv);
    mDuplicateToDebugWindow = UIManager::showDebugWindow();
    if (mDuplicateToDebugWindow) {
        if (!UIManager::postToDebugWindow(debugEnv.getColorImage(), debugEnv.getDebugImage()))
            mDuplicateToDebugWindow = false;
    }
    return true;
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
    if (!hWndED)
        return false;
    mDuplicateToDebugWindow = UIManager::hasDebugWindow();
    if (!mDuplicateToDebugWindow)
        return false;
    if (!captureWindow(mClassifyEnv)) {
        LOG(ERROR) << "Cannot capture window";
        return false;
    }
    return UIManager::postToDebugWindow(mClassifyEnv.getColorImage());
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
    if (!isForeground()) {
        SetForegroundWindow(hWndED);
        Sleep(200);
        if (!isForeground()) {
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

    cv::Vec4b color = debugEnv.getColorImage().at<cv::Vec4b>( (rect.tl() + rect.br())/2 );
    LOG(INFO) << "Selected rect BGRA color (center dot): " << color;

    cv::imwrite("debug-rect-gray.png", cv::Mat(debugEnv.getGrayImage(), rect));
    cv::imwrite("debug-rect-color.png", cv::Mat(debugEnv.getColorImage(), rect));

//    std::string text;
//    if (Master::ocrMarketText(debugEnv.getGrayImage(), rect, text) > 50) {
//        Commodity *commodity = mConfiguration->getCommodityByName(text, true);
//        if (commodity) {
//            if (mConfiguration->lng != EN)  // they capitalize text
//                text = commodity->name;
//            std::string lng;
//            if (mConfiguration->lng == RU)
//                lng = "-rus";
//            else if (mConfiguration->lng == EN)
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

WState Master::detectButtonState(const widget::Widget* item) {
    if (!item)
        return WState::Unknown;
    cv::Rect r = mClassifyEnv.calcReferenceRect(item->rect);
    if (r.empty())
        return WState::Unknown;
    mClassifyEnv.classified.emplace_back(ClsDetType::Widget, item->name, r);

    int sz = int(16 * mClassifyEnv.getScale());
    int x, y;
    if (r.width > 9*sz || item->tp == WidgetType::Spinner) {
        x = r.x + r.width - 2*sz;
        y = r.y + r.height / 2 - sz/2;
        if (item->tp == WidgetType::Spinner)
            x -= r.height + sz;
    } else {
        x = r.x + r.width - sz - 2;
        y = r.y + r.height / 2 - sz/2;
    }
    ClassifiedRect& clsBtnRect = mClassifyEnv.classified.back();
    clsBtnRect.u.widg.widget = item;
    mButtonStateDetector->mRect = cv::Rect(cv::Point(x,y), cv::Size(sz,sz));
    mButtonStateDetector->classify(mClassifyEnv);
    auto& values = mButtonStateDetector->mLastValues;
    int idx = int(std::max_element(values.begin(), values.end()) - values.begin());
    double value = values[idx];
    WState ws = WState::Unknown;
    if (value > 0.80) {
        ws = enum_cast<WState>(idx).value();
        clsBtnRect.u.widg.ws = ws;
        LOG_IF(ws == WState::Focused, INFO) << "Focused: " << item->name;
        LOG_IF(ws == WState::Disabled, INFO) << "Disabld: " << item->name;
    }
    return ws;
}

void Master::detectListState(const widget::List* lst, DetectLevel level) {
    if (!lst)
        return;
    cv::Rect listReferenceRect = mClassifyEnv.calcReferenceRect(lst->rect);
    cv::Rect rect =  mClassifyEnv.cvtReferenceToCaptured(listReferenceRect);
    if (rect.empty())
        return;

    mClassifyEnv.classified.emplace_back(ClsDetType::Widget, lst->name, listReferenceRect);
    ClassifiedRect& clsListRect = mClassifyEnv.classified.back();
    clsListRect.u.widg.widget = lst;

    unsigned buttonGrayColor = mConfiguration->getLstRowGrayColor(WState::Normal);
    cv::Mat grayImage(mClassifyEnv.getGrayImage(), rect);

    cv::Mat thrImage;
    cv::threshold(grayImage, thrImage, buttonGrayColor - 2, 255, cv::THRESH_BINARY);
    cv::Mat erodedImage;
    if (USE_EROSION > 0) {
        cv::Mat kernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(3, 3));
        cv::erode(thrImage, erodedImage, kernel, cv::Point(-1, -1), USE_EROSION, cv::BORDER_CONSTANT, cv::Scalar::all(0));
    } else {
        erodedImage = thrImage;
    }

    auto expected_row_height = mClassifyEnv.scaleToCaptured(cv::Size(0, lst->row_height)).height;
    double minArea =  rect.width * (expected_row_height-2*USE_EROSION) * 0.75;

    std::vector<cv::Rect> detectedRows;
    std::vector<std::vector<cv::Point>> contours;
    cv::findContoursLinkRuns(erodedImage, contours);
    for (const auto &contour: contours) {
        std::vector<cv::Point> convex;
        cv::convexHull(contour, convex);
        if (convex.size() >= 4) {
            std::vector<cv::Point> approx;
            cv::approxPolyN(convex, approx, 4, 5, true);
            if (cv::contourArea(approx) > minArea) {
                cv::Rect bbox = cv::boundingRect(approx);
                if (bbox.height < expected_row_height * 1.5) {
                    detectedRows.push_back(bbox);
                } else {
                    int count = (int)std::floor(0.1 + double(bbox.height) / double(expected_row_height));
                    int split_height = bbox.height / count;
                    for (int i=0; i < count; i++) {
                        cv::Rect bb(bbox.x, bbox.y+(i*split_height), bbox.width, split_height-1);
                        detectedRows.push_back(bb);
                    }
                }
            }
        }
    }

    for (auto& r : detectedRows) {
        WState ws = WState::Unknown;
        int x = rect.x + r.x + r.width - 36;
        int y = rect.y + r.y + r.height / 2 - 8;
        mLstRowStateDetector->mRect = mClassifyEnv.cvtCapturedToReference(cv::Rect(cv::Point(x,y), cv::Size(16,16)));
        mLstRowStateDetector->classify(mClassifyEnv);
        auto& values = mLstRowStateDetector->mLastValues;
        int idx = int(std::max_element(values.begin(), values.end()) - values.begin());
        double value = values[idx];
        if (value > 0.90)
            ws = enum_cast<WState>(idx).value();

        std::string text;
        if (mTesseractApiForMarket && ws == WState::Focused && level >= DetectLevel::ListOcrFocusedRow) {
            ocrMarketText(grayImage, r, text);
        }

        cv::Rect rowReferenceRect = mClassifyEnv.cvtCapturedToReference(r+rect.tl());
        mClassifyEnv.classified.emplace_back(ClsDetType::ListRow, text, rowReferenceRect);
        ClassifiedRect& clsRowRect = mClassifyEnv.classified.back();
        clsRowRect.u.lrow.list = lst;
        clsRowRect.u.lrow.ws = ws;
        if (ws == WState::Focused)
            clsListRect.u.widg.ws = WState::Focused;
    }
}

int Master::ocrMarketText(const cv::Mat& grayImage, cv::Rect rect, std::string& text, std::optional<bool> invert) {
    text.clear();
    if (!mTesseractApiForMarket)
        return 0;
    cv::Mat rowImage(grayImage, rect);
    int outConf = 0;
    if (!invert.has_value() || !invert.value()) {
        mTesseractApiForMarket->SetImage(rowImage.data, rowImage.cols, rowImage.rows, 1, rowImage.step);
        char *outText = mTesseractApiForMarket->GetUTF8Text();
        text = trim(outText);
        outConf = mTesseractApiForMarket->MeanTextConf();
        delete[] outText;
        if (outConf > 30) {
            LOG(INFO) << "OCR Output: '" << text << "' words conf=" << outConf << "%";
            return outConf;
        }
    }

    if (!invert.has_value() || invert.value()) {
        // try hard - detect background, and if it's dark - threshold and invert the image
        int histSize = 256;
        float range[]{0, 256}; //the upper boundary is exclusive
        const float *histRange[]{range};
        cv::Mat hist;
        cv::calcHist(&rowImage, 1, nullptr, cv::Mat(), hist, 1, &histSize, histRange);
        int maxLoc[4]{};
        cv::minMaxIdx(hist, nullptr, nullptr, nullptr, maxLoc);
        int background = maxLoc[0] + 5;
        if (background > 127)
            return 0;

        cv::Mat invertedImage;
        cv::bitwise_not(rowImage, invertedImage);
        cv::Mat thrImage;
        cv::threshold(invertedImage, thrImage, 255 - background, 255, cv::THRESH_BINARY);
        mTesseractApiForMarket->SetImage(thrImage.data, thrImage.cols, thrImage.rows, 1, thrImage.step);
        char *outText = mTesseractApiForMarket->GetUTF8Text();
        text = trim(outText);
        outConf = mTesseractApiForMarket->MeanTextConf();
        delete[] outText;
        if (outConf > 30) {
            LOG(INFO) << "OCR Output: '" << text << "' words conf=" << outConf << "% (retried with negative)";
            return outConf;
        }
    }
    return outConf;
}

Widget* Master::detectAllButtonsStates(const widget::Widget* parent, DetectLevel level) {
    if (!parent)
        return nullptr;
    widget::Widget* focused = nullptr;
    for (Widget* item : parent->have) {
        if (item->tp == WidgetType::Button || item->tp == WidgetType::TileBtn || item->tp == WidgetType::Spinner) {
            WState ws = detectButtonState(item);
            if (ws == WState::Focused) {
                if (!focused)
                    focused = item;
            }
        }
        if (item->tp == WidgetType::Label) {
            cv::Rect r = mClassifyEnv.calcReferenceRect(item->rect);
            mClassifyEnv.classified.emplace_back(ClsDetType::Widget, item->name, r);
            mClassifyEnv.classified.back().u.widg.widget = item;
        }
        if (item->tp == WidgetType::List && level >= DetectLevel::ListRows) {
            detectListState(dynamic_cast<List*>(item), level);
        }
    }
    widget::Widget* parent_focused = nullptr;
    if (parent->tp == WidgetType::Mode) {
        parent_focused = detectAllButtonsStates(parent->parent, level);
    }
    if (focused)
        return focused;
    return parent_focused;
}
const UIState& Master::detectEDState(DetectLevel level) {
    mLastEDState.clear();
    // make screenshot
    if (!captureWindow(mClassifyEnv)) {
        LOG(ERROR) << "Cannot capture screen";
        mLastEDState.valid = false;
        return mLastEDState;
    }
    mLastEDState.valid = true;
    mLastEDState.guiFocus = mConfiguration->getGuiFocus();
    if (level == DetectLevel::None)
        return mLastEDState;

    // detect screen and widget
    GuiFocus guiFocus = mConfiguration->guiFocus;
    for (auto widget: mScreensRoot->have) {
        if (widget->tp != WidgetType::Screen)
            continue;
        widget::Screen* screen = static_cast<widget::Screen*>(widget);
        if (!screen || (screen->gui.has_value() && screen->gui != guiFocus))
            continue;
        Widget *subItem = matchWithSubItems(screen);
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
    if (level >= DetectLevel::Buttons) {
        if (mButtonStateDetector) {
            // detect focused button
            const Widget *focused = detectAllButtonsStates(mLastEDState.widget, level);
            if (focused) {
                mLastEDState.focused = focused;
                LOG(DEBUG) << "Detected focused button: " << focused->name;
            } else {
                LOG(DEBUG) << "Focused button not detected";
            }
        } else {
            LOG(ERROR) << "Colors not calibrated, cannot detect focused widget";
        }
    }

    LOG(INFO) << "Detected UI state: " << mLastEDState;
    return mLastEDState;
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
    cv::Rect captureRect = capturer->getCaptureRect();
    cv::Rect monitorRect = capturer->getMonitorVirtualRect();
    env.init(monitorRect, captureRect, std::move(recycle));
    return env.mFrame && env.mFrame->valid();
}

void Master::processDetectRequest(pCommand &cmd) {
    CommandDetectRequest *c = dynamic_cast<CommandDetectRequest *>(cmd.get());
    if (!c)
        return;
    try {
        c->promise.set_value(detectEDState(c->level));
    } catch (const std::exception& ex) {
        LOG(ERROR) << "Exception in processDetectRequest: " << GET_EXCEPTION_STACK_TRACE;
        try {
            c->promise.set_exception(std::current_exception());
        } catch (...) {}
    }
}
