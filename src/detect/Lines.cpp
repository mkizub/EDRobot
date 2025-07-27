//
// Created by mkizub on 02.07.2025.
//
#include "../pch.h"

#include "Detector.h"

#include <iomanip>

namespace detect {

LineDetector::LineDetector(ImageTemplate* anchor, spEvalPoint p0, spEvalPoint p1)
        : referenceP0(p0),
          referenceP1(p1)
{
    anchorDetector.reset(anchor);
}

double LineDetector::match(ClassifyEnv &env) {
    ImageTemplate* an = anchorDetector.get();
    if (!an)
        return 0;
    double anchorMatch = an->match(env);

    cv::Point refP0 = referenceP0->calcReferencePoint(env);
    cv::Point refP1 = referenceP1->calcReferencePoint(env);
    captureP0 = env.cvtReferenceToCaptured(refP0) + an->matchedCaptureOffset;
    captureP1 = captureP0 + env.scaleToCaptured(refP1 - refP0);
    int captureWidth = cv::norm(captureP1 - captureP0);
    cv::Rect r0 = cv::Rect(captureP0 - env.scaleToCaptured(extendLT),
                           captureP0 + env.scaleToCaptured(extendRB));
    cv::Rect r1 = cv::Rect(captureP1 - env.scaleToCaptured(extendLT),
                           captureP1 + env.scaleToCaptured(extendRB));
    lineMatchRect = r0 | r1;
    env.cropToCapture(lineMatchRect);

    if (anchorMatch < 0.5 || an->lastTemplatedx < 0) {
        LOG(DEBUG) << "LineDetector: anchor '" << anchorDetector->filename << "' not found";
        return 0;
    }
    LOG(DEBUG) << "LineDetector '" << name << "' anchor found '" << an->imagesPrepared[an->lastTemplatedx].name << "', offset: " << an->matchedCaptureOffset;

    cv::Mat imagePrepared = cv::Mat(env.getColorImage(), lineMatchRect);
    imagePrepared = ImageTemplate::scaleImage(imagePrepared, imageScaleX, imageScaleY);
    imagePrepared = ImageTemplate::applyFilters(filters, imagePrepared);

    cv::Mat thrMat;
    cv::threshold(imagePrepared, thrMat, binaryThreshold, 255, cv::THRESH_BINARY);

    cv::Point referenceDist = refP1 - refP0;
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
    if (minDist > an->captureRect.width) {
        if (lastDeltaAngle == 180)
            LOG(DEBUG) << "LineDetector '" << name << "': no lines found";
        else
            LOG(DEBUG) << "LineDetector '" << name << "': distance too large";
        return 0;
    }
    captureP1.x = captureP0.x + captureWidth * std::cos(lastLineAngle * M_PI / 180);
    captureP1.y = captureP0.y + captureWidth * std::sin(lastLineAngle * M_PI / 180);

    env.classified.emplace_back(ClsDetType::LineDetected, env.isWarpMode(),
                                name + ':' + an->imagesPrepared[an->lastTemplatedx].name,
                                an->refRect + env.scaleToReference(an->matchedCaptureOffset));
    env.classified.back().u.ldet.referenceP0 = env.cvtCapturedToReference(captureP0);
    env.classified.back().u.ldet.referenceP1 = env.cvtCapturedToReference(captureP1);
    env.classified.back().u.ldet.scale = 1;
    env.classified.back().u.ldet.angle = lastDeltaAngle;
    env.classified.back().u.ldet.match = 1;
    env.classified.back().u.ldet.offset = env.scaleToReference(an->matchedCaptureOffset);
    return 1;
}

double LineDetector::debugMatch(ClassifyEnv &env) {
    double value = match(env);
    if (value > 0.5) {
        cv::Scalar color(255, 255, 96);
        cv::line(env.getDebugImage(), captureP0, captureP1, color, 2);
    }
    //if (!lineMatchRect.empty())
    //    tryCannyParamsGUI(env);
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
    imagePrepared = ImageTemplate::applyFilters(filters, ImageTemplate::scaleImage(imagePrepared, imageScaleX, imageScaleY));

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
        imagePrepared = ImageTemplate::applyFilters(filters, ImageTemplate::scaleImage(imagePrepared, imageScaleX, imageScaleY));
        linesMat = ImageTemplate::scaleImage(cv::Mat(env.getColorImage(), lineMatchRect), imageScaleX, imageScaleY).clone();

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
