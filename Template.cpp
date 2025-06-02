//
// Created by mkizub on 23.05.2025.
//

#include "pch.h"

#include "Template.h"
#include <format>
#include <iomanip>


cv::Point ClassifyEnv::cvtReferenceToImage(const cv::Point& point) {
    cv::Point screenPoint(point);
    screenPoint.x -= captureRect.x;
    screenPoint.y -= captureRect.y;
    if (captureRect.width != REFERENCE_SCREEN_WIDTH || captureRect.height != REFERENCE_SCREEN_HEIGHT) {
        double x_scale = double(captureRect.width) / REFERENCE_SCREEN_WIDTH;
        double y_scale = double(captureRect.height) / REFERENCE_SCREEN_HEIGHT;
        double scale = std::min(x_scale, y_scale);
        cv::Point lt_rel = screenPoint - cv::Point(REFERENCE_SCREEN_WIDTH/2, REFERENCE_SCREEN_HEIGHT/2);
        lt_rel *= scale;
        cv::Point lt = lt_rel + cv::Point(captureRect.width/2, captureRect.height/2);
        screenPoint = lt;
    }
    return screenPoint;
}
cv::Size  ClassifyEnv::cvtReferenceToImage(const cv::Size& size) {
    cv::Size screenSize(size);
    if (captureRect.width != REFERENCE_SCREEN_WIDTH || captureRect.height != REFERENCE_SCREEN_HEIGHT) {
        double x_scale = double(captureRect.width) / REFERENCE_SCREEN_WIDTH;
        double y_scale = double(captureRect.height) / REFERENCE_SCREEN_HEIGHT;
        double scale = std::min(x_scale, y_scale);
        screenSize.width *= scale;
        screenSize.height *= scale;
    }
    return screenSize;
}
cv::Rect  ClassifyEnv::cvtReferenceToImage(const cv::Rect& rect) {
    cv::Rect screenRect(rect);
    screenRect.x -= captureRect.x;
    screenRect.y -= captureRect.y;
    if (captureRect.width != REFERENCE_SCREEN_WIDTH || captureRect.height != REFERENCE_SCREEN_HEIGHT) {
        double x_scale = double(captureRect.width) / REFERENCE_SCREEN_WIDTH;
        double y_scale = double(captureRect.height) / REFERENCE_SCREEN_HEIGHT;
        double scale = std::min(x_scale, y_scale);
        cv::Point lt_rel = screenRect.tl() - cv::Point(REFERENCE_SCREEN_WIDTH/2, REFERENCE_SCREEN_HEIGHT/2);
        cv::Point rb_rel = screenRect.br() - cv::Point(REFERENCE_SCREEN_WIDTH/2, REFERENCE_SCREEN_HEIGHT/2);
        lt_rel *= scale;
        rb_rel *= scale;
        cv::Point lt = lt_rel + cv::Point(captureRect.width/2, captureRect.height/2);
        cv::Point rb = rb_rel + cv::Point(captureRect.width/2, captureRect.height/2);
        screenRect = cv::Rect(lt, rb);
    }
    return screenRect;
}

