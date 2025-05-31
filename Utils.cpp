//
// Created by mkizub on 23.05.2025.
//

#include "pch.h"

#include <locale>
#include <codecvt>

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
    return toString(messageBuffer, size);
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

std::string trim(const std::string & source) {
    std::string s(source);
    s.erase(0,s.find_first_not_of(" \n\r\t"));
    s.erase(s.find_last_not_of(" \n\r\t")+1);
    return s;
}

std::string toString(char* buffer, size_t size) {
    return {buffer, size};
}
std::string toString(wchar_t* buffer, size_t size) {
    std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>, wchar_t> converter;
    std::string utf8_string = converter.to_bytes(buffer, buffer+size);
    return utf8_string;
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
