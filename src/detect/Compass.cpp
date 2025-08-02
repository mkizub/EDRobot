//
// Created by mkizub on 02.07.2025.
//
#include "../pch.h"

#include "Detector.h"
#include "../EDWidget.h"
#include "../OCR.h"

#include <opencv4/opencv2/ximgproc/find_ellipses.hpp>

#include <iomanip>

namespace detect {

CompassDetector::CompassDetector()
        : ImageTemplate("templates/compass/compass_full.png", std::make_shared<ConstRect>(679,803,71,71))
        , threshold_dot{0.7}
        , lastHemisphere(0)
        , navTargetFound(false)
{
    testAngles = {0, -1, +1};
    baseTestScales = {1, 1.025, 0.975, 1.05, 0.95, 1.075, 0.925, 1.1, 0.9, 1.125, 0.875};
    testScales = baseTestScales;
    navTargetScales = { 0.97, 1.0, 1.030927835, 1.06185567, /*1.092783505 , 1.12371134*/};

    auto hsvFilter = new HsvColorCropFilter();
    hsvFilter->ranges.emplace_back(cv::Vec3b(10,60,60),cv::Vec3b(30,255,255)); // limit Hue[10..30]
    filters.push_back(std::unique_ptr<ImageFilter>(hsvFilter));

    hsvFilter = new HsvColorCropFilter();
    hsvFilter->ranges.emplace_back(cv::Vec3b(0,0,80),cv::Vec3b(255,90,255)); // limit Saturation[..90] and Value[80..]
    dotsFilters.push_back(std::unique_ptr<ImageFilter>(hsvFilter));

    hsvFilter = new HsvColorCropFilter();
    hsvFilter->ranges.emplace_back(cv::Vec3b(20,240,160),cv::Vec3b(35,255,255));
    navTargetFilters.push_back(std::unique_ptr<ImageFilter>(hsvFilter));
    navTargetFilters.push_back(std::unique_ptr<ImageFilter>(new LaplacianFilter(1, 5)));

    hsvFilter = new HsvColorCropFilter();
    hsvFilter->ranges.emplace_back(cv::Vec3b(20,240,100),cv::Vec3b(35,255,255));
    distOCRFilters.push_back(std::unique_ptr<ImageFilter>(hsvFilter));

    extendLT = {40, 80};
    extendRB = {50, 140};
    threshold_min = 0.3;
    threshold_max = 0.8;
    cv::Mat dotFwdImage;
    cv::Mat dotFwdMask;
    cv::Mat dotBwdImage;
    cv::Mat dotBwdMask;
    loadImageAndMask("templates/compass/dot_fwd.png", dotFwdImage, dotFwdMask);
    loadImageAndMask("templates/compass/dot_bwd.png", dotBwdImage, dotBwdMask);
    compassDotsOrig.emplace_back(1.0, 0, "dot_fwd.png", dotFwdImage, dotFwdMask);
    compassDotsOrig.emplace_back(1.0, 0, "dot_bwd.png", dotBwdImage, dotBwdMask);

    targetReferenceRect = {180,540,1920-180*2,230};
    targetReferenceRadius = 48;
    cv::Mat targetImage;
    cv::Mat targetMask;
    loadImageAndMask("templates/compass/nav_target_base.png", targetImage, targetMask);
    navTargetOrig.emplace_back(1.0, 0, "nav_target_base.png", targetImage, targetMask);
}

double CompassDetector::match(ClassifyEnv &env) {
    lastHemisphere = 0;
    navTargetFound = false;
    lastNavTargetText.clear();

    {
        const std::string &ship = Master::getInstance().getConfiguration()->getShipType();
        auto fov = Master::getInstance().getConfiguration()->getConfigFOV();
        if (preprocessedShip != ship || preprocessedFOV != fov) {
            preprocessedTemplateScale = 0;
            preprocessedDotsScale = 0;
            preprocessedShip = ship;
            preprocessedFOV = fov;

            double shipCompassScale = 1;
            const widget::Screen *scr_cockpit = (const widget::Screen *) Master::getInstance().getCfgItem("scr-cockpit");
            if (scr_cockpit) {
                auto &varSet = scr_cockpit->varSetMap.at("compass");
                for (auto &vars: varSet) {
                    if (vars.keys.empty() || std::count(vars.keys.begin(), vars.keys.end(), ship)) {
                        shipCompassScale = vars.values.at("scale")[0];
                        break;
                    }
                }
            }
            testScales.clear();
            for (auto scl : baseTestScales)
                testScales.push_back(scl * shipCompassScale);
        }
    }

    auto startTime = std::chrono::high_resolution_clock::now();
    double compassMatch = ImageTemplate::match(env);
    auto elapsedTime = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - startTime);
    LOG(INFO) << "Compass detect took: " << elapsedTime.count() << "us";
    if (compassMatch < 0.5 || lastTemplatedx < 0) {
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
        for (auto &im: navTargetOrig) {
            for (auto scale : navTargetScales) {
                cv::Mat templImagePrepared = applyFilters(navTargetFilters, im.templImage);
                templImagePrepared = scaleImage(templImagePrepared, scale*env.getScale(), scale*env.getScale());
                cv::Mat templMaskPrepared;
                cv::threshold(templImagePrepared, templMaskPrepared, 10, 255, cv::THRESH_BINARY);
                navTargetPrepared.emplace_back(scale, 0, im.name, templImagePrepared, templMaskPrepared);
            }
        }

        int w = env.captureRect.width;
        int h = env.captureRect.height;
        cv::Mat spaceTargetRemap(h, w, CV_32FC2);
        int cx = w / 2;
        int cy = h / 2;

        auto fov = Master::getInstance().getConfiguration()->getConfigFOV();
        preprocessedFOV = fov;
        double fov_scale = std::lerp(1.0, 1.3, (fov-54)/6.);
        const widget::Screen *scr_cockpit = (const widget::Screen *) Master::getInstance().getCfgItem("scr-cockpit");
        if (scr_cockpit) {
            std::string res = std::format("{}x{}",w,h);
            auto &varSet = scr_cockpit->varSetMap.at("undistort");
            for (auto &vars: varSet) {
                if (vars.keys.empty() || std::count(vars.keys.begin(), vars.keys.end(), res)) {
                    auto& vals = vars.values.at("fov"); // for FOV [54,60]
                    fov_scale = std::lerp(vals[0], vals[1], (fov-54)/6.);
                    break;
                }
            }
        }
        double A = fov_scale / cx;
        spaceTargetRemap.forEach<cv::Point2f>([=](cv::Point2f& pixel, const int position[]) -> void {
            int y = position[0];
            int x = position[1];
            if (x == cx && y == cy) {
                pixel.x = x;
                pixel.y = y;
            } else {
                double off_x = x - cx;
                double off_y = y - cy;
                double radius = std::sqrt((off_x * off_x) + (off_y * off_y));
                double r_tan = A * radius;
                double scale = r_tan / std::atan(r_tan);
                pixel.x = cx + scale * off_x;
                pixel.y = cy + scale * off_y;
            }
        });
        cv::convertMaps(spaceTargetRemap, cv::noArray(), navTargetRemap1, navTargetRemap2, CV_16SC2, false);
    }

