#include "pch.h"

#include <cstdlib>
#include "ui/UIManager.h"
#include "Configuration.h"
#include "Galaxy.h"

#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/sinks/rotating_file_sink.h"
#include "spdlog/sinks/wincolor_sink.h"
#include "spdlog/pattern_formatter.h"

//INITIALIZE_EASYLOGGINGPP

std::thread::id main_thread_id;

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

int main(int argc, char *argv[]) {
    main_thread_id = std::this_thread::get_id();

//    START_EASYLOGGINGPP(argc, argv);
//    el::Loggers::getLogger("OpenCV");
//    el::Loggers::configureFromGlobal("logging.conf");

    auto http_console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    http_console_sink->set_level(spdlog::level::info);
    http_console_sink->set_color(spdlog::level::info, 0);
    http_console_sink->set_color(spdlog::level::warn, FOREGROUND_RED | FOREGROUND_GREEN);

    auto http_file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>("cache/http.log", 5*1024*1024, 3, true);
    http_file_sink->set_level(spdlog::level::debug);

    std::vector<spdlog::sink_ptr> sinks {http_console_sink, http_file_sink};
    auto http_logger = std::make_shared<spdlog::logger>("http", sinks.begin(), sinks.end());
    http_logger->set_level(spdlog::level::debug);
    spdlog::register_logger(http_logger);

    spdlog::flush_every(std::chrono::seconds(3));

    auto formatter = std::make_unique<spdlog::pattern_formatter>();
    formatter->add_flag<maybe_name_flag>('?');
    formatter->set_pattern("%^%Y-%m-%d %H:%M:%S.%e %L:%=4? %v%$");
    formatter->need_localtime(false);
    spdlog::set_formatter(std::move(formatter));

    http_file_sink->set_formatter(std::unique_ptr<spdlog::formatter>(
            new spdlog::pattern_formatter("%Y-%m-%d %H:%M:%S.%e %L: %v", spdlog::pattern_time_type::utc)));
    auto default_logger = spdlog::default_logger();
    auto sink = default_logger->sinks()[0];
    if (auto console = dynamic_cast<spdlog::sinks::wincolor_stdout_sink_mt*>(sink.get())) {
        console->set_color(spdlog::level::info, 0);
        console->set_color(spdlog::level::warn, FOREGROUND_RED | FOREGROUND_GREEN);
    }

    Master& master = Master::getInstance();
    if (master.initialize(argc, argv))
        master.loop();
    master.shutdown();
    LOG(INFO) << "Shutdown";
    spdlog::shutdown(); //el::Loggers::flushAll();
    return 0;
}

