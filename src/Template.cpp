//
// Created by mkizub on 23.05.2025.
//

#include "pch.h"

#include "Template.h"
#include <format>
#include <iomanip>

void ClassifyEnv::init(const cv::Rect& monRect, const cv::Rect& captRect, const cv::Mat& imgColor, const cv::Mat& imgGray) {
    ClassifyEnv* self = const_cast<ClassifyEnv*>(this);
    const_cast<cv::Rect&>(monitorRect) = monRect;
    const_cast<cv::Rect&>(captureRect) = captRect;
    const_cast<cv::Rect&>(captureCrop) = cv::Rect(cv::Point(), captureRect.size());
    const_cast<cv::Mat&>(imageColor) = imgColor;
    const_cast<cv::Mat&>(imageGray) = imgGray;
    if (captureRect.size() != ReferenceScreenSize) {
        double x_scale = double(captureRect.width) / ReferenceScreenSize.width;
        double y_scale = double(captureRect.height) / ReferenceScreenSize.height;
        scaleToCaptured_ = std::min(x_scale, y_scale);
        needScaling_ = true;
    } else {
        needScaling_ = false;
        scaleToCaptured_ = 1;
    }
    captureCenter = cv::Point(captureRect.size()) / 2;
}

void ClassifyEnv::clear() {
    const_cast<cv::Rect&>(monitorRect) = cv::Rect();
    const_cast<cv::Rect&>(captureRect) = cv::Rect();
    const_cast<cv::Rect&>(captureCrop) = cv::Rect();
    const_cast<cv::Mat&>(imageColor) = cv::Mat();
    const_cast<cv::Mat&>(imageGray) = cv::Mat();
    const_cast<cv::Mat&>(debugImage) = cv::Mat();
    needScaling_ = false;
    scaleToCaptured_ = 1;
    captureCenter = ReferenceScreenCenter;
    classified.clear();
}

cv::Point ClassifyEnv::cvtReferenceToDesktop(const cv::Point& point) const {
    return monitorRect.tl() + captureRect.tl() + cvtReferenceToCaptured(point);
}

cv::Point ClassifyEnv::cvtReferenceToCaptured(const cv::Point& point) const {
    cv::Point screenPoint(point);
    if (needScaling_) {
        cv::Point relative = screenPoint - ReferenceScreenCenter;
        relative *= scaleToCaptured_;
        screenPoint = relative + captureCenter;
    }
    return screenPoint;
}
cv::Rect  ClassifyEnv::cvtReferenceToCaptured(const cv::Rect& rect) const {
    cv::Rect screenRect(rect);
    if (needScaling_) {
        cv::Point lt_rel = screenRect.tl() - ReferenceScreenCenter;
        cv::Point rb_rel = screenRect.br() - ReferenceScreenCenter;
        lt_rel *= scaleToCaptured_;
        rb_rel *= scaleToCaptured_;
        cv::Point lt = lt_rel + captureCenter;
        cv::Point rb = rb_rel + captureCenter;
        screenRect = cv::Rect(lt, rb);
    }
    return screenRect;
}

cv::Point ClassifyEnv::cvtCapturedToReference(const cv::Point& point) const {
    cv::Point referencePoint(point);
    if (needScaling_) {
        cv::Point lt_rel = referencePoint - captureCenter;
        lt_rel /= scaleToCaptured_;
        cv::Point lt = lt_rel + ReferenceScreenCenter;
        referencePoint = lt;
    }
    return referencePoint;
}
cv::Rect  ClassifyEnv::cvtCapturedToReference(const cv::Rect& rect) const {
    cv::Rect referenceRect(rect);
    if (needScaling_) {
        cv::Point lt_rel = referenceRect.tl() - captureCenter;
        cv::Point rb_rel = referenceRect.br() - captureCenter;
        lt_rel /= scaleToCaptured_;
        rb_rel /= scaleToCaptured_;
        cv::Point lt = lt_rel + ReferenceScreenCenter;
        cv::Point rb = rb_rel + ReferenceScreenCenter;
        referenceRect = cv::Rect(lt,rb);
    }
    return referenceRect;
}

