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
    OpenClipboard(nullptr);
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
        if (mod == "ctrl")
            flags |= keyboard::CTRL;
        else if (mod == "alt")
            flags |= keyboard::ALT;
        else if (mod == "shift")
            flags |= keyboard::SHIFT;
        else if (mod == "win" || mod == "meta")
            flags |= keyboard::WIN;
        else
            LOG(ERROR) << "Unknown key modifier " << mod;
        key = key.substr(pos+1);
    }
    return std::make_pair(toLower(key), flags);
}

std::string encodeShortcut(const std::string& name, unsigned flags) {
    std::string res;
    if (flags & keyboard::CTRL) res += "Ctrl+";
    if (flags & keyboard::ALT) res += "Alt+";
    if (flags & keyboard::SHIFT) res += "Shift+";
    if (flags & keyboard::WIN) res += "Win+";
    res += keyboard::getNamesForKey(name)[0];
    return res;
}

//bool writePNG(const cv::Mat& image, const std::string& filename) {
//    std::filesystem::path fpath(filename);
//    std::filesystem::path directory_path = fpath.parent_path();
//    if (!std::filesystem::exists(directory_path))
//        std::filesystem::create_directories(directory_path);
//
//    png_image pimg = {};
//    pimg.version = PNG_IMAGE_VERSION;
//    pimg.width = image.cols;
//    pimg.height = image.rows;
//
//    if (image.channels() == 4)
//        pimg.format = PNG_FORMAT_RGBA;
//    else if (image.channels() == 3)
//        pimg.format = PNG_FORMAT_RGB;
//    else if (image.channels() == 1)
//        pimg.format = PNG_FORMAT_GRAY;
//    else {
//        LOG(ERROR) << "Unknown image format, num channels: " << image.channels();
//        return false;
//    }
//
//    int ok = png_image_write_to_file(&pimg, filename.c_str(), 0, image.data, image.step, nullptr);
//
//    return ok;
//}
//
