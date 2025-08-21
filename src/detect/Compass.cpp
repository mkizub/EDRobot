//
// Created by mkizub on 02.07.2025.
//
#include "../pch.h"

#include "Detector.h"
#include "../EDWidget.h"
#include "../OCR.h"

#include <iomanip>

namespace detect {

CompassDetector::CompassDetector()
        : Detector()
        , threshold_dot{0.7}
        , lastHemisphere(0)
        , navTargetFound(false)
{
    baseTestScales = {1, 1.025, 0.975, 1.05, 0.95, /*1.075, 0.925, 1.1, 0.9, 1.125, 0.875*/};
    compassDetector = std::make_unique<ImageTemplate>("templates/compass/compass_full.png", std::make_shared<ConstRect>(679,803,71,71));
    compassDetector->testAngles = {0}; //{0, -1, +1};
    compassDetector->testScales = baseTestScales;
    compassDetector->extendLT = {20, 40}; //{40, 80};
    compassDetector->extendRB = {60, 60}; //{50, 140};
    compassDetector->threshold_min = 0.3;
    compassDetector->threshold_max = 0.8;
    compassDetector->matchMethod = cv::TM_CCORR;

    navTargetScales = {1.0};

    HsvMaskFilter* hsvFilter = new HsvMaskFilter();
    hsvFilter->rangesU.emplace_back(cv::Vec3b(10,50,254),cv::Vec3b(35,255,255)); // limit Hue[10..30]
    compassDetector->filters.push_back(std::unique_ptr<ImageFilter>(new GainBiasFilter(1.65, 0)));
    compassDetector->filters.push_back(std::unique_ptr<ImageFilter>(hsvFilter));

    hsvFilter = new HsvColorCropFilter();
    hsvFilter->rangesU.emplace_back(cv::Vec3b(0,0,80),cv::Vec3b(255,90,255)); // limit Saturation[..90] and Value[80..]
    dotsFilters.push_back(std::unique_ptr<ImageFilter>(hsvFilter));

    // HsvMaskCropFilter or HsvGrayCropFilter + LaplacianFilter + DilateFilter
    hsvFilter = new HsvMaskFilter();
    hsvFilter->rangesU.emplace_back(cv::Vec3b(20,120,160),cv::Vec3b(35,255,255));
    //hsvFilter->rangesU.emplace_back(cv::Vec3b(10,75,150),cv::Vec3b(25,110,255));
    navTargetFilters.push_back(std::unique_ptr<ImageFilter>(hsvFilter));
    //navTargetFilters.push_back(std::unique_ptr<ImageFilter>(new LaplacianFilter(1, 5)));
    //navTargetFilters.push_back(std::unique_ptr<ImageFilter>(new DilateFilter(3, 3, 2)));

    hsvFilter = new HsvGrayCropFilter();
    hsvFilter->rangesU.emplace_back(cv::Vec3b(15,120,200),cv::Vec3b(35,255,255));
    distOCRFilters.push_back(std::unique_ptr<ImageFilter>(new GainBiasFilter(1.2, 0)));
    distOCRFilters.push_back(std::unique_ptr<ImageFilter>(hsvFilter));

    XMat dotFwdImage;
    XMat dotBwdImage;
    ImageTemplate::loadImageAndMask("templates/compass/dot_fwd.png", dotFwdImage);
    ImageTemplate::loadImageAndMask("templates/compass/dot_bwd.png", dotBwdImage);
    compassDotsOrig.emplace_back(1.0, 0, "dot_fwd.png", dotFwdImage);
    compassDotsOrig.emplace_back(1.0, 0, "dot_bwd.png", dotBwdImage);

    targetReferenceRect = {1920/2-540,1080/2-540,540*2,540+260};
    targetReferenceRadius = 48;
    XMat targetImage;
    ImageTemplate::loadImageAndMask("templates/compass/nav_target_base.png", targetImage);
    navTargetOrig.emplace_back(1.0, 0, "nav_target_base.png", targetImage);
}

double CompassDetector::match(ClassifyEnv &env) {
    lastHemisphere = 0;
    navTargetFound = false;
    lastNavTargetText.clear();

    {
        const std::string &ship = Cfg.getShipType();
        double fov = Cfg.getConfigFOV();
        if (preprocessedShip != ship || preprocessedFOV != fov) {
            compassDetector->preprocessedTemplateScale = 0;
            preprocessedDotsScale = 0;
            preprocessedShip = ship;
            preprocessedFOV = fov;
            // config FOV is Vertical for 16:9 aspect ratio
            {
                double x_scale = double(env.captureRect.width) / env.ReferenceScreenSize.width;
                double y_scale = double(env.captureRect.height) / env.ReferenceScreenSize.height;
                double scale = std::min(x_scale, y_scale);
                double d = env.ReferenceScreenSize.height * 0.5 / std::tan(fov/2*M_PI/180); // distance to screen in pixels for reference 1920x1080
                double y =  env.captureRect.height * 0.5 / scale; // half-height of screen scaled to reference
                double x =  env.captureRect.width * 0.5 / scale; // half-width of screen scaled to reference
                captureFovY = 2 * std::atan(y / d) * 180 / M_PI;
                captureFovX = 2 * std::atan(x / d) * 180 / M_PI;
                captureFovY = std::round(captureFovY * 65536) / 65536;
                captureFovX = std::round(captureFovX * 65536) / 65536;
            }

            shipCompassScale = 1;
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
            compassDetector->testScales.clear();
            for (auto scl : baseTestScales)
                compassDetector->testScales.push_back(scl * shipCompassScale);
        }
    }

    auto totalStartTime = std::chrono::high_resolution_clock::now();

    //auto startTime = std::chrono::high_resolution_clock::now();
    double compassMatch = compassDetector->match(env);
    //auto elapsedTime = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - startTime);
    //LOG(INFO) << "Compass detect took: " << elapsedTime.count() << "us";
    if (compassMatch < 0.5 || compassDetector->lastTemplatedx < 0) {
        return compassMatch;
    }

    //
    // Detect compass dot
    //
    if (preprocessedDotsScale != env.getScale()) {
        preprocessedDotsScale = env.getScale();
        for (auto &im: compassDotsOrig) {
            compassDotsPrepared.push_back(ImageTemplate::prepareImageMatrix(env, dotsFilters, im.templImageU, 1, 0, im.name));
        }
        for (auto &im: navTargetOrig) {
            for (auto scale : navTargetScales) {
                navTargetPrepared.push_back(ImageTemplate::prepareImageMatrix(env, navTargetFilters, im.templImageU, scale/env.getScale(), 0, im.name));
            }
        }

        auto fov = Cfg.getConfigFOV();
        preprocessedFOV = fov;
        double fov_scale = std::lerp(1.0, 1.3, (fov-54)/6.);
        const widget::Screen *scr_cockpit = (const widget::Screen *) Master::getInstance().getCfgItem("scr-cockpit");
        if (scr_cockpit) {
            std::string res = std::format("{}x{}", env.captureRect.width, env.captureRect.height);
            auto &varSet = scr_cockpit->varSetMap.at("undistort");
            for (auto &vars: varSet) {
                if (vars.keys.empty() || std::count(vars.keys.begin(), vars.keys.end(), res)) {
                    auto& vals = vars.values.at("fov"); // for FOV [54,60]
                    fov_scale = std::lerp(vals[0], vals[1], (fov-54)/6.);
                    break;
                }
            }
        }

        cv::Rect remapRect = targetReferenceRect;
        remapRect = ImageTemplate::makeOptimalMatchRect(remapRect);
        targetRemapRect = remapRect;
        navTargetRemapXY.create(remapRect.height, remapRect.width, CV_32FC2);
        const int cx = env.ReferenceScreenCenter.x;
        const int cy = env.ReferenceScreenCenter.y;
        const int dx = remapRect.x;
        const int dy = remapRect.y;
        const int ccx = env.captureRect.width/2;
        const int ccy = env.captureRect.height/2;

        const double A = fov_scale / cx;
        const double env_scale = env.getScale();
        navTargetRemapXY.forEach<cv::Point2f>([=](cv::Point2f& pixel, const int position[]) -> void {
            int y = position[0] + dy;
            int x = position[1] + dx;
            double off_x = x - cx;
            double off_y = y - cy;
            double radius = std::sqrt((off_x * off_x) + (off_y * off_y));
            double r_tan = A * radius;
            double undistort_scale;
            if (r_tan == 0)
                undistort_scale = 1;
            else
                undistort_scale = r_tan / std::atan(r_tan);
            pixel.x = ccx + undistort_scale * off_x * env_scale;
            pixel.y = ccy + undistort_scale * off_y * env_scale;
        });
        cv::convertMaps(navTargetRemapXY, cv::noArray(), navTargetRemap1, navTargetRemap2, CV_16SC2, false);
    }

    //startTime = std::chrono::high_resolution_clock::now();

    {
        cv::Rect matchRect = ImageTemplate::makeOptimalMatchRect(compassDetector->captureRect);
        env.cropToCapture(matchRect);
        XMat imagePrepared = ImageTemplate::applyFilters(dotsFilters, env.getColorImage()(matchRect));
        ImageTemplate::MatchResult dr;
        ImageTemplate::matchTemplates(cv::TM_CCORR_NORMED, imagePrepared, compassDotsPrepared, dr);
        //elapsedTime = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - startTime);
        //LOG(INFO) << "Compass dot detect took: " << elapsedTime.count() << "us";
        if (dr.value >= threshold_dot) {
            LOG(DEBUG) << std::format("compass dot match result: {:.3f} for {}", dr.value,
                                      ((dr.index & 1) ? "backward" : "forward"));
            cv::Size dotSize = {dr.im->opt_w, dr.im->opt_h};
            lastDotValue = dr.value;
            lastHemisphere = dr.index & 1 ? -1 : +1;
            dotCaptureRect = {matchRect.tl() + dr.loc , dotSize};
            double radius = env.getScale() * (compassDetector->refRect.width - 14.0) * 0.5 * compassDetector->imagesPrepared[dr.index].scale;
            dotSpherePosition = {
                    std::clamp((dr.loc.x + dotSize.width * 0.5 - matchRect.width * 0.5) / radius, -1.0, +1.0),
                    std::clamp(-(dr.loc.y + dotSize.height * 0.5 - matchRect.height * 0.5) / radius, -1.0, +1.0),
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
                                      lastDotValue, ((lastHemisphere < 0) ? "backward" : "forward"))
                       << ", sphere pos=" << dotSpherePosition
                       << std_format(" pitch:{}, yaw:{}, roll:{}", int(pitch), int(yaw), int(roll));
        }
    }

    if ((lastHemisphere > 0 && std::abs(lastTgtYaw) < 25 && lastTgtPitch >= -10 && lastTgtPitch <= 25) || env.isDebugMatch()) {
        //startTime = std::chrono::high_resolution_clock::now();

        XMat correctedColorImage;
        cv::remap(env.getColorImage(), correctedColorImage, navTargetRemap1, navTargetRemap2, cv::INTER_LINEAR);
        if (env.isDebugMatch()) {
            cv::imwrite("undistorted-screen-color.png", correctedColorImage);
        }
        XMat imagePrepared = ImageTemplate::applyFilters(navTargetFilters, correctedColorImage);

        //elapsedTime = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - startTime);
        //LOG(INFO) << "Compass nav target image prepare took: " << elapsedTime.count() << "us";

        //startTime = std::chrono::high_resolution_clock::now();

        ImageTemplate::MatchResult nr;
        ImageTemplate::matchTemplates(cv::TM_CCORR, imagePrepared, navTargetPrepared, nr);
        if (nr.value > 0.5 && nr.im) {
            LOG(DEBUG) << std::format("compass target match result: {:.3f}", nr.value);
            cv::Point foundPos = nr.loc + cv::Point(nr.im->opt_w, nr.im->opt_h) / 2;
            auto capturePosF = navTargetRemapXY.at<cv::Point2f>(foundPos);
            cv::Point referencePos = env.cvtCapturedToReference(capturePosF);
            lastNavTargetOffset = referencePos - env.ReferenceScreenCenter;
            navTargetFound = true;
        } else {
            lastNavTargetOffset = {};
            navTargetFound = false;
        }
        //elapsedTime = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - startTime);
        //LOG(INFO) << "Compass nav target detect took: " << elapsedTime.count() << "us";

        if (navTargetFound) {
            double fov = Cfg.getConfigFOV();
            double d = env.ReferenceScreenSize.height * 0.5 / std::tan(fov/2*M_PI/180); // distance to screen in pixels for reference 1920x1080
            double yaw = std::atan(lastNavTargetOffset.x / d) * 180 / M_PI;
            double pitch = -std::atan(lastNavTargetOffset.y / d) * 180 / M_PI;
            double roll = std::atan2(lastNavTargetOffset.x, -lastNavTargetOffset.y) * 90 / M_PI_2;
            LOG(DEBUG) << "Update compass from nav target: "
                << std::format("pitch:{:+.1f} yaw:{:+.1f} roll:{:+.1f} (delta: {:+.1f}; {:+.1f}; {:+.1f})",
                               pitch, yaw, roll, pitch-lastTgtPitch, yaw-lastTgtYaw, roll-lastTgtRoll);
            lastTgtPitch = pitch;
            lastTgtYaw = yaw;
            lastTgtRoll = roll;
        }

        if (navTargetFound) {
            //startTime = std::chrono::high_resolution_clock::now();

            float cx = env.ReferenceScreenCenter.x;
            float cy = env.ReferenceScreenCenter.y;
            cv::Point foundPos = nr.loc + cv::Point(nr.im->opt_w, nr.im->opt_h) / 2;
            double dx = (foundPos.x + targetRemapRect.x - cx) / cx;
            double dy = (foundPos.y + targetRemapRect.y - cy) / cx;
            double sin_a = 0.3 * std::pow(std::abs(dx*dx*dy) + std::abs(dy*dy*dx), 0.8);
            if (dx*dy < 0)
                sin_a *= -1;
            double cos_a = std::sqrt(1 - sin_a*sin_a);
            double angle = std::asin(sin_a) * 180 / M_PI;

            int scaledSz = 210; // 420x210
            cv::Rect ocrRect {210, 114, 170, ocr::LINE_HEIGHT};

            int sz = nr.im->opt_h;
            double scale = double(scaledSz) / nr.im->opt_h;

            cv::Matx23d affineMatrix = cv::getRotationMatrix2D_({sz*0.5f, sz*0.5f}, angle, scale);
            affineMatrix.val[2] += (scaledSz - sz) * 0.5;
            affineMatrix.val[5] += (scaledSz - sz) * 0.5;

            cv::Rect srcRect {foundPos.x-sz/2, foundPos.y-sz/2, 2*sz, sz};
            srcRect &= cv::Rect(0,0,correctedColorImage.cols,correctedColorImage.rows);
            XMat targetImage (correctedColorImage, srcRect);
            XMat rotatedImge;
            cv::warpAffine(targetImage, rotatedImge, affineMatrix, {2*scaledSz, scaledSz}, cv::INTER_LINEAR, cv::BORDER_TRANSPARENT);
            if (env.isDebugMatch()) {
                cv::Mat debugImage;
                rotatedImge.copyTo(debugImage);
                cv::rectangle(debugImage, ocrRect, {255,255,255});
                cv::imwrite("nav-target-screen-color.png", debugImage);
            }
            XMat ocrImage = ImageTemplate::applyFilters(distOCRFilters, rotatedImge(ocrRect));
            cv::bitwise_not(ocrImage, ocrImage);
            std::string text;
            int conf = ocr::ocrLine("(nav dist)", toMat(ocrImage), text, nullptr);
            if (conf > 80) {
                lastNavTargetText = text;
                lastNavDist = parseDist(toUtf16(text));
            }
            //elapsedTime = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - startTime);
            //LOG(INFO) << "Compass nav target dist text took: " << elapsedTime.count() << "us";
        }
    }
    auto totalElapsedTime = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - totalStartTime);
    LOG(INFO) << "Compass detection took: " << std::format("{}ms",totalElapsedTime.count());

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

    cv::Mat img = toMat(env.getColorImage())(matchRect);
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