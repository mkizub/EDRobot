//
// Created by mkizub on 23.05.2025.
//

#pragma once

#ifndef EDROBOT_UTILS_H
#define EDROBOT_UTILS_H

extern std::string getErrorMessage();
extern std::string getErrorMessage(unsigned errorCode);
extern void pasteToClipboard(const std::string& text);
extern std::string trim(const std::string & source);
extern std::string toString(char* buffer, size_t size);
extern std::string toString(wchar_t* buffer, size_t size);
extern std::string toLower(const std::string& str);
extern std::string toUpper(const std::string& str);
inline bool isLatinLetter(char ch) { return ch >= 'A' && ch <= 'Z' || ch >= 'a' && ch <= 'z'; }

extern cv::Vec3b encodeRGB(unsigned rgb);
extern unsigned decodeRGB(cv::Vec3b rgb);

extern unsigned  rgb2gray(cv::Vec3b rgb);
extern cv::Vec3b gray2rgb(unsigned gray);
extern cv::Vec3b rgb2luv(const cv::Vec3b& rgb);
extern cv::Vec3b luv2rgb(const cv::Vec3b& luv);


#endif //EDROBOT_UTILS_H