    startTime = std::chrono::high_resolution_clock::now();

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
        //LOG(DEBUG) << std::format("compass dot match result: {:.3f} for {}", (1-minVal), ((dotIdx&1)? "backward" : "forward"));
        if (1-minVal > bestDotVal) {
            bestDotVal = 1-minVal;
            bestDotIdx = dotIdx;
            bestDotLoc = minLoc;
            bestDotSize = {sm.templImage.cols, sm.templImage.rows};
        }
    }
    elapsedTime = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - startTime);
    LOG(INFO) << "Compass dot detect took: " << elapsedTime.count() << "us";
    if (bestDotVal >= threshold_dot) {
        LOG(DEBUG) << std::format("compass dot match result: {:.3f} for {}", (1-bestDotVal), ((bestDotIdx&1)? "backward" : "forward"));
        lastDotValue = bestDotVal;
        lastHemisphere = bestDotIdx & 1 ? -1 : +1;
        dotCaptureRect = { captureRect.tl()+bestDotLoc, bestDotSize };
        double radius = env.getScale() * (refRect.width-15.5) * 0.5 * imagesPrepared[lastTemplatedx].scale;
        dotSpherePosition = {
                std::clamp( ((bestDotLoc.x+bestDotSize.width*0.5) - captureRect.width*0.5) / radius, -1.0, +1.0),
                std::clamp(-((bestDotLoc.y+bestDotSize.height*0.5) - captureRect.height*0.5) / radius, -1.0, +1.0),
        };

        double pitch = std::asin(dotSpherePosition.y) * 90 / M_PI_2;
        double yaw = std::asin(dotSpherePosition.x) * 90 / M_PI_2;
        double roll = std::atan2(dotSpherePosition.x, dotSpherePosition.y) * 90 / M_PI_2;

        if (lastHemisphere < 0) {
            if (pitch > 0)
                pitch = 180 - pitch;
            else
                pitch = -180 - pitch;
        }
        if (lastHemisphere < 0) {
            if (yaw > 0)
                yaw = 180 - yaw;
            else
                yaw = -180 - yaw;
        }
        lastTgtPitch = pitch;
        lastTgtYaw = yaw;
        lastTgtRoll = roll;

        LOG(DEBUG) << std::format("Compass dot value={:.3f}, direction={}",
                                  lastDotValue, ((lastHemisphere<0) ? "backward" : "forward"))
                  << ", sphere pos=" << dotSpherePosition
                  << std_format(" pitch:{}, yaw:{}, roll:{}", int(pitch), int(yaw), int(roll));
    }

    if ((lastHemisphere > 0 && std::abs(lastTgtYaw) < 40 && lastTgtPitch >= -15 && lastTgtPitch <= 40) || env.isDebugMatch()) {
        startTime = std::chrono::high_resolution_clock::now();

        cv::Rect targetCaptureRect = env.scaleToCaptured(targetReferenceRect);
        targetCaptureRect.height += targetCaptureRect.y;
        targetCaptureRect.y = 0;
        cv::Mat correctedColorImage;
        cv::remap(env.getColorImage(), correctedColorImage, navTargetRemap1, navTargetRemap2, cv::INTER_LINEAR);
        if (env.isDebugMatch())
            cv::imwrite("undistorted-screen-color.png", correctedColorImage);
        imagePrepared = ImageTemplate::applyFilters(navTargetFilters, cv::Mat(correctedColorImage, targetCaptureRect));

        elapsedTime = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - startTime);
        LOG(INFO) << "Compass nav target image prepare took: " << elapsedTime.count() << "us";

        startTime = std::chrono::high_resolution_clock::now();

        double bestVal = -1000;
        cv::Point bestLoc;
        ImageMatrix* bestTempl = nullptr;
        for (auto& sm : navTargetPrepared) {
            int result_cols = targetCaptureRect.width - sm.templImage.cols + 1;
            int result_rows = targetCaptureRect.height - sm.templImage.rows + 1;
            cv::Mat result(result_rows, result_cols, CV_32FC1);
            cv::matchTemplate(imagePrepared, sm.templImage, result, cv::TM_SQDIFF_NORMED); // , sm.templMask
            //LOG(ERROR) << "dot " << dotIdx << " match result: " << result;
            //fixNaNinResult(result, sm.name);
            double minVal, maxVal;
            cv::Point minLoc, maxLoc;
            cv::minMaxLoc(result, &minVal, &maxVal, &minLoc, &maxLoc);
            // TM_SQDIFF_NORMED - the lower - the better, so use 1-minVal and minLoc
            if ((1-minVal) > bestVal) {
                bestVal = (1-minVal);
                bestLoc = minLoc;
                bestTempl = &sm;
            }
        }

        if (bestVal > 0.5 && bestTempl) {
            LOG(DEBUG) << std::format("compass target match result: {:.3f} scale: {:.3f}", bestVal, bestTempl->scale);
            cv::Point capturePos = bestLoc + targetCaptureRect.tl() + cv::Point(bestTempl->templImage.cols, bestTempl->templImage.rows) / 2;
            capturePos = env.scaleToReference(capturePos);
            lastNavTargetOffset = capturePos - env.ReferenceScreenCenter;
            navTargetFound = true;
        } else {
            lastNavTargetOffset = {};
        }
        elapsedTime = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - startTime);
        LOG(INFO) << "Compass nav target detect took: " << elapsedTime.count() << "us";

        if (navTargetFound) {
            startTime = std::chrono::high_resolution_clock::now();

            cv::Point2f imageCenter {float(correctedColorImage.cols), float(correctedColorImage.rows)};
            imageCenter *= 0.5f;
            cv::Point capturePos = bestLoc + targetCaptureRect.tl() + cv::Point(bestTempl->templImage.cols, bestTempl->templImage.rows) / 2;
            double dx = (capturePos.x - imageCenter.x) / imageCenter.x;
            double dy = (capturePos.y - imageCenter.y) / imageCenter.x;
            double sin_a = 0.3 * std::pow(std::abs(dx*dx*dy) + std::abs(dy*dy*dx), 0.8);
            if (dx*dy < 0)
                sin_a *= -1;
            double cos_a = std::sqrt(1 - sin_a*sin_a);
            double angle = std::asin(sin_a) * 180 / M_PI;

            int scaledSz = 200; // 400x200
            cv::Rect ocrRect {208, 109, 160, ocr::LINE_HEIGHT};

            int sz = bestTempl->templImage.rows;
            double scale = double(scaledSz) / bestTempl->templImage.rows;

            cv::Matx23d affineMatrix = cv::getRotationMatrix2D_({sz*0.5f, sz*0.5f}, angle, scale);
            affineMatrix.val[2] += (scaledSz - sz) * 0.5;
            affineMatrix.val[5] += (scaledSz - sz) * 0.5;

            cv::Rect srcRect {capturePos.x-sz/2, capturePos.y-sz/2, 2*sz, sz};
            cv::Mat targetImage (correctedColorImage, srcRect);
            cv::Mat rotatedImge;
            cv::warpAffine(targetImage, rotatedImge, affineMatrix, {2*scaledSz, scaledSz}, cv::INTER_CUBIC, cv::BORDER_TRANSPARENT);
            if (env.isDebugMatch()) {
                cv::Mat debugImage = rotatedImge.clone();
                cv::rectangle(debugImage, ocrRect, {255,255,255});
                cv::imwrite("nav-target-screen-color.png", debugImage);
            }
            cv::Mat ocrImage = ImageTemplate::applyFilters(distOCRFilters, cv::Mat(rotatedImge, ocrRect));
            cv::bitwise_not(ocrImage, ocrImage);
            std::string text;
            int conf = ocr::ocrLine("(nav dist)", ocrImage, text, nullptr);
            if (conf > 90) {
                lastNavTargetText = text;
                lastNavDist = parseDist(toUtf16(text));
            }
            elapsedTime = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - startTime);
            LOG(INFO) << "Compass nav target dist text took: " << elapsedTime.count() << "us";
        }
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


} // detect