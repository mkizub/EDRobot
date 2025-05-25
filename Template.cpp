//
// Created by mkizub on 23.05.2025.
//

#include "Template.h"
#include "Master.h"
#include "easylogging++.h"
#include <format>
#include <iomanip>
#include <opencv2/opencv.hpp>

double SequenceTemplate::evaluate(Master* master) {
    double sum = 0;
    for (auto& oracle : oracles) {
        double value = oracle->match(master);
        if (value <= 0)
            return 0;
        sum += value;
        if (sum >= 1)
            return 1;
    }
    return sum;
}

double SequenceTemplate::match(Master* master) {
    return evaluate(master) >= 0.8 ? 1 : 0;
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

double ImageTemplate::evaluate(Master* master) {
    cv::Mat image = master->getGrayScreenshot();
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
        screenRectScaled = screenRectScaled + cv::Point(-4,-4) + cv::Size(8, 8);
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

double ImageTemplate::match(Master* master) {
    double value = evaluate(master);
    if (value >= threshold_max)
        return 1;
    if (value < threshold_min)
        return 0;
    return (value - threshold_min) * (threshold_max - threshold_min);
}
