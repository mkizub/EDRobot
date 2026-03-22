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

    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    console_sink->set_level(spdlog::level::info);
    console_sink->set_color(spdlog::level::info, 0);
    console_sink->set_color(spdlog::level::warn, BACKGROUND_RED | BACKGROUND_GREEN | BACKGROUND_INTENSITY);
    console_sink->set_color(spdlog::level::err, FOREGROUND_BLUE | FOREGROUND_GREEN | BACKGROUND_RED | BACKGROUND_INTENSITY);

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

    Master& master = Master::getInstance();
    if (master.initialize(argc, argv))
        master.loop();
    master.shutdown();
    LOG(INFO) << "Shutdown";
    spdlog::shutdown();
    return 0;
}

