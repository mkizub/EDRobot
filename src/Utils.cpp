//
// Created by mkizub on 23.05.2025.
//

#include "pch.h"

#include <locale>
#include <codecvt>

#include "Keyboard.h"

std::string getErrorMessage() {
    return getErrorMessage(GetLastError());
}
std::string getErrorMessage(unsigned errorCode) {
    TCHAR messageBuffer[256];
    size_t size = FormatMessage(
            FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
            nullptr,
            errorCode,
            MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US),
            messageBuffer,
            0,
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

cv::Vec3b encodeRGB(unsigned rgb) {
    return cv::Vec3b(rgb & 0xFF, (rgb>>8) & 0xFF, (rgb>>16)&0xFF);
}
unsigned decodeRGB(cv::Vec3b rgb) {
    return rgb[0] | (rgb[1] << 8) | (rgb[2] << 16);
}

unsigned rgb2gray(cv::Vec3b rgb) {
    cv::Mat3b in(rgb, false);
    cv::Mat1b out;
    cv::cvtColor(in, out, cv::COLOR_RGB2GRAY);
    return out.at<uchar>(0);
}
cv::Vec3b gray2rgb(unsigned gray) {
    cv::Mat3b in;
    in.at<uchar>(0) = gray;
    cv::Mat1b out;
    cv::cvtColor(in, out, cv::COLOR_GRAY2RGB);
    return out.at<cv::Vec3b>(0);
}
cv::Vec3b rgb2luv(const cv::Vec3b& rgb) {
    cv::Mat3b in(rgb, false);
    cv::Mat3b out;
    cv::cvtColor(in, out, cv::COLOR_RGB2Luv);
    return out.at<cv::Vec3b>(0);
}
cv::Vec3b luv2rgb(const cv::Vec3b& luv) {
    cv::Mat3b in(luv, false);
    cv::Mat3b out;
    cv::cvtColor(in, out, cv::COLOR_Luv2RGB);
    return out.at<cv::Vec3b>(0);
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

