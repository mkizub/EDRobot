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


const wchar_t ED_WINDOW_NAME[] = L"Elite - Dangerous (CLIENT)";
const wchar_t ED_WINDOW_CLASS[] = L"FrontierDevelopmentsAppWinClass";
const wchar_t ED_WINDOW_EXE[] = L"EliteDangerous64.exe";

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

std::string UIState::to_string() const {
    if (!this->widget)
        return "unknown";
    if (this->focused)
        return widget->path+"@"+this->focused->name;
    return widget->path;
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
        return initializeInternal(ocr_dir);
    } CATCH(const std::exception& e) {
        LOG(ERROR) << "Exception in initialization: " << GET_EXCEPTION_STACK_TRACE;
        return 1;
    }
    return 0;
}
int Master::initializeInternal(std::string ocr_dir) {

    //cv::utils::logging::internal::replaceWriteLogMessage(writeOpenCVLogMessageFunc);
    //cv::utils::logging::internal::replaceWriteLogMessageEx(writeOpenCVLogMessageFuncEx);

    UI::initializeUI();

    mConfiguration = std::make_unique<Configuration>();
    mConfiguration->load();
    initButtonStateDetector();

    if (ocr_dir.empty())
        ocr_dir = mConfiguration->mTesseractDataPath;
    if (ocr_dir.empty())
        ocr_dir = "tessdata";

    {
        const char* tesseractLang = "eng";
        if (mConfiguration->lng == RU)
            tesseractLang = "rus+eng";
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

    {
        mTesseractApiForDigits = std::make_unique<tesseract::TessBaseAPI>();
        const std::vector<std::string> vars_vec{"tessedit_char_whitelist"};
        const std::vector<std::string> vars_values{"0123456789+-/."};
        // "eng" for English language
        if (mTesseractApiForDigits->Init(ocr_dir.c_str(), "eng", tesseract::OEM_LSTM_ONLY)) {
            LOG(ERROR) << "Error: Could not initialize tesseract.";
            mTesseractApiForDigits.reset();
        } else {
            mTesseractApiForDigits->SetPageSegMode(tesseract::PSM_SINGLE_LINE);
        }
    }

    std::vector<std::string> keys;
    for (auto& m : mConfiguration->keyMapping)
        keys.push_back(m.first.first);
    keyboard::intercept(std::move(keys));
    keyboard::start(tradingKbHook);

    return 0;
}


void Master::setCalibrationResult(const std::array<cv::Vec3b,4>& buttonLuv, const std::array<cv::Vec3b,4>& lstRowLuv) {
    mConfiguration->setCalibrationResult(buttonLuv, lstRowLuv);
    initButtonStateDetector();
}
void Master::initButtonStateDetector() {
    std::vector<cv::Vec3b> buttonColors(mConfiguration->mButtonLuv.begin(), mConfiguration->mButtonLuv.end());
    mButtonStateDetector.reset(new HistogramTemplate(HistogramTemplate::CompareMode::Luv, cv::Rect(), buttonColors));
    LOG(INFO) << "Button state detector installed";
    std::vector<cv::Vec3b> lstRowColors(mConfiguration->mLstRowLuv.begin(), mConfiguration->mLstRowLuv.end());
    mLstRowStateDetector.reset(new HistogramTemplate(HistogramTemplate::CompareMode::Luv, cv::Rect(), lstRowColors));
    LOG(INFO) << "List row state detector installed";
}


Master::Master() {
    mScreensRoot = std::make_unique<widget::Root>();
    hWndED = FindWindow(ED_WINDOW_CLASS, ED_WINDOW_NAME);
}

Master::~Master() {
    keyboard::stop();
    stopTrade();
}


void Master::loop() {
    TRY {
        while (true) {
            pCommand cmd;
            popCommand(cmd);
            switch (cmd->command) {
            case Command::NoOp:
                break;
            case Command::TaskFinished:
                clearCurrentTask();
                break;
            case Command::Start:
                startTrade();
                break;
            case Command::Pause:
            case Command::Stop:
                stopTrade();
                break;
            case Command::UserNotify:
                showNotification(cmd);
                break;
            case Command::Calibrate:
                startCalibration();
                break;
            case Command::DebugTemplates: {
                    ClassifyEnv env;
                    debugTemplates(nullptr, env);
                }
                break;
            case Command::DebugButtons:
                debugButtons();
                break;
            case Command::DevRectScreenshot:
                debugRectScreenshot(cmd);
                break;
            case Command::DevRectSelect:
                UI::askSelectRectWindow();
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
    LOG(DEBUG) << "Key '"+encodeShortcut(name,flags)+"' pressed";
    Master& self = getInstance();
    auto keyMapping = self.mConfiguration->keyMapping;
    auto cmd = keyMapping.find(std::make_pair(toLower(name),flags));
    if (cmd != keyMapping.end()) {
        LOG(INFO) << "Command " << magic_enum::enum_name(cmd->second) << " by key '"+encodeShortcut(name,flags)+"' pressed";
        self.pushCommand(cmd->second);
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
    UI::showToast(title, text);
}
void Master::notifyError(const std::string& title, const std::string& text) {
    LOG(ERROR) << title << ": " << text;
    UI::showToast(title, text);
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
    mCommandCond.wait(lock, [this]() { return !mCommandQueue.empty(); });
    std::swap(cmd, mCommandQueue.front());
    mCommandQueue.pop();
}

void Master::showNotification(pCommand& cmd) {
    CommandNotify* c = dynamic_cast<CommandNotify*>(cmd.get());
    if (c)
        UI::showToast(c->title, c->text);
}

bool Master::preInitTask(bool checkCalibration) {
    if (currentTask) {
        LOG(ERROR) << "Exiting current task";
        clearCurrentTask();
    }

    hWndED = FindWindow(ED_WINDOW_CLASS, ED_WINDOW_NAME);
    if (!hWndED) {
        LOG(ERROR) << "Window [class " << ED_WINDOW_CLASS << "; name " << ED_WINDOW_NAME << "] not found" ;
        return false;
    }
    Capturer *capturer = Capturer::getEDCapturer(hWndED);
    if (!capturer)
        return false;
    bool ok = mConfiguration->checkResolutionSupported(capturer->getCaptureRect().size());
    if (!ok)
        return false;
    if (checkCalibration && mConfiguration->checkNeedColorCalibration()) {
        UI::showCalibrationDialog(_("Color calibration required"));
        return false;
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
    currentTask = std::make_unique<TaskCalibrate>();
    currentTaskThread = std::thread(Master::runCurrentTask);
    return true;
}


const ClassifyEnv::ResultListRow* Master::getFocusedRow(const std::string& lst_name) {
    auto it_rows = mLastEDState.cEnv.classifiedListRows.find(lst_name);
    if (it_rows == mLastEDState.cEnv.classifiedListRows.end()) {
        LOG(ERROR) << _("Rows in list are not detected");
        return nullptr;
    }
    auto &classified_rows = it_rows->second;
    if (classified_rows.empty()) {
        LOG(ERROR) << _("Rows in list are not detected");
        return nullptr;
    }
    for (auto &row: classified_rows) {
        if (row.bs == WState::Focused)
            return &row;
    }
    return nullptr;
}

bool Master::startTrade() {
    if (!mConfiguration->loadMarket()) {
        UI::showToast(_("Cannot sell"), _("Cannot load market"));
        return false;
    }

    if (!mConfiguration->loadCargo() || mConfiguration->currentCargo.count <= 0) {
        UI::showToast(_("Cannot sell"), _("Ship cargo empty"));
        return false;
    }

    std::string cargo_name;
    detectEDState(DetectLevel::ListOcrFocusedRow);
    if (isEDStateMatch("scr-market:mod-sell")) {
        auto row = getFocusedRow("lst-goods");
        if (row) {
            spCargoCommodity cc = mConfiguration->getCargoByName(row->text, true);
            if (cc)
                cargo_name = cc->commodity.name;
        }
    }

    int total = 0;
    int chunk = 1;
    bool res = UI::askSellInput(total, chunk, cargo_name, mConfiguration->currentCargo);
    if (!res || total <= 0 || chunk <= 0)
        return false;
    int sells = (int)std::ceil(double(total) / double(chunk));

    if (!preInitTask())
        return false;

    LOG(INFO) << "Staring new trade task";
    currentTask = std::make_unique<TaskSell>(nullptr, sells, chunk);
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
    if (item->tp == WidgetType::Button)
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
    if (item->tp == WidgetType::List) {
        List* lst = static_cast<List*>(item);
        ////////////////////////////////////////////////
        unsigned buttonGrayColor = rgb2gray(luv2rgb(mConfiguration->mLstRowLuv[int(WState::Normal)]));
        cv::Mat listImage(grayED, rect);

        //cv::imshow("List image: " + mLastEDState.path(), listImage);

        cv::Mat thresholdedImage;
        cv::threshold(listImage, thresholdedImage, buttonGrayColor - 2, 255, cv::THRESH_BINARY);

        //cv::imshow("Threshold image: " + mLastEDState.path(), thresholdedImage);

        std::vector<cv::Rect> detectedRows;
        std::vector<std::vector<cv::Point>> contours;
        cv::findContoursLinkRuns(thresholdedImage, contours);
        for (const auto &contour: contours) {
            std::vector<cv::Point> convex;
            cv::convexHull(contour, convex);
            if (convex.size() >= 4) {
                std::vector<cv::Point> approx;
                cv::approxPolyN(convex, approx, 4, 5, true);
                auto row_height = mLastEDState.cEnv.scaleToCaptured(cv::Size(0, lst->row_height)).height;
                if (cv::contourArea(approx) > rect.width * (row_height-2)) {
                    cv::Rect bbox = cv::boundingRect(approx);
                    if (bbox.height < row_height * 1.5) {
                        detectedRows.push_back(bbox);
                        cv::rectangle(debugImage, bbox + rect.tl(), cv::Scalar(0, 255, 0), 1);
                    } else {
                        int count = (int)std::floor(0.1 + double(bbox.height) / double(row_height));
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

        if (mTesseractApiForMarket) {
            for (auto &r: detectedRows) {
                cv::Mat rowImage(listImage, r);
                mTesseractApiForMarket->SetImage(rowImage.data, rowImage.cols, rowImage.rows, 1, rowImage.step);
                char *outText = mTesseractApiForMarket->GetUTF8Text();
                LOG(INFO) << "OCR Output: " << outText;
                delete outText;
            }
        }

        //cv::imshow("Contour image: " + mLastEDState.path(), contourDebugImage);

        //cv::waitKey();
        //cv::destroyAllWindows();
    }
}
bool Master::debugButtons() {
    detectEDState(DetectLevel::ListOcrAllRows);
    const widget::Widget* widget = mLastEDState.widget;
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
            return false;
        }
    }
    cv::Rect captureRect;
    cv::Mat imgColor, imgGray;
    if (!captureWindow(captureRect, imgColor, imgGray)) {
        LOG(ERROR) << "Cannot capture screen";
        return false;
    }
    if ((rect & captureRect) != rect) {
        LOG(ERROR) << "Cannot make screenshot because dev rect is beyond of game client area";
        return false;
    }
    rect -= captureRect.tl();

    std::string text;
    std::string name;
    if (mTesseractApiForMarket) {
        cv::Mat imgOcr(imgGray, rect);
        mTesseractApiForMarket->SetImage(imgOcr.data, imgOcr.cols, imgOcr.rows, 1, imgOcr.step);
        char* ocrText = mTesseractApiForMarket->GetUTF8Text();
        text = trim(ocrText);
        delete ocrText;
        LOG(INFO) << "OCR text for selected rect " << rect << ": " << text;
        std::transform(text.begin(), text.end(), text.begin(),[](unsigned char c){ return std::isspace(c) ? ' ' : c; });
        Commodity* commodity = mConfiguration->getCommodityByName(text, true);
        if (commodity) {
            name = commodity->nameId;
            if (mConfiguration->lng != EN)  // they capitalize text
                text = commodity->name;
        } else {
            name.clear();
            text.clear();
        }
    }

    cv::imwrite("debug-rect-gray.png", cv::Mat(imgGray, rect));
    cv::imwrite("debug-rect-color.png", cv::Mat(imgColor, rect));
    if (!name.empty()) {
        int histSize = 256;
        float range[]{0, 256};
        const float* histRange[]{range};
        cv::Mat imgOcr(imgGray, rect);
        //cv::Mat hist;
        //cv::calcHist(&imgOcr, 1, nullptr, cv::Mat(), hist, 1, &histSize, histRange);
        //int maxLoc[4]{};
        //cv::minMaxIdx(hist, nullptr, nullptr, nullptr, maxLoc);
        //int backgroundColor = maxLoc[0];
        //cv::Mat imgThr;
        //cv::threshold(imgOcr, imgThr, backgroundColor+1, 255, cv::THRESH_TOZERO_INV);
        //cv::Mat imgNorm;
        //cv::normalize(imgThr, imgNorm, backgroundColor+1, 255, cv::NORM_MINMAX); // cv::NORM_L1

        std::string lng;
        if (mConfiguration->lng == RU)
            lng = "-rus";
        else if (mConfiguration->lng == EN)
            lng = "-eng";
        else
            lng = "-xxx";
        cv::imwrite(name + lng + ".png", imgOcr);
        std::ofstream wf(name+lng+".gt.txt", std::ios::trunc | std::ios::binary);
        if (wf.is_open()) {
            wf << text;
            wf.close();
        }
    }
    return true;
}

double Master::detectButtonState(const widget::Widget* item) {
    if (!item)
        return 0;
    cv::Rect r = item->calcRect(mLastEDState.cEnv);
    if (r.empty())
        return 0;
    int x = r.x + r.width - 36;
    int y = r.y + r.height / 2 - 8;
    if (item->tp == WidgetType::Spinner)
        x -= r.height + 10;
    mButtonStateDetector->mRect = cv::Rect(cv::Point(x,y), cv::Size(16,16));
    mButtonStateDetector->classify(mLastEDState.cEnv);
    auto& values = mButtonStateDetector->mLastValues;
    int idx = int(std::max_element(values.begin(), values.end()) - values.begin());
    double value = values[idx];
    if (value > 0.90) {
        WState bs = (WState)idx;
        mLastEDState.cEnv.classifiedButtonStates[item->name] = bs;
        LOG_IF(bs == WState::Focused, INFO) << "Focused: " << item->name;
        LOG_IF(bs == WState::Disabled, INFO) << "Disabld: " << item->name;
        return value;
    }
    return 0;
}

void Master::detectListState(const widget::List* lst, DetectLevel level) {
    if (!lst)
        return;
    cv::Rect rect = lst->calcRect(mLastEDState.cEnv);
    if (rect.empty())
        return;

    unsigned buttonGrayColor = rgb2gray(luv2rgb(mConfiguration->mLstRowLuv[int(WState::Normal)]));
    cv::Mat grayImage(grayED, rect);

    cv::Mat thresholdedImage;
    cv::threshold(grayImage, thresholdedImage, buttonGrayColor * 0.75, 255, cv::THRESH_BINARY);

    std::vector<cv::Rect> detectedRows;
    std::vector<std::vector<cv::Point>> contours;
    cv::findContoursLinkRuns(thresholdedImage, contours);
    for (const auto &contour: contours) {
        std::vector<cv::Point> convex;
        cv::convexHull(contour, convex);
        if (convex.size() >= 4) {
            std::vector<cv::Point> approx;
            cv::approxPolyN(convex, approx, 4, 5, true);
            auto row_height = mLastEDState.cEnv.scaleToCaptured(cv::Size(0, lst->row_height)).height;
            if (cv::contourArea(approx) > rect.width * (row_height-2)) {
                cv::Rect bbox = cv::boundingRect(approx);
                if (bbox.height < row_height * 1.5) {
                    detectedRows.push_back(bbox);
                } else {
                    int count = (int)std::floor(0.1 + double(bbox.height) / double(row_height));
                    int split_height = bbox.height / count;
                    for (int i=0; i < count; i++) {
                        cv::Rect bb(bbox.x, bbox.y+(i*split_height), bbox.width, split_height-1);
                        detectedRows.push_back(bb);
                    }
                }
            }
        }
    }

    std::vector<ClassifyEnv::ResultListRow> rows;

    for (auto& r : detectedRows) {
        WState bs = WState::Disabled;
        if (level >= DetectLevel::ListRowStates) {
            int x = rect.x + r.x + r.width - 36;
            int y = rect.y + r.y + r.height / 2 - 8;
            mLstRowStateDetector->mRect = cv::Rect(cv::Point(x,y), cv::Size(16,16));
            mLstRowStateDetector->classify(mLastEDState.cEnv);
            auto& values = mLstRowStateDetector->mLastValues;
            int idx = int(std::max_element(values.begin(), values.end()) - values.begin());
            double value = values[idx];
            if (value > 0.90)
                bs = (WState)idx;
        }
        std::string text;
        if (mTesseractApiForMarket && (level >= DetectLevel::ListOcrAllRows || (bs == WState::Focused && level >= DetectLevel::ListOcrFocusedRow))) {
            cv::Mat rowImage(grayImage, r);
            mTesseractApiForMarket->SetImage(rowImage.data, rowImage.cols, rowImage.rows, 1, rowImage.step);
            char* ocrText = mTesseractApiForMarket->GetUTF8Text();
            text = trim(ocrText);
            delete ocrText;
            LOG(INFO) << "OCR text: " << text;
        }
        rows.emplace_back(r+rect.tl(), bs, text);
    }

    mLastEDState.cEnv.classifiedListRows[lst->name] = rows;
}

Widget* Master::detectAllButtonsStates(const widget::Widget* parent, DetectLevel level) {
    if (!parent)
        return nullptr;
    widget::Widget* focused = nullptr;
    double focused_value = 0;
    for (Widget* item : parent->have) {
        if (item->tp == WidgetType::Button || item->tp == WidgetType::Spinner) {
            double value = detectButtonState(item);
            if (mLastEDState.cEnv.classifiedButtonStates[item->name] == WState::Focused) {
                if (!focused || value > focused_value) {
                    focused = item;
                    focused_value = value;
                }
            }
        }
        if (item->tp == WidgetType::List && level >= DetectLevel::ListRows) {
            detectListState(dynamic_cast<List*>(item), level);
        }
    }
    widget::Widget* parent_focused = nullptr;
    if (parent->tp == WidgetType::Mode) {
        parent_focused = detectAllButtonsStates(parent->parent, level);
    }
    if (focused && focused_value > 0.96)
        return focused;
    return parent_focused;
}
const UIState& Master::detectEDState(DetectLevel level) {
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
