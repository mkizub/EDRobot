//
// Created by mkizub on 23.05.2025.
//

#include "Template.h"
#include "Master.h"
#include "easylogging++.h"
#include <format>
#include <opencv2/opencv.hpp>

const int SCREEN_WIDTH = 1920;
const int SCREEN_HEIGHT = 1080;

ImageTemplate::ImageTemplate(ImagePlace place, double threshold)
    : threshold(threshold)
    , place(place)
{
    templ = cv::imread(place.filename, cv::IMREAD_GRAYSCALE);
    if (templ.empty())
        throw std::runtime_error(std::format("Cannot read %s", place.filename));
    int x1 = std::max(0, place.screenRect.x-4);
    int y1 = std::max(0, place.screenRect.y-4);
    if (place.screenRect.width <= 0)
        place.screenRect.width = templ.cols;
    if (place.screenRect.height <= 0)
        place.screenRect.height = templ.rows;
    int x2 = std::max(0, x1+place.screenRect.width+8);
    int y2 = std::max(0, y1+place.screenRect.height+8);
    screenRect = cv::Rect(x1, y1, x2-x1, y2-y1);
}

bool ImageTemplate::match(Master* master) {
    cv::Mat image = master->getGrayScreenshot();
    if (image.empty() || templ.empty())
        return false;
    int result_cols = screenRect.width - templ.cols + 1;
    int result_rows = screenRect.height - templ.rows + 1;
    cv::Mat result(result_rows, result_cols, CV_32FC1);
    cv::matchTemplate(cv::Mat(image, screenRect), templ, result, cv::TM_CCOEFF_NORMED);
    //LOG(ERROR) << "match result: " << result;
    double minVal, maxVal;
    cv::Point minLoc, maxLoc;
    cv::minMaxLoc(result, &minVal, &maxVal, &minLoc, &maxLoc);
    LOG(ERROR) << "match result: " << maxVal << " for " << place.filename;
    return (maxVal > threshold);
}
