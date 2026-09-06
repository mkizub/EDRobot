//
// Created by mkizub on 02.07.2025.
//
#include "../pch.h"

#include "Detector.h"
#include "Compass.h"
#include "Lines.h"
#include "../widget/EDWidget.h"
#include "../OCR.h"

#include <iomanip>
#include <glob/glob.h>

namespace detect {

void CompassDetector::standaloneTest(std::string image_filename) {
    cv::Mat fileImage = cv::imread(image_filename, cv::IMREAD_UNCHANGED); // assume GRAY/BGR/BGRA
    XMat debugImage = toXMat(fileImage);
    ClassifyEnv debugEnv;
    debugEnv.init(debugImage, false, 1);

    auto det = std::make_unique<detect::CompassDetector>();
    double match = det->match(debugEnv);
    LOG_INFO("CompassDetector::standaloneTest: {:.1f}", match);
}

CompassDetector::CompassDetector()
        : Detector()
        , threshold_dot{0.4}
        , lastHemisphere(0)
        , navTargetFound(false)
{
    loadCompass();
    HsvMaskFilter* hsvFilter;

    // dot in compass (forward/backward)
    XMat dotFwdImage;
    XMat dotBwdImage;
    ImageTemplate::loadImageAndMask("templates/compass/dot_fwd.png", dotFwdImage);
    ImageTemplate::loadImageAndMask("templates/compass/dot_bwd.png", dotBwdImage);
    compassDotsOrig.emplace_back(1.0, 0, "dot_fwd.png", dotFwdImage);
    compassDotsOrig.emplace_back(1.0, 0, "dot_bwd.png", dotBwdImage);

    //hsvFilter = new HsvGrayCropFilter();
    //hsvFilter->rangesU.emplace_back(cv::Vec3b(30,0,180),cv::Vec3b(100,90,255)); // limit Saturation[..90] and Value[80..]
    //dotsFilters.push_back(std::unique_ptr<ImageFilter>(hsvFilter));
    //dotsFilters.push_back(std::unique_ptr<ImageFilter>(new SobelFilter));
    dotsFilters.push_back(std::unique_ptr<ImageFilter>(new GradientFilter(false, 2)));

    // nav target (3/4 circle with distance)
    navTargetReferenceRadius = 48;
    navTargetScales = {1.0};
    targetReferenceRect = {1920/2-540,1080/2-540,540*2,540+260};
    XMat targetImage;
    ImageTemplate::loadImageAndMask("templates/compass/nav_target_base.png", targetImage);
    FovScale fov_scale;
    fov_scale.scale60 = 0.857;
    cv::Size targetSize = fov_scale.apply(targetImage.size(), fov_scale.fov54);
    if (targetSize != targetImage.size())
        cv::resize(targetImage, targetImage, targetSize);
    navTargetOrig.emplace_back(1.0, 0, "nav_target_base.png", targetImage);

    // HsvMaskCropFilter or HsvGrayCropFilter + LaplacianFilter + DilateFilter
    hsvFilter = new HsvMaskFilter();
    hsvFilter->rangesU.emplace_back(cv::Vec3b(20,120,160),cv::Vec3b(35,255,255));
    //hsvFilter->rangesU.emplace_back(cv::Vec3b(10,75,150),cv::Vec3b(25,110,255));
    navTargetFilters.push_back(std::unique_ptr<ImageFilter>(hsvFilter));
    //navTargetFilters.push_back(std::unique_ptr<ImageFilter>(new LaplacianFilter(1, 5)));
    //navTargetFilters.push_back(std::unique_ptr<ImageFilter>(new DilateFilter(3, 3, 2)));
    navTargetFilters.push_back(std::unique_ptr<ImageFilter>(new DilateFilter(3, 3, 1)));

    lineDetector = std::make_unique<detect::LineDetector>(nullptr);
    lineDetector->filters.push_back(std::unique_ptr<ImageFilter>(new ChannelFilter(ChannelFilter::gray)));
    lineDetector->filters.push_back(std::unique_ptr<ImageFilter>(new LinesFilter(false, 1, 10, 1, 0, 0)));
    lineDetector->extendLT = {1, 110};
    lineDetector->extendRB = {1, 110};
    lineDetector->extendAngleMin = 15;
    lineDetector->extendAngleMax = 15;
    lineDetector->angleStep = 1;
    lineDetector->houghThreshold = 60;

    hsvFilter = new HsvGrayCropFilter();
    hsvFilter->rangesU.emplace_back(cv::Vec3b(15,120,200),cv::Vec3b(35,255,255));
    distOCRFilters.push_back(std::unique_ptr<ImageFilter>(hsvFilter));
}

CompassDetector::~CompassDetector() {
    navTargetRemapXY.release();
    navTargetRemap1.release();
    navTargetRemap2.release();
    compassDetector.reset();
    dotsFilters.clear();
    navTargetFilters.clear();
    distOCRFilters.clear();
    compassDotsPrepared.clear();
    compassDotsOrig.clear();
    navTargetPrepared.clear();
    navTargetOrig.clear();
}

void CompassDetector::loadCompass() {
    preprocessedShip = toLower(st::shipInfo.shipType);
    const widget::Screen *scr_cockpit = (const widget::Screen *) Master::getInstance().getCfgItem("scr-cockpit");
    cv::Rect compassRefRect;
    cv::Point compassExtLT;
    cv::Point compassExtRB;
    FovScale fov_scale;
    fov_scale.scale60 = 0.857;
    auto &varSet = scr_cockpit->varSetMap.at("compass");
    for (auto &vars: varSet) {
        if (vars.keys.empty() || std::count(vars.keys.begin(), vars.keys.end(), preprocessedShip)) {
            auto v = vars.values;
            compassRefSize.width = v["size"][0];
            compassRefSize.height = v["size"][1];
            compassRefSize *= fov_scale.getScaleForFOV(fov_scale.fov54);
            compassRefRect.x = v["rect"][0];
            compassRefRect.y = v["rect"][1];
            compassRefRect.width = v["rect"][2];
            compassRefRect.height = v["rect"][3];
            compassRefRect = fov_scale.apply(compassRefRect, fov_scale.fov54);
            compassExtLT.x = v["ext"][0];
            compassExtLT.y = v["ext"][1];
            compassExtLT = fov_scale.apply(cv::Size(compassExtLT), fov_scale.fov54);
            compassExtRB.x = v["ext"][2];
            compassExtRB.y = v["ext"][3];
            compassExtRB = fov_scale.apply(cv::Size(compassExtRB), fov_scale.fov54);
            break;
        }
    }
    compassImageName = std::format("templates/compass/compass*_{}.png",preprocessedShip);
    bool use_gray_compass = false;
    auto paths = glob::glob(compassImageName);
    if (paths.empty()) {
        use_gray_compass = true;
        compassImageName = "templates/compass/compass*_default.png";
    }

    compassDetector = std::make_unique<ImageTemplate>(compassImageName, std::make_shared<ConstRect>(compassRefRect));
    compassDetector->testAngles = {0}; //{0, -1, +1};
    compassDetector->testScales = {1, 82./80., 80./82., 84./80., 80./84};
    compassDetector->cropOptimalSize = true;
    compassDetector->extendLT = compassExtLT;
    compassDetector->extendRB = compassExtRB;
    compassDetector->threshold_min = 0.3;
    compassDetector->threshold_max = 0.8;
    compassDetector->matchMethod = cv::TM_CCORR_NORMED;

    compassDetector->filters.push_back(std::unique_ptr<ImageFilter>(new CompassFilter));
}

double CompassDetector::match(ClassifyEnv &env) {
    bool tryNavTargetFirst = navTargetFound;
    clear();

    {
        if (preprocessedShip != toLower(st::shipInfo.shipType)) {
            loadCompass();
        }
        double fov = Cfg.getConfigFOV();
        XMat image = env.getColorImage();
        if (preprocessedFOV != fov) {
            compassDetector->preprocessedTemplateScale = 0;
            preprocessedDotsScale = 0;
            preprocessedFOV = fov;

            // config FOV is Vertical for 16:9 aspect ratio
            {
                double d = ReferenceScreenSize.height * 0.5 / std::tan(fov/2*M_PI/180); // distance to screen in pixels for reference 1920x1080
                double y = image.rows * 0.5 / env.getScale(); // half-height of screen scaled to reference
                double x = image.cols * 0.5 / env.getScale(); // half-width of screen scaled to reference
                captureFovY = 2 * std::atan(y / d) * 180 / M_PI;
                captureFovX = 2 * std::atan(x / d) * 180 / M_PI;
                captureFovY = std::round(captureFovY * 65536) / 65536;
                captureFovX = std::round(captureFovX * 65536) / 65536;
            }
        }
        if (preprocessedDotsScale != env.getScale()) {
            preprocessedDotsScale = env.getScale();
            for (auto &im: compassDotsOrig) {
                compassDotsPrepared.push_back(
                        ImageTemplate::prepareImageMatrix(env, dotsFilters, im.templImageU, 1, 0, im.name));
            }
            for (auto &im: navTargetOrig) {
                for (auto scale: navTargetScales) {
                    navTargetPrepared.push_back(ImageTemplate::prepareImageMatrix(env, navTargetFilters, im.templImageU,
                                                                                  scale / env.getScale(), 0, im.name));
                }
            }

            auto fov = Cfg.getConfigFOV();
            preprocessedFOV = fov;
            double fov_scale = std::lerp(1.0, 1.3, (fov - 54) / 6.);
            const widget::Screen *scr_cockpit = (const widget::Screen *) Master::getInstance().getCfgItem(
                    "scr-cockpit");
            if (scr_cockpit) {
                std::string res = std::format("{}x{}", image.cols, image.rows);
                auto &varSet = scr_cockpit->varSetMap.at("undistort");
                for (auto &vars: varSet) {
                    if (vars.keys.empty() || std::count(vars.keys.begin(), vars.keys.end(), res)) {
                        auto &vals = vars.values.at("fov"); // for FOV [54,60]
                        fov_scale = std::lerp(vals[0], vals[1], (fov - 54) / 6.);
                        break;
                    }
                }
            }

            cv::Rect remapRect = targetReferenceRect;
            remapRect = ImageTemplate::makeOptimalMatchRect(remapRect);
            targetRemapRect = remapRect;
            navTargetRemapXY.create(remapRect.height, remapRect.width, CV_32FC2);
            const int cx = ReferenceScreenCenter.x;
            const int cy = ReferenceScreenCenter.y;
            const int dx = remapRect.x;
            const int dy = remapRect.y;
            const int ccx = image.cols / 2;
            const int ccy = image.rows / 2;

            const double A = fov_scale / cx;
            const double env_scale = env.getScale();
            navTargetRemapXY.forEach<cv::Point2f>([=](cv::Point2f &pixel, const int position[]) -> void {
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
    }

    //LOG(INFO) << "Compass detect started";
    auto totalStartTime = std::chrono::high_resolution_clock::now();

    if (tryNavTargetFirst) {
        detectNavTarget(env, true);
        if (navTargetFound) {
            //auto totalElapsedTime = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - totalStartTime);
            //LOG(INFO) << "NavTarget detection took: " << std::format("{}ms",totalElapsedTime.count());
            return true;
        }
    }

    bool can_use_compass = true; //!(st::ship.flags.fsd_charging || st::ship.flags2.fsd_hyperdrive_charging);
    double compassMatch = 0;
    if (can_use_compass) {
        //auto startTime = std::chrono::high_resolution_clock::now();
        compassMatch = compassDetector->match(env);
        //auto elapsedTime = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - startTime);
        //LOG(INFO) << "Compass detect took: " << elapsedTime.count() << "us";
    }
    if (compassMatch >= 0.5) {
        //
        // Detect compass dot
        //
        //auto startTime = std::chrono::high_resolution_clock::now();
        cv::Rect matchRect = compassDetector->captureRect;
        env.cropToCapture(matchRect);
        XMat imagePrepared = ImageTemplate::applyFilters(dotsFilters, env.getColorImage()(matchRect), {.convertToFloat=true});
        ImageTemplate::MatchResult dr;
        ImageTemplate::matchTemplates(cv::TM_CCOEFF_NORMED, imagePrepared, compassDotsPrepared, dr);
        //auto elapsedTime = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - startTime);
        //LOG(INFO) << "Compass dot detect took: " << elapsedTime.count() << "us";
        if (dr.value >= threshold_dot) {
            LOG_DEBUG("compass dot match result: {:.3f} for {}", dr.value,
                                      ((dr.index & 1) ? "backward" : "forward"));
            cv::Size dotSize = {dr.im->org_w, dr.im->org_h};
            lastHemisphere = dr.index & 1 ? -1 : +1;
            dotCaptureRect = {matchRect.tl() + dr.loc, dotSize};
            double radiusX = env.getScale() * compassRefSize.width * 0.5 * compassDetector->imagesPrepared[dr.index].scale;
            double radiusY = env.getScale() * compassRefSize.height * 0.5 * compassDetector->imagesPrepared[dr.index].scale;
            dotSpherePosition = {
                    std::clamp((dr.loc.x + dotSize.width * 0.5 - matchRect.width * 0.5) / radiusX, -1.0, +1.0),
                    std::clamp(-(dr.loc.y + dotSize.height * 0.5 - matchRect.height * 0.5) / radiusY, -1.0, +1.0),
            };
            double normSpherePosition = cv::norm(dotSpherePosition);
            if (normSpherePosition > 1)
                dotSpherePosition /= normSpherePosition*1.00000000001;

            double pitch = std::asin(dotSpherePosition.y) * 90 / M_PI_2;
            double yaw = std::asin(dotSpherePosition.x) * 90 / M_PI_2;
            double angle = std::asin(norm(dotSpherePosition)) * 90 / M_PI_2;
            double roll = 0;
            if (angle >= 3)
                roll = std::atan2(dotSpherePosition.x, dotSpherePosition.y) * 90 / M_PI_2;

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
            if (lastHemisphere < 0)
                angle = 180 - angle;
            lastTgtPitch = pitch;
            lastTgtYaw = yaw;
            lastTgtRoll = roll;
            lastTgtAngle = angle;

            LOG_DEBUG("Compass dot dir={}, sphere pos=[{:3f},{:3f}] pitch:{}, yaw:{}, roll:{}",
                      ((lastHemisphere < 0) ? "bwd" : "fwd"), dotSpherePosition.x, dotSpherePosition.y,
                      int(pitch), int(yaw), int(roll));
        } else {
            LOG_WARNING("compass dot failed, match result: {:.3f}", dr.value);
        }
    }

    if (!can_use_compass || !tryNavTargetFirst || (lastHemisphere > 0 && std::abs(lastTgtYaw) < 25 && lastTgtPitch >= -10 && lastTgtPitch <= 25) || env.isDebugMatch())
        detectNavTarget(env, false);

    //auto totalElapsedTime = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - totalStartTime);
    //LOG(INFO) << "Compass detection took: " << std::format("{}ms",totalElapsedTime.count());

    return navTargetFound ? 1 : can_use_compass ? compassMatch : 0;
}

void CompassDetector::detectNavTarget(ClassifyEnv& env, bool afterCompass) {
    //auto startTime = std::chrono::high_resolution_clock::now();

    XMat correctedColorImage;
    cv::remap(env.getColorImage(), correctedColorImage, navTargetRemap1, navTargetRemap2, cv::INTER_LINEAR);
    if (env.isDebugMatch()) {
        cv::imwrite("cache/undistorted-screen-color.png", correctedColorImage);
    }
    XMat imagePrepared = ImageTemplate::applyFilters(navTargetFilters, correctedColorImage);

    //auto elapsedTime = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - startTime);
    //LOG(INFO) << "Compass nav target image prepare took: " << elapsedTime.count() << "us";

    //startTime = std::chrono::high_resolution_clock::now();

    ImageTemplate::MatchResult nr;
    ImageTemplate::matchTemplates(cv::TM_CCORR_NORMED, imagePrepared, navTargetPrepared, nr);
    cv::Point2f navCapturePos;
    if (nr.value > 0.5 && nr.im) {
        LOG_DEBUG("compass target match result: {:.3f}", nr.value);
        cv::Point foundPos = nr.loc + cv::Point(nr.im->org_w, nr.im->org_h) / 2;
        navCapturePos = navTargetRemapXY.at<cv::Point2f>(foundPos);
        cv::Point referencePos = env.cvtCapturedToReference(navCapturePos);
        lastNavTargetOffset = referencePos - ReferenceScreenCenter;
        navTargetFound = true;
        lastHemisphere = 1;
    } else {
        lastNavTargetOffset = {};
        navTargetFound = false;
    }
    //elapsedTime = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - startTime);
    //LOG(INFO) << "Compass nav target detect took: " << elapsedTime.count() << "us";

    if (navTargetFound) {
        double fov = Cfg.getConfigFOV();
        double d = ReferenceScreenSize.height * 0.5 / std::tan(fov/2*M_PI/180); // distance to screen in pixels for reference 1920x1080
        double yaw = std::atan(lastNavTargetOffset.x / d) * 180 / M_PI;
        double pitch = -std::atan(lastNavTargetOffset.y / d) * 180 / M_PI;
        double angle = std::asin(std::min(1.0,cv::norm(lastNavTargetOffset) / d)) * 90 / M_PI_2;
        double roll = 0;
        if (angle > 1)
            roll = std::atan2(lastNavTargetOffset.x, -lastNavTargetOffset.y) * 90 / M_PI_2;
        if (lastHemisphere < 0)
            angle = 180 - angle;
        if (afterCompass) {
            LOG_DEBUG("Update compass from nav target: pitch:{:+.1f} yaw:{:+.1f} roll:{:+.1f} (delta: {:+.1f}; {:+.1f}; {:+.1f})",
                                      pitch, yaw, roll, pitch - lastTgtPitch, yaw - lastTgtYaw, roll - lastTgtRoll);
        } else {
            LOG_DEBUG("Detected nav target: pitch:{:+.1f} yaw:{:+.1f} roll:{:+.1f}", pitch, yaw, roll);
        }
        lastTgtPitch = pitch;
        lastTgtYaw = yaw;
        lastTgtRoll = roll;
        lastTgtAngle = angle;
    }

    if (navTargetFound && lastTgtPitch >= -10 && lastTgtPitch <= +20 && lastTgtYaw >= -25 && lastTgtYaw <= +25) {
        //auto startTime = std::chrono::high_resolution_clock::now();

        int scaledSz = 210; // 420x210
        cv::Rect lineRect {200, 0, 210, 210};
        cv::Rect ocrRect {210, 114, 170, 92};

        int sz = nr.im->org_h;
//            double scale = double(scaledSz) / nr.im->org_h;
//
//            cv::Matx23d affineMatrix = cv::getRotationMatrix2D_({sz*0.5f, sz*0.5f}, angle, scale);
//            affineMatrix.val[2] += (scaledSz - sz) * 0.5;
//            affineMatrix.val[5] += (scaledSz - sz) * 0.5;

        cv::Rect srcRect {(int)std::round(navCapturePos.x-sz*0.5*env.getScale()),
                          (int)std::round(navCapturePos.y-sz*0.5*env.getScale()),
                          (int)std::round(2*sz*env.getScale()),
                          (int)std::round(sz*env.getScale())};
        env.cropToCapture(srcRect);
        XMat targetImage (env.getColorImage(), srcRect);
        XMat normImage;
//            if (std::abs(angle) > 2) {
//                cv::warpAffine(targetImage, normImage, affineMatrix, {2 * scaledSz, scaledSz}, cv::INTER_LINEAR, cv::BORDER_TRANSPARENT);
//            } else {
        cv::resize(targetImage, normImage, {2 * scaledSz, scaledSz}, cv::INTER_LINEAR);
        {
            XMat lineImage = normImage(lineRect);
            ClassifyEnv lineEnv;
            lineEnv.init(lineImage, true, 1);
            lineDetector->withRefLine = cv::Line(0, 105, lineRect.width, 105);
            double ln_match = lineDetector->match(lineEnv);
            if (ln_match > 0.5 && std::abs(lineDetector->lastAvrgAngle) > 2) {
                cv::Size size = {2 * scaledSz, scaledSz};
                cv::Point2f center {navCapturePos.x, navCapturePos.y};
                double scale = double(scaledSz) / sz;
                cv::Matx23d rotationMatrix = cv::getRotationMatrix2D_(center, lineDetector->lastAvrgAngle, scale);
                rotationMatrix.val[2] -= navCapturePos.x-scaledSz*0.5*env.getScale();
                rotationMatrix.val[5] -= navCapturePos.y-scaledSz*0.5*env.getScale();
                XMat out;
                cv::warpAffine(env.getColorImage(), out, rotationMatrix, size, cv::INTER_LINEAR, cv::BORDER_TRANSPARENT);
                normImage = out;
            }
        }
        if (env.isDebugMatch()) {
            cv::Mat debugImage;
            normImage.copyTo(debugImage);
            cv::rectangle(debugImage, ocrRect, {255,255,255});
            cv::imwrite("cache/nav-target-screen-color.png", debugImage);
        }
        XMat ocrImage = ImageTemplate::applyFilters(distOCRFilters, normImage(ocrRect));
        cv::bitwise_not(ocrImage, ocrImage);
        std::string text;
        int conf =  ocr::ocrTargetDistText(toMat(ocrImage), text);
        int nl = text.find('\n');
        if (nl == std::string::npos) {
            lastNavDist = parseDist(toUtf16(text), conf);
        } else {
            lastNavDist = parseDist(toUtf16(text.substr(0,nl)), conf);
            lastNavDist.tsec = parseDistTime(toUtf16(text.substr(nl+1)));
        }
//        if (env.isDebugMatch()) {
//            static int counter = 0;
//            if (!counter) {
//                for (const auto &entry: std::filesystem::directory_iterator("cache/testset-edr")) {
//                    if (!entry.is_regular_file())
//                        continue;
//                    auto &ep = entry.path();
//                    if (!ep.has_extension() || ep.extension() != ".png")
//                        continue;
//                    if (!ep.filename().string().starts_with("nav-tgt-"))
//                        continue;
//                    auto strings = split(ep.filename().string(), '-');
//                    int num = std::stoi(strings[2], nullptr, 10);
//                    counter = std::max(num, counter);
//                }
//            }
//            counter += 1;
//            cv::Mat distImage;
//            cv::Mat timeImage;
//            ocr::ocrTargetDistForTraining(toMat(ocrImage), text, distImage, timeImage);
//            int nl = text.find('\n');
//            std::string dist_text;
//            std::string time_text;
//            if (nl == std::string::npos) {
//                dist_text = text;
//            } else {
//                dist_text = text.substr(0,nl);
//                time_text = text.substr(nl+1);
//            }
//
//            std::string filename = std::format("cache/testset-edr/nav-tgt-dist-{:04d}", counter);
//            cv::imwrite(filename+".png", distImage);
//            std::ofstream gt_txt(filename+".gt.txt", std::ios::trunc | std::ios::binary);
//            gt_txt << dist_text;
//            gt_txt.close();
//            filename = std::format("cache/testset-edr/nav-tgt-time-{:04d}", counter);
//            cv::imwrite(filename+".png", timeImage);
//            gt_txt.open(filename+".gt.txt", std::ios::trunc | std::ios::binary);
//            gt_txt << time_text;
//            gt_txt.close();
//        }
        //auto elapsedTime = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - startTime);
        //LOG(INFO) << "Compass nav target dist text took: " << elapsedTime.count() << "us";
    }
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