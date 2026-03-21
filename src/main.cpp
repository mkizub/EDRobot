#include "pch.h"

#include <cstdlib>
#include "ui/UIManager.h"
#include "Configuration.h"
#include "Galaxy.h"

#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/sinks/rotating_file_sink.h"
#include "spdlog/pattern_formatter.h"

INITIALIZE_EASYLOGGINGPP

std::thread::id main_thread_id;

int main(int argc, char *argv[]) {
    main_thread_id = std::this_thread::get_id();

    START_EASYLOGGINGPP(argc, argv);
    el::Loggers::getLogger("OpenCV");
    el::Loggers::configureFromGlobal("logging.conf");

    auto http_console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    http_console_sink->set_level(spdlog::level::info);

    auto http_file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>("cache/http.log", 5*1024*1024, 3, true);
    http_file_sink->set_level(spdlog::level::debug);

    std::vector<spdlog::sink_ptr> sinks {http_console_sink, http_file_sink};
    auto http_logger = std::make_shared<spdlog::logger>("http", sinks.begin(), sinks.end());
    http_logger->set_level(spdlog::level::debug);
    spdlog::register_logger(http_logger);

    spdlog::flush_every(std::chrono::seconds(3));
    spdlog::set_pattern("%^%Y-%m-%d %H:%M:%S.%e %l: [%n] %v%$", spdlog::pattern_time_type::utc);
    http_file_sink->set_formatter(std::unique_ptr<spdlog::formatter>(
            new spdlog::pattern_formatter("%Y-%m-%d %H:%M:%S.%e %L: %v", spdlog::pattern_time_type::utc)));

    Master& master = Master::getInstance();
    if (master.initialize(argc, argv))
        master.loop();
    master.shutdown();
    LOG(INFO) << "Shutdown";
    el::Loggers::flushAll();
    return 0;
}

