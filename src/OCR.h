//
// Created by mkizub on 22.07.2025.
//

#pragma once

#include "pch.h"

#ifndef EDROBOT_OCR_H
#define EDROBOT_OCR_H

namespace ocr {

// I use market dialog label for commodity name for reference font metrics and train OCR on these metrics.
// Font height (ascent+descent) is 32 pixels
// font leading (inter-line distance) is 6 pixels (also this leading space is used for icons in nav panel list)
// So full line height is 38 pixels
// baseline is 8 pixels from bottom, 30 pixels from top

const int LINE_HEIGHT  = 38;
const int LEADING      = 6;
const int ASCENT       = 24;
const int DESCENT      = 8;
const int BASELINE_Y   = 30;

extern void init(const std::string& tessdata, Lang lng);
extern void shutdown();
// returns confidence 0..100, fill text, rect
extern int ocrLine(const char* dbg, const cv::Mat& grayImage, std::string& text, cv::Rect* rectOut);

extern int ocrRowText(const cv::Mat& grayImage, const ResolvedEnv&, const ClassifiedRect&, int tab, std::string& text, cv::Rect* rectOut=nullptr);
extern int ocrRowTextForTraining(const cv::Mat& grayImage, const ResolvedEnv&, const ClassifiedRect&, int tab, std::string& text, cv::Mat& dumpImage);

extern int ocrMarketLblText(const cv::Mat& grayImage, const ResolvedEnv&, const ClassifiedRect&, std::string& text);
extern int ocrMarketLblTextForTraining(const cv::Mat& grayImage, const ResolvedEnv&, const ClassifiedRect&, std::vector<std::string>& texts, std::vector<cv::Mat>& dumpImages);

extern int ocrNavigationLblText(const cv::Mat& grayImage, const ResolvedEnv&, const ClassifiedRect&, std::string& text);
extern int ocrNavigationLblTextForTraining(const cv::Mat& grayImage, const ResolvedEnv&, const ClassifiedRect&, std::string& text, cv::Mat& dumpImage);

}

#endif //EDROBOT_OCR_H
