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

// Tesseract Page segmentation modes:
// 0    Orientation and script detection (OSD) only.
// 1    Automatic page segmentation with OSD.
// 2    Automatic page segmentation, but no OSD, or OCR. (not implemented)
// 3    Fully automatic page segmentation, but no OSD. (Default)
// 4    Assume a single column of text of variable sizes.
// 5    Assume a single uniform block of vertically aligned text.
// 6    Assume a single uniform block of text.
// 7    Treat the image as a single text line.
// 8    Treat the image as a single word.
// 9    Treat the image as a single word in a circle.
// 10   Treat the image as a single character.
// 11   Sparse text. Find as much text as possible in no particular order.
// 12   Sparse text with OSD.
// 13   Raw line. Treat the image as a single text line,
//      bypassing hacks that are Tesseract-specific.

enum TextType {
    GENERIC,
    DISTANCE,
    NUMERIC,
};

enum TextPSM {
    AUTO_PSM_3 = 3,
    AUTO_PSM_4 = 4,
    BLOCK_PSM_5 = 5,
    BLOCK_PSM_6 = 6,
    LINE_PSM_7 = 7,
    LINE_PSM_13 = 13,
};

extern bool init(const std::string& tessdata);
extern void shutdown();

extern bool ocrPageSegm(const cv::Mat& grayImage, cv::Rect& rectOut, std::vector<cv::Line>& baselineOut);
// returns confidence 0..100, fill text, rect
extern int ocrLine(TextType tt, int psm, const char* dbg, const cv::Mat& grayImage, int minConf, std::string& text, cv::Rect* rectOut);
extern cv::Mat normalizeTextImage(cv::Mat& grayImage, int blackPixelsLimit=50, int blackAdd=10, int whiteSub=10, int bin=4);

extern int ocrRowText(TextType tt, const cv::Mat& grayImage, const ResolvedEnv&, const ClassifiedRect&, std::string_view tab_name, std::string& text, cv::Rect* rectOut=nullptr);
extern int ocrRowTextForTraining(TextType tt, const cv::Mat& grayImage, const ResolvedEnv&, const ClassifiedRect&, std::string_view tab_name, std::string& text, cv::Mat& dumpImage);

extern int ocrMarketLblText(const cv::Mat& grayImage, const ResolvedEnv&, const ClassifiedRect&, std::string& text);
extern int ocrMarketLblTextForTraining(const cv::Mat& grayImage, const ResolvedEnv&, const ClassifiedRect&, std::vector<std::string>& texts, std::vector<cv::Mat>& dumpImages);

extern int ocrNavigationLblText(const cv::Mat& grayImage, const ResolvedEnv&, const ClassifiedRect&, std::string& text);
extern int ocrNavigationLblTextForTraining(const cv::Mat& grayImage, const ResolvedEnv&, const ClassifiedRect&, std::string& text, cv::Mat& dumpImage);

extern cv::Mat normalizeTargetDistText(cv::Mat& grayImage);
extern int ocrTargetDistText(cv::Mat grayImage, std::string& text);

extern int ocrTileLblText(double font_height, cv::Mat& grayImage, WState ws, std::string& text);

extern int ocrDetectorText(TextType tt, double font_height, bool multiline, const cv::Mat& grayImage, const ResolvedEnv&, std::string& text, cv::Rect* rectOut);
}

#endif //EDROBOT_OCR_H
