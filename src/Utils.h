//
// Created by mkizub on 23.05.2025.
//

#pragma once

#ifndef EDROBOT_UTILS_H
#define EDROBOT_UTILS_H

inline RECT toRECT(cv::Rect& r) {
    return {r.x, r.y, r.br().x, r.br().y};
}
inline cv::Rect fromRECT(RECT& r) {
    return {r.left, r.top, r.right-r.left, r.bottom-r.top};
}

extern std::string getErrorMessage();
extern std::string getErrorMessage(unsigned errorCode);
extern void pasteToClipboard(const std::string& text);
extern std::string trim(const char* source);
extern std::string trim(const std::string & source);
extern std::wstring trim(const std::wstring & source);
extern std::string trimWithPunktuation(const std::string & source);
extern std::wstring trimWithPunktuation(const std::wstring & source);
extern std::string toString(const char* buffer, size_t size);
extern std::wstring toString(const wchar_t* buffer, size_t size);
extern std::string toUtf8(const wchar_t* buffer, size_t size);
extern std::string toUtf8(const std::wstring& str);
extern std::wstring toUtf16(const char* buffer, size_t size);
extern std::wstring toUtf16(const std::string& str);
extern std::string toLower(const std::string& str);
extern std::string toUpper(const std::string& str);
extern bool equalsIgnoreCase(const std::string_view& str1, const std::string_view& str2);
inline bool isLatinLetter(char ch) { return ch >= 'A' && ch <= 'Z' || ch >= 'a' && ch <= 'z'; }
std::vector<std::string> split(const std::string &s, char delim);
bool contains(const std::vector<std::string>& strings, const std::string& str);

extern cv::Vec3b encodeBGR(unsigned bgr);
extern unsigned decodeBGR(const cv::Vec3b& bgr);
extern cv::Vec3b lBgr2sBgr(const cv::Vec3f& lbgr);
extern cv::Vec3f sBgr2lBgr(const cv::Vec3b& sbgr);

extern unsigned  sBgr2sGray(const cv::Vec3b& sbgr);
extern cv::Vec3b sGray2sBgr(unsigned gray);
extern cv::Vec3b sBgr2Luv(const cv::Vec3b& sbgr);
extern cv::Vec3b sBgr2Hsv(const cv::Vec3b& sbgr);
extern cv::Vec3b luv2sBgr(const cv::Vec3b& luv);
extern cv::Vec3b hsv2sBgr(const cv::Vec3b& hsv);
extern int distanceBGR(const cv::Vec3b& bgr1, const cv::Vec3b& bgr2);
extern int distanceLuv(const cv::Vec3b& luv1, const cv::Vec3b& luv2);
extern int distanceHsv(const cv::Vec3b& hsv1, const cv::Vec3b& hsv2);

extern std::pair<std::string,unsigned> decodeShortcut(std::string key);
extern std::string encodeShortcut(const std::string& name, unsigned flags);

struct dist_t {
    enum Unit {
        X, M, KM, MM, LS, LY
    } unit;
    double dist;

    dist_t() : unit(X), dist(0) {}
    dist_t(Unit u, double d) : unit(u), dist(d) {}

    operator bool() const { return unit != X; }
    dist_t convertTo(Unit u) const;
    std::string to_string() const;

    friend std::ostream& operator<<(std::ostream& os, const dist_t& obj);
};
extern dist_t parseDist(std::wstring dist);


#endif //EDROBOT_UTILS_H
