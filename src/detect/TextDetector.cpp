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
    std::string text;
    cv::Rect ocrRect;
    double line_height = captureRect.height;
    if (mFontHeight.has_value())
        line_height = mFontHeight.value() * env.getScale();
    int ocr_conf = ocr::ocrDetectorText(ocr::GENERIC, line_height, mMultiLine, ocrImage, env, text, &ocrRect);
    if (ocr_conf < mOcrConfThreshold)
        return 0;
    std::wstring wtext = toUtf16(text);
    double bestRatio = 0;
    const std::string* bestLabel = nullptr;
    cv::Rect bestRect;
    FuzzyMatch fm;
    for (auto& lbl : labels) {
        for (auto& t : lbl.second) {
            std::wstring wt = wtext.substr(0, t.size());
            double r = fm.ratio(wt, t);
            if (r > bestRatio) {
                bestRatio = r;
                bestLabel = &lbl.first;
                bestRect = ocrRect;
            }
        }
    }
    cv::Rect detectedRect = env.cvtCapturedToReference(bestRect + captureRect.tl());
    double matchValue = bestRatio * 0.01;
    if (matchValue < mThresholdMin || !bestLabel) {
        LOG_DEBUG("TextDetector matched failed: {:.3f} ({:.3f}) for {} rect [{}:{},{}x{}]",
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
    LOG_DEBUG("TextDetector matched result: {:.3f} ({:.3f}) for {} rect [{}:{},{}x{}]",
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