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
using namespace std::chrono_literals;
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

#include <tsl/ordered_hash.h>
#include <tsl/ordered_map.h>
#include <json5pp/json5pp.hpp>
namespace js = ::json5pp;

#include "opencv2/opencv.hpp"
#include "ed_opencv.h"
#define EDROBOT_USE_OPENCL
#ifdef EDROBOT_USE_OPENCL
typedef cv::UMat XMat;
extern bool g_DisableOpenCL;
inline bool useOpenCL() { return !g_DisableOpenCL; }
#define toXMat(M) (M).getUMat(cv::ACCESS_READ)
#define toMat(M) (M).getMat(cv::ACCESS_READ)
#else
typedef cv::Mat XMat;
inline bool useOpenCL() { return false; }
#define toXMat(M) M
#define toMat(M) M
#endif
#if !defined(EDROBOT_USE_OPENCL) && defined(NDEBUG)
# error "EDROBOT_USE_OPENCL not defined in release build"
#endif

#include "libintl.h"
#include <clocale>
#define pgettext(P,T) gettext(P "\004" T)
#define _gt(T) gettext(T)
#define _lc(T) T
template <class... _Types>
[[nodiscard]] std::string lc_format(const std::format_string<_Types...> _Fmt, _Types&&... _Args) {
    auto lc_fmt = gettext(_Fmt.get().data());
    return std::vformat(lc_fmt, std::make_format_args(_Args...));
}

#include <magic_enum/magic_enum.hpp>
using namespace magic_enum;

// forward declarations
class Capturer;

namespace detect {
    class Detector;
    class CompassDetector;
    class Histogram;
    class LineDetector;
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

extern std::thread::id main_thread_id;

#include "Types.h"
#include "Utils.h"
#include "State.h"
#include "Configuration.h"
#include "ClassifyEnv.h"
#include "Master.h"

#endif //PCH_H
