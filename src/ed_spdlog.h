//
// Created by mkizub on 21.03.2026.
//

#pragma once

#ifndef EDROBOT_ED_SPDLOG_H
#define EDROBOT_ED_SPDLOG_H

#if defined(NDEBUG)
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_DEBUG
#define SPDLOG_NO_SOURCE_LOC 1
#else
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_TRACE
#endif
#define SPDLOG_WCHAR_TO_UTF8_SUPPORT 1
#include "spdlog/spdlog.h"
#include "spdlog/sinks/sink.h"
extern spdlog::sink_ptr console_sink;

class streamable_logger_t {
    spdlog::level::level_enum log_level_;
    std::ostringstream log_stream_;

public:
    explicit streamable_logger_t(const spdlog::level::level_enum log_level)
            : log_level_(log_level) {

    }

    template <typename T>
    streamable_logger_t& operator<<(const T& value) {
        log_stream_ << value;
        return *this;
    }

    streamable_logger_t& operator<<(const std::wstring& value) {
        if (value.size()) {
            spdlog::memory_buf_t buf;
            spdlog::details::os::wstr_to_utf8buf(spdlog::wstring_view_t(value.data(), value.size()), buf);
            log_stream_ << buf;
        }
        return *this;
    }
    streamable_logger_t& operator<<(std::wstring_view value) {
        if (value.size()) {
            spdlog::memory_buf_t buf;
            spdlog::details::os::wstr_to_utf8buf(spdlog::wstring_view_t(value.data(), value.size()), buf);
            log_stream_ << buf;
        }
        return *this;
    }
    streamable_logger_t& operator<<(const wchar_t* value) {
        if (value && *value) {
            spdlog::memory_buf_t buf;
            spdlog::details::os::wstr_to_utf8buf(spdlog::wstring_view_t(value, wcslen(value)), buf);
            log_stream_ << buf;
        }
        return *this;
    }


    ~streamable_logger_t() {
        spdlog::log(log_level_, "{}", log_stream_.str());
    }
};

#define LOG(LEVEL) LOG_STREAM_##LEVEL
#define LOG_IF(cond,LEVEL) if (cond) LOG_STREAM_##LEVEL

#define LOGT(...) SPDLOG_LOGGER_TRACE(spdlog::default_logger_raw(), __VA_ARGS__)
#define LOGD(...) SPDLOG_LOGGER_DEBUG(spdlog::default_logger_raw(), __VA_ARGS__)
#define LOGI(...) SPDLOG_LOGGER_INFO(spdlog::default_logger_raw(), __VA_ARGS__)
#define LOGW(...) SPDLOG_LOGGER_WARN(spdlog::default_logger_raw(), __VA_ARGS__)
#define LOGE(...) SPDLOG_LOGGER_ERROR(spdlog::default_logger_raw(), __VA_ARGS__)
#define LOG_TRACE(...) SPDLOG_LOGGER_TRACE(spdlog::default_logger_raw(), __VA_ARGS__)
#define LOG_DEBUG(...) SPDLOG_LOGGER_DEBUG(spdlog::default_logger_raw(), __VA_ARGS__)
#define LOG_INFO(...) SPDLOG_LOGGER_INFO(spdlog::default_logger_raw(), __VA_ARGS__)
#define LOG_WARNING(...) SPDLOG_LOGGER_WARN(spdlog::default_logger_raw(), __VA_ARGS__)
#define LOG_ERROR(...) SPDLOG_LOGGER_ERROR(spdlog::default_logger_raw(), __VA_ARGS__)

#define LOG_STREAM_TRACE streamable_logger_t(spdlog::level::trace)
#define LOG_STREAM_DEBUG streamable_logger_t(spdlog::level::debug)
#define LOG_STREAM_INFO streamable_logger_t(spdlog::level::info)
#define LOG_STREAM_WARNING streamable_logger_t(spdlog::level::warn)
#define LOG_STREAM_ERROR streamable_logger_t(spdlog::level::err)


#endif //EDROBOT_ED_SPDLOG_H
