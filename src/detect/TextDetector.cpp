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
    ocr::TextType tt = mRaw ? ocr::TextType::GENERIC_RAW : ocr::TextType::GENERIC;
    int ocr_conf = ocr::ocrDetectorText(tt, ocrImage, env, text, &ocrRect);
    if (ocr_conf < mOcrConfThreshold)
        return 0;
    std::wstring wtext = toUtf16(text);
    double bestRatio = 0;
    const std::string* bestLabel = nullptr;
    FuzzyMatch fm;
    for (auto& lbl : labels) {
        for (auto& t : lbl.second) {
            double r = fm.ratio(wtext, t);
            if (r > bestRatio) {
                bestRatio = r;
                bestLabel = &lbl.first;
            }
        }
    }
    cv::Rect detectedRect = env.cvtCapturedToReference(ocrRect + captureRect.tl());
    double matchValue = bestRatio * ocr_conf * 1.e-4;
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
    tdet.matchRect = ocrRect + captureRect.tl();
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