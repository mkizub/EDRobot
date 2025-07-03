//
// Created by mkizub on 02.07.2025.
//
#include "../pch.h"

#include "Detector.h"

#include <glob/glob.h>

#include <iomanip>

namespace detect {

ImageTemplate::ImageTemplate(
        const std::string &filename, cv::Rect rect)
        : filename(filename)
        , referenceRect(std::move(rect))
        , channels(0)
        , threshold_min(0.8)
        , threshold_max(0.8)
{
    if (!filename.empty()) {
        auto paths = glob::glob(filename);
        for (auto &path: paths) {
            cv::Mat templImage;
            cv::Mat templMask;
            if (!loadImageAndMask(path.string(), templImage, templMask) || templImage.empty())
                throw std::runtime_error("Cannot load image: " + path.string());
            if (templImage.cols != referenceRect.width || templImage.rows != referenceRect.height)
                throw std::runtime_error(std::format(
                        "Image size {}x{} does not match expected size {}x{}",
                        templImage.cols, templImage.rows, referenceRect.width, referenceRect.height));
            if (!channels)
                channels = templImage.channels();
            else if (channels != templImage.channels()) {
                throw std::runtime_error(std::format("Images for '{}' have different channels: {} != {}",
                                                     filename, channels, templImage.channels()));
            }
            imagesOrig.emplace_back(1, 0, path.filename().string(), templImage, templMask);
        }
        if (imagesOrig.empty())
            throw std::runtime_error("No images found for: " + filename);
    }
}

bool ImageTemplate::loadImageAndMask(const std::string &filename, cv::Mat &image, cv::Mat &mask) {
    image = cv::imread(filename, cv::IMREAD_UNCHANGED); // assume BGR/BGRA
    if (image.empty()) {
        LOG(ERROR) << "Template image " << filename << " not found";
        throw std::runtime_error(std::format("Cannot read %s", filename));
    }
    extractImageMask(image, mask);
    return true;
}

bool ImageTemplate::extractImageMask(cv::Mat &image, cv::Mat &mask) {
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
    } else if (image.channels() == 3) {
        cv::Mat bgra;
        cv::cvtColor(image, bgra, cv::COLOR_BGR2BGRA);
        image = bgra;
        mask.release();
    }
    return true;
}

double ImageTemplate::classify(ClassifyEnv &env) {
    double value = match(env);
    return toResult(value);
}

