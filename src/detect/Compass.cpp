//
// Created by mkizub on 02.07.2025.
//
#include "../pch.h"

#include "Detector.h"

#include <iomanip>

namespace detect {

CompassDetector::CompassDetector()
        : ImageMultiScaleTemplate("templates/space_compass.png", cv::Mat(),
                                  spEvalRect(new ConstRect(cv::Rect(679, 803, 71, 71))),
                                  {1, 1.025, 0.975, 1.05, 0.95, 1.075, 0.925, 1.1, 0.9, 1.125, 0.875}),
          threshold_dot{0.7} {
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
    compassDots.emplace_back(1.0, dotFwdImage, dotFwdMask);
    compassDots.emplace_back(1.0, dotBwdImage, dotBwdMask);
}

double CompassDetector::match(ClassifyEnv &env) {
    double compassValue = ImageMultiScaleTemplate::match(env);
    if (compassValue < threshold_min)
        return compassValue;

//    //
//    // Detect compass dot
//    //
//
//    int bestDotIdx = -1;
//    double bestDotVal = 0;
//    cv::Point bestDotLoc;
//    cv::Size bestDotSize;
//
//    cv::Point dotMatchedCaptureOffset;
//    imageFiltered = cv::Mat(env.getColorImage(), captureRect);
//
////    cv::imshow("Detected compass", imageFiltered);
////    cv::imshow("Dot fwd compass", compassDots[0].templImage);
////    cv::imshow("Dot bwd compass", compassDots[1].templImage);
////    cv::waitKey();
////    cv::destroyAllWindows();
//
//    for (int dotIdx=0; dotIdx < compassDots.size(); dotIdx++) {
//        auto& sm = compassDots[dotIdx];
//        int result_cols = captureRect.width - sm.templImage.cols + 1;
//        int result_rows = captureRect.height - sm.templImage.rows + 1;
//        cv::Mat result(result_rows, result_cols, CV_32FC1);
//        cv::matchTemplate(imageFiltered, sm.templImage, result, cv::TM_SQDIFF_NORMED, sm.templMask);
//        //LOG(ERROR) << "dot " << dotIdx << " match result: " << result;
//        fixNaNinResult(result);
//        double minVal, maxVal;
//        cv::Point minLoc, maxLoc;
//        cv::minMaxLoc(result, &minVal, &maxVal, &minLoc, &maxLoc);
//        // TM_SQDIFF_NORMED - the lower - the better, so use 1-minVal and minLoc
//        LOG(DEBUG) << std::format("compass dot match result: {:.3f} for {}", (1-minVal), ((dotIdx&1)? "backward" : "forward"));
//        if (1-minVal > bestDotVal) {
//            bestDotVal = 1-minVal;
//            bestDotIdx = dotIdx;
//            bestDotLoc = minLoc;
//            bestDotSize = {sm.templImage.cols, sm.templImage.rows};
//        }
//    }
//    if (bestDotVal >= threshold_dot) {
//        lastDotValue = bestDotVal;
//        lastDotIdx = bestDotIdx;
//        dotCaptureRect = { captureRect.tl()+bestDotLoc, bestDotSize };
//        dotSpherePosition = {
//                std::clamp( ((bestDotLoc.x+bestDotSize.width*0.5) - captureRect.width*0.5) / ((captureRect.width-16)*0.5), -1.0, +1.0),
//                std::clamp(-((bestDotLoc.y+bestDotSize.height*0.5) - captureRect.height*0.5) / ((captureRect.height-16)*0.5), -1.0, +1.0),
//        };
//
//        double pitch = std::asin(dotSpherePosition.y) * 90 / M_PI_2;
//        double yaw = std::asin(dotSpherePosition.x) * 90 / M_PI_2;
//        double roll = 90-std::atan2(dotSpherePosition.y, dotSpherePosition.x) * 90 / M_PI_2;
//
//        if (lastDotIdx&1)
//            pitch = 180 - pitch;
//        if (lastDotIdx&1)
//            yaw = 180 - yaw;
//        if (pitch > 180) pitch = 360 - pitch;
//        if (pitch < -180) pitch = 360 + pitch;
//        if (yaw > 180) yaw = 360 - yaw;
//        if (yaw < -180) yaw = 360 + yaw;
//        if (roll > 180) roll = 360 - roll;
//        if (roll < -180) roll = 360 + roll;
//        lastTgtPitch = pitch;
//        lastTgtYaw = yaw;
//        lastTgtRoll = roll;
//
//        LOG(INFO) << std::format("Compass dot value={:.3f}, direction={}",
//                                 lastDotValue, ((lastDotIdx&1) ? "backward" : "forward"))
//                  << ", sphere pos=" << dotSpherePosition
//                  << " pitch,yaw,roll=" << std_format("{:.0f},{:.0f},{:.0f}", pitch, yaw, roll);
//    } else {
//        lastDotIdx = -1;
//        dotCaptureRect = {};
//        lastTgtPitch = 0;
//        lastTgtYaw = 0;
//        lastTgtRoll = 0;
//    }

    return compassValue;
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
    double value = BaseImageTemplate::debugMatch(env);
    if (lastDotValue >= threshold_min && lastDotIdx >= 0) {
        cv::Scalar color;
        if ((lastDotIdx & 1) == 0)
            color = {255, 96, 96};
        else
            color = {96, 96, 255};
        cv::rectangle(env.getDebugImage(), dotCaptureRect.tl(), dotCaptureRect.br(), color, 1);
        std::string text = std::format("{}/{}/{}", int(lastTgtPitch), int(lastTgtYaw), int(lastTgtRoll));
        cv::Point orig = captureRect.tl() + cv::Point(0, -10);
        color = {254, 254, 254};
        cv::putText(env.getDebugImage(), text, orig, cv::FONT_HERSHEY_PLAIN, 1, color);
    }
    return value;
}


} // detect