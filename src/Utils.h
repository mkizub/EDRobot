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
inline std::string operator*(std::string_view sv) {
    return std::string(sv);
}

extern std::string getErrorMessage();
extern std::string getErrorMessage(unsigned errorCode);
extern void pasteToClipboard(const std::string& text);
extern std::string textFromClipboard();
extern std::string trim(const char* source);
extern std::string trim(std::string_view source);
extern std::wstring trim(std::wstring_view source);
extern std::string trimWithPunktuation(std::string_view source);
extern std::wstring trimWithPunktuation(std::wstring_view source);
extern std::string trimTextLine(std::string_view source);
extern std::wstring trimTextLine(std::wstring_view source);
extern std::string toString(const char* buffer, size_t size);
extern std::wstring toString(const wchar_t* buffer, size_t size);
extern std::string toUtf8(const wchar_t* buffer, size_t size);
extern std::string toUtf8(std::wstring_view str);
extern std::wstring toUtf16(const char* buffer, size_t size);
extern std::wstring toUtf16(std::string_view str);
extern std::string toLower(std::string_view str);
extern std::string toUpper(std::string_view str);
extern std::wstring toLower(std::wstring_view str);
extern std::wstring toUpper(std::wstring_view str);
extern bool equalsIgnoreCase(std::string_view str1, std::string_view str2);
inline bool isLatinLetter(char ch) { return ch >= 'A' && ch <= 'Z' || ch >= 'a' && ch <= 'z'; }
std::vector<std::string> split(const std::string &s, char delim);
bool contains(const std::vector<std::string>& strings, const std::string& str);
class Commodity;
bool contains(const std::vector<Commodity*>& inventory, Commodity* com);

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

extern std::string formatTimestampString(Timestamp timestamp, bool nanos=false);
extern bool parseTimestampString(std::string_view str, Timestamp& timestamp);
extern bool parseTimestamp(const js::value& value, Timestamp& timestamp);
extern dist_t parseDist(std::wstring dist, int conf);
extern int parseDistTime(std::wstring dist);
extern bool parseInt(std::string_view str, int64_t& out);
extern bool parseReal(std::string_view str, double& out);

extern js::value parseJsonFile(std::wstring_view file);

#endif //EDROBOT_UTILS_H