double SequenceTemplate::match(ClassifyEnv& env) {
    double sum = 0;
    for (auto& oracle : oracles) {
        double value = oracle->classify(env);
        if (value <= 0)
            return 0;
        if (sum < 1)
            sum += 2*(value-0.5);
    }
    return sum;
}

double SequenceTemplate::classify(ClassifyEnv& env) {
    const auto sz = env.classified.size();
    double result = match(env);
    if (result < 0.96) {
        while (sz < env.classified.size())
            env.classified.pop_back();
    }
    return result;
}

double SequenceTemplate::debugMatch(ClassifyEnv& env) {
    json5pp::value j_arr = json5pp::array({});
    for (auto& oracle : oracles) {
        double value = oracle->debugMatch(env);
        j_arr.as_array().emplace_back(value);
    }
    double sum = 0;
    for (auto& jv : j_arr.as_array()) {
        auto value = jv.as_number();
        if (value <= 0) { sum = 0; break; }
        if (sum < 1)
            sum += 2*(value-0.5);
        if (sum >= 1) { sum = 1; }
    }
    LOG(INFO) << "match result: " << sum << (sum >= 1 ? " (pass)" : " (fail)") << " for " << j_arr;
    return sum;
}

HistogramTemplate::HistogramTemplate(CompareMode mode, const cv::Rect& rect, const std::vector<cv::Vec3b>& colors)
    : mMode(mode)
    , mRect(rect)
    , mColors(colors)
{
    mLastDistance.resize(mColors.size());
    mLastValues.resize(mColors.size());
}

HistogramTemplate::HistogramTemplate(CompareMode mode, const cv::Rect& rect, const cv::Vec3b& colors)
        : mMode(mode)
        , mRect(rect)
        , mColors(1U, colors)
{
    mLastDistance.resize(mColors.size());
    mLastValues.resize(mColors.size());
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
    rect &= env.captureCrop;
    if (rect.empty())
        return 0;
    int colorPlanes;
    std::vector<cv::Mat> imagePlanes;
    if (mMode == CompareMode::Gray) {
        colorPlanes = 1;
        if (env.imageGray.empty())
            return 0;
        imagePlanes.push_back(env.imageGray);
    } else {
        colorPlanes = 3;
        if (env.imageColor.empty())
            return 0;
        cv::split(env.imageColor, imagePlanes);
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
    cv::Vec3d delta;
    switch (mMode) {
    case CompareMode::Gray:
        mLastColor = cv::Vec3b(resultColor, 0, 0);
        for (size_t i=0; i < mColors.size(); i++) {
            mLastDistance[i] = std::abs(int(mLastColor[0]) - int(mColors[i][0]));
            mLastValues[i] = xxx(mLastDistance[i], 7);
        }
        break;
    case CompareMode::Luv:
        mLastColor = rgb2luv(encodeRGB(resultColor));
        for (size_t i=0; i < mColors.size(); i++) {
            delta = cv::Vec3d(mLastColor) - cv::Vec3d(mColors[i]);
            mLastDistance[i] = cv::norm(delta);
            mLastValues[i] = xxx(mLastDistance[i], 20);
        }
        break;
    case CompareMode::RGB:
        mLastColor = encodeRGB(resultColor);
        for (size_t i=0; i < mColors.size(); i++) {
            delta = cv::Vec3d(mLastColor) - cv::Vec3d(mColors[i]);
            mLastDistance[i] = cv::norm(delta);
            mLastValues[i] = xxx(mLastDistance[i], 25);
        }
        break;
    }
    LOG(DEBUG) << "Colors result: " << mLastValues << " for color " << mLastColor << " and colors " << mColors << " with distance " << mLastDistance;
    imagePlanes.clear();
    return mLastValues[0];
}
double HistogramTemplate::classify(ClassifyEnv& env) {
    return match(env) >= 0.8;
}
double HistogramTemplate::debugMatch(ClassifyEnv& env) {
    return match(env);
}


BaseImageTemplate::BaseImageTemplate(
        const std::string& name, const std::string& filename, cv::Mat& image, bool edge,
        spEvalRect& refRect, cv::Point extLT, cv::Point extRB, double tmin, double tmax)
        : name(name)
        , filename(filename)
        , edgeLaplacian(edge)
        , referenceRect(refRect)
        , extendLT(extLT)
        , extendRB(extRB)
        , threshold_min(tmin)
        , threshold_max(tmax)
{
    loadImageAndMask(filename, templImage, templMask);
}

bool BaseImageTemplate::loadImageAndMask(const std::string& filename, cv::Mat& image, cv::Mat& mask) {
    image = cv::imread(filename, cv::IMREAD_UNCHANGED); // assume RGB/RGBA
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
        cv::Mat rgba;
        cv::cvtColor(image, rgba, cv::COLOR_RGB2RGBA);
        image = rgba;
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
        cv::rectangle(env.debugImage, captureRect.tl(), captureRect.br(), color, 1);
        cv::rectangle(env.debugImage, matchRect.tl(), matchRect.br(), color, 1);
        return 1;
    }
    if (value < threshold_min) {
        cv::Scalar color(96, 96, 255);
        cv::rectangle(env.debugImage, matchRect.tl(), matchRect.br(), color, 1);
        cv::Point lt = matchRect.tl();
        cv::Point rb = matchRect.br();
        cv::line(env.debugImage, lt, rb, color, 1);
        cv::Point lb = cv::Point(matchRect.tl().x, matchRect.br().y);
        cv::Point rt = cv::Point(matchRect.br().x, matchRect.tl().y);
        cv::line(env.debugImage, lb, rt, color, 1);
        return 0;
    }
    double result = toResult(value);
    cv::Scalar color(96, 210, 210);
    cv::rectangle(env.debugImage, captureRect.tl(), captureRect.br(), color, 1);
    cv::rectangle(env.debugImage, matchRect.tl(), matchRect.br(), color, 1);
    cv::Point lt = matchRect.tl();
    cv::Point rb = matchRect.br();
    cv::line(env.debugImage, lt, rb, color, 1);
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


cv::Mat BaseImageTemplate::makeLaplacian(cv::Mat m) {
    if (!edgeLaplacian)
        return m;
    cv::Mat smooth;
    cv::Mat lapl16S;
    cv::Mat lapl8U;
    cv::GaussianBlur(m, smooth, cv::Size(3,3), 0);
    cv::Laplacian(smooth, lapl16S, CV_16S, 3);
    cv::convertScaleAbs(lapl16S, lapl8U);
    return lapl8U;
}

cv::Mat BaseImageTemplate::makeGaussianBlur(cv::Mat m, int kernelSize, double sigma) {
    cv::Mat smooth;
    cv::GaussianBlur(m, smooth, cv::Size(kernelSize,kernelSize), sigma);
    return smooth;
}

void BaseImageTemplate::fixNaNinResult(cv::Mat& result) {
    // bypass error in cv::matchTemplate that sometimes return NaN/Inf, instead of [0..1] valies
    auto* ptr = result.ptr<float>(0);
    auto* pend = ptr + result.rows * result.cols;
    for (; ptr < pend; ++ptr) {
        if (std::isnan(*ptr) || std::isinf(*ptr))
            *ptr = 0;
    }
}

ImageTemplate::ImageTemplate(const std::string& name, const std::string& filename, cv::Mat& image, bool edge,
                             spEvalRect& refRect, cv::Point extLT, cv::Point extRB, double tmin, double tmax)
    : BaseImageTemplate(name, filename, image, edge, refRect, extLT, extRB, tmin, tmax)
{
    templImageScaled = makeLaplacian(templImage);
    templMaskScaled = templMask;
}

double ImageTemplate::match(ClassifyEnv& env) {
    if (!this->referenceRect || templImage.empty())
        return 0;
    cv::Rect referenceRect = env.calcReferenceRect(this->referenceRect);
    if (referenceRect.empty())
        return 0;
    cv::Mat image = templImage.channels()==1 ? env.imageGray : env.imageColor;
    if (image.empty())
        return 0;
    if (env.getScale() != preprocessedTemplateScale) {
        preprocessedTemplateScale = env.getScale();
        preprocessedLaplacian = false;
        cv::resize(templImage, templImageScaled, cv::Size(), env.getScale(), env.getScale());
        if (!templMask.empty()) {
            cv::resize(templMask, templMaskScaled, templImageScaled.size(), env.getScale(), env.getScale());
        }
    }
    if (preprocessedLaplacian != edgeLaplacian) {
        templImageScaled = makeLaplacian(templImageScaled);
        preprocessedLaplacian = edgeLaplacian;
    }
    int ext = Master::getInstance().getSearchRegionExtent();
    captureRect = env.cvtReferenceToCaptured(referenceRect);
    matchRect = cv::Rect(captureRect.tl()-env.scaleToCaptured(extendLT+cv::Point(ext,ext)),
                         captureRect.br()+env.scaleToCaptured(extendRB+cv::Point(ext,ext)));
    matchRect &= env.captureCrop;
    int result_cols = matchRect.width - templImageScaled.cols + 1;
    int result_rows = matchRect.height - templImageScaled.rows + 1;
    cv::Mat result(result_rows, result_cols, CV_32FC1);
    cv::Mat imagePrepared = cv::Mat(image, matchRect);
    if (edgeLaplacian)
        imagePrepared = makeLaplacian(imagePrepared);
    cv::matchTemplate(imagePrepared, templImageScaled, result, cv::TM_CCOEFF_NORMED, templMaskScaled);
    fixNaNinResult(result);
    //LOG(ERROR) << "match result: " << result;
    double maxVal;
    cv::Point maxLoc;
    cv::minMaxLoc(result, nullptr, &maxVal, nullptr, &maxLoc);
    LOG(DEBUG) << "match result: " << std::setprecision(3) << maxVal << " for " << filename;
    if (!name.empty() && maxVal >= threshold_min) {
        matchedCaptureOffset = maxLoc - (captureRect.tl() - matchRect.tl());
        captureRect = {captureRect.tl()+matchedCaptureOffset, captureRect.br()+matchedCaptureOffset};
        env.classified.emplace_back(ClsDetType::TemplateDetected, name, referenceRect + env.scaleToReference(matchedCaptureOffset));
        env.classified.back().u.templ.referenceRect = referenceRect;
        env.classified.back().u.templ.scale = 1;
    }
    return maxVal;
}

ImageMultiScaleTemplate::ImageMultiScaleTemplate(
        const string &name, const string &filename, cv::Mat& image,
        double scaleMin, double scaleMax, double scaleStep, bool edge,
        spEvalRect& refRect, cv::Point extLT, cv::Point extRB, double thrMin, double thrMax)
    : BaseImageTemplate(name, filename, image, edge, refRect, extLT, extRB, thrMin, thrMax)
    , generateScaleMin(scaleMin)
    , generateScaleMax(scaleMax)
    , generateScaleStep(scaleStep)
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
    cv::Mat image = templImage.channels() == 1 ? env.imageGray : env.imageColor;
    if (image.empty())
        return 0;
    if (scales.empty() || env.getScale() != preprocessedTemplateScale) {
        preprocessedTemplateScale = env.getScale();
        preprocessedLaplacian = false;
        scales.clear();
        cv::Mat tmpImage = templImage;
        cv::Mat tmpMask = templMask;
        if (env.needScaling()) {
            cv::resize(templImage, tmpImage, cv::Size(), env.getScale(), env.getScale());
            if (!templMask.empty())
                cv::resize(templMask, tmpMask, tmpImage.size(), env.getScale(), env.getScale());
        }
        scales.emplace_back(1.0, tmpImage, tmpMask);
        double upScale = 1 + generateScaleStep;
        double downScale = 1 - generateScaleStep;
        while (upScale < generateScaleMax || downScale > generateScaleMin) {
            if (upScale < generateScaleMax) {
                cv::resize(templImage, tmpImage, cv::Size(), upScale*env.getScale(), upScale*env.getScale());
                if (!templMask.empty())
                    cv::resize(templMask, tmpMask, tmpImage.size(), upScale*env.getScale(), upScale*env.getScale());
                scales.emplace_back(upScale, tmpImage, tmpMask);
            }
            if (downScale > generateScaleMin) {
                cv::resize(templImage, tmpImage, cv::Size(), downScale*env.getScale(), downScale*env.getScale());
                if (!templMask.empty())
                    cv::resize(templMask, tmpMask, tmpImage.size(), downScale*env.getScale(), downScale*env.getScale());
                scales.emplace_back(downScale, tmpImage, tmpMask);
            }
            upScale *= 1.0 + generateScaleStep;
            downScale *= 1.0 - generateScaleStep;
        }
    }
    if (preprocessedLaplacian != edgeLaplacian) {
        for (auto& sm : scales) {
            sm.templImage = makeLaplacian(sm.templImage);
        }
        preprocessedLaplacian = edgeLaplacian;
    }
    int ext = Master::getInstance().getSearchRegionExtent();
    captureRect = env.cvtReferenceToCaptured(referenceRect);
    matchRect = cv::Rect(captureRect.tl() - env.scaleToCaptured(extendLT + cv::Point(ext, ext)),
                         captureRect.br() + env.scaleToCaptured(extendRB + cv::Point(ext, ext)));
    matchRect &= env.captureCrop;

    lastScaleIdx = -1;
    lastScale = std::numeric_limits<double>::quiet_NaN();
    int bestScaleIdx = -1;
    double bestScaleVal = 0;
    cv::Point bestScaleLoc;
    cv::Mat imagePrepared = cv::Mat(image, matchRect);
    if (edgeLaplacian) {
        imagePrepared = makeLaplacian(imagePrepared);
        //cv::imshow("Prepared Laplacian screen region", imagePrepared);
        //cv::imshow("Prepared Laplacian template", scales[0].templImage);
        //cv::waitKey();
        //cv::destroyAllWindows();
    }
    for (int scaleIdx=0; scaleIdx < scales.size(); scaleIdx++) {
        auto& sm = scales[scaleIdx];
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
        lastScale = scales[bestScaleIdx].scale;
        matchedCaptureOffset = bestScaleLoc - (captureRect.tl() - matchRect.tl());
        captureRect += matchedCaptureOffset;
        captureRect.width *= lastScale;
        captureRect.height *= lastScale;
    }
    if (!name.empty() && bestScaleVal >= threshold_min) {
        env.classified.emplace_back(ClsDetType::TemplateDetected, name,
                                    referenceRect + env.scaleToReference(matchedCaptureOffset));
        env.classified.back().u.templ.referenceRect = referenceRect;
        env.classified.back().u.templ.scale = lastScale;
    }
    return bestScaleVal;
}

double ImageMultiScaleTemplate::debugMatch(ClassifyEnv& env) {
    double value = BaseImageTemplate::debugMatch(env);
    LOG(INFO) << "best match was at scale index " << lastScaleIdx << " scale " << std::format("{:.7f}",lastScale);
    return value;
}

CompassDetector::CompassDetector(cv::Mat& image, spEvalRect& refRect)
        : BaseImageTemplate("", "templates/space_compass.png", image, false, refRect,
                            cv::Point(40,80), cv::Point(50,140), 0.3, 0.8)
        , luvLower {90, 90, 90}
        , luvUpper {130, 255, 255}
        , threshold_dot {0.7}
{
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
    if (!this->referenceRect || templImage.empty())
        return 0;
    cv::Rect referenceRect = env.calcReferenceRect(this->referenceRect);
    if (referenceRect.empty())
        return 0;
    if (env.imageColor.empty())
        return 0;
    if (compassScales.empty() || env.getScale() != preprocessedTemplateScale) {
        preprocessedTemplateScale = env.getScale();
        preprocessedLaplacian = false;
        compassScales.clear();

        cv::Mat tmpCompassImage = templImage;
        cv::Mat tmpCompassMask = templMask;
        if (env.needScaling()) {
            cv::resize(templImage, tmpCompassImage, cv::Size(), env.getScale(), env.getScale());
            if (!templMask.empty())
                cv::resize(templMask, tmpCompassMask, tmpCompassImage.size(), env.getScale(), env.getScale());
        }
        compassScales.emplace_back(1.0, tmpCompassImage, cv::Mat());
        const double generateScaleStep = 0.025;
        double upScale = 1 + generateScaleStep;
        double downScale = 1 - generateScaleStep;
        for (int i=0; i < 5; i++) {
            {
                cv::resize(templImage, tmpCompassImage, cv::Size(), upScale*env.getScale(), upScale*env.getScale());
                if (!templMask.empty())
                    cv::resize(templMask, tmpCompassMask, tmpCompassImage.size(), upScale*env.getScale(), upScale*env.getScale());
                compassScales.emplace_back(upScale, tmpCompassImage, tmpCompassMask);
            }
            {
                cv::resize(templImage, tmpCompassImage, cv::Size(), downScale*env.getScale(), downScale*env.getScale());
                if (!templMask.empty())
                    cv::resize(templMask, tmpCompassMask, tmpCompassImage.size(), downScale*env.getScale(), downScale*env.getScale());
                compassScales.emplace_back(downScale, tmpCompassImage, tmpCompassMask);
            }
            upScale *= 1.0 + generateScaleStep;
            downScale *= 1.0 - generateScaleStep;
        }
    }

    //
    // Detect compass
    //
    captureRect = env.cvtReferenceToCaptured(referenceRect);
    matchRect = cv::Rect(captureRect.tl() - env.scaleToCaptured(extendLT),
                         captureRect.br() + env.scaleToCaptured(extendRB));
    matchRect &= env.captureCrop;

    cv::Mat imageFiltered = cv::Mat(env.imageColor, matchRect);

    int bestScaleIdx = -1;
    double bestScaleVal = 0;
    cv::Point bestScaleLoc;

    for (int scaleIdx=0; scaleIdx < compassScales.size(); scaleIdx++) {
        auto& sm = compassScales[scaleIdx];
        int result_cols = matchRect.width - sm.templImage.cols + 1;
        int result_rows = matchRect.height - sm.templImage.rows + 1;
        cv::Mat result(result_rows, result_cols, CV_32FC1);
        cv::matchTemplate(imageFiltered, sm.templImage, result, cv::TM_CCOEFF_NORMED, sm.templMask);
        fixNaNinResult(result);
        double maxVal;
        cv::Point maxLoc;
        cv::minMaxLoc(result, nullptr, &maxVal, nullptr, &maxLoc);
        LOG(DEBUG) << std::format("compass match result: {:.3f} for scale {:.6f} file ", maxVal, sm.scale,  filename);
        if (maxVal > bestScaleVal) {
            bestScaleVal = maxVal;
            bestScaleIdx = scaleIdx;
            bestScaleLoc = maxLoc;
        }
    }
    double compassValue = 0;
    if (bestScaleVal >= threshold_min) {
        compassValue = bestScaleVal;
        lastScaleIdx = bestScaleIdx;
        lastScale = compassScales[bestScaleIdx].scale;
        matchedCaptureOffset = bestScaleLoc - (captureRect.tl() - matchRect.tl());
        captureRect += matchedCaptureOffset;
        captureRect.width *= lastScale;
        captureRect.height *= lastScale;
        LOG(INFO) << std::format("Compass value={:.3f} for scale {:.6f}", compassValue, lastScale);
    } else {
        LOG(INFO) << std::format("Compass value={:.3f} below threshold={:.3f}", compassValue, threshold_min);
        return 0;
    }

    //
    // Detect compass dot
    //

    int bestDotIdx = -1;
    double bestDotVal = 0;
    cv::Point bestDotLoc;
    cv::Size bestDotSize;

    cv::Point dotMatchedCaptureOffset;
    imageFiltered = cv::Mat(env.imageColor, captureRect);

//    cv::imshow("Detected compass", imageFiltered);
//    cv::imshow("Dot fwd compass", compassDots[0].templImage);
//    cv::imshow("Dot bwd compass", compassDots[1].templImage);
//    cv::waitKey();
//    cv::destroyAllWindows();

    for (int dotIdx=0; dotIdx < compassDots.size(); dotIdx++) {
        auto& sm = compassDots[dotIdx];
        int result_cols = captureRect.width - sm.templImage.cols + 1;
        int result_rows = captureRect.height - sm.templImage.rows + 1;
        cv::Mat result(result_rows, result_cols, CV_32FC1);
        cv::matchTemplate(imageFiltered, sm.templImage, result, cv::TM_SQDIFF_NORMED, sm.templMask);
        //LOG(ERROR) << "dot " << dotIdx << " match result: " << result;
        fixNaNinResult(result);
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
        lastDotIdx = bestDotIdx;
        dotCaptureRect = { captureRect.tl()+bestDotLoc, bestDotSize };
        dotSpherePosition = {
                std::clamp( ((bestDotLoc.x+bestDotSize.width*0.5) - captureRect.width*0.5) / ((captureRect.width-16)*0.5), -1.0, +1.0),
                std::clamp(-((bestDotLoc.y+bestDotSize.height*0.5) - captureRect.height*0.5) / ((captureRect.height-16)*0.5), -1.0, +1.0),
        };

        double pitch = std::asin(dotSpherePosition.y) * 90 / M_PI_2;
        double yaw = std::asin(dotSpherePosition.x) * 90 / M_PI_2;
        double roll = 90-std::atan2(dotSpherePosition.y, dotSpherePosition.x) * 90 / M_PI_2;

        if (lastDotIdx&1)
            pitch = 180 - pitch;
        if (lastDotIdx&1)
            yaw = 180 - yaw;
        if (pitch > 180) pitch = 360 - pitch;
        if (pitch < -180) pitch = 360 + pitch;
        if (yaw > 180) yaw = 360 - yaw;
        if (yaw < -180) yaw = 360 + yaw;
        if (roll > 180) roll = 360 - roll;
        if (roll < -180) roll = 360 + roll;
        lastTgtPitch = pitch;
        lastTgtYaw = yaw;
        lastTgtRoll = roll;

        LOG(INFO) << std::format("Compass dot value={:.3f}, direction={}",
                                 lastDotValue, ((lastDotIdx&1) ? "backward" : "forward"))
                  << ", sphere pos=" << dotSpherePosition
                  << " pitch,yaw,roll=" << std_format("{:.0f},{:.0f},{:.0f}", pitch, yaw, roll);
    } else {
        lastDotIdx = -1;
        dotCaptureRect = {};
        lastTgtPitch = 0;
        lastTgtYaw = 0;
        lastTgtRoll = 0;
    }

    return compassValue;
}

