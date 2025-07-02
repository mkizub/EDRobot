//
// Created by mkizub on 02.07.2025.
//
#include "../pch.h"

#include "Detector.h"

#include <iomanip>

namespace detect {

LineDetector::LineDetector(std::vector<std::string> anchors, spEvalRect anchorRect, cv::Point p0, cv::Point p1)
        : BaseImageTemplate("", cv::Mat(), anchorRect), anchorFiles(std::move(anchors)), referenceP0(p0),
          referenceP1(p1) {
    for (auto &fname: anchorFiles) {
        cv::Mat anchorImage;
        cv::Mat anchorMask;
        loadImageAndMask(fname, anchorImage, anchorMask);
        anchorSource.emplace_back(fname, anchorImage);
    }
}

double LineDetector::match(ClassifyEnv &env) {
    if (!this->referenceRect || anchorSource.empty())
        return 0;
    cv::Rect referenceRect = env.calcReferenceRect(this->referenceRect);
    if (referenceRect.empty())
        return 0;
    if (env.getScale() != preprocessedTemplateScale) {
        preprocessedTemplateScale = env.getScale();
        anchorScaled.clear();
        cv::Mat tmpImageFiltered = applyFilters(templImage);
        for (auto &as: anchorSource) {
            cv::Mat tmpImage;
            cv::resize(applyFilters(as.templImage), tmpImage, cv::Size(), env.getScale(), env.getScale());
            anchorScaled.emplace_back(as.name, tmpImage);
        }
    }
    int ext = Master::getInstance().getSearchRegionExtent();
    captureRect = env.cvtReferenceToCaptured(referenceRect);
    matchRect = cv::Rect(captureRect.tl() - env.scaleToCaptured(extendLT + cv::Point(ext, ext)),
                         captureRect.br() + env.scaleToCaptured(extendRB + cv::Point(ext, ext)));
    env.cropToCapture(matchRect);
    AnchorMatrix *bestAnchor = nullptr;
    double bestAnchorVal = 0;
    cv::Point bestAnchorLoc;
    cv::Mat imagePrepared = cv::Mat(env.getColorImage(), matchRect);
    imagePrepared = applyFilters(imagePrepared);
    for (auto &sm: anchorScaled) {
        int result_cols = matchRect.width - sm.templImage.cols + 1;
        int result_rows = matchRect.height - sm.templImage.rows + 1;
        cv::Mat result(result_rows, result_cols, CV_32FC1);
        cv::matchTemplate(imagePrepared, sm.templImage, result, cv::TM_CCOEFF_NORMED);
        fixNaNinResult(result);
        double maxVal;
        cv::Point maxLoc;
        cv::minMaxLoc(result, nullptr, &maxVal, nullptr, &maxLoc);
        LOG(DEBUG) << "match result: " << std::setprecision(3) << maxVal << " for anchor " << sm.name;
        if (maxVal > bestAnchorVal) {
            bestAnchorVal = maxVal;
            bestAnchorLoc = maxLoc;
            bestAnchor = &sm;
        }
        //LOG(DEBUG) << "match result: " << std::setprecision(3) << maxVal << " for " << ic.name;
    }
    if (bestAnchorVal < threshold_min) {
        LOG(INFO) << "LineDetector: anchor detect rate: " << bestAnchorVal << " less then minimal threshold "
                  << threshold_min;
        return bestAnchorVal;
    }
    matchedCaptureOffset = bestAnchorLoc - (captureRect.tl() - matchRect.tl());
    captureRect += matchedCaptureOffset;
    LOG(DEBUG) << "LineDetector: anchor found, offset: " << matchedCaptureOffset;

    captureP0 = env.cvtReferenceToCaptured(referenceP0) + matchedCaptureOffset;
    captureP1 = captureP0 + env.scaleToCaptured(referenceP1 - referenceP0);
    int captureWidth = cv::norm(captureP1 - captureP0);
    cv::Rect r0 = cv::Rect(captureP0 - env.scaleToCaptured(extendLT + cv::Point(ext, ext)),
                           captureP0 + env.scaleToCaptured(extendRB + cv::Point(ext, ext)));
    cv::Rect r1 = cv::Rect(captureP1 - env.scaleToCaptured(extendLT + cv::Point(ext, ext)),
                           captureP1 + env.scaleToCaptured(extendRB + cv::Point(ext, ext)));
    lineMatchRect = r0 | r1;
    env.cropToCapture(lineMatchRect);

    imagePrepared = cv::Mat(env.getColorImage(), lineMatchRect);
    imagePrepared = scaleImage(imagePrepared, imageScaleX, imageScaleY);
    imagePrepared = applyFilters(imagePrepared);

    cv::Mat thrMat;
    cv::threshold(imagePrepared, thrMat, binaryThreshold, 255, cv::THRESH_BINARY);

    cv::Point referenceDist = referenceP1 - referenceP0;
    float referenceAngle = std::atan2(referenceDist.y, referenceDist.x) * 180 / M_PI;
    double minDist = 1000;
    float lastDeltaAngle = 180;
    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(thrMat, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
    for (auto &cont: contours) {
        auto rotRect = cv::minAreaRect(cont);
        normalizeRotatedRect(rotRect);
        if (rotRect.size.width < 0.8 * captureWidth || rotRect.size.height > 20)
            continue;
        cv::Point2f rectPoints[4]; // bl, tl, tr, br
        rotRect.points(rectPoints);
        auto lineP0 = cv::Point(rectPoints[0] + rectPoints[1]) / 2;
        auto lineP1 = cv::Point(rectPoints[2] + rectPoints[3]) / 2;
        cv::Point detectedDist = lineP1 - lineP0;
        float detectedAngle = std::atan2(detectedDist.y, detectedDist.x) * 180 / M_PI;
        auto offset = (captureP0 - lineMatchRect.tl() - lineP0);
        auto distRate = cv::Point(offset.x, offset.y * 4);
        if (cv::norm(distRate) < minDist) {
            LOG(DEBUG) << "LineDetector: line found, offset: " << offset << " angle delta: "
                       << (detectedAngle - referenceAngle);
            minDist = cv::norm(distRate);
            lastLineAngle = rotRect.angle;
            lastDeltaAngle = detectedAngle - referenceAngle;
        }
    }
    if (minDist > captureRect.width) {
        if (lastDeltaAngle == 180)
            LOG(WARNING) << "LineDetector: no lines found";
        else
            LOG(WARNING) << "LineDetector: distance too large";
        return 0;
    }
    captureP1.x = captureP0.x + captureWidth * std::cos(lastLineAngle * M_PI / 180);
    captureP1.y = captureP0.y + captureWidth * std::sin(lastLineAngle * M_PI / 180);

    env.classified.emplace_back(ClsDetType::LineDetected, env.isWarpMode(), name,
                                referenceRect + env.scaleToReference(matchedCaptureOffset));
    env.classified.back().u.ldet.offset = env.scaleToReference(matchedCaptureOffset);
    env.classified.back().u.ldet.angle = lastDeltaAngle;
    env.classified.back().u.ldet.scale = 1;
    env.classified.back().u.ldet.referenceP0 = env.cvtCapturedToReference(captureP0);
    env.classified.back().u.ldet.referenceP1 = env.cvtCapturedToReference(captureP1);
    return 1;
}

double LineDetector::debugMatch(ClassifyEnv &env) {
    double value = match(env);
    if (value >= threshold_max) {
        cv::Scalar color(255, 255, 96);
        cv::line(env.getDebugImage(), captureP0, captureP1, color, 2);
    }
    //tryCannyParamsGUI(env);
    return value;
}

void LineDetector::normalizeRotatedRect(cv::RotatedRect &rr) {
    if (rr.angle > +90) rr.angle -= 180;
    if (rr.angle < -90) rr.angle += 180;
    if (rr.angle > +45) {
        rr.angle -= 90;
        std::swap(rr.size.width, rr.size.height);
    }
    if (rr.angle < -45) {
        rr.angle += 90;
        std::swap(rr.size.width, rr.size.height);
    }
}

void LineDetector::tryCannyParamsGUI(ClassifyEnv &env) {
    int minWidth = cv::norm(captureP1 - captureP0) * 0.8;

    cv::Mat imagePrepared = cv::Mat(env.getColorImage(), lineMatchRect);
    imagePrepared = applyFilters(scaleImage(imagePrepared, imageScaleX, imageScaleY));

    GaussFilter *gaussFilter = nullptr;
    for (auto &filter: filters) {
        gaussFilter = dynamic_cast<GaussFilter *>(filter.get());
        if (gaussFilter)
            break;
    }

    const std::string gaussWindow = "Prepared (press Q to quit)";
    cv::namedWindow(gaussWindow, cv::WINDOW_AUTOSIZE);
    //cv::resizeWindow(gaussWindow, imagePrepared.cols, 500);

    const std::string edgesWindow = "Edges (press Q to quit)";
    cv::namedWindow(edgesWindow, cv::WINDOW_NORMAL);
    cv::resizeWindow(edgesWindow, imagePrepared.cols, 500);

    const std::string linesWindow = "Lines (press Q to quit)";
    cv::namedWindow(linesWindow, cv::WINDOW_NORMAL);
    cv::resizeWindow(linesWindow, imagePrepared.cols, 500);

    // gauss filter params
    int gaussKernMax = 11;
    int gaussDisable = 0;
    int gaussKernX = 1;
    int gaussKernY = 1;
    // binary threshold
    int thrMin = binaryThreshold;

    // create trackbars for gauss filter
    if (gaussFilter) {
        gaussDisable = gaussFilter->disabled ? 1 : 0;
        gaussKernX = gaussFilter->kernX;
        gaussKernY = gaussFilter->kernY;
        cv::createTrackbar("Disbl", gaussWindow, &gaussDisable, 1);
        cv::createTrackbar("gKrnX", gaussWindow, &gaussKernX, (gaussKernMax - 1) / 2);
        cv::setTrackbarMax("gKrnX", gaussWindow, gaussKernMax);
        cv::setTrackbarMin("gKrnX", gaussWindow, 1);
        cv::createTrackbar("gKrnY", gaussWindow, &gaussKernY, (gaussKernMax - 1) / 2);
        cv::setTrackbarMax("gKrnY", gaussWindow, gaussKernMax);
        cv::setTrackbarMin("gKrnY", gaussWindow, 1);
    }
    // create trackbars for image threshold
    cv::createTrackbar("Thr", edgesWindow, &thrMin, 255);

    cv::Mat edgeMat;
    cv::Mat linesMat;
    while (1) {
        if (gaussFilter) {
            gaussKernX = (gaussKernX & ~1) + 1;
            gaussKernY = (gaussKernY & ~1) + 1;
            const_cast<int &>(gaussFilter->kernX) = gaussKernX;
            const_cast<int &>(gaussFilter->kernY) = gaussKernY;
            const_cast<bool &>(gaussFilter->disabled) = gaussDisable > 0;
        }
        imagePrepared = cv::Mat(env.getColorImage(), lineMatchRect);
        imagePrepared = applyFilters(scaleImage(imagePrepared, imageScaleX, imageScaleY));
        linesMat = scaleImage(cv::Mat(env.getColorImage(), lineMatchRect), imageScaleX, imageScaleY).clone();

        cv::threshold(imagePrepared, edgeMat, thrMin, 255, cv::THRESH_BINARY);

        std::vector<std::vector<cv::Point>> contours;
        cv::findContours(edgeMat, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
        for (auto &cont: contours) {
            auto rotRect = cv::minAreaRect(cont);
            normalizeRotatedRect(rotRect);
            float w = rotRect.size.width;
            float h = rotRect.size.height;
            if (w < 60 || h > 20)
                continue;
            cv::Point2f rectPoints[4]; // bl, tl, tr, br
            rotRect.points(rectPoints);
            if (w < minWidth || h > 20) {
                for (int j = 0; j < 4; j++)
                    cv::line(linesMat, rectPoints[j], rectPoints[(j + 1) % 4], {0, 0, 255}, 1);
            } else {
                auto lp0 = (rectPoints[0] + rectPoints[1]) / 2;
                auto lp1 = (rectPoints[2] + rectPoints[3]) / 2;
                cv::line(linesMat, lp0, lp1, {255, 255, 255}, 2);
            }
        }

        // Display output image
        cv::imshow(gaussWindow, imagePrepared);
        cv::imshow(edgesWindow, edgeMat);
        cv::imshow(linesWindow, linesMat);
        // Wait longer to prevent freeze for videos.
        if (cv::waitKey(33) == 'q')
            break;
    }

    cv::destroyAllWindows();
}

} // detect