double ImageTemplate::debugMatch(ClassifyEnv &env) {
    double value = match(env);
    LOG(INFO) << "ImageTemplate match result: " << std::setprecision(3) << value <<
              "[" << threshold_min << ":" << threshold_max << "] >> " << toResult(value) << " for " << filename <<
              "; offset: " << env.scaleToReference(matchedCaptureOffset);
    if (lastTemplatedx >= 0 && lastTemplatedx < imagesPrepared.size()) {
        auto& im = imagesPrepared[lastTemplatedx];
        LOG(INFO) << "ImageTemplate best match was at template: "
                  << std::format("index {} scale {:.6f} angle {} name {}", lastTemplatedx, im.scale, im.angle, im.name);
    }
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

double ImageTemplate::toResult(double matchValue) {
    if (matchValue >= threshold_max)
        return 1;
    if (matchValue < threshold_min)
        return 0;
    double x = (matchValue - threshold_min) / (threshold_max - threshold_min);
    x = (x - 0.5) * 8;
    return 1 / (1 + std::exp(-x));
}

cv::Mat ImageTemplate::applyFilters(const std::vector<std::unique_ptr<ImageFilter>>& filters, cv::Mat image) {
    if (image.empty())
        return image;
    cv::Mat out = image;
    for (auto &filter: filters) {
        out = filter->apply(out);
    }
    return out;
}

cv::Mat ImageTemplate::scaleImage(cv::Mat image, double scaleX, double scaleY) {
    if (scaleY == 0)
        scaleY = scaleX;
    if ((scaleX == 1 && scaleY == 1) || image.empty())
        return image;
    cv::Mat out;
    cv::resize(image, out, {}, scaleX, scaleY);
    return out;
}

cv::Mat ImageTemplate::rotateImage(cv::Mat image, int angle, double scale) {
    if ((angle == 0 && scale == 1) || image.empty())
        return image;
    cv::Size size = {image.cols, image.rows};
    cv::Point2f center(image.cols * 0.5f, image.rows * 0.5f);
    cv::Mat rotationMatrix = cv::getRotationMatrix2D(center, angle, scale);
    cv::Mat out;
    cv::warpAffine(image, out, rotationMatrix, size, cv::INTER_LINEAR, cv::BORDER_TRANSPARENT);
    return out;
}


cv::Mat GaussFilter::apply(cv::Mat image) {
    if (disabled)
        return image;
    cv::Mat out;
    cv::GaussianBlur(image, out, cv::Size(kernX, kernY), 0, 0);
    return out;
}

cv::Mat LaplacianFilter::apply(cv::Mat image) {
    cv::Mat lapl16S;
    cv::Mat lapl8U;
    cv::Laplacian(image, lapl16S, CV_16S, kern, scale);
    cv::convertScaleAbs(lapl16S, lapl8U);
    return lapl8U;
}

cv::Mat HsvColorCropFilter::apply(cv::Mat image) {
    if (ranges.empty())
        return image;
    cv::Mat hsv;
    cv::cvtColor(image, hsv, cv::COLOR_BGR2HSV);
    cv::Mat mask(image.rows, image.cols, CV_8UC1);
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

void ImageTemplate::fixNaNinResult(cv::Mat &result, const std::string& filename) {
    // bypass error in cv::matchTemplate that sometimes return NaN/Inf, instead of [0..1] valies
    auto *ptr = result.ptr<float>(0);
    auto *pend = ptr + result.rows * result.cols;
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
    LOG_IF(bad_image, ERROR) << "Bad image for TM_CCORR_NORMED: " << filename;
#endif
}

void ImageTemplate::prepareImages(ClassifyEnv& env) {
    if (env.getScale() != preprocessedTemplateScale) {
        preprocessedTemplateScale = env.getScale();
        if (testScales.empty())
            testScales.push_back(1);
        if (testAngles.empty())
            testAngles.push_back(0);
        for (double scale : testScales) {
            for (int angle : testAngles) {
                for (auto &im: imagesOrig) {
                    cv::Mat templImagePrepared = applyFilters(filters, im.templImage);
                    if (channels == 1 && im.templImage.channels() != 1) {
                        cv::Mat grayImage;
                        cv::cvtColor(templImagePrepared, grayImage, cv::COLOR_BGR2GRAY);
                        templImagePrepared = grayImage;
                    }
                    if (angle == 0)
                        templImagePrepared = scaleImage(templImagePrepared, scale * env.getScale(), scale * env.getScale());
                    else
                        templImagePrepared = rotateImage(templImagePrepared, angle, scale * env.getScale());
                    cv::Mat templMaskPrepared;
                    if (angle == 0)
                        templMaskPrepared = scaleImage(im.templMask, scale * env.getScale(), scale * env.getScale());
                    else
                        templMaskPrepared = rotateImage(im.templMask, angle, scale * env.getScale());
                    assert (templMaskPrepared.empty() || templMaskPrepared.size == templImagePrepared.size);
                    imagesPrepared.emplace_back(scale, angle, im.name, templImagePrepared, templMaskPrepared);
                }
            }
        }
    }
}

double ImageTemplate::match(ClassifyEnv &env) {
    if (imagesOrig.empty() || referenceRect.empty() || !channels)
        return 0;
    cv::Mat gameImage = channels == 1 ? env.getGrayImage() : env.getColorImage();
    if (gameImage.empty())
        return 0;

    prepareImages(env);

    int ext = Master::getInstance().getSearchRegionExtent();
    captureRect = env.cvtReferenceToCaptured(referenceRect);
    matchRect = cv::Rect(captureRect.tl() - env.scaleToCaptured(extendLT + cv::Point(ext, ext)),
                         captureRect.br() + env.scaleToCaptured(extendRB + cv::Point(ext, ext)));
    env.cropToCapture(matchRect);

    cv::Mat gameImagePrepared = applyFilters(filters, cv::Mat(gameImage, matchRect));
    if (gameImagePrepared.empty())
        return 0;

    lastTemplatedx = -1;
    double bestVal = -1000;
    cv::Point bestLoc;
    ImageMatrix* bestTempl = nullptr;
    for (auto& im : imagesPrepared) {
        int result_cols = matchRect.width - im.templImage.cols + 1;
        int result_rows = matchRect.height - im.templImage.rows + 1;
        if (result_cols <= 0 || result_rows <= 0)
            continue;
        cv::Mat result(result_rows, result_cols, CV_32FC1);
        //if (templMaskScaled.empty())
        cv::matchTemplate(gameImagePrepared, im.templImage, result, cv::TM_CCOEFF_NORMED/*, im.templMask*/);
        //else
        //    cv::matchTemplate(gameImagePrepared, im.templImage, result, cv::TM_CCORR_NORMED, im.templMask);
        fixNaNinResult(result, im.name);
        //LOG(ERROR) << "match result: " << result;
        double maxVal;
        cv::Point maxLoc;
        cv::minMaxLoc(result, nullptr, &maxVal, nullptr, &maxLoc);
        LOG(DEBUG) << "ImageTemplate match result: " << std::setprecision(3) << maxVal << " for " << im.name;
        if (maxVal > bestVal) {
            bestVal = maxVal;
            bestLoc = maxLoc;
            bestTempl = &im;
        }
    }
    if (bestVal >= threshold_min) {
        lastTemplatedx = bestTempl - &imagesPrepared.front();
        matchedCaptureOffset = bestLoc - (captureRect.tl() - matchRect.tl());
        captureRect = {captureRect.tl() + matchedCaptureOffset, captureRect.br() + matchedCaptureOffset};
    }
    if (!name.empty() && bestVal >= threshold_min) {
        env.classified.emplace_back(ClsDetType::Detected, env.isWarpMode(), name,
                                    referenceRect + env.scaleToReference(matchedCaptureOffset));
        env.classified.back().u.tdet.referenceRect = referenceRect;
        env.classified.back().u.tdet.scale = bestTempl->scale;
        env.classified.back().u.tdet.angle = bestTempl->angle;
    }
    return bestVal;
}

} // detect