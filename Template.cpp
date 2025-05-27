//
// Created by mkizub on 23.05.2025.
//

#include "Template.h"
#include "Master.h"
#include "easylogging++.h"
#include <format>
#include <iomanip>
#include <opencv2/opencv.hpp>

double SequenceTemplate::match() {
    double sum = 0;
    for (auto& oracle : oracles) {
        double value = oracle->classify();
        if (value <= 0)
            return 0;
        sum += 2*(value-0.5);
        if (sum >= 1)
            return 1;
    }
    return sum;
}

double SequenceTemplate::classify() {
    return match();
}

double SequenceTemplate::debugMatch(cv::Mat drawToImage) {
    json5pp::value j_arr = json5pp::array({});
    for (auto& oracle : oracles) {
        double value = oracle->debugMatch(drawToImage);
        j_arr.as_array().emplace_back(value);
    }
    double sum = 0;
    for (auto& jv : j_arr.as_array()) {
        auto value = jv.as_number();
        if (value <= 0) { sum = 0; break; }
        sum += 2*(value-0.5);
        if (sum >= 1) { sum = 1; break; }
    }
    LOG(INFO) << "match result: " << sum << (sum >= 1 ? " (pass)" : " (fail)") << " for " << j_arr;
    return sum;
}

const int SCREEN_WIDTH = 1920;
const int SCREEN_HEIGHT = 1080;

ImageTemplate::ImageTemplate(const std::string& filename, cv::Point lt, cv::Point extLT, cv::Point extRB, double tmin, double tmax)
    : filename(filename)
    , screenLT{lt}
    , screenRB{lt}
    , extendLT{extLT}
    , extendRB{extRB}
    , threshold_min(tmin)
    , threshold_max(tmax)
    , lastScale(1)
{
    auto image = cv::imread(filename, cv::IMREAD_UNCHANGED);
    if (image.empty())
        throw std::runtime_error(std::format("Cannot read %s", filename));
    screenRB.x += image.cols;
    screenRB.y += image.rows;
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

double ImageTemplate::match() {
    cv::Mat image = Master::getInstance().getGrayScreenshot();
    if (image.empty() || templGray.empty())
        return 0;
    if (image.cols != SCREEN_WIDTH || image.rows != SCREEN_HEIGHT) {
        double x_scale = double(image.cols) / SCREEN_WIDTH;
        double y_scale = double(image.rows) / SCREEN_HEIGHT;
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
    cv::Rect matchRect = getMatchRect(image.cols, image.rows);
    int result_cols = matchRect.width - templGrayScaled.cols + 1;
    int result_rows = matchRect.height - templGrayScaled.rows + 1;
    cv::Mat result(result_rows, result_cols, CV_32FC1);
    cv::matchTemplate(cv::Mat(image, matchRect), templGrayScaled, result, cv::TM_CCOEFF_NORMED, templMaskScaled); // cv::TM_CCORR_NORMED
    // bypass error in cv::matchTemplate that sometimes return NaN/Inf, instead of [0..1] valies
    float* ptr = result.ptr<float>(0);
    float* pend = ptr + result_rows * result_cols;
    for (; ptr < pend; ++ptr) {
        if (std::isnan(*ptr))
            *ptr = 0;
        else if (std::isinf(*ptr))
            *ptr = 0;
    }
    //LOG(ERROR) << "match result: " << result;
    double minVal, maxVal;
    cv::Point minLoc;
    cv::minMaxLoc(result, &minVal, &maxVal, &minLoc, &matchedLoc);
    LOG(DEBUG) << "match result: " << std::setprecision(3) << maxVal << " for " << filename;
    return maxVal;
}

cv::Rect ImageTemplate::getMatchRect(int imageWidth, int imageHeight) {
    int ext = Master::getInstance().getSearchRegionExtent();
    cv::Point ext_lt = extendLT + cv::Point(ext,ext);
    cv::Point ext_rb = extendRB + cv::Point(ext,ext);
    cv::Rect screenRect(screenLT - ext_lt, screenRB + ext_rb);
    if (imageWidth != SCREEN_WIDTH || imageHeight != SCREEN_HEIGHT) {
        double x_scale = double(imageWidth) / SCREEN_WIDTH;
        double y_scale = double(imageHeight) / SCREEN_HEIGHT;
        lastScale = std::min(x_scale, y_scale);
        ext_lt *= lastScale;
        ext_rb *= lastScale;
        cv::Point lt_rel = screenLT - cv::Point(SCREEN_WIDTH/2, SCREEN_HEIGHT/2);
        cv::Point rb_rel = screenRB - cv::Point(SCREEN_WIDTH/2, SCREEN_HEIGHT/2);
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


double ImageTemplate::classify() {
    double value = match();
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

double ImageTemplate::debugMatch(cv::Mat drawToImage) {
    double value = match();
    cv::Rect matchRect = getMatchRect(drawToImage.cols, drawToImage.rows);
    LOG(INFO) << "match result: " << std::setprecision(3) << value <<
               "[" << threshold_min << ":" << threshold_max << "] >> " << toResult(value) << " for " << filename <<
               "; offset: " << (scaledScreenLT-matchRect.tl()-matchedLoc) << "; scale: " << lastScale;
    if (value >= threshold_max) {
        cv::Scalar color(255, 255, 255);
        cv::rectangle(drawToImage, matchRect.tl(), matchRect.br(), color, 4);
        cv::rectangle(drawToImage, scaledScreenLT, scaledScreenRB, color, 2);
        return 1;
    }
    if (value < threshold_min) {
        cv::Scalar color(127, 127, 127);
        cv::rectangle(drawToImage, matchRect.tl(), matchRect.br(), color, 2);
        cv::rectangle(drawToImage, scaledScreenLT, scaledScreenRB, color, 1);
        cv::Point lt = matchRect.tl();
        cv::Point rb = matchRect.br();
        cv::line(drawToImage, lt, rb, color, 1);
        cv::Point lb = cv::Point(matchRect.tl().x, matchRect.br().y);
        cv::Point rt = cv::Point(matchRect.br().x, matchRect.tl().y);
        cv::line(drawToImage, lb, rt, color, 1);
        return 0;
    }
    double result = toResult(value);
    int g = 127 + int(127*result);
    cv::Scalar color(g, g, g);
    cv::rectangle(drawToImage, matchRect.tl(), matchRect.br(), color, 2);
    cv::rectangle(drawToImage, scaledScreenLT, scaledScreenRB, color, 1);
    cv::Point lt = matchRect.tl();
    cv::Point rb = matchRect.br();
    cv::line(drawToImage, lt, rb, color, 1);
    return result;
}
