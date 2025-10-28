//
// Created by mkizub on 23.05.2025.
//

#include "pch.h"

#include <locale>
#include <codecvt>

#include <png.h>
#include <stdio.h>
#include <filesystem>

#include "Keyboard.h"

std::string getErrorMessage() {
    return getErrorMessage(GetLastError());
}
std::string getErrorMessage(unsigned errorCode) {
    TCHAR messageBuffer[1024];
    size_t size = FormatMessage(
            FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
            nullptr,
            errorCode,
            MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US),
            messageBuffer,
            sizeof(messageBuffer),
            nullptr
    );
#ifdef UNICODE
    return toUtf8(messageBuffer, size);
#else
    return toString(messageBuffer, size);
#endif
}

void pasteToClipboard(const std::string& text) {
    if (!OpenClipboard(NULL)) {
        LOG(ERROR) << "Cannot open clipboard";
        return;
    }
    EmptyClipboard();
    HGLOBAL hg=GlobalAlloc(GMEM_MOVEABLE, text.size()+1);
    if (!hg) {
        CloseClipboard();
        return;
    }
    memcpy(GlobalLock(hg), text.c_str(), text.size()+1);
    GlobalUnlock(hg);
    SetClipboardData(CF_TEXT,hg);
    CloseClipboard();
    GlobalFree(hg);
}

std::string textFromClipboard() {
    if (!OpenClipboard(NULL)) {
        LOG(ERROR) << "Cannot open clipboard";
        return {};
    }
    HANDLE hData = GetClipboardData(CF_TEXT);
    if (hData == NULL) {
        LOG(ERROR) << "No text data in clipboard";
        CloseClipboard();
        return {};
    }
    const char* str = static_cast<const char*>(GlobalLock(hData));
    if (str == NULL) {
        LOG(ERROR) << "Cannot get text from clipboard";
        GlobalUnlock(hData);
        CloseClipboard();
        return {};
    }
    std::string text = str;
    GlobalUnlock(hData);
    CloseClipboard();
    return text;
}

std::string trim(const char* source) {
    if (!source || !*source)
        return {};
    return trim(std::string(source));
}
std::string trim(const std::string & source) {
    std::string s(source);
    s.erase(0,s.find_first_not_of(" \n\r\t"));
    s.erase(s.find_last_not_of(" \n\r\t")+1);
    return s;
}

std::wstring trim(const std::wstring & source) {
    std::wstring s(source);
    s.erase(0,s.find_first_not_of(L" \n\r\t"));
    s.erase(s.find_last_not_of(L" \n\r\t")+1);
    return s;
}

std::string trimWithPunktuation(const std::string & source) {
    std::string s(source);
    s.erase(0,s.find_first_not_of(" \n\r\t.,`~!@#$%^&*()-+=[]{}:;\'\"|\\<>?"));
    s.erase(s.find_last_not_of(" \n\r\t.,`~!@#$%^&*()-+=[]{}:;\'\"|\\<>?")+1);
    return s;
}

std::wstring trimWithPunktuation(const std::wstring & source) {
    std::wstring s(source);
    s.erase(0,s.find_first_not_of(L" \n\r\t.,`~!@#$%^&*()-+=[]{}:;\'\"|\\<>«»?"));
    s.erase(s.find_last_not_of(L" \n\r\t.,`~!@#$%^&*()-+=[]{}:;\'\"|\\<>«»?")+1);
    return s;
}

std::string toString(const char* buffer, size_t size) {
    return {buffer, size};
}
std::wstring toString(const wchar_t* buffer, size_t size) {
    return {buffer, size};
}
std::string toUtf8(const wchar_t* buffer, size_t size) {
    std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>, wchar_t> converter;
    std::string utf8_string = converter.to_bytes(buffer, buffer+size);
    return utf8_string;
}
std::string toUtf8(const std::wstring& str) {
    return toUtf8(str.c_str(), str.size());
}
std::wstring toUtf16(const char* buffer, size_t size) {
    std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>, wchar_t> converter;
    std::wstring utf16_string = converter.from_bytes(buffer, buffer+size);
    return utf16_string;
}
std::wstring toUtf16(const std::string& str) {
    return toUtf16(str.c_str(), str.size());
}

std::string toLower(const std::string& str) {
    std::string lower = str;
    std::transform(lower.begin(), lower.end(), lower.begin(),[](unsigned char c){ return std::tolower(c); });
    return lower;
}
std::string toUpper(const std::string& str) {
    std::string upper = str;
    std::transform(upper.begin(), upper.end(), upper.begin(),[](unsigned char c){ return std::toupper(c); });
    return upper;
}

bool equalsIgnoreCase(const std::string_view& str1, const std::string_view& str2) {
    if (str1.length() != str2.length())
        return false;

    for (int i = 0; i < str1.length(); ++i) {
        if (tolower(str1[i]) != tolower(str2[i]))
            return false;
    }

    return true;
}

