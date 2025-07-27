//
// Created by mkizub on 02.07.2025.
//
#include "../pch.h"

#include "Detector.h"

#include <iomanip>

namespace detect {

CompassDetector::CompassDetector()
        : ImageTemplate("templates/space_compass.png", std::make_shared<ConstRect>(679,803,71,71))
        , threshold_dot{0.7}
{
    testScales = {1, 1.025, 0.975, 1.05, 0.95, 1.075, 0.925, 1.1, 0.9, 1.125, 0.875};
    testAngles = {0, -5, +5};
    auto hsvFilter = new HsvColorCropFilter();
    hsvFilter->ranges.emplace_back(cv::Vec3b(0,0,120),cv::Vec3b(30,255,255)); // limit Hue[0..30] and Value[120..]
    filters.push_back(std::unique_ptr<ImageFilter>(hsvFilter));

    hsvFilter = new HsvColorCropFilter();
    hsvFilter->ranges.emplace_back(cv::Vec3b(0,0,80),cv::Vec3b(255,90,255)); // limit Saturation[..90] and Value[80..]
    dotsFilters.push_back(std::unique_ptr<ImageFilter>(hsvFilter));

    extendLT = {40, 80};
    extendRB = {50, 140};
    threshold_min = 0.3;
    threshold_max = 0.8;
    cv::Mat dotFwdImage;
    cv::Mat dotFwdMask;
    cv::Mat dotBwdImage;
    cv::Mat dotBwdMask;
    loadImageAndMask("templates/space_compass_dot_fwd.png", dotFwdImage, dotFwdMask);
    loadImageAndMask("templates/space_compass_dot_bwd.png", dotBwdImage, dotBwdMask);
    compassDotsOrig.emplace_back(1.0, 0, "space_compass_dot_fwd.png", dotFwdImage, dotFwdMask);
    compassDotsOrig.emplace_back(1.0, 0, "space_compass_dot_bwd.png", dotBwdImage, dotBwdMask);
}

double CompassDetector::match(ClassifyEnv &env) {
    lastHemisphere = -1;
    double compassMatch = ImageTemplate::match(env);
    if (compassMatch < 0.5) {
        return compassMatch;
    }

    //
    // Detect compass dot
    //
    if (preprocessedDotsScale != env.getScale()) {
        preprocessedDotsScale = env.getScale();
        for (auto &im: compassDotsOrig) {
            cv::Mat templImagePrepared = applyFilters(dotsFilters, im.templImage);
            templImagePrepared = scaleImage(templImagePrepared, env.getScale(), env.getScale());
            cv::Mat templMaskPrepared;
            compassDotsPrepared.emplace_back(1, 0, im.name, templImagePrepared, templMaskPrepared);
        }
    }

    int bestDotIdx = -1;
    double bestDotVal = 0;
    cv::Point bestDotLoc;
    cv::Size bestDotSize;

    cv::Mat imagePrepared = ImageTemplate::applyFilters(dotsFilters, cv::Mat(env.getColorImage(), captureRect));

    for (int dotIdx=0; dotIdx < compassDotsPrepared.size(); dotIdx++) {
        auto& sm = compassDotsPrepared[dotIdx];
        int result_cols = captureRect.width - sm.templImage.cols + 1;
        int result_rows = captureRect.height - sm.templImage.rows + 1;
        if (result_cols <= 0 || result_rows <= 0)
            continue;
        cv::Mat result(result_rows, result_cols, CV_32FC1);
        cv::matchTemplate(imagePrepared, sm.templImage, result, cv::TM_SQDIFF_NORMED, sm.templMask);
        //LOG(ERROR) << "dot " << dotIdx << " match result: " << result;
        //fixNaNinResult(result, sm.name);
        double minVal, maxVal;
        cv::Point minLoc, maxLoc;
        cv::minMaxLoc(result, &minVal, &maxVal, &minLoc, &maxLoc);
        // TM_SQDIFF_NORMED - the lower - the better, so use 1-minVal and minLoc
        LOG(DEBUG) << std::format("compass dot match result: {:.3f} for {}", (1-minVal), ((dotIdx&1)? "backward" : "forward"));
        if (1-minVal > bestDotVal) {
            bestDotVal = 1-minVal;
            bestDotIdx = dotIdx;
            bestDotLoc = minLoc;
            bestDotSize = {sm.templImage.cols, sm.templImage.rows};
        }
    }
    if (bestDotVal >= threshold_dot) {
        lastDotValue = bestDotVal;
        lastHemisphere = bestDotIdx;
        dotCaptureRect = { captureRect.tl()+bestDotLoc, bestDotSize };
        double radius = env.getScale() * (refRect.width-15.5) * 0.5;
        dotSpherePosition = {
                std::clamp( ((bestDotLoc.x+bestDotSize.width*0.5) - captureRect.width*0.5) / radius, -1.0, +1.0),
                std::clamp(-((bestDotLoc.y+bestDotSize.height*0.5) - captureRect.height*0.5) / radius, -1.0, +1.0),
        };

        double pitch = std::asin(dotSpherePosition.y) * 90 / M_PI_2;
        double yaw = std::asin(dotSpherePosition.x) * 90 / M_PI_2;
        double roll = std::atan2(dotSpherePosition.x, dotSpherePosition.y) * 90 / M_PI_2;

        if (lastHemisphere & 1) {
            if (pitch > 0)
                pitch = 180 - pitch;
            else
                pitch = -180 - pitch;
        }
        if (lastHemisphere & 1) {
            if (yaw > 0)
                yaw = 180 - yaw;
            else
                yaw = -180 - yaw;
        }
        lastTgtPitch = pitch;
        lastTgtYaw = yaw;
        lastTgtRoll = roll;

        LOG(DEBUG) << std::format("Compass dot value={:.3f}, direction={}",
                                  lastDotValue, ((lastHemisphere&1) ? "backward" : "forward"))
                  << ", sphere pos=" << dotSpherePosition
                  << std_format(" pitch:{}, yaw:{}, roll:{}", int(pitch), int(yaw), int(roll));
    }

    return compassMatch;
}

void CompassDetector::tryLowerUpperBoundsGUI(ClassifyEnv &env, cv::Rect referenceRect) {
    cv::Point extendLT(50, 150);
    cv::Point extendRB(50, 50);

    cv::Rect captureRect = env.cvtReferenceToCaptured(referenceRect);
    cv::Rect matchRect = cv::Rect(captureRect.tl() - env.scaleToCaptured(extendLT),
                                  captureRect.br() + env.scaleToCaptured(extendRB));
    env.cropToCapture(matchRect);

    const std::string windowName = "My test image";
    cv::namedWindow(windowName, cv::WINDOW_NORMAL);
    cv::resizeWindow(windowName, 500, 500);

    int hMin, sMin, vMin, hMax, sMax, vMax;
    hMin = sMin = vMin = 0;
    hMax = sMax = vMax = 255;
    // create trackbars for color change
    cv::createTrackbar("HMin", windowName, &hMin, 179);
    cv::createTrackbar("SMin", windowName, &sMin, 255);
    cv::createTrackbar("VMin", windowName, &vMin, 255);
    cv::createTrackbar("HMax", windowName, &hMax, 179);
    cv::createTrackbar("SMax", windowName, &sMax, 255);
    cv::createTrackbar("VMax", windowName, &vMax, 255);

    cv::Mat img = cv::Mat(env.getColorImage(), matchRect);
    cv::Mat output = img;

    while (1) {

        // Set minimum and max HSV values to display
        cv::Vec3b lower = {(uchar) hMin, (uchar) sMin, (uchar) vMin};
        cv::Vec3b upper = {(uchar) hMax, (uchar) sMax, (uchar) vMax};

        // Create HSV Image and threshold into a range.
        cv::Mat hsv;
        cv::cvtColor(img, hsv, cv::COLOR_BGR2HSV);
        cv::Mat mask;
        cv::inRange(hsv, lower, upper, mask);
        output.release();
        cv::bitwise_and(img, img, output, mask);

        // Display output image
        cv::imshow(windowName, output);

        // Wait longer to prevent freeze for videos.
        if (cv::waitKey(33) == 'q')
            break;
    }

    cv::destroyAllWindows();
}

double CompassDetector::debugMatch(ClassifyEnv &env) {
    double value = ImageTemplate::debugMatch(env);
    if (lastDotValue >= threshold_min && lastHemisphere >= 0) {
        cv::Scalar color;
        if ((lastHemisphere & 1) == 0)
            color = {255, 96, 96};
        else
            color = {96, 96, 255};
        cv::rectangle(env.getDebugImage(), dotCaptureRect.tl(), dotCaptureRect.br(), color, 1);
        std::string text = std::format("{}/{}/{}", int(lastTgtPitch), int(lastTgtYaw), int(lastTgtRoll));
        cv::Point orig = captureRect.tl() + cv::Point(0, -10);
        color = {254, 254, 254};
        cv::putText(env.getDebugImage(), text, orig, cv::FONT_HERSHEY_PLAIN, 1, color);
    }

//    int ext = Master::getInstance().getSearchRegionExtent();
//    cv::Rect matchRect = cv::Rect(
//            referenceRect.tl() - extendLT - cv::Point(ext, ext),
//            referenceRect.br() + extendRB + cv::Point(ext, ext));
//    tryLowerUpperBoundsGUI(env, matchRect);

    return value;
}


} // detect