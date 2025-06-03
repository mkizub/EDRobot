//
// Created by mkizub on 23.05.2025.
//

#include "pch.h"

#include "Template.h"
#include <format>
#include <iomanip>

void ClassifyEnv::init(const cv::Rect& captRect, const cv::Mat& imgColor, const cv::Mat& imgGray) {
    ClassifyEnv* self = const_cast<ClassifyEnv*>(this);
    const_cast<cv::Rect&>(captureRect) = captRect;
    const_cast<cv::Rect&>(captureCrop) = cv::Rect(cv::Point(), captureRect.size());
    const_cast<cv::Mat&>(imageColor) = imgColor;
    const_cast<cv::Mat&>(imageGray) = imgGray;
    if (captureRect.size() != ReferenceScreenSize) {
        double x_scale = double(captureRect.width) / ReferenceScreenSize.width;
        double y_scale = double(captureRect.height) / ReferenceScreenSize.height;
        scaleToCaptured_ = std::min(x_scale, y_scale);
        needScaling_ = true;
    } else {
        needScaling_ = false;
        scaleToCaptured_ = 1;
    }
    captureCenter = cv::Point(captureRect.size()) / 2;
}

void ClassifyEnv::clear() {
    const_cast<cv::Rect&>(captureRect) = cv::Rect();
    const_cast<cv::Rect&>(captureCrop) = cv::Rect();
    const_cast<cv::Mat&>(imageColor) = cv::Mat();
    const_cast<cv::Mat&>(imageGray) = cv::Mat();
    const_cast<cv::Mat&>(debugImage) = cv::Mat();
    needScaling_ = false;
    scaleToCaptured_ = 1;
    captureCenter = ReferenceScreenCenter;
    classifiedRects.clear();
}

cv::Point ClassifyEnv::cvtReferenceToDesktop(const cv::Point& point) const {
    return cvtReferenceToCaptured(point) + captureRect.tl();
}

cv::Point ClassifyEnv::cvtReferenceToCaptured(const cv::Point& point) const {
    cv::Point screenPoint(point);
    if (needScaling_) {
        cv::Point relative = screenPoint - ReferenceScreenCenter;
        relative *= scaleToCaptured_;
        screenPoint = relative + captureCenter;
    }
    return screenPoint;
}
cv::Rect  ClassifyEnv::cvtReferenceToCaptured(const cv::Rect& rect) const {
    cv::Rect screenRect(rect);
    if (needScaling_) {
        cv::Point lt_rel = screenRect.tl() - ReferenceScreenCenter;
        cv::Point rb_rel = screenRect.br() - ReferenceScreenCenter;
        lt_rel *= scaleToCaptured_;
        rb_rel *= scaleToCaptured_;
        cv::Point lt = lt_rel + captureCenter;
        cv::Point rb = rb_rel + captureCenter;
        screenRect = cv::Rect(lt, rb);
    }
    return screenRect;
}

cv::Point ClassifyEnv::cvtCapturedToReference(const cv::Point& point) const {
    cv::Point referencePoint(point);
    if (needScaling_) {
        cv::Point lt_rel = referencePoint - captureCenter;
        lt_rel /= scaleToCaptured_;
        cv::Point lt = lt_rel + ReferenceScreenCenter;
        referencePoint = lt;
    }
    return referencePoint;
}
cv::Rect  ClassifyEnv::cvtCapturedToReference(const cv::Rect& rect) const {
    cv::Rect referenceRect(rect);
    if (needScaling_) {
        cv::Point lt_rel = referenceRect.tl() - captureCenter;
        cv::Point rb_rel = referenceRect.br() - captureCenter;
        lt_rel /= scaleToCaptured_;
        rb_rel /= scaleToCaptured_;
        cv::Point lt = lt_rel + ReferenceScreenCenter;
        cv::Point rb = rb_rel + ReferenceScreenCenter;
        referenceRect = cv::Rect(lt,rb);
    }
    return referenceRect;
}

double SequenceTemplate::match(ClassifyEnv& env) {
    double sum = 0;
    for (auto& oracle : oracles) {
        double value = oracle->classify(env);
        if (value <= 0)
            return 0;
        if (sum < 1)
            sum += 2*(value-0.5);
    }
    return sum;
}

double SequenceTemplate::classify(ClassifyEnv& env) {
    const auto sz = env.classifiedRects.size();
    double result = match(env);
    if (result < 0.96) {
        while (sz < env.classifiedRects.size())
            env.classifiedRects.pop_back();
    }
    return result;
}

double SequenceTemplate::debugMatch(ClassifyEnv& env) {
    json5pp::value j_arr = json5pp::array({});
    for (auto& oracle : oracles) {
        double value = oracle->debugMatch(env);
        j_arr.as_array().emplace_back(value);
    }
    double sum = 0;
    for (auto& jv : j_arr.as_array()) {
        auto value = jv.as_number();
        if (value <= 0) { sum = 0; break; }
        if (sum < 1)
            sum += 2*(value-0.5);
        if (sum >= 1) { sum = 1; }
    }
    LOG(INFO) << "match result: " << sum << (sum >= 1 ? " (pass)" : " (fail)") << " for " << j_arr;
    return sum;
}