std::vector<std::string> split(const std::string &s, char delim) {
    std::vector<std::string> elems;
    // Check to see if empty string, give consistent result
    if(s.empty()) {
        elems.emplace_back();
    } else {
        std::stringstream ss;
        ss.str(s);
        std::string item;
        while(std::getline(ss, item, delim)) {
            elems.push_back(item);
        }
    }
    return elems;
}

bool contains(const std::vector<std::string>& strings, const std::string& str) {
    auto it = std::find(strings.begin(), strings.end(), str);
    return it != strings.end();
}

cv::Vec3b encodeBGR(unsigned bgr) {
    return cv::Vec3b(bgr & 0xFF, (bgr>>8) & 0xFF, (bgr>>16)&0xFF);
}
unsigned decodeBGR(const cv::Vec3b& bgr) {
    return bgr[0] | (bgr[1] << 8) | (bgr[2] << 16);
}

// convert a single linear BRG/RGB component (0.0-1.0) to sRGB
static inline uchar linearToSrgb(float c) {
    assert (c >= 0.f && c <= 1.f);
    uchar res;
    if (c <= 0.0031308f) {
        res = (uchar)(255.f * c * 12.92f);
    } else {
        res = (uchar)255.f * (1.055f * std::pow(c, 1.0f / 2.4f) - 0.055f);
    }
    return res;
}

cv::Vec3b lBgr2sBgr(const cv::Vec3f& lbgr) {
    uchar b = linearToSrgb(lbgr[0]);
    uchar g = linearToSrgb(lbgr[1]);
    uchar r = linearToSrgb(lbgr[2]);
    return {b, g, r};
}

static inline float srgbToLinear(float c) {
    assert (c >= 0.f && c <= 1.f);
    float res;
    if (c <= 0.04045f) {
        res = c / 12.92f;
    } else {
        res = std::pow((c + 0.055f) / 1.055f, 2.4f);
    }
    return res;
}
cv::Vec3f sBgr2lBgr(const cv::Vec3b& sbgr) {
    float b = srgbToLinear(sbgr[0] / 255.f);
    float g = srgbToLinear(sbgr[1] / 255.f);
    float r = srgbToLinear(sbgr[2] / 255.f);
    return {b, g, r};
}

unsigned sBgr2sGray(const cv::Vec3b& bgr) {
    float gr = (0.114f * bgr[0]) + (0.587f * bgr[1]) + (0.299f * bgr[2]);
    return std::clamp((unsigned)std::round(gr), 0U, 255U);;
//    cv::Mat3b in(bgr, false);
//    cv::Mat1b out;
//    cv::cvtColor(in, out, cv::COLOR_BGR2GRAY);
//    return out.at<uchar>(0);
}
cv::Vec3b sGray2sBgr(unsigned gray) {
    uchar gr = std::clamp(gray, 0U, 255U);
    return {gr, gr, gr};
//    float g = std::clamp(gray / 255.f, 0.f, 1.f);
//    cv::Mat1f in(1, 1, g);
//    cv::Mat3f out;
//    cv::cvtColor(in, out, cv::COLOR_GRAY2BGR);
//    cv::Vec3f& res = out.at<cv::Vec3f>(0);
//    return lBgr2sBgr(res);
}
cv::Vec3b sBgr2Luv(const cv::Vec3b& bgr) {
    cv::Mat3b in(bgr, false);
    cv::Mat3b out;
    cv::cvtColor(in, out, cv::COLOR_BGR2Luv);
    return out.at<cv::Vec3b>(0);
}
cv::Vec3b luv2sBgr(const cv::Vec3b& luv) {
    cv::Mat3b in(luv, false);
    cv::Mat3b out;
    cv::cvtColor(in, out, cv::COLOR_Luv2BGR);
    return out.at<cv::Vec3b>(0);
}
cv::Vec3b sBgr2Hsv(const cv::Vec3b& bgr) {
    cv::Mat3b in(bgr, false);
    cv::Mat3b out;
    cv::cvtColor(in, out, cv::COLOR_BGR2HSV);
    return out.at<cv::Vec3b>(0);
}
cv::Vec3b hsv2sBgr(const cv::Vec3b& hsv) {
    cv::Mat3b in(hsv, false);
    cv::Mat3b out;
    cv::cvtColor(in, out, cv::COLOR_HSV2BGR);
    return out.at<cv::Vec3b>(0);
}
int distanceBGR(const cv::Vec3b& bgr1, const cv::Vec3b& bgr2) {
    cv::Vec3f delta = cv::Vec3f(bgr1) - cv::Vec3f(bgr2);
    int dist = (int)std::round(cv::norm(delta));
    return dist;
}
int distanceLuv(const cv::Vec3b& luv1, const cv::Vec3b& luv2) {
    cv::Vec3f delta = cv::Vec3f(luv1) - cv::Vec3f(luv2);
    int dist = (int)std::round(cv::norm(delta));
    return dist;
}
int distanceHsv(const cv::Vec3b& hsv1_, const cv::Vec3b& hsv2_) {
    cv::Vec3f hsv1 = cv::Vec3f(hsv1_);
    cv::Vec3f hsv2 = cv::Vec3f(hsv2_);
    float delta_hue = std::min(std::abs(hsv1[0] - hsv2[0]), 360 - std::abs(hsv1[0] - hsv2[0]));
    float delta_sat = hsv1[1] - hsv2[1];
    float delta_val = hsv1[2] - hsv2[2];
    cv::Vec3f delta = {255.f/180.f*delta_hue, delta_sat, delta_val};
    int dist = (int)std::round(cv::norm(delta));
    return dist;
}


std::pair<std::string,unsigned> decodeShortcut(std::string key) {
    unsigned flags = 0;
    for (;;) {
        size_t pos = key.find_first_of('+');
        if (pos == std::string::npos)
            break;
        std::string mod = toLower(key.substr(0, pos));
        if (mod == "ctrl" || mod == "lctrl")
            flags |= kbd::LCTRL;
        else if (mod == "rctrl")
            flags |= kbd::RCTRL;
        else if (mod == "alt" || mod == "lalt" || mod == "menu" || mod == "lmenu")
            flags |= kbd::LALT;
        else if (mod == "ralt" || mod == "rmenu")
            flags |= kbd::RALT;
        else if (mod == "shift" || mod == "lshift")
            flags |= kbd::LSHIFT;
        else if (mod == "rshift")
            flags |= kbd::RSHIFT;
        else if (mod == "win" || mod == "meta" || mod == "lwin" || mod == "lmeta")
            flags |= kbd::LWIN;
        else if (mod == "rwin" || mod == "rmeta")
            flags |= kbd::RWIN;
        else
            LOG(ERROR) << "Unknown key modifier " << mod;
        key = key.substr(pos+1);
    }
    return std::make_pair(toLower(key), flags);
}

std::string encodeShortcut(const std::string& name, unsigned flags) {
    std::string res;
    if (flags & kbd::LCTRL) res += "LCtrl+";
    if (flags & kbd::RCTRL) res += "RCtrl+";
    if (flags & kbd::LALT) res += "LAlt+";
    if (flags & kbd::RALT) res += "RAlt+";
    if (flags & kbd::LSHIFT) res += "LShift+";
    if (flags & kbd::RSHIFT) res += "RShift+";
    if (flags & kbd::LWIN) res += "LWin+";
    if (flags & kbd::RWIN) res += "RWin+";
    res += kbd::getNamesForKey(name)[0];
    return res;
}

bool parseTimestampString(const std::string& str, Timestamp& timestamp) {
    std::istringstream iss(str);
    iss >> std::chrono::parse("%Y-%m-%dT%H:%M:%SZ", timestamp);
    if (iss.fail()) {
        LOG(ERROR) << "Timestamp parse failed, event corrupted?";
        return false;
    }
    return true;
}

bool parseTimestamp(const json5pp::value& value, Timestamp& timestamp) {
    if (value.is_string())
        return parseTimestampString(value.as_string(), timestamp);
    auto& ts = value["timestamp"];
    if (!ts.is_string())
        return false;
    return parseTimestampString(ts.as_string(), timestamp);
}


const double MM_LS = 299.792458;
const double LS_LY = 31536000;
const double MM_LY = MM_LS * LS_LY;
static double DIST_UNIT_SCALE[6][6] {
    //   X, M,            KM,           MM,      LS,         LY
/* X*/  {1, 1,             1,            1,       1,          1 },
/* M*/  {1, 1,          1e-3,         1e-6,    1e-6/MM_LS, 1e-6/MM_LY},
/*KM*/  {1, 1e+3,          1,         1e-3,    1e-3/MM_LS, 1e-3/MM_LY},
/*MM*/  {1, 1e+6,       1e+3,            1,       1/MM_LS,    1/MM_LY},
/*LS*/  {1, 1e+6*MM_LS, 1e+3*MM_LS,  MM_LS,       1,          1/LS_LY},
/*LY*/  {1, 1e+6*MM_LY, 1e+3*MM_LY,  MM_LY,   LS_LY,          1 },
};

dist_t dist_t::convertTo(dist_t::Unit u) const {
    if (u == Unit::X || unit == Unit::X) return *this;
    return dist_t(u, dist*DIST_UNIT_SCALE[unit][u]);
}

double dist_t::get(Unit u) {
    if (u == Unit::X || unit == Unit::X) return -1;
    return dist*DIST_UNIT_SCALE[unit][u];
}

std::string dist_t::to_string() const {
    switch (unit) {
    default:
    case Unit::X:
        return std::format("{:.2f}", dist);
    case Unit::M:
        return std::format("{:.2f}m", dist);
    case Unit::KM:
        return std::format("{:.2f}km", dist);
    case Unit::MM:
        return std::format("{:.2f}mm", dist);
    case Unit::LS:
        return std::format("{:.2f}ls", dist);
    case Unit::LY:
        return std::format("{:.2f}ly", dist);
    }
}

std::ostream& operator<<(std::ostream& os, const dist_t& obj) {
    os << obj.to_string();
    return os;
}

dist_t parseDist(std::wstring dist) {
    bool cruise = st::ship.flags.cruise;
    dist_t::Unit unit = dist_t::X;
    dist = trim(dist);
    for (int garbage=0; garbage < 4; garbage++) {
        //M, KM, MM, LS, LY
        if (dist.ends_with(L"Mм") || dist.ends_with(L"Mm")) {
            unit = dist_t::MM;
            dist = trim(dist.substr(0, dist.size() - 2));
            break;
        } else if (dist.ends_with(L"км") || dist.ends_with(L"kм") || dist.ends_with(L"кm") || dist.ends_with(L"km")) {
            unit = dist_t::KM;
            dist = trim(dist.substr(0, dist.size() - 2));
            break;
        } else if (!cruise && garbage == 0 && dist.ends_with(L"м") || dist.ends_with(L"m")) {
            unit = dist_t::M;
            dist = trim(dist.substr(0, dist.size() - 1));
            break;
        } else if (cruise && garbage > 0 && dist.ends_with(L"M")) {
            unit = dist_t::MM;
            dist = trim(dist.substr(0, dist.size() - 1));
            break;
        } else if (dist.ends_with(L"c.л.")) {
            unit = dist_t::LY;
            dist = trim(dist.substr(0, dist.size() - 4));
            break;
        } else if (dist.ends_with(L"c.л")) {
            unit = dist_t::LY;
            dist = trim(dist.substr(0, dist.size() - 3));
            break;
        } else if (dist.ends_with(L"cв. c")) {
            unit = dist_t::LS;
            dist = trim(dist.substr(0, dist.size() - 5));
            break;
        } else if (dist.ends_with(L"cв.c") || dist.ends_with(L"cв c")) {
            unit = dist_t::LS;
            dist = trim(dist.substr(0, dist.size() - 4));
            break;
        } else if (dist.ends_with(L"cв.") || dist.ends_with(L"cв ")) {
            unit = dist_t::LS;
            dist = trim(dist.substr(0, dist.size() - 3));
            break;
        } else if (dist.ends_with(L"cв")) {
            unit = dist_t::LS;
            dist = trim(dist.substr(0, dist.size() - 2));
            break;
        }
        dist = trim(dist.substr(0, dist.size() - 1)); // maybe some OCR garbage after distance unit
    }
    if (unit == dist_t::X)
        return {};

    std::string num;
    for (auto dig : dist) {
        if (dig >= '0' && dig <= '9')
            num.push_back((char)dig);
        else if (dig == ',' || dig == '.')
            num.push_back('.');
        else if (dig == ' ')
            continue;
        else
            return {};
    }
    if (num.empty())
        return {};
    try {
        double d = std::stod(num);
        return {unit, d};
    } catch (...) {
        return {};
    }
}

bool utc_timer::expired() {
    auto now = std::chrono::utc_clock::now();
    return now >= time_limit;
}
int utc_timer::sec_passed() {
    auto now = std::chrono::utc_clock::now();
    return std::chrono::duration_cast<std::chrono::seconds>(now - time_start).count();
}
int utc_timer::sec_left() {
    auto now = std::chrono::utc_clock::now();
    return std::chrono::duration_cast<std::chrono::seconds>(time_limit - now).count();
}
std::string utc_timer::passed() {
    auto now = std::chrono::utc_clock::now();
    auto dur = std::chrono::duration_cast<std::chrono::seconds>(now - time_start);
    int sec = dur.count();
    if (sec < 60)
        return std::format("{}s", sec);
    if (sec < 60*60)
        return std::format("{0:%M:%S}s", dur);
    return std::format("{0:%T}", dur);
}
std::string utc_timer::left() {
    auto now = std::chrono::utc_clock::now();
    auto dur = std::chrono::duration_cast<std::chrono::seconds>(time_limit - now);
    int sec = dur.count();
    if (sec < 60)
        return std::format("{}s", sec);
    if (sec < 60*60)
        return std::format("{0:%M:%S}s", dur);
    return std::format("{0:%T}", dur);
}
