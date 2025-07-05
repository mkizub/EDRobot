// pch.h: This is a precompiled header file.
// Files listed below are compiled only once, improving build performance for future builds.
// This also affects IntelliSense performance, including code completion and many code browsing features.
// However, files listed here are ALL re-compiled if any one of them is updated between builds.
// Do not add files here that you will be updating frequently as this negates the performance advantage.

#pragma once

#ifndef PCH_H
#define PCH_H

#ifndef NOMINMAX
# define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <vector>
#include <map>
#include <unordered_map>
#include <cstring>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <format>
#include <variant>
#include <chrono>
#include <future>
#include <utility>
#include <filesystem>
#define _USE_MATH_DEFINES
#include <cmath>

#define ELPP_PERFORMANCE_MICROSECONDS
#define ELPP_STL_LOGGING
//#define ELPP_THREAD_SAFE
// #define ELPP_FEATURE_CRASH_LOG -- Stack trace not available for MinGW GCC
#define ELPP_LOG_STD_ARRAY
#define ELPP_LOG_UNORDERED_MAP
#include <easylogging/easylogging++.h>

#include <json5pp/json5pp.hpp>
#include <meojson/json5.hpp>

#include "opencv2/opencv.hpp"

#include "libintl.h"
#include <clocale>
#define pgettext(P,T) gettext(P "\004" T)
#define _(T) gettext(T)
template<typename... Args>
std::string std_format(std::string_view rt_fmt_str, Args&&... args) {
    return std::vformat(rt_fmt_str, std::make_format_args(args...));
}

#include <magic_enum/magic_enum.hpp>
using namespace magic_enum;

// forward declarations
class Capturer;

namespace detect {
    class Detector;
    class CompassDetector;
    class Histogram;
}

namespace ai {
    class AIManager;
    class Task;
}
namespace widget {
    class Root;
    class Widget;
    class List;
    class BaseDialog;
    class Screen;
}

#include "Utils.h"
#include "Configuration.h"
#include "ClassifyEnv.h"
#include "Master.h"

#endif //PCH_H
