//
// Created by mkizub on 02.07.2025.
//
#include "../pch.h"

#include "Detector.h"

#include <iomanip>

namespace detect {

BaseImageTemplate::BaseImageTemplate(
        const std::string &filename, cv::Mat image, spEvalRect refRect)
        : filename(filename), referenceRect(std::move(refRect)), threshold_min(0.8), threshold_max(0.8) {
    if (image.empty()) {
        if (!filename.empty())
            loadImageAndMask(filename, templImage, templMask);
    } else {
        templImage = image;
        extractImageMask(image, templMask);
    }
}

bool BaseImageTemplate::loadImageAndMask(const std::string &filename, cv::Mat &image, cv::Mat &mask) {
    image = cv::imread(filename, cv::IMREAD_UNCHANGED); // assume BGR/BGRA
    if (image.empty()) {
        LOG(ERROR) << "Template image " << filename << " not found";
        throw std::runtime_error(std::format("Cannot read %s", filename));
    }
    extractImageMask(image, mask);
    return true;
}

bool BaseImageTemplate::extractImageMask(cv::Mat &image, cv::Mat &mask) {
    if (image.channels() == 4) {
        // extract mask
        std::vector<cv::Mat> channels;
        cv::split(image, channels);
        cv::Mat alphaMask = channels[3];
        double mean = cv::mean(alphaMask)[0];
        if (mean > 254) {
            mask.release();
        } else {
            alphaMask.convertTo(mask, CV_32F);
        }
        struct ClearAlpha {
            void operator()(cv::Vec4b &pixel, const int *position) const {
                pixel[3] = 255;
            }
        } Functor;
        image.forEach<cv::Vec4b>(Functor);
    } else if (image.channels() == 3) {
        cv::Mat bgra;
        cv::cvtColor(image, bgra, cv::COLOR_BGR2BGRA);
        image = bgra;
        mask.release();
    }
    return true;
}

double BaseImageTemplate::classify(ClassifyEnv &env) {
    double value = match(env);
    return toResult(value);
}


double BaseImageTemplate::debugMatch(ClassifyEnv &env) {
    double value = match(env);
    LOG(INFO) << "match result: " << std::setprecision(3) << value <<
              "[" << threshold_min << ":" << threshold_max << "] >> " << toResult(value) << " for " << filename <<
              "; offset: " << env.scaleToReference(matchedCaptureOffset);
    if (value >= threshold_max) {
        cv::Scalar color(96, 255, 96);
        cv::rectangle(env.getDebugImage(), captureRect.tl(), captureRect.br(), color, 1);
        cv::rectangle(env.getDebugImage(), matchRect.tl(), matchRect.br(), color, 1);
        return 1;
    }
    if (value < threshold_min) {
        cv::Scalar color(96, 96, 255);
        cv::rectangle(env.getDebugImage(), matchRect.tl(), matchRect.br(), color, 1);
        cv::Point lt = matchRect.tl();
        cv::Point rb = matchRect.br();
        cv::line(env.getDebugImage(), lt, rb, color, 1);
        cv::Point lb = cv::Point(matchRect.tl().x, matchRect.br().y);
        cv::Point rt = cv::Point(matchRect.br().x, matchRect.tl().y);
        cv::line(env.getDebugImage(), lb, rt, color, 1);
        return 0;
    }
    double result = toResult(value);
    cv::Scalar color(96, 210, 210);
    cv::rectangle(env.getDebugImage(), captureRect.tl(), captureRect.br(), color, 1);
    cv::rectangle(env.getDebugImage(), matchRect.tl(), matchRect.br(), color, 1);
    cv::Point lt = matchRect.tl();
    cv::Point rb = matchRect.br();
    cv::line(env.getDebugImage(), lt, rb, color, 1);
    return result;
}

double BaseImageTemplate::toResult(double matchValue) {
    if (matchValue >= threshold_max)
        return 1;
    if (matchValue < threshold_min)
        return 0;
    double x = (matchValue - threshold_min) / (threshold_max - threshold_min);
    x = (x - 0.5) * 8;
    return 1 / (1 + std::exp(-x));
}

cv::Mat BaseImageTemplate::applyFilters(cv::Mat image) {
    if (image.empty())
        return image;
    cv::Mat out = image;
    for (auto &filter: filters) {
        out = filter->apply(out);
    }
    return out;
}

cv::Mat BaseImageTemplate::scaleImage(cv::Mat image, double scaleX, double scaleY) {
    if (scaleX == 1 && scaleY == 1)
        return image;
    cv::Mat out;
    cv::resize(image, out, {}, scaleX, scaleY);
    return out;
}


cv::Mat GaussFilter::apply(cv::Mat image) {
    if (disabled)
        return image;
    cv::Mat out;
    cv::GaussianBlur(image, out, cv::Size(kernX, kernY), 0, 0);
    return out;
}

cv::Mat LaplacianFilter::apply(cv::Mat image) {
    cv::Mat lapl16S;
    cv::Mat lapl8U;
    cv::Laplacian(image, lapl16S, CV_16S, kern, scale);
    cv::convertScaleAbs(lapl16S, lapl8U);
    return lapl8U;
}

cv::Mat HsvColorCropFilter::apply(cv::Mat image) {
    if (ranges.empty())
        return image;
    cv::Mat hsv;
    cv::cvtColor(image, hsv, cv::COLOR_BGR2HSV);
    cv::Mat mask(image.cols, image.rows, CV_8UC1);
    if (ranges.size() == 1) {
        cv::inRange(hsv, ranges.front().first, ranges.front().second, mask);
        cv::Mat masked, gray;
    } else {
        for (auto &r: ranges) {
            cv::Mat m;
            cv::inRange(hsv, r.first, r.second, m);
            cv::bitwise_or(mask, m, mask);
        }
    }
    cv::Mat masked, gray;
    image.copyTo(masked, mask);
    cv::cvtColor(masked, gray, cv::COLOR_BGR2GRAY);
    return gray;
}

void BaseImageTemplate::fixNaNinResult(cv::Mat &result) {
    // bypass error in cv::matchTemplate that sometimes return NaN/Inf, instead of [0..1] valies
    auto *ptr = result.ptr<float>(0);
    auto *pend = ptr + result.rows * result.cols;
#ifndef NDEBUG
    bool bad_image = false;
#endif
    for (; ptr < pend; ++ptr) {
        if (std::isnan(*ptr) || std::isinf(*ptr)) {
#ifndef NDEBUG
            bad_image = true;
#endif
            *ptr = 0;
        }
    }
#ifndef NDEBUG
    LOG_IF(bad_image, ERROR) << "Bad image for TM_CCORR_NORMED: " << filename;
#endif
}

ImageTemplate::ImageTemplate(const std::string &filename, cv::Mat image, spEvalRect refRect)
        : BaseImageTemplate(filename, std::move(image), std::move(refRect)) {
}

double ImageTemplate::match(ClassifyEnv &env) {
    if (!this->referenceRect || templImage.empty())
        return 0;
    cv::Rect referenceRect = env.calcReferenceRect(this->referenceRect);
    if (referenceRect.empty())
        return 0;
    cv::Mat image = templImage.channels() == 1 ? env.getGrayImage() : env.getColorImage();
    if (image.empty())
        return 0;
    if (env.getScale() != preprocessedTemplateScale) {
        preprocessedTemplateScale = env.getScale();
        cv::Mat templImageFiltered = applyFilters(templImage);
        cv::resize(templImageFiltered, templImageScaled, cv::Size(), env.getScale(), env.getScale());
        if (!templMask.empty())
            cv::resize(templMask, templMaskScaled, templImageScaled.size(), env.getScale(), env.getScale());
    }
    int ext = Master::getInstance().getSearchRegionExtent();
    captureRect = env.cvtReferenceToCaptured(referenceRect);
    matchRect = cv::Rect(captureRect.tl() - env.scaleToCaptured(extendLT + cv::Point(ext, ext)),
                         captureRect.br() + env.scaleToCaptured(extendRB + cv::Point(ext, ext)));
    env.cropToCapture(matchRect);
    int result_cols = matchRect.width - templImageScaled.cols + 1;
    int result_rows = matchRect.height - templImageScaled.rows + 1;
    cv::Mat result(result_rows, result_cols, CV_32FC1);
    cv::Mat imagePrepared = cv::Mat(image, matchRect);
    imagePrepared = applyFilters(imagePrepared);
    //if (templMaskScaled.empty())
    cv::matchTemplate(imagePrepared, templImageScaled, result, cv::TM_CCOEFF_NORMED);
    //else
    //    cv::matchTemplate(imagePrepared, templImageScaled, result, cv::TM_CCORR_NORMED, templMaskScaled);
    fixNaNinResult(result);
    //LOG(ERROR) << "match result: " << result;
    double maxVal;
    cv::Point maxLoc;
    cv::minMaxLoc(result, nullptr, &maxVal, nullptr, &maxLoc);
    LOG(DEBUG) << "match result: " << std::setprecision(3) << maxVal << " for " << filename;
    if (!name.empty() && maxVal >= threshold_min) {
        matchedCaptureOffset = maxLoc - (captureRect.tl() - matchRect.tl());
        captureRect = {captureRect.tl() + matchedCaptureOffset, captureRect.br() + matchedCaptureOffset};
        env.classified.emplace_back(ClsDetType::Detected, env.isWarpMode(), name,
                                    referenceRect + env.scaleToReference(matchedCaptureOffset));
        env.classified.back().u.tdet.referenceRect = referenceRect;
        env.classified.back().u.tdet.scale = 1;
    }
    return maxVal;
}

double ImageTemplate::debugMatch(ClassifyEnv &env) {
    double result = BaseImageTemplate::debugMatch(env);
    //CompassDetector::tryLowerUpperBoundsGUI(env, matchRect);
    return result;
}


ImageMultiScaleTemplate::ImageMultiScaleTemplate(
        const string &filename, cv::Mat image, spEvalRect refRect, std::vector<double> scales)
        : BaseImageTemplate(filename, std::move(image), std::move(refRect)), scales(std::move(scales)),
          lastScaleIdx(-1), lastScale(1) {
}

double ImageMultiScaleTemplate::match(ClassifyEnv &env) {
    if (!this->referenceRect || templImage.empty())
        return 0;
    cv::Rect referenceRect = env.calcReferenceRect(this->referenceRect);
    if (referenceRect.empty())
        return 0;
    cv::Mat image = templImage.channels() == 1 ? env.getGrayImage() : env.getColorImage();
    if (image.empty())
        return 0;
    if (env.getScale() != preprocessedTemplateScale) {
        preprocessedTemplateScale = env.getScale();
        scaledImages.clear();
        cv::Mat tmpImageFiltered = applyFilters(templImage);
        for (double scale: scales) {
            cv::Mat tmpImage;
            cv::Mat tmpMask;
            cv::resize(tmpImageFiltered, tmpImage, cv::Size(), scale * env.getScale(), scale * env.getScale());
            if (!templMask.empty())
                cv::resize(templMask, tmpMask, tmpImage.size(), scale * env.getScale(), scale * env.getScale());
            scaledImages.emplace_back(scale, tmpImage, tmpMask);
        }
    }
    int ext = Master::getInstance().getSearchRegionExtent();
    captureRect = env.cvtReferenceToCaptured(referenceRect);
    matchRect = cv::Rect(captureRect.tl() - env.scaleToCaptured(extendLT + cv::Point(ext, ext)),
                         captureRect.br() + env.scaleToCaptured(extendRB + cv::Point(ext, ext)));
    env.cropToCapture(matchRect);

    lastScaleIdx = -1;
    lastScale = std::numeric_limits<double>::quiet_NaN();
    int bestScaleIdx = -1;
    double bestScaleVal = 0;
    cv::Point bestScaleLoc;
    cv::Mat imagePrepared = cv::Mat(image, matchRect);
    for (int scaleIdx = 0; scaleIdx < scales.size(); scaleIdx++) {
        auto &sm = scaledImages[scaleIdx];
        int result_cols = matchRect.width - sm.templImage.cols + 1;
        int result_rows = matchRect.height - sm.templImage.rows + 1;
        cv::Mat result(result_rows, result_cols, CV_32FC1);
        cv::matchTemplate(imagePrepared, sm.templImage, result, cv::TM_CCOEFF_NORMED, sm.templMask);
        fixNaNinResult(result);
        //LOG(ERROR) << "match result: " << result;
        double maxVal;
        cv::Point maxLoc;
        cv::minMaxLoc(result, nullptr, &maxVal, nullptr, &maxLoc);
        LOG(DEBUG) << "match result: " << std::setprecision(3) << maxVal << " for scale " << sm.scale << " file "
                   << filename;
        if (maxVal > bestScaleVal) {
            bestScaleVal = maxVal;
            bestScaleIdx = scaleIdx;
            bestScaleLoc = maxLoc;
        }
    }
    if (bestScaleVal >= threshold_min) {
        lastScaleIdx = bestScaleIdx;
        lastScale = scaledImages[bestScaleIdx].scale;
        matchedCaptureOffset = bestScaleLoc - (captureRect.tl() - matchRect.tl());
        captureRect += matchedCaptureOffset;
        captureRect.width *= lastScale;
        captureRect.height *= lastScale;
    }
    if (!name.empty() && bestScaleVal >= threshold_min) {
        env.classified.emplace_back(ClsDetType::Detected, env.isWarpMode(), name,
                                    referenceRect + env.scaleToReference(matchedCaptureOffset));
        env.classified.back().u.tdet.referenceRect = referenceRect;
        env.classified.back().u.tdet.scale = lastScale;
    }
    return bestScaleVal;
}

double ImageMultiScaleTemplate::debugMatch(ClassifyEnv &env) {
    double value = BaseImageTemplate::debugMatch(env);
    LOG(INFO) << "best match was at scale index " << lastScaleIdx << " scale " << std::format("{:.7f}", lastScale);
    return value;
}

} // detect