HistogramTemplate::HistogramTemplate(unsigned value, bool gray, spEvalRect rect)
    : mGray(gray)
{
    mRect.swap(rect);
    if (mGray) {
        mValueGray = value & 0xFF;
        mValueRGB = gray2rgb(mValueGray);
        mValueLuv = rgb2luv(mValueRGB);
    } else {
        mValueRGB = encodeRGB(value);
        mValueGray = rgb2gray(mValueRGB);
        mValueLuv = rgb2luv(mValueRGB);
    }
}

HistogramTemplate::HistogramTemplate(cv::Vec3b luv, spEvalRect rect)
    : mGray(false)
{
    mRect.swap(rect);
    mValueLuv = luv;
    mValueRGB = luv2rgb(luv);
    mValueGray = rgb2gray(mValueRGB);
}

double HistogramTemplate::match(ClassifyEnv& env) {
    if (!mRect)
        return 0;
    cv::Rect rect = mRect->calcRect(env);
    rect = env.cvtReferenceToCaptured(rect);
    rect &= env.captureCrop;
    if (rect.empty())
        return 0;
    int colorPlanes;
    std::vector<cv::Mat> imagePlanes;
    if (mGray) {
        colorPlanes = 1;
        if (env.imageGray.empty())
            return 0;
        imagePlanes.push_back(env.imageGray);
    } else {
        colorPlanes = 3;
        if (env.imageColor.empty())
            return 0;
        cv::split(env.imageColor, imagePlanes);
    }
    unsigned resultColor = 0;
    for (auto i=0; i < colorPlanes; i++) {
        int histSize = 256;
        float range[]{0, 256}; //the upper boundary is exclusive
        const float* histRange[]{range};
        cv::Mat subImage(imagePlanes[i], rect);
        cv::Mat hist;
        cv::calcHist(&subImage, 1, nullptr, cv::Mat(), hist, 1, &histSize, histRange);
        int maxLoc[4]{};
        cv::minMaxIdx(hist, nullptr, nullptr, nullptr, maxLoc);
        resultColor |= maxLoc[0] << (i*8);
    }
    mLastColorRGB = mGray ? gray2rgb(resultColor) : encodeRGB(resultColor);
    mLastColorLuv = rgb2luv(mLastColorRGB);
    LOG(DEBUG) << "Colors: result RGB:" << mLastColorRGB << " Luv:" << mLastColorLuv << "; expected Luv:" << mValueLuv;
    double dist = cv::norm(mLastColorLuv - mValueLuv);
    LOG(DEBUG) << "Distance: " << dist;
    mLastValue = 1.0 - std::erf(dist);
    imagePlanes.clear();
    return mLastValue;
}
double HistogramTemplate::classify(ClassifyEnv& env) {
    return match(env) >= 0.8;
}
double HistogramTemplate::debugMatch(ClassifyEnv& env) {
    return match(env);
}

ImageTemplate::ImageTemplate(const std::string& name, const std::string& filename, cv::Mat image, spEvalRect refRect, cv::Point extLT, cv::Point extRB, double tmin, double tmax)
    : name(name)
    , filename(filename)
    , extendLT{extLT}
    , extendRB{extRB}
    , threshold_min(tmin)
    , threshold_max(tmax)
    , lastScale(1)
{
    referenceRect.swap(refRect);

    if (image.empty()) {
        image = cv::imread(filename, cv::IMREAD_UNCHANGED);
        if (image.empty()) {
            LOG(ERROR) << "Template image " << filename << " not found";
            throw std::runtime_error(std::format("Cannot read %s", filename));
        }
    }
    // get grayscale
    cv::cvtColor(image, templGray, cv::COLOR_RGBA2GRAY);
    // extract mask
    std::vector<cv::Mat> channels;
    cv::split(image, channels);
    templMask = channels[3];
    int nonZeroCount = cv::countNonZero(templMask);
    if (nonZeroCount == templMask.total()) {
        templMask.release();
    } else {
        cv::Mat mask;
        cv::threshold(templMask, mask, 127, 255, cv::THRESH_BINARY);
        templMask = mask;
    }

    templGrayScaled = templGray;
    templMaskScaled = templMask;
}

