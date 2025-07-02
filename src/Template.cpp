//
// Created by mkizub on 23.05.2025.
//

#include "pch.h"

#include "Template.h"
#include <format>
#include <iomanip>

double SequenceTemplate::match(ClassifyEnv& env) {
    double sumWeights = 0;
    for (auto& oracle : oracles) {
        if (oracle->classifierWeight <= 0)
            continue;
        sumWeights += oracle->classifierWeight;
    }
    double sum = 0;
    for (auto& oracle : oracles) {
        if (oracle->classifierWeight <= 0)
            continue;
        double value = oracle->classify(env);
        double weight = oracle->classifierWeight / sumWeights;
        sum += weight * (2*value - 1);
    }
    return (sum + 1) / 2;
}

double SequenceTemplate::classify(ClassifyEnv& env) {
    const auto sz = env.classified.size();
    double result = match(env);
    if (result < 0.5) {
        while (sz < env.classified.size())
            env.classified.pop_back();
    }
    return result;
}

double SequenceTemplate::debugMatch(ClassifyEnv& env) {
    json5pp::value j_arr = json5pp::array({});
    double sumWeights = 0;
    for (auto& oracle : oracles) {
        if (oracle->classifierWeight <= 0)
            continue;
        sumWeights += oracle->classifierWeight;
    }
    double sum = 0;
    for (auto& oracle : oracles) {
        if (oracle->classifierWeight <= 0)
            continue;
        double value = oracle->classify(env);
        j_arr.as_array().emplace_back(value);
        double weight = oracle->classifierWeight / sumWeights;
        sum += weight * (2*value - 1);
    }
    double result = (sum + 1) / 2;
    LOG(INFO) << "match result: " << result << " for " << j_arr;
    return result;
}


double BestOfTemplate::match(ClassifyEnv& env) {
    int bestIdx = -1;
    double bestVal = 0;
    for (int i=0; i < oracles.size(); i++) {
        double value = oracles[i]->classify(env);
        if (value > bestVal) {
            bestVal = value;
            bestIdx = i;
        }
    }
    return bestVal;
}

double BestOfTemplate::classify(ClassifyEnv& env) {
    const auto sz = env.classified.size();
    double result = match(env);
    if (result < 0.5) {
        while (sz < env.classified.size())
            env.classified.pop_back();
    }
    return result;
}

double BestOfTemplate::debugMatch(ClassifyEnv& env) {
    json5pp::value j_arr = json5pp::array({});
    int bestIdx = -1;
    double bestVal = 0;
    for (int i=0; i < oracles.size(); i++) {
        double value = oracles[i]->classify(env);
        j_arr.as_array().emplace_back(value);
        if (value > bestVal) {
            bestVal = value;
            bestIdx = i;
        }
    }
    LOG(INFO) << "match result: " << bestVal << " index " << bestIdx << " between " << j_arr;
    return bestVal;
}


HistogramTemplate::HistogramTemplate(CompareMode mode, const cv::Rect& rect, const std::array<cv::Vec3b,4>& colors)
    : mMode(mode)
    , mRect(rect)
    , mColors(colors)
{
}

double gaussian(double x) {
    return exp(-x*x / 2) / (sqrt(2 * M_PI));
}
double xxx(double x, double downscale) {
    return gaussian(x/downscale) / gaussian(0);
}
double HistogramTemplate::match(ClassifyEnv& env) {
    cv::Rect rect = mRect;
    rect = env.cvtReferenceToCaptured(rect);
    env.cropToCapture(rect);
    if (rect.empty())
        return 0;
    int colorPlanes;
    std::vector<cv::Mat> imagePlanes;
    if (mMode == CompareMode::Gray) {
        colorPlanes = 1;
        imagePlanes.push_back(env.getGrayImage());
    } else {
        colorPlanes = 3;
        cv::split(env.getColorImage(), imagePlanes);
    }
    unsigned resultColor = 0;
    for (auto i=0; i < colorPlanes; i++) {
        int histSize = 256;
        float range[]{0, 256}; //the upper boundary is exclusive
        const float* histRange[]{range};
        cv::Mat subImage(imagePlanes[i], rect);
        cv::Mat hist;
        cv::calcHist(&subImage, 1, nullptr, cv::Mat(), hist, 1, &histSize, histRange);
        int maxLoc[4]{};
        cv::minMaxIdx(hist, nullptr, nullptr, nullptr, maxLoc);
        resultColor |= maxLoc[0] << (i*8);
    }
    mLastColorBGR = encodeBGR(resultColor);
    cv::Vec3b cmpColor;
    switch (mMode) {
    case CompareMode::Gray:
        mLastColorBGR = sGray2sBgr(resultColor);
        for (size_t i=0; i < mColors.size(); i++) {
            mLastDistance[i] = std::abs(int(resultColor) - int(mColors[i][0]));
            mLastValues[i] = xxx(mLastDistance[i], 15);
        }
        LOG(DEBUG) << "Colors result: " << std::fixed << std::setprecision(3) << mLastValues << " for gray level " <<resultColor << " and colors " << mColors << " with distance " << mLastDistance;
        break;
    case CompareMode::Hsv:
        cmpColor = sBgr2Hsv(mLastColorBGR);
        for (size_t i=0; i < mColors.size(); i++) {
            mLastDistance[i] = distanceHsv(cmpColor, mColors[i]);
            mLastValues[i] = xxx(mLastDistance[i], 50);
        }
        LOG(DEBUG) << "Colors result: " << std::fixed << std::setprecision(3) << mLastValues << " for hsv color " << cmpColor << " and colors " << mColors << " with distance " << mLastDistance;
        break;
    case CompareMode::Luv:
        cmpColor = sBgr2Luv(mLastColorBGR);
        for (size_t i=0; i < mColors.size(); i++) {
            mLastDistance[i] = distanceLuv(cmpColor, mColors[i]);
            mLastValues[i] = xxx(mLastDistance[i], 40);
        }
        LOG(DEBUG) << "Colors result: " << std::fixed << std::setprecision(3) << mLastValues << " for luv color " << cmpColor << " and colors " << mColors << " with distance " << mLastDistance;
        break;
    case CompareMode::BGR:
        for (size_t i=0; i < mColors.size(); i++) {
            mLastDistance[i] = distanceBGR(mLastColorBGR, mColors[i]);
            mLastValues[i] = xxx(mLastDistance[i], 50);
        }
        LOG(DEBUG) << "Colors result: " << std::fixed << std::setprecision(3) << mLastValues << " for bgr color " << mLastColorBGR << " and colors " << mColors << " with distance " << mLastDistance;
        break;
    }
    imagePlanes.clear();
    return *std::max_element(mLastValues.begin(), mLastValues.end());
}
double HistogramTemplate::classify(ClassifyEnv& env) {
    return match(env) >= 0.8;
}
double HistogramTemplate::debugMatch(ClassifyEnv& env) {
    return match(env);
}


BaseImageTemplate::BaseImageTemplate(
        const std::string& filename, cv::Mat image, spEvalRect refRect)
        : filename(filename)
        , referenceRect(std::move(refRect))
        , threshold_min(0.8)
        , threshold_max(0.8)
{
    if (image.empty()) {
        if (!filename.empty())
            loadImageAndMask(filename, templImage, templMask);
    }
    else {
        templImage = image;
        extractImageMask(image, templMask);
    }
}

bool BaseImageTemplate::loadImageAndMask(const std::string& filename, cv::Mat& image, cv::Mat& mask) {
    image = cv::imread(filename, cv::IMREAD_UNCHANGED); // assume BGR/BGRA
    if (image.empty()) {
        LOG(ERROR) << "Template image " << filename << " not found";
        throw std::runtime_error(std::format("Cannot read %s", filename));
    }
    extractImageMask(image, mask);
    return true;
}

bool BaseImageTemplate::extractImageMask(cv::Mat& image, cv::Mat& mask) {
    if (image.channels() == 4) {
        // extract mask
        std::vector<cv::Mat> channels;
        cv::split(image, channels);
        cv::Mat alphaMask = channels[3];
        double mean = cv::mean(alphaMask)[0];
        if (mean > 254) {
            mask.release();
        } else {
            alphaMask.convertTo(mask, CV_32F);
        }
        struct ClearAlpha {
            void operator()(cv::Vec4b &pixel, const int *position) const {
                pixel[3] = 255;
            }
        } Functor;
        image.forEach<cv::Vec4b>(Functor);
    }
    else if (image.channels() == 3) {
        cv::Mat bgra;
        cv::cvtColor(image, bgra, cv::COLOR_BGR2BGRA);
        image = bgra;
        mask.release();
    }
    return true;
}

double BaseImageTemplate::classify(ClassifyEnv& env) {
    double value = match(env);
    return toResult(value);
}


double BaseImageTemplate::debugMatch(ClassifyEnv &env) {
    double value = match(env);
    LOG(INFO) << "match result: " << std::setprecision(3) << value <<
              "[" << threshold_min << ":" << threshold_max << "] >> " << toResult(value) << " for " << filename <<
              "; offset: " << env.scaleToReference(matchedCaptureOffset);
    if (value >= threshold_max) {
        cv::Scalar color(96, 255, 96);
        cv::rectangle(env.getDebugImage(), captureRect.tl(), captureRect.br(), color, 1);
        cv::rectangle(env.getDebugImage(), matchRect.tl(), matchRect.br(), color, 1);
        return 1;
    }
    if (value < threshold_min) {
        cv::Scalar color(96, 96, 255);
        cv::rectangle(env.getDebugImage(), matchRect.tl(), matchRect.br(), color, 1);
        cv::Point lt = matchRect.tl();
        cv::Point rb = matchRect.br();
        cv::line(env.getDebugImage(), lt, rb, color, 1);
        cv::Point lb = cv::Point(matchRect.tl().x, matchRect.br().y);
        cv::Point rt = cv::Point(matchRect.br().x, matchRect.tl().y);
        cv::line(env.getDebugImage(), lb, rt, color, 1);
        return 0;
    }
    double result = toResult(value);
    cv::Scalar color(96, 210, 210);
    cv::rectangle(env.getDebugImage(), captureRect.tl(), captureRect.br(), color, 1);
    cv::rectangle(env.getDebugImage(), matchRect.tl(), matchRect.br(), color, 1);
    cv::Point lt = matchRect.tl();
    cv::Point rb = matchRect.br();
    cv::line(env.getDebugImage(), lt, rb, color, 1);
    return result;
}

double BaseImageTemplate::toResult(double matchValue) {
    if (matchValue >= threshold_max)
        return 1;
    if (matchValue < threshold_min)
        return 0;
    double x = (matchValue - threshold_min) / (threshold_max - threshold_min);
    x = (x - 0.5) * 8;
    return 1 / (1 + std::exp(-x));
}

cv::Mat BaseImageTemplate::applyFilters(cv::Mat image) {
    if (image.empty())
        return image;
    cv::Mat out = image;
    for (auto& filter : filters) {
        out = filter->apply(out);
    }
    return out;
}

cv::Mat BaseImageTemplate::scaleImage(cv::Mat image, double scaleX, double scaleY) {
    if (scaleX == 1 && scaleY == 1)
        return image;
    cv::Mat out;
    cv::resize(image, out, {}, scaleX, scaleY);
    return out;
}


cv::Mat GaussFilter::apply(cv::Mat image) {
    if (disabled)
        return image;
    cv::Mat out;
    cv::GaussianBlur(image, out, cv::Size(kernX,kernY), 0, 0);
    return out;
}
cv::Mat LaplacianFilter::apply(cv::Mat image) {
    cv::Mat lapl16S;
    cv::Mat lapl8U;
    cv::Laplacian(image, lapl16S, CV_16S, kern, scale);
    cv::convertScaleAbs(lapl16S, lapl8U);
    return lapl8U;
}

cv::Mat HsvColorCropFilter::apply(cv::Mat image){
    if (ranges.empty())
        return image;
    cv::Mat hsv;
    cv::cvtColor(image, hsv, cv::COLOR_BGR2HSV);
    cv::Mat mask(image.cols, image.rows, CV_8UC1);
    if (ranges.size() == 1) {
        cv::inRange(hsv, ranges.front().first, ranges.front().second, mask);
        cv::Mat masked, gray;
    } else {
        for (auto &r: ranges) {
            cv::Mat m;
            cv::inRange(hsv, r.first, r.second, m);
            cv::bitwise_or(mask, m, mask);
        }
    }
    cv::Mat masked, gray;
    image.copyTo(masked, mask);
    cv::cvtColor(masked, gray, cv::COLOR_BGR2GRAY);
    return gray;
}

void BaseImageTemplate::fixNaNinResult(cv::Mat& result) {
    // bypass error in cv::matchTemplate that sometimes return NaN/Inf, instead of [0..1] valies
    auto* ptr = result.ptr<float>(0);
    auto* pend = ptr + result.rows * result.cols;
#ifndef NDEBUG
    bool bad_image = false;
#endif
    for (; ptr < pend; ++ptr) {
        if (std::isnan(*ptr) || std::isinf(*ptr)) {
#ifndef NDEBUG
            bad_image = true;
#endif
            *ptr = 0;
        }
    }
#ifndef NDEBUG
    LOG_IF(bad_image,ERROR) << "Bad image for TM_CCORR_NORMED: " << filename;
#endif
}

ImageTemplate::ImageTemplate(const std::string& filename, cv::Mat image, spEvalRect refRect)
    : BaseImageTemplate(filename, std::move(image), std::move(refRect))
{
}

double ImageTemplate::match(ClassifyEnv& env) {
    if (!this->referenceRect || templImage.empty())
        return 0;
    cv::Rect referenceRect = env.calcReferenceRect(this->referenceRect);
    if (referenceRect.empty())
        return 0;
    cv::Mat image = templImage.channels()==1 ? env.getGrayImage() : env.getColorImage();
    if (image.empty())
        return 0;
    if (env.getScale() != preprocessedTemplateScale) {
        preprocessedTemplateScale = env.getScale();
        cv::Mat templImageFiltered = applyFilters(templImage);
        cv::resize(templImageFiltered, templImageScaled, cv::Size(), env.getScale(), env.getScale());
        if (!templMask.empty())
            cv::resize(templMask, templMaskScaled, templImageScaled.size(), env.getScale(), env.getScale());
    }
    int ext = Master::getInstance().getSearchRegionExtent();
    captureRect = env.cvtReferenceToCaptured(referenceRect);
    matchRect = cv::Rect(captureRect.tl()-env.scaleToCaptured(extendLT+cv::Point(ext,ext)),
                         captureRect.br()+env.scaleToCaptured(extendRB+cv::Point(ext,ext)));
    env.cropToCapture(matchRect);
    int result_cols = matchRect.width - templImageScaled.cols + 1;
    int result_rows = matchRect.height - templImageScaled.rows + 1;
    cv::Mat result(result_rows, result_cols, CV_32FC1);
    cv::Mat imagePrepared = cv::Mat(image, matchRect);
    imagePrepared = applyFilters(imagePrepared);
    //if (templMaskScaled.empty())
        cv::matchTemplate(imagePrepared, templImageScaled, result, cv::TM_CCOEFF_NORMED);
    //else
    //    cv::matchTemplate(imagePrepared, templImageScaled, result, cv::TM_CCORR_NORMED, templMaskScaled);
    fixNaNinResult(result);
    //LOG(ERROR) << "match result: " << result;
    double maxVal;
    cv::Point maxLoc;
    cv::minMaxLoc(result, nullptr, &maxVal, nullptr, &maxLoc);
    LOG(DEBUG) << "match result: " << std::setprecision(3) << maxVal << " for " << filename;
    if (!name.empty() && maxVal >= threshold_min) {
        matchedCaptureOffset = maxLoc - (captureRect.tl() - matchRect.tl());
        captureRect = {captureRect.tl()+matchedCaptureOffset, captureRect.br()+matchedCaptureOffset};
        env.classified.emplace_back(ClsDetType::Detected, env.isWarpMode(), name, referenceRect + env.scaleToReference(matchedCaptureOffset));
        env.classified.back().u.tdet.referenceRect = referenceRect;
        env.classified.back().u.tdet.scale = 1;
    }
    return maxVal;
}

double ImageTemplate::debugMatch(ClassifyEnv &env) {
    double result = BaseImageTemplate::debugMatch(env);
    //CompassDetector::tryLowerUpperBoundsGUI(env, matchRect);
    return result;
}


ImageMultiScaleTemplate::ImageMultiScaleTemplate(
        const string &filename, cv::Mat image, spEvalRect refRect, std::vector<double> scales)
    : BaseImageTemplate(filename, std::move(image), std::move(refRect))
    , scales(std::move(scales))
    , lastScaleIdx(-1)
    , lastScale(1)
{
}

double ImageMultiScaleTemplate::match(ClassifyEnv &env) {
    if (!this->referenceRect || templImage.empty())
        return 0;
    cv::Rect referenceRect = env.calcReferenceRect(this->referenceRect);
    if (referenceRect.empty())
        return 0;
    cv::Mat image = templImage.channels() == 1 ? env.getGrayImage() : env.getColorImage();
    if (image.empty())
        return 0;
    if (env.getScale() != preprocessedTemplateScale) {
        preprocessedTemplateScale = env.getScale();
        scaledImages.clear();
        cv::Mat tmpImageFiltered = applyFilters(templImage);
        for (double scale : scales) {
            cv::Mat tmpImage;
            cv::Mat tmpMask;
            cv::resize(tmpImageFiltered, tmpImage, cv::Size(), scale*env.getScale(), scale*env.getScale());
            if (!templMask.empty())
                cv::resize(templMask, tmpMask, tmpImage.size(), scale*env.getScale(), scale*env.getScale());
            scaledImages.emplace_back(scale, tmpImage, tmpMask);
        }
    }
    int ext = Master::getInstance().getSearchRegionExtent();
    captureRect = env.cvtReferenceToCaptured(referenceRect);
    matchRect = cv::Rect(captureRect.tl() - env.scaleToCaptured(extendLT + cv::Point(ext, ext)),
                         captureRect.br() + env.scaleToCaptured(extendRB + cv::Point(ext, ext)));
    env.cropToCapture(matchRect);

    lastScaleIdx = -1;
    lastScale = std::numeric_limits<double>::quiet_NaN();
    int bestScaleIdx = -1;
    double bestScaleVal = 0;
    cv::Point bestScaleLoc;
    cv::Mat imagePrepared = cv::Mat(image, matchRect);
    for (int scaleIdx=0; scaleIdx < scales.size(); scaleIdx++) {
        auto& sm = scaledImages[scaleIdx];
        int result_cols = matchRect.width - sm.templImage.cols + 1;
        int result_rows = matchRect.height - sm.templImage.rows + 1;
        cv::Mat result(result_rows, result_cols, CV_32FC1);
        cv::matchTemplate(imagePrepared, sm.templImage, result, cv::TM_CCOEFF_NORMED, sm.templMask);
        fixNaNinResult(result);
        //LOG(ERROR) << "match result: " << result;
        double maxVal;
        cv::Point maxLoc;
        cv::minMaxLoc(result, nullptr, &maxVal, nullptr, &maxLoc);
        LOG(DEBUG) << "match result: " << std::setprecision(3) << maxVal << " for scale " << sm.scale << " file " << filename;
        if (maxVal > bestScaleVal) {
            bestScaleVal = maxVal;
            bestScaleIdx = scaleIdx;
            bestScaleLoc = maxLoc;
        }
    }
    if (bestScaleVal >= threshold_min) {
        lastScaleIdx = bestScaleIdx;
        lastScale = scaledImages[bestScaleIdx].scale;
        matchedCaptureOffset = bestScaleLoc - (captureRect.tl() - matchRect.tl());
        captureRect += matchedCaptureOffset;
        captureRect.width *= lastScale;
        captureRect.height *= lastScale;
    }
    if (!name.empty() && bestScaleVal >= threshold_min) {
        env.classified.emplace_back(ClsDetType::Detected, env.isWarpMode(), name,
                                    referenceRect + env.scaleToReference(matchedCaptureOffset));
        env.classified.back().u.tdet.referenceRect = referenceRect;
        env.classified.back().u.tdet.scale = lastScale;
    }
    return bestScaleVal;
}

double ImageMultiScaleTemplate::debugMatch(ClassifyEnv& env) {
    double value = BaseImageTemplate::debugMatch(env);
    LOG(INFO) << "best match was at scale index " << lastScaleIdx << " scale " << std::format("{:.7f}",lastScale);
    return value;
}

CompassDetector::CompassDetector()
        : ImageMultiScaleTemplate("templates/space_compass.png", cv::Mat(),
                                  spEvalRect(new ConstRect(cv::Rect(679,803,71,71))),
                                  {1, 1.025, 0.975, 1.05, 0.95, 1.075, 0.925, 1.1, 0.9, 1.125, 0.875})
        , threshold_dot {0.7}
{
    extendLT = {40,80};
    extendRB = {50,140};
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
    cv::Point extendLT(50,150);
    cv::Point extendRB(50,50);

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
    cv::createTrackbar("HMin",windowName,&hMin,179);
    cv::createTrackbar("SMin",windowName,&sMin,255);
    cv::createTrackbar("VMin",windowName,&vMin,255);
    cv::createTrackbar("HMax",windowName,&hMax,179);
    cv::createTrackbar("SMax",windowName,&sMax,255);
    cv::createTrackbar("VMax",windowName,&vMax,255);

    cv::Mat img = cv::Mat(env.getColorImage(), matchRect);
    cv::Mat output = img;

    while(1) {

        // Set minimum and max HSV values to display
        cv::Vec3b lower = {(uchar)hMin, (uchar)sMin, (uchar)vMin};
        cv::Vec3b upper = {(uchar)hMax, (uchar)sMax, (uchar)vMax};

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

double CompassDetector::debugMatch(ClassifyEnv& env) {
    double value = BaseImageTemplate::debugMatch(env);
    if (lastDotValue >= threshold_min && lastDotIdx >= 0) {
        cv::Scalar color;
        if ((lastDotIdx & 1) == 0)
            color = {255, 96, 96};
        else
            color = {96, 96, 255};
        cv::rectangle(env.getDebugImage(), dotCaptureRect.tl(), dotCaptureRect.br(), color, 1);
        std::string text = std::format("{}/{}/{}", int(lastTgtPitch), int(lastTgtYaw), int(lastTgtRoll));
        cv::Point orig = captureRect.tl() + cv::Point(0,-10);
        color = {254,254,254};
        cv::putText(env.getDebugImage(), text, orig, cv::FONT_HERSHEY_PLAIN, 1, color);
    }
    return value;
}

TilesDetector::TilesDetector(const std::string& name, spEvalRect& rect, int rows, int cols, int gap, double tmin, double tmax, std::vector<std::string> icon_files)
    : name(name)
    , mRect(rect)
    , mMaxRows(rows)
    , mMaxCols(cols)
    , mGap(gap)
    , threshold_min(tmin)
    , threshold_max(tmax)
    , mIconFiles(icon_files)
{
    for (auto& icf : icon_files) {
        cv::Mat image, mask;
        std::string filename = "templates/" + icf;
        if (BaseImageTemplate::loadImageAndMask(filename, image, mask)) {
            std::string name = icf.substr(0, icf.size()-4);
            iconsSource.emplace_back(1.0, name, image);
            iconsScaled.emplace_back(1.0, name, image);
        }
    }
}

bool TilesDetector::getColSpan(int& col, int& span, cv::Rect& bbox, cv::Rect& captureRect, int gap) const {
    col = -1;
    span = -1;
    for (int i=0; i <= mMaxCols; i++) {
        int x_col = i * captureRect.width / mMaxCols;
        if (bbox.x >= x_col-gap && bbox.x <= x_col+gap) {
            col = i;
        }
        if ((bbox.x+bbox.width) >= x_col-gap && (bbox.x+bbox.width) <= x_col+gap) {
            span = i - col;
        }
    }
    return col >= 0 && span > 0;
}

double TilesDetector::match(ClassifyEnv &env) {
    cv::Rect captureRect = env.cvtReferenceToCaptured(mRect->calcReferenceRect(env));
    cv::Mat roiImage(env.getGrayImage(), captureRect);
    if (roiImage.empty())
        return 0;

    unsigned buttonGrayColor = Master::getInstance().getConfiguration()->getButtonGrayColor(WState::Normal);
    cv::Mat thrImage;
    cv::threshold(roiImage, thrImage, buttonGrayColor - 2, 255, cv::THRESH_BINARY);

    if (mPreprocessedTemplateScale != env.getScale()) {
        mPreprocessedTemplateScale = env.getScale();
        iconsScaled.clear();
        for (size_t i=0; i < iconsSource.size(); i++) {
            IconMatrix& src = iconsSource[i];
            cv::Mat templImageScaled;
            cv::resize(src.templImage, templImageScaled, cv::Size(), env.getScale(), env.getScale());
            iconsScaled.emplace_back(env.getScale(), src.name, templImageScaled);
        }
    }

    int hGaps = (mMaxCols - 1) * mGap * env.getScale();
    int minTileWidth = (captureRect.width - hGaps) / mMaxCols - 8;
    int vGaps = (mMaxRows - 1) * mGap * env.getScale();
    int minTileHeight = (captureRect.height - vGaps) / mMaxRows - 6;
    int minTileArea = minTileWidth * minTileHeight;
    int maxTileArea = captureRect.area() - minTileArea;

    mDetectedTiles.clear();
    std::vector<std::vector<cv::Point>> contours;
    cv::findContoursLinkRuns(thrImage, contours);
    for (const auto &contour: contours) {
        std::vector<cv::Point> convex;
        cv::convexHull(contour, convex);
        if (convex.size() >= 4) {
            std::vector<cv::Point> approx;
            cv::approxPolyN(convex, approx, 4, 5, true);
            cv::Rect bbox = cv::boundingRect(approx);
            if (bbox.width >= minTileWidth && bbox.height >= minTileHeight &&
                bbox.area() >= minTileArea && bbox.area() <= maxTileArea)
            {
                int col, span;
                if (!getColSpan(col, span, bbox, captureRect, int(mGap * env.getScale())))
                    continue;
                bbox += captureRect.tl();
                bbox &= captureRect;
                cv::Rect refRect = env.cvtCapturedToReference(bbox);
                mDetectedTiles.emplace_back(ClsDetType::Tile, env.isWarpMode(), name+":", refRect);
                mDetectedTiles.back().u.tile.row = -1;
                mDetectedTiles.back().u.tile.col = col;
                mDetectedTiles.back().u.tile.span = span;
            }
        }
    }

    int area = 0;
    for (auto& cr : mDetectedTiles)
        area += env.scaleToCaptured(cr.detectedRect.size()).area();
    if (area < captureRect.area() * 0.8)
        return 0;

    for (int c=0; c < mMaxCols; c++) {
        std::vector<ClassifiedRect*> colSet;
        for (auto& cr : mDetectedTiles) {
            if (cr.u.tile.col <= c && cr.u.tile.col + cr.u.tile.span > c)
                colSet.push_back(&cr);
        }
        std::sort(colSet.begin(), colSet.end(), [](ClassifiedRect* c1, ClassifiedRect* c2){
            return c1->detectedRect.y < c2->detectedRect.y;
        });
        int row = 0;
        for (ClassifiedRect* cr : colSet) {
            if (cr->u.tile.row > row)
                row = cr->u.tile.row + 1;
            else
                cr->u.tile.row = row++;
        }
    }

    for (auto& cr : mDetectedTiles) {
        cv::Rect tileRect = env.cvtReferenceToCaptured(cr.detectedRect);
        tileRect &= captureRect;
        tileRect -= captureRect.tl();
        IconMatrix* bestIcon = nullptr;
        double bestIconVal = 0;
        for (auto& ic : iconsScaled) {
            int result_cols = tileRect.width - ic.templImage.cols + 1;
            int result_rows = tileRect.height - ic.templImage.rows + 1;
            cv::Mat result(result_rows, result_cols, CV_32FC1);
            cv::Mat tileImage = cv::Mat(roiImage, tileRect);
            cv::matchTemplate(tileImage, ic.templImage, result, cv::TM_CCOEFF_NORMED);
            //LOG(ERROR) << "match result: " << result;
            double maxVal;
            cv::Point maxLoc;
            cv::minMaxLoc(result, nullptr, &maxVal, nullptr, &maxLoc);
            //LOG(DEBUG) << "match result: " << std::setprecision(3) << maxVal << " for " << ic.name;
            if (maxVal >= threshold_min && maxVal > bestIconVal) {
                bestIcon = &ic;
                bestIconVal = maxVal;
            }
        }
        if (!name.empty() && bestIcon && bestIconVal >= threshold_min) {
            cr.text = name + ":" + bestIcon->name;
            LOG(DEBUG) << "TilesDetector matched result: " << std::setprecision(3) << bestIconVal
                       << " for " << cr.text
                       << " row:" << cr.u.tile.row << " col:" << cr.u.tile.col << " span:" << cr.u.tile.span;
            env.classified.push_back(cr);
        } else {
            LOG(DEBUG) << "TilesDetector matched failed: " << std::setprecision(3) << bestIconVal
                       << " for " << (bestIcon ? bestIcon->name : "all")
                       << " row:" << cr.u.tile.row << " col:" << cr.u.tile.col << " span:" << cr.u.tile.span
                       << " rect " << cr.detectedRect;
            env.classified.push_back(cr);
        }
    }
    return 1;
}

double TilesDetector::classify(ClassifyEnv &env) {
    return match(env);
}

double TilesDetector::debugMatch(ClassifyEnv &env) {
    //CompassDetector::tryLowerUpperBoundsGUI(env, mRect->calcReferenceRect(env));
    double value = match(env);
    LOG(INFO) << " detected " << mDetectedTiles.size() << " tiles:";
    for (auto& cr : env.classified) {
        if (cr.cdt == ClsDetType::Tile && cr.text.starts_with(name+":")) {
            LOG(INFO) << "   tile: '" << cr.text << "' rect: " << cr.detectedRect
                      << " col: " << cr.u.tile.col << " row: " << cr.u.tile.row;
        }
    }
    cv::Scalar colorOk(96, 255, 255);
    cv::Scalar colorNo(96, 96, 255);
    cv::Rect captureRect = env.cvtReferenceToCaptured(mRect->calcReferenceRect(env));
    cv::rectangle(env.getDebugImage(), captureRect.tl(), captureRect.br(), (value<0.5?colorNo:colorOk), 1);
    for (auto& cr : mDetectedTiles) {
        cv::Rect r = env.cvtReferenceToCaptured(cr.detectedRect);
        cv::Scalar color = cr.text.size() > name.size()+1 ? colorOk : colorNo;
        cv::rectangle(env.getDebugImage(), r.tl(), r.br(), color, 1);
    }
    return value;
}

LineDetector::LineDetector(std::vector<std::string> anchors, spEvalRect anchorRect, cv::Point p0, cv::Point p1)
    : BaseImageTemplate("", cv::Mat(), anchorRect)
    , anchorFiles(std::move(anchors))
    , referenceP0(p0)
    , referenceP1(p1)
{
    for (auto& fname : anchorFiles) {
        cv::Mat anchorImage;
        cv::Mat anchorMask;
        loadImageAndMask(fname, anchorImage, anchorMask);
        anchorSource.emplace_back(fname, anchorImage);
    }
}

double LineDetector::match(ClassifyEnv& env) {
    if (!this->referenceRect || anchorSource.empty())
        return 0;
    cv::Rect referenceRect = env.calcReferenceRect(this->referenceRect);
    if (referenceRect.empty())
        return 0;
    if (env.getScale() != preprocessedTemplateScale) {
        preprocessedTemplateScale = env.getScale();
        anchorScaled.clear();
        cv::Mat tmpImageFiltered = applyFilters(templImage);
        for (auto& as : anchorSource) {
            cv::Mat tmpImage;
            cv::resize(applyFilters(as.templImage), tmpImage, cv::Size(), env.getScale(), env.getScale());
            anchorScaled.emplace_back(as.name, tmpImage);
        }
    }
    int ext = Master::getInstance().getSearchRegionExtent();
    captureRect = env.cvtReferenceToCaptured(referenceRect);
    matchRect = cv::Rect(captureRect.tl()-env.scaleToCaptured(extendLT+cv::Point(ext,ext)),
                         captureRect.br()+env.scaleToCaptured(extendRB+cv::Point(ext,ext)));
    env.cropToCapture(matchRect);
    AnchorMatrix* bestAnchor = nullptr;
    double bestAnchorVal = 0;
    cv::Point bestAnchorLoc;
    cv::Mat imagePrepared = cv::Mat(env.getColorImage(), matchRect);
    imagePrepared = applyFilters(imagePrepared);
    for (auto& sm : anchorScaled) {
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
        LOG(INFO) << "LineDetector: anchor detect rate: " << bestAnchorVal << " less then minimal threshold " << threshold_min;
        return bestAnchorVal;
    }
    matchedCaptureOffset = bestAnchorLoc - (captureRect.tl() - matchRect.tl());
    captureRect += matchedCaptureOffset;
    LOG(DEBUG) << "LineDetector: anchor found, offset: " << matchedCaptureOffset;

    captureP0 = env.cvtReferenceToCaptured(referenceP0) + matchedCaptureOffset;
    captureP1 = captureP0 + env.scaleToCaptured(referenceP1-referenceP0);
    int captureWidth = cv::norm(captureP1 - captureP0);
    cv::Rect r0 = cv::Rect(captureP0 - env.scaleToCaptured(extendLT+cv::Point(ext,ext)),
                           captureP0 + env.scaleToCaptured(extendRB+cv::Point(ext,ext)));
    cv::Rect r1 = cv::Rect(captureP1 - env.scaleToCaptured(extendLT+cv::Point(ext,ext)),
                           captureP1 + env.scaleToCaptured(extendRB+cv::Point(ext,ext)));
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
    for (auto& cont : contours) {
        auto rotRect = cv::minAreaRect(cont);
        normalizeRotatedRect(rotRect);
        if (rotRect.size.width < 0.8*captureWidth || rotRect.size.height > 20)
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
            LOG(DEBUG) << "LineDetector: line found, offset: " << offset << " angle delta: " << (detectedAngle - referenceAngle);
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
    captureP1.x = captureP0.x + captureWidth * std::cos(lastLineAngle*M_PI/180);
    captureP1.y = captureP0.y + captureWidth * std::sin(lastLineAngle*M_PI/180);

    env.classified.emplace_back(ClsDetType::LineDetected, env.isWarpMode(), name,
                                referenceRect + env.scaleToReference(matchedCaptureOffset));
    env.classified.back().u.ldet.offset = env.scaleToReference(matchedCaptureOffset);
    env.classified.back().u.ldet.angle = lastDeltaAngle;
    env.classified.back().u.ldet.scale = 1;
    env.classified.back().u.ldet.referenceP0 = env.cvtCapturedToReference(captureP0);
    env.classified.back().u.ldet.referenceP1 = env.cvtCapturedToReference(captureP1);
    return 1;
}

double LineDetector::debugMatch(ClassifyEnv& env) {
    double value = match(env);
    if (value >= threshold_max) {
        cv::Scalar color(255, 255, 96);
        cv::line(env.getDebugImage(), captureP0, captureP1, color, 2);
    }
    //tryCannyParamsGUI(env);
    return value;
}

void LineDetector::normalizeRotatedRect(cv::RotatedRect& rr) {
    if (rr.angle > +90) rr.angle -= 180;
    if (rr.angle < -90) rr.angle += 180;
    if (rr.angle > +45) { rr.angle -= 90; std::swap(rr.size.width, rr.size.height); }
    if (rr.angle < -45) { rr.angle += 90; std::swap(rr.size.width, rr.size.height); }
}

void LineDetector::tryCannyParamsGUI(ClassifyEnv &env) {
    int minWidth = cv::norm(captureP1 - captureP0) * 0.8;

    cv::Mat imagePrepared = cv::Mat(env.getColorImage(), lineMatchRect);
    imagePrepared = applyFilters(scaleImage(imagePrepared, imageScaleX, imageScaleY));

    GaussFilter* gaussFilter = nullptr;
    for (auto& filter : filters) {
        gaussFilter = dynamic_cast<GaussFilter*>(filter.get());
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
        cv::createTrackbar("gKrnX", gaussWindow, &gaussKernX, (gaussKernMax-1)/2);
        cv::setTrackbarMax("gKrnX", gaussWindow, gaussKernMax);
        cv::setTrackbarMin("gKrnX", gaussWindow, 1);
        cv::createTrackbar("gKrnY", gaussWindow, &gaussKernY, (gaussKernMax-1)/2);
        cv::setTrackbarMax("gKrnY", gaussWindow, gaussKernMax);
        cv::setTrackbarMin("gKrnY", gaussWindow, 1);
    }
    // create trackbars for image threshold
    cv::createTrackbar("Thr", edgesWindow, &thrMin, 255);

    cv::Mat edgeMat;
    cv::Mat linesMat;
    while(1) {
        if (gaussFilter) {
            gaussKernX = (gaussKernX & ~1) + 1;
            gaussKernY = (gaussKernY & ~1) + 1;
            const_cast<int&>(gaussFilter->kernX) = gaussKernX;
            const_cast<int&>(gaussFilter->kernY) = gaussKernY;
            const_cast<bool&>(gaussFilter->disabled) = gaussDisable > 0;
        }
        imagePrepared = cv::Mat(env.getColorImage(), lineMatchRect);
        imagePrepared = applyFilters(scaleImage(imagePrepared, imageScaleX, imageScaleY));
        linesMat = scaleImage(cv::Mat(env.getColorImage(), lineMatchRect), imageScaleX, imageScaleY).clone();

        cv::threshold(imagePrepared, edgeMat, thrMin, 255, cv::THRESH_BINARY);

        std::vector<std::vector<cv::Point>> contours;
        cv::findContours(edgeMat, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
        for (auto& cont : contours) {
            auto rotRect = cv::minAreaRect(cont);
            normalizeRotatedRect(rotRect);
            float w = rotRect.size.width;
            float h = rotRect.size.height;
            if (w < 60 || h > 20)
                continue;
            cv::Point2f rectPoints[4]; // bl, tl, tr, br
            rotRect.points(rectPoints);
            if (w < minWidth  || h > 20) {
                for (int j = 0; j < 4; j++)
                    cv::line(linesMat, rectPoints[j], rectPoints[(j+1) % 4], {0,0,255}, 1);
            } else {
                auto lp0 = (rectPoints[0] + rectPoints[1]) / 2;
                auto lp1 = (rectPoints[2] + rectPoints[3]) / 2;
                cv::line(linesMat, lp0, lp1, {255,255,255}, 2);
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

