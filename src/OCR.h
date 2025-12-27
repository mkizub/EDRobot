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

enum TextType {
    GENERIC,
    GENERIC_RAW,
    DISTANCE,
    NUMERIC,
};

extern bool init(const std::string& tessdata);
extern void shutdown();
// returns confidence 0..100, fill text, rect
extern int ocrLine(TextType tt, const char* dbg, const cv::Mat& grayImage, std::string& text, cv::Rect* rectOut);

extern int ocrRowText(TextType tt, const cv::Mat& grayImage, const ResolvedEnv&, const ClassifiedRect&, std::string_view tab_name, std::string& text, cv::Rect* rectOut=nullptr);
extern int ocrRowTextForTraining(TextType tt, const cv::Mat& grayImage, const ResolvedEnv&, const ClassifiedRect&, std::string_view tab_name, std::string& text, cv::Mat& dumpImage);

extern int ocrMarketLblText(const cv::Mat& grayImage, const ResolvedEnv&, const ClassifiedRect&, std::string& text);
extern int ocrMarketLblTextForTraining(const cv::Mat& grayImage, const ResolvedEnv&, const ClassifiedRect&, std::vector<std::string>& texts, std::vector<cv::Mat>& dumpImages);

extern int ocrNavigationLblText(const cv::Mat& grayImage, const ResolvedEnv&, const ClassifiedRect&, std::string& text);
extern int ocrNavigationLblTextForTraining(const cv::Mat& grayImage, const ResolvedEnv&, const ClassifiedRect&, std::string& text, cv::Mat& dumpImage);

extern cv::Mat normalizeTargetDistText(const cv::Mat& grayImage);
extern int ocrTargetDistText(const cv::Mat& grayImage, std::string& text);

extern int ocrTileLblText(const cv::Mat& grayImage, WState ws, std::string& text);

extern cv::Mat normalizeDetectorText(const cv::Mat& grayImage);
extern int ocrDetectorText(TextType tt, const cv::Mat& grayImage, const ResolvedEnv&, std::string& text, cv::Rect* rectOut);
}

#endif //EDROBOT_OCR_H