double ImageTemplate::match(ClassifyEnv& env) {
    if (!this->referenceRect)
        return 0;
    cv::Rect referenceRect = this->referenceRect->calcRect(env);
    if (referenceRect.empty())
        return 0;
    cv::Mat image = env.imageGray;
    if (image.empty() || templGray.empty())
        return 0;
    if (image.cols != env.ReferenceScreenSize.width || image.rows != env.ReferenceScreenSize.height) {
        double x_scale = double(image.cols) / env.ReferenceScreenSize.width;
        double y_scale = double(image.rows) / env.ReferenceScreenSize.height;
        double scale = std::min(x_scale, y_scale);
        cv::resize(templGray, templGrayScaled, cv::Size(), scale, scale);
        if (!templMask.empty()) {
            cv::resize(templMask, templMaskScaled, templGrayScaled.size(), scale, scale);
            cv::Mat mask;
            cv::threshold(templMaskScaled, mask, 127, 255, cv::THRESH_BINARY);
            templMaskScaled = mask;
        }
        //cv::imwrite("scaled-template.png", templGrayScaled);
        //cv::imwrite("scaled-screen-region.png", cv::Mat(image, screenRectScaled));
    }
    //cv::Rect matchRect = getMatchRect(env, image.cols, image.rows);
    int ext = Master::getInstance().getSearchRegionExtent();
    captureRect = env.cvtReferenceToCaptured(referenceRect);
    matchRect = cv::Rect(captureRect.tl()-env.scaleToCaptured(extendLT+cv::Point(ext,ext)),
                         captureRect.br()+env.scaleToCaptured(extendRB+cv::Point(ext,ext)));
    matchRect &= env.captureCrop;
    int result_cols = matchRect.width - templGrayScaled.cols + 1;
    int result_rows = matchRect.height - templGrayScaled.rows + 1;
    cv::Mat result(result_rows, result_cols, CV_32FC1);
    cv::matchTemplate(cv::Mat(image, matchRect), templGrayScaled, result, cv::TM_CCOEFF_NORMED, templMaskScaled); // cv::TM_CCORR_NORMED
    // bypass error in cv::matchTemplate that sometimes return NaN/Inf, instead of [0..1] valies
    float* ptr = result.ptr<float>(0);
    float* pend = ptr + result_rows * result_cols;
    for (; ptr < pend; ++ptr) {
        if (std::isnan(*ptr) || std::isinf(*ptr))
            *ptr = 0;
    }
    //LOG(ERROR) << "match result: " << result;
    double minVal, maxVal;
    cv::Point minLoc, maxLoc;
    cv::minMaxLoc(result, &minVal, &maxVal, &minLoc, &maxLoc);
    LOG(DEBUG) << "match result: " << std::setprecision(3) << maxVal << " for " << filename;
    if (!name.empty() && maxVal >= threshold_min) {
        matchedCaptureOffset = maxLoc - (captureRect.tl() - matchRect.tl());
        env.classifiedRects.emplace_back(maxVal, name, referenceRect, referenceRect + env.scaleToReference(matchedCaptureOffset));
    }
    return maxVal;
}

double ImageTemplate::classify(ClassifyEnv& env) {
    double value = match(env);
    return toResult(value);
}

double ImageTemplate::toResult(double matchValue) {
    if (matchValue >= threshold_max)
        return 1;
    if (matchValue < threshold_min)
        return 0;
    double x = (matchValue - threshold_min) / (threshold_max - threshold_min);
    x = (x - 0.5) * 8;
    return 1 / (1 + std::exp(-x));
}

double ImageTemplate::debugMatch(ClassifyEnv& env) {
    double value = match(env);
    LOG(INFO) << "match result: " << std::setprecision(3) << value <<
               "[" << threshold_min << ":" << threshold_max << "] >> " << toResult(value) << " for " << filename <<
               "; offset: " << env.scaleToReference(matchedCaptureOffset) << "; scale: " << lastScale;
    if (value >= threshold_max) {
        cv::Scalar color(96, 255, 96);
        cv::rectangle(env.debugImage, captureRect.tl()+matchedCaptureOffset, captureRect.br()+matchedCaptureOffset, color, 3);
        cv::rectangle(env.debugImage, matchRect.tl(), matchRect.br(), color, 1);
        return 1;
    }
    if (value < threshold_min) {
        cv::Scalar color(96, 96, 255);
        cv::rectangle(env.debugImage, matchRect.tl(), matchRect.br(), color, 1);
        cv::Point lt = matchRect.tl();
        cv::Point rb = matchRect.br();
        cv::line(env.debugImage, lt, rb, color, 1);
        cv::Point lb = cv::Point(matchRect.tl().x, matchRect.br().y);
        cv::Point rt = cv::Point(matchRect.br().x, matchRect.tl().y);
        cv::line(env.debugImage, lb, rt, color, 1);
        return 0;
    }
    double result = toResult(value);
    cv::Scalar color(96, 210, 210);
    cv::rectangle(env.debugImage, captureRect.tl()+matchedCaptureOffset, captureRect.br()+matchedCaptureOffset, color, 2);
    cv::rectangle(env.debugImage, matchRect.tl(), matchRect.br(), color, 1);
    cv::Point lt = matchRect.tl();
    cv::Point rb = matchRect.br();
    cv::line(env.debugImage, lt, rb, color, 1);
    return result;
}
