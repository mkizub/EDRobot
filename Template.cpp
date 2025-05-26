//
// Created by mkizub on 23.05.2025.
//

#include "Template.h"
#include "Master.h"
#include "easylogging++.h"
#include <format>
#include <iomanip>
#include <opencv2/opencv.hpp>

double SequenceTemplate::evaluate() {
    double sum = 0;
    for (auto& oracle : oracles) {
        double value = oracle->match();
        if (value <= 0)
            return 0;
        sum += value;
        if (sum >= 1)
            return 1;
    }
    return sum;
}

double SequenceTemplate::match() {
    return evaluate() >= 0.8 ? 1 : 0;
}

double SequenceTemplate::debugMatch(cv::Mat drawToImage) {
    double sum = 0;
    for (auto& oracle : oracles) {
        double value = oracle->debugMatch(drawToImage);
        if (value <= 0)
            sum = 0;
        sum += value;
        if (sum >= 1)
            sum = 1;
    }
    return sum > 1 ? 1 : sum < 0 ? 0 : sum;
}

const int SCREEN_WIDTH = 1920;
const int SCREEN_HEIGHT = 1080;

ImageTemplate::ImageTemplate(const std::string& filename, int x, int y, int l, int t, int r, int b, double tmin, double tmax)
    : filename(filename)
    , screenPoint{x, y}
    , extLT{l, t}
    , extRB{r, b}
    , threshold_min(tmin)
    , threshold_max(tmax)
{
    auto image = cv::imread(filename, cv::IMREAD_UNCHANGED);
    if (image.empty())
        throw std::runtime_error(std::format("Cannot read %s", filename));
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

    screenRect = cv::Rect(screenPoint, cv::Size(templGray.cols, templGray.rows));
    screenRect.x -= extLT.width;
    screenRect.y -= extLT.height;
    screenRect.width += extLT.width + extRB.width;
    screenRect.height += extLT.height + extRB.height;
    screenRect &= cv::Rect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    screenRectScaled = screenRect;
    templGrayScaled = templGray;
    templMaskScaled = templMask;
}

double ImageTemplate::evaluate() {
    cv::Mat image = Master::getInstance().getGrayScreenshot();
    if (image.empty() || templGray.empty())
        return 0;
    if (image.cols != SCREEN_WIDTH || image.rows != SCREEN_HEIGHT) {
        double x_scale = double(image.cols) / SCREEN_WIDTH;
        double y_scale = double(image.rows) / SCREEN_HEIGHT;
        cv::resize(templGray, templGrayScaled, cv::Size(), x_scale, y_scale);
        if (!templMask.empty()) {
            cv::resize(templMask, templMaskScaled, templGrayScaled.size(), x_scale, y_scale);
            cv::Mat mask;
            cv::threshold(templMaskScaled, mask, 127, 255, cv::THRESH_BINARY);
            templMaskScaled = mask;
        }
        //cv::imwrite("scaled-template.png", templGrayScaled);
        //cv::imwrite("scaled-screen-region.png", cv::Mat(image, screenRectScaled));
        screenRectScaled.x = int(screenRect.x * x_scale + 0.5);
        screenRectScaled.y = int(screenRect.y * y_scale + 0.5);
        screenRectScaled.width = int(screenRect.width * x_scale + 0.5);
        screenRectScaled.height = int(screenRect.height * y_scale + 0.5);
        screenRectScaled &= cv::Rect(0, 0, image.cols, image.rows);
    }
    int result_cols = screenRectScaled.width - templGrayScaled.cols + 1;
    int result_rows = screenRectScaled.height - templGrayScaled.rows + 1;
    cv::Mat result(result_rows, result_cols, CV_32FC1);
    cv::matchTemplate(cv::Mat(image, screenRectScaled), templGrayScaled, result, cv::TM_CCOEFF_NORMED, templMaskScaled); // cv::TM_CCORR_NORMED
    //LOG(ERROR) << "match result: " << result;
    double minVal, maxVal;
    cv::Point minLoc, maxLoc;
    cv::minMaxLoc(result, &minVal, &maxVal, &minLoc, &maxLoc);
    LOG(INFO) << "match result: " << std::setprecision(3) << maxVal << "[" << threshold_min << ":" << threshold_max << "] for " << filename;
    return maxVal;
}

double ImageTemplate::match() {
    double value = evaluate();
    if (value >= threshold_max)
        return 1;
    if (value < threshold_min)
        return 0;
    return (value - threshold_min) * (threshold_max - threshold_min);
}

double ImageTemplate::debugMatch(cv::Mat drawToImage) {
    double value = evaluate();
    if (value >= threshold_max) {
        cv::Scalar color(255, 255, 255);
        cv::rectangle(drawToImage, screenRectScaled.tl(), screenRectScaled.br(), color, 2);
        return 1;
    }
    if (value < threshold_min) {
        cv::Scalar color(127, 127, 127);
        cv::rectangle(drawToImage, screenRectScaled.tl(), screenRectScaled.br(), color, 2);
        cv::Point p1 = screenRectScaled.tl();
        cv::Point p2 = screenRectScaled.br();
        cv::line(drawToImage, p1, p2, color, 1);
        p1 = cv::Point(screenRectScaled.tl().x, screenRectScaled.br().y);
        p2 = cv::Point(screenRectScaled.br().x, screenRectScaled.tl().y);
        cv::line(drawToImage, p1, p2, color, 1);
        return 0;
    }
    double result = (value - threshold_min) * (threshold_max - threshold_min);
    int g = 127 + int(127*result);
    cv::Scalar color(g, g, g);
    cv::rectangle(drawToImage, screenRectScaled.tl(), screenRectScaled.br(), color, 2);
    cv::Point p1 = screenRectScaled.tl();
    cv::Point p2 = screenRectScaled.br();
    cv::line(drawToImage, p1, p2, color, 1);
    return result;
}
