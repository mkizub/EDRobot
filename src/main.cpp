#include "pch.h"

#include "Configuration.h"

#include <spdlog/sinks/ansicolor_sink.h>
#include "spdlog/sinks/ansicolor_sink-inl.h"
#include "spdlog/sinks/rotating_file_sink.h"
#include "spdlog/pattern_formatter.h"
#include <io.h>
#include <fcntl.h>

std::thread::id main_thread_id;
spdlog::sink_ptr console_sink;

BOOL WINAPI ConsoleCtrlHandler(DWORD dwCtrlType) {
    static bool shutdownCalled = false;
    static bool exitCalled = false;
    if (dwCtrlType == CTRL_C_EVENT) {
        if (!shutdownCalled) {
            LOG(ERROR) << "Ctrl-C received. Performing shutdown...";
            Mgr.pushCommand(Command::Shutdown);
            shutdownCalled = true;
            return TRUE; // Indicate that we've handled the event
        }
        if (!exitCalled) {
            LOG(ERROR) << "Ctrl-C received. Performing ExitProcess...";
            ExitProcess(1);
            exitCalled = true;
            return TRUE; // Indicate that we've handled the event
        }
        LOG(ERROR) << "Ctrl-C received. Performing TerminateProcess...";
        HANDLE hProcess = GetCurrentProcess();
        if (!TerminateProcess(hProcess, 1)) {
            CloseHandle(hProcess);
            LOG(ERROR) << "Ctrl-C received. TerminateProcess failed!!!";
        }
        return FALSE;
    }
    return FALSE;
}

void GlobalTerminateHandler() {
    std::cerr << "Global terminate handler." << std::endl;
    // Dump the stored backtrace messages to the log sink immediately
    SPDLOG_CRITICAL("--- CRASH DETECTED ---");
    spdlog::dump_backtrace();
    spdlog::shutdown();
    std::abort(); // Must terminate without returning
}

LONG WINAPI UnhandledExceptionHandler(struct _EXCEPTION_POINTERS* ExceptionInfo) {
    std::cerr << "Global unhandled exception filter caught an exception." << std::endl;
    // Dump the stored backtrace messages to the log sink immediately
    SPDLOG_CRITICAL("--- CRASH DETECTED ---");
    spdlog::dump_backtrace();
    spdlog::shutdown();
    return EXCEPTION_CONTINUE_SEARCH;
}

class maybe_name_flag : public spdlog::custom_flag_formatter
{
public:
    void format(const spdlog::details::log_msg &msg, const std::tm &, spdlog::memory_buf_t &dest) override {
        if (!msg.logger_name.empty())
            dest.append(" [").append(msg.logger_name).append("]");
    }

    [[nodiscard]] std::unique_ptr<custom_flag_formatter> clone() const override {
        return spdlog::details::make_unique<maybe_name_flag>();
    }
};

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR lpCmdLine, int nCmdShow) {
    HWND hRobotWnd = FindWindow(Master::ROBOT_WINDOW_CLASS,Master::ROBOT_WINDOW_NAME);
    if (hRobotWnd) {
        ShowWindow(hRobotWnd, SW_RESTORE);
        SetForegroundWindow(hRobotWnd);
        BringWindowToTop(hRobotWnd);
        return 0;
    }

    main_thread_id = std::this_thread::get_id();

    AllocConsole();
    if (auto consoleWindow = GetConsoleWindow()) {
        ShowWindow(consoleWindow, SW_HIDE);
        if (HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE); hOut != INVALID_HANDLE_VALUE) {
            DWORD dwMode = 0;
            GetConsoleMode(hOut, &dwMode);
            dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
            SetConsoleMode(hOut, dwMode);
        }
    }
    SetConsoleCtrlHandler(ConsoleCtrlHandler, TRUE);
    SetConsoleOutputCP(CP_UTF8);

    {
        HANDLE hConsole = CreateFile(L"CONOUT$", GENERIC_WRITE, FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
        FILE *fpConsole = _fdopen(_open_osfhandle((intptr_t) hConsole, _O_TEXT), "w");
        auto c_sink = new spdlog::sinks::ansicolor_sink<spdlog::details::console_mutex>(fpConsole,
                                                                                        spdlog::color_mode::always);
        console_sink.reset(c_sink);
        c_sink->set_level(spdlog::level::info);
        c_sink->set_color(spdlog::level::info, "\033[38;5;7m");
        c_sink->set_color(spdlog::level::debug, "\033[38;5;8m");
        c_sink->set_color(spdlog::level::trace, "\033[38;5;6m");
    }

    auto http_file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>("cache/network.log", 5*1024*1024, 3, true);
    http_file_sink->set_level(spdlog::level::debug);

    auto base_file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>("cache/edrobot.log", 5*1024*1024, 3, true);
    base_file_sink->set_level(spdlog::level::debug);

    auto http_logger = std::make_shared<spdlog::logger>(spdlog::logger{"http", {console_sink, http_file_sink}});
    http_logger->set_level(spdlog::level::debug);
    spdlog::register_logger(http_logger);

    auto default_logger = std::make_shared<spdlog::logger>(spdlog::logger{"", {console_sink, base_file_sink}});
    default_logger->set_level(spdlog::level::debug);
    spdlog::set_default_logger(default_logger);

    auto formatter = std::make_unique<spdlog::pattern_formatter>("%^%Y-%m-%d %H:%M:%S.%e %L:%=4? %v%$",spdlog::pattern_time_type::utc);
    formatter->add_flag<maybe_name_flag>('?');
    spdlog::set_formatter(std::move(formatter));

    spdlog::flush_every(std::chrono::seconds(3));
    spdlog::flush_on(spdlog::level::err);
    SetUnhandledExceptionFilter(UnhandledExceptionHandler);
    std::set_terminate(GlobalTerminateHandler);

    Master& master = Master::getInstance();
    if (master.initialize())
        master.loop();
    master.shutdown();
    LOG(INFO) << "Shutdown";
    spdlog::shutdown();
    FreeConsole();
    return 0;
}