void CompassDetector::tryLowerUpperBoundsGUI(ClassifyEnv &env, cv::Rect referenceRect) {
    cv::Point extendLT(50,150);
    cv::Point extendRB(50,50);

    cv::Rect captureRect = env.cvtReferenceToCaptured(referenceRect);
    cv::Rect matchRect = cv::Rect(captureRect.tl() - env.scaleToCaptured(extendLT),
                                  captureRect.br() + env.scaleToCaptured(extendRB));
    matchRect &= env.captureCrop;

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

    cv::Mat img = cv::Mat(env.imageColor, matchRect);
    cv::Mat output = img;

    while(1) {

        // Set minimum and max HSV values to display
        cv::Vec3b lower = {(uchar)hMin, (uchar)sMin, (uchar)vMin};
        cv::Vec3b upper = {(uchar)hMax, (uchar)sMax, (uchar)vMax};

        // Create HSV Image and threshold into a range.
        cv::Mat hsv;
        cv::cvtColor(img, hsv, cv::COLOR_RGB2HSV);
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
        cv::rectangle(env.debugImage, dotCaptureRect.tl(), dotCaptureRect.br(), color, 1);
        std::string text = std::format("{}/{}/{}", int(lastTgtPitch), int(lastTgtYaw), int(lastTgtRoll));
        cv::Point orig = captureRect.tl() + cv::Point(0,-10);
        color = {254,254,254};
        cv::putText(env.debugImage, text, orig, cv::FONT_HERSHEY_PLAIN, 1, color);
    }
    return value;
}

