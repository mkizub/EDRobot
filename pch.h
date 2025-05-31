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

#define ELPP_PERFORMANCE_MICROSECONDS
//#ifdef __MINGW64__
#define ELPP_STL_LOGGING
// #define ELPP_FEATURE_CRASH_LOG -- Stack trace not available for MinGW GCC
#define ELPP_LOG_STD_ARRAY
#define ELPP_LOG_UNORDERED_MAP
//#endif  __MINGW64__
#include "easylogging++.h"

#include "json5pp.hpp"

#include <opencv2/opencv.hpp>

#include "Utils.h"
#include "EDWidget.h"
#include "Master.h"

#endif //PCH_H