cv::Point ClassifyEnv::cvtImageToReference(const cv::Point& point) {
    cv::Point referencePoint(point);
    if (captureRect.width != REFERENCE_SCREEN_WIDTH || captureRect.height != REFERENCE_SCREEN_HEIGHT) {
        double x_scale = double(captureRect.width) / REFERENCE_SCREEN_WIDTH;
        double y_scale = double(captureRect.height) / REFERENCE_SCREEN_HEIGHT;
        double scale = std::min(x_scale, y_scale);
        cv::Point lt_rel = referencePoint - cv::Point(captureRect.width/2, captureRect.height/2);
        lt_rel /= scale;
        cv::Point lt = lt_rel + cv::Point(REFERENCE_SCREEN_WIDTH/2, REFERENCE_SCREEN_HEIGHT/2);
        referencePoint = lt;
    }
    referencePoint.x += captureRect.x;
    referencePoint.y += captureRect.y;
    return referencePoint;
}
cv::Size  ClassifyEnv::cvtImageToReference(const cv::Size& size) {
    cv::Size referenceSize(size);
    if (captureRect.width != REFERENCE_SCREEN_WIDTH || captureRect.height != REFERENCE_SCREEN_HEIGHT) {
        double x_scale = double(captureRect.width) / REFERENCE_SCREEN_WIDTH;
        double y_scale = double(captureRect.height) / REFERENCE_SCREEN_HEIGHT;
        double scale = std::min(x_scale, y_scale);
        referenceSize.width /= scale;
        referenceSize.height /= scale;
    }
    return referenceSize;
}
cv::Rect  ClassifyEnv::cvtImageToReference(const cv::Rect& rect) {
    cv::Rect referenceRect(rect);
    if (captureRect.width != REFERENCE_SCREEN_WIDTH || captureRect.height != REFERENCE_SCREEN_HEIGHT) {
        double x_scale = double(captureRect.width) / REFERENCE_SCREEN_WIDTH;
        double y_scale = double(captureRect.height) / REFERENCE_SCREEN_HEIGHT;
        double scale = std::min(x_scale, y_scale);
        cv::Point lt_rel = referenceRect.tl() - cv::Point(captureRect.width/2, captureRect.height/2);
        cv::Point rb_rel = referenceRect.br() - cv::Point(captureRect.width/2, captureRect.height/2);
        lt_rel /= scale;
        rb_rel /= scale;
        cv::Point lt = lt_rel + cv::Point(REFERENCE_SCREEN_WIDTH/2, REFERENCE_SCREEN_HEIGHT/2);
        cv::Point rb = rb_rel + cv::Point(REFERENCE_SCREEN_WIDTH/2, REFERENCE_SCREEN_HEIGHT/2);
        referenceRect = cv::Rect(lt,rb);
    }
    referenceRect.x += captureRect.x;
    referenceRect.y += captureRect.y;
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
    if (rect.empty())
        return 0;
    cv::Mat image;
    std::vector<cv::Mat> imagePlanes;
    if (mGray) {
        image = env.imageGray;
        if (image.empty())
            return 0;
        imagePlanes.push_back(image);
    } else {
        image = env.imageColor;
        if (image.empty())
            return 0;
        cv::split(image, imagePlanes);
    }
    unsigned resultColor = 0;
    for (auto i=0; i < imagePlanes.size(); i++) {
        int histSize = 256;
        float range[]{0, 256}; //the upper boundary is exclusive
        const float* histRange[]{range};
        cv::Mat subImage(imagePlanes[i], rect);
        cv::Mat hist;
        cv::calcHist(&subImage, 1, nullptr, cv::Mat(), hist, 1, &histSize, histRange);
        int maxLoc = -1;
        cv::minMaxIdx(hist, nullptr, nullptr, nullptr, &maxLoc);
        resultColor |= maxLoc << (i*8);
    }
    mLastColorRGB = mGray ? gray2rgb(resultColor) : encodeRGB(resultColor);
    mLastColorLuv = rgb2luv(mLastColorRGB);
    LOG(DEBUG) << "Colors: result RGB:" << mLastColorRGB << " Luv:" << mLastColorLuv << "; expected Luv:" << mValueLuv;
    double dist = cv::norm(mLastColorLuv - mValueLuv);
    LOG(DEBUG) << "Distance: " << dist;
    mLastValue = 1.0 - std::erf(dist);
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
    if (!referenceRect)
        return 0;
    cv::Rect screenRect = referenceRect->calcRect(env);
    if (screenRect.empty())
        return 0;
    cv::Mat image = env.imageGray;
    if (image.empty() || templGray.empty())
        return 0;
    if (image.cols != REFERENCE_SCREEN_WIDTH || image.rows != REFERENCE_SCREEN_HEIGHT) {
        double x_scale = double(image.cols) / REFERENCE_SCREEN_WIDTH;
        double y_scale = double(image.rows) / REFERENCE_SCREEN_HEIGHT;
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
    scaledScreenLT = env.cvtReferenceToImage(screenRect.tl()-extendLT-cv::Point(ext,ext));
    scaledScreenRB = env.cvtReferenceToImage(screenRect.br()+extendRB+cv::Point(ext,ext));
    cv::Rect matchRect(scaledScreenLT, scaledScreenRB);
    cv::Rect xMatchRect = getMatchRect(env, image.cols, image.rows);
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
    cv::Point minLoc;
    cv::minMaxLoc(result, &minVal, &maxVal, &minLoc, &matchedLoc);
    LOG(DEBUG) << "match result: " << std::setprecision(3) << maxVal << " for " << filename;
    if (!name.empty() && maxVal >= threshold_min) {
        cv::Point offset = matchedLoc + matchRect.tl() - scaledScreenLT;
        offset = env.cvtImageToReference(offset);
        env.classifiedRects.emplace_back(maxVal, name, screenRect, screenRect + offset);
    }
    return maxVal;
}

cv::Rect ImageTemplate::getMatchRect(ClassifyEnv& env, int imageWidth, int imageHeight) {
    int ext = Master::getInstance().getSearchRegionExtent();
    cv::Point ext_lt = extendLT + cv::Point(ext,ext);
    cv::Point ext_rb = extendRB + cv::Point(ext,ext);
    cv::Rect screenRect = referenceRect->calcRect(env);
    if (screenRect.empty())
        return screenRect;
    cv::Point screenLT = screenRect.tl();
    cv::Point screenRB = screenRect.br();
    screenRect.x -= ext_lt.x;
    screenRect.y -= ext_lt.y;
    screenRect.width = templGray.cols + ext_lt.x + ext_rb.x;
    screenRect.height = templGray.rows + ext_lt.y + ext_rb.y;
    if (imageWidth != REFERENCE_SCREEN_WIDTH || imageHeight != REFERENCE_SCREEN_HEIGHT) {
        double x_scale = double(imageWidth) / REFERENCE_SCREEN_WIDTH;
        double y_scale = double(imageHeight) / REFERENCE_SCREEN_HEIGHT;
        lastScale = std::min(x_scale, y_scale);
        ext_lt *= lastScale;
        ext_rb *= lastScale;
        cv::Point lt_rel = screenLT - cv::Point(REFERENCE_SCREEN_WIDTH/2, REFERENCE_SCREEN_HEIGHT/2);
        cv::Point rb_rel = screenRB - cv::Point(REFERENCE_SCREEN_WIDTH/2, REFERENCE_SCREEN_HEIGHT/2);
        lt_rel *= lastScale;
        rb_rel *= lastScale;
        scaledScreenLT = lt_rel + cv::Point(imageWidth/2, imageHeight/2);
        scaledScreenRB = rb_rel + cv::Point(imageWidth/2, imageHeight/2);
        screenRect = cv::Rect(scaledScreenLT - ext_lt, scaledScreenRB + ext_rb);
    } else {
        lastScale = 1;
        scaledScreenLT = screenLT;
        scaledScreenRB = screenRB;
    }
    screenRect &= cv::Rect(0, 0, imageWidth, imageHeight);
    return screenRect;
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
    cv::Rect matchRect = getMatchRect(env, env.debugImage.cols, env.debugImage.rows);
    cv::Point offset = matchedLoc + matchRect.tl() - scaledScreenLT;
    LOG(INFO) << "match result: " << std::setprecision(3) << value <<
               "[" << threshold_min << ":" << threshold_max << "] >> " << toResult(value) << " for " << filename <<
               "; offset: " << offset << "; scale: " << lastScale;
    if (value >= threshold_max) {
        cv::Scalar color(255, 255, 255);
        cv::rectangle(env.debugImage, matchRect.tl(), matchRect.br(), color, 4);
        cv::rectangle(env.debugImage, scaledScreenLT+offset, scaledScreenRB+offset, color, 2);
        return 1;
    }
    if (value < threshold_min) {
        cv::Scalar color(127, 127, 127);
        cv::rectangle(env.debugImage, matchRect.tl(), matchRect.br(), color, 2);
        cv::rectangle(env.debugImage, scaledScreenLT+offset, scaledScreenRB+offset, color, 1);
        cv::Point lt = matchRect.tl();
        cv::Point rb = matchRect.br();
        cv::line(env.debugImage, lt, rb, color, 1);
        cv::Point lb = cv::Point(matchRect.tl().x, matchRect.br().y);
        cv::Point rt = cv::Point(matchRect.br().x, matchRect.tl().y);
        cv::line(env.debugImage, lb, rt, color, 1);
        return 0;
    }
    double result = toResult(value);
    int g = 127 + int(127*result);
    cv::Scalar color(g, g, g);
    cv::rectangle(env.debugImage, matchRect.tl(), matchRect.br(), color, 2);
    cv::rectangle(env.debugImage, scaledScreenLT, scaledScreenRB, color, 1);
    cv::Point lt = matchRect.tl();
    cv::Point rb = matchRect.br();
    cv::line(env.debugImage, lt, rb, color, 1);
    return result;
}
