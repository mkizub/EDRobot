//
// Created by mkizub on 23.12.2025.
//
#include "../pch.h"

#include "Detector.h"
#include "TextDetector.h"
#include "../FuzzyMatch.h"
#include "../OCR.h"

namespace detect {

TextDetector::TextDetector(const string &name, spEvalRect rect)
    : name(name)
    , refEvalRect(rect)
{
}

double TextDetector::match(ClassifyEnv &env) {
    cv::Rect refRect = refEvalRect->calcReferenceRect(env);
    cv::Rect captureRect = env.cvtReferenceToCaptured(refRect);
    XMat roiColorImage = env.getColorImage()(captureRect);
    ChannelFilter grayFilter(ChannelFilter::gray);
    XMat roiGrayImage = grayFilter.apply(roiColorImage,{});
    if (roiGrayImage.empty())
        return 0;

    cv::Mat ocrImage = toMat(roiGrayImage);
    int ocr_conf_raw = 0;
    int ocr_conf_gen = 0;
    std::string text_raw;
    std::string text_gen;
    cv::Rect ocrRectRaw;
    cv::Rect ocrRectGen;
    double line_height = captureRect.height;
    if (mLineHeight.has_value())
        line_height = mLineHeight.value() * env.getScale();
    if (mOcrPSM.value_or(13) == 13) {
        ocr_conf_raw = ocr::ocrDetectorText(ocr::LINE_PSM_13, line_height, ocrImage, env, text_raw, &ocrRectRaw);
        if (ocr_conf_raw < mOcrConfThreshold)
            text_raw.clear();
    }
    {
        ocr::TextType tt;
        switch (mOcrPSM.value_or(7)) {
        case 3:
            tt = ocr::TextType::AUTO_PSM_3;
            break;
        case 4:
            tt = ocr::TextType::AUTO_PSM_4;
            break;
        case 5:
            tt = ocr::TextType::BLOCK_PSM_5;
            break;
        case 6:
            tt = ocr::TextType::BLOCK_PSM_6;
            break;
        case 7:
            tt = ocr::TextType::LINE_PSM_7;
            break;
        default:
            tt = ocr::TextType::LINE_PSM_13;
            break;
        }
        if (tt != ocr::TextType::LINE_PSM_13)
            ocr_conf_gen = ocr::ocrDetectorText(tt, line_height, ocrImage, env, text_gen, &ocrRectGen);
        if (ocr_conf_gen < mOcrConfThreshold)
            text_gen.clear();
    }
    if (text_raw.empty() && text_gen.empty())
        return 0;
    std::wstring wtext_raw = toUtf16(text_raw);
    std::wstring wtext_gen = toUtf16(text_gen);
    double bestRatio = 0;
    const std::string* bestLabel = nullptr;
    cv::Rect bestRect;
    FuzzyMatch fm;
    for (auto& lbl : labels) {
        for (auto& t : lbl.second) {
            if (!wtext_raw.empty()) {
                std::wstring wt = wtext_raw.substr(0, t.size());
                double r = fm.ratio(wt, t);
                if (r > bestRatio) {
                    bestRatio = r;
                    bestLabel = &lbl.first;
                    bestRect = ocrRectRaw;
                }
            }
            if (!wtext_gen.empty()) {
                std::wstring wt = wtext_gen.substr(0, t.size());
                double r = fm.ratio(wt, t);
                if (r > bestRatio) {
                    bestRatio = r;
                    bestLabel = &lbl.first;
                    bestRect = ocrRectGen;
                }
            }
        }
    }
    cv::Rect detectedRect = env.cvtCapturedToReference(bestRect + captureRect.tl());
    double matchValue = bestRatio * 0.01;
    if (matchValue < mThresholdMin || !bestLabel) {
        LOG(DEBUG) << std::format("TextDetector matched failed: {:.3f} ({:.3f}) for {} rect [{}:{},{}x{}]",
                                  matchValue, toResult(matchValue), bestLabel ? *bestLabel : "?",
                                  detectedRect.x, detectedRect.y, detectedRect.width, detectedRect.height);
        return 0;
    }

    env.classified.emplace_back(ClsDetType::Detected, name + ":" + *bestLabel, detectedRect);
    auto& tdet = env.classified.back().u.tdet;
    tdet.referenceRect = refRect;
    tdet.scale = 1;
    tdet.angle = 0;
    tdet.match = matchValue;
    tdet.matchRect = bestRect + captureRect.tl();
    LOG(DEBUG) << std::format("TextDetector matched result: {:.3f} ({:.3f}) for {} rect [{}:{},{}x{}]",
                              matchValue, toResult(matchValue), *bestLabel,
                              detectedRect.x, detectedRect.y, detectedRect.width, detectedRect.height);

    return toResult(matchValue);
}

double TextDetector::toResult(double matchValue) {
    if (matchValue >= mThresholdMax)
        return 1;
    if (matchValue < mThresholdMin)
        return 0;
    double x = (matchValue - mThresholdMin) / (mThresholdMax - mThresholdMin);
    x = (x - 0.5) * 4.25;
    return std::clamp(1.125 / (1 + std::exp(-x)), 0.0, 1.0);
}

} // namespace detect