//
// Created by mkizub on 02.07.2025.
//
#include "../pch.h"

#include "Detector.h"

#include <glob/glob.h>

#include <iomanip>

namespace detect {

ImageTemplate::ImageTemplate(
        const std::string &filename, spEvalRect rect)
        : filename(filename)
        , refEvalRect(std::move(rect))
        , channels(0)
        , threshold_min(0.8)
        , threshold_max(0.8)
{
    setTemplate(filename);
}

void ImageTemplate::setTemplate(const std::string& filename) {
    imagesOrig.clear();
    imagesPrepared.clear();

    this->filename = filename;
    if (!filename.empty()) {
        auto paths = glob::glob(filename);
        for (auto &path: paths) {
            XMat templImage;
            if (!loadImageAndMask(path.string(), templImage) || templImage.empty())
                throw std::runtime_error("Cannot load image: " + path.string());
            if (refSize.width == 0 || refSize.height == 0) {
                if (auto cr = std::dynamic_pointer_cast<ConstRect>(refEvalRect); cr) {
                    refSize = cr->mRect.size();
                } else {
                    refSize = templImage.size();
                }
            }
            if (templImage.size() != refSize) {
                XMat tmp;
                cv::resize(templImage, tmp, refSize);
                templImage = tmp;
            }
            if (!channels)
                channels = templImage.channels();
            else if (channels != templImage.channels()) {
                throw std::runtime_error(std::format("Images for '{}' have different channels: {} != {}",
                                                     filename, channels, templImage.channels()));
            }
            imagesOrig.emplace_back(1, 0, path.filename().string(), templImage);
        }
        if (imagesOrig.empty())
            throw std::runtime_error("No images found for: " + filename);
    }
}

bool ImageTemplate::loadImageAndMask(const std::string &filename, XMat &image) {
    cv::Mat src = cv::imread(filename, cv::IMREAD_UNCHANGED); // assume GRAY/BGR/BGRA
    if (src.empty()) {
        LOG(ERROR) << "Template image " << filename << " not found";
        throw std::runtime_error(std::format("Cannot read %s", filename));
    }
    XMat srcX = toXMat(src);
    if (src.channels() == 1 || src.channels() == 4) {
        image = srcX;
    } else {
        cv::cvtColor(srcX, image, cv::COLOR_RGB2RGBA);
    }
    return true;
}

double ImageTemplate::toResult(double matchValue) {
    if (matchValue >= threshold_max)
        return 1;
    if (matchValue < threshold_min)
        return 0;
    double x = (matchValue - threshold_min) / (threshold_max - threshold_min);
    x = (x - 0.5) * 4.25;
    return std::clamp(1.125 / (1 + std::exp(-x)), 0.0, 1.0);
}

ImageTemplate::ImageMatrix ImageTemplate::prepareImageMatrix(
        const ClassifyEnv& env, const std::vector<std::unique_ptr<ImageFilter>>& filters,
        XMat image, double scale, int angle, const std::string& name, ImageFilter::Params params)
{
    XMat prep = image;
    if (angle == 0)
        prep = scaleImage(prep, scale * env.getScale(), scale * env.getScale());
    else
        prep = rotateImage(prep, angle, scale * env.getScale());
    prep = applyFilters(filters, prep, {.convertToFloat=false});
    uint16_t org_w = prep.cols;
    uint16_t org_h = prep.rows;
    uint16_t opt_w = cv::getOptimalDFTSize(org_w);
    uint16_t opt_h = cv::getOptimalDFTSize(org_h);
    uint16_t opt_top = 0;
    uint16_t opt_bottom = 0;
    uint16_t opt_left = 0;
    uint16_t opt_right = 0;
    if (prep.cols != opt_w || prep.rows != opt_h) {
        opt_top = (opt_h - prep.rows) / 2;
        opt_bottom = opt_h - prep.rows - opt_top;
        opt_left = (opt_w - prep.cols) / 2;
        opt_right = opt_w - prep.cols - opt_left;
        XMat opt_prep;
        cv::copyMakeBorder(prep, opt_prep, opt_top, opt_bottom, opt_left, opt_right, cv::BORDER_REPLICATE);
        prep = opt_prep;
    }
    XMat prepU, prepF;
    if (prep.depth() == CV_8U) {
        prepU = prep;
        prepU.convertTo(prepF, CV_32F, 1.0/255.0);
    } else {
        prepF = prep;
        prepF.convertTo(prepU, CV_8U, 255.0);
    }
    return {scale, double(angle), name, prepU, prepF, org_w, org_h, opt_w, opt_h, opt_left, opt_top, opt_right, opt_bottom};
}

XMat ImageTemplate::applyFilters(const std::vector<std::unique_ptr<ImageFilter>>& filters, XMat image, ImageFilter::Params params) {
    if (image.empty())
        return image;
    XMat out = image;
    for (auto &filter: filters) {
        out = filter->apply(out, params);
    }
    if (params.convertToFloat && out.depth() == CV_8U) {
        XMat out32f;
        out.convertTo(out32f, CV_32F, 1.0/255.0);
        out = out32f;
    }
    return out;
}

XMat ImageTemplate::scaleImage(XMat image, double scaleX, double scaleY) {
    if (scaleY == 0)
        scaleY = scaleX;
    if ((scaleX == 1 && scaleY == 1) || image.empty())
        return image;
    XMat out;
    cv::resize(image, out, {}, scaleX, scaleY);
    return out;
}

XMat ImageTemplate::rotateImage(XMat image, int angle, double scale) {
    if ((angle == 0 && scale == 1) || image.empty())
        return image;
    cv::Size size = {image.cols, image.rows};
    cv::Point2f center(image.cols * 0.5f, image.rows * 0.5f);
    cv::Matx23d rotationMatrix = cv::getRotationMatrix2D_(center, angle, scale);
    XMat out;
    cv::warpAffine(image, out, rotationMatrix, size, cv::INTER_LINEAR, cv::BORDER_TRANSPARENT);
    return out;
}


//void ImageTemplate::fixNaNinResult(cv::Mat &result, const std::string& filename) {
//    // bypass error in cv::matchTemplate that sometimes return NaN/Inf, instead of [0..1] valies
//    auto *ptr = result.ptr<float>(0);
//    auto *pend = ptr + result.rows * result.cols;
//#ifndef NDEBUG
//    bool bad_image = false;
//#endif
//    for (; ptr < pend; ++ptr) {
//        if (std::isnan(*ptr) || std::isinf(*ptr)) {
//#ifndef NDEBUG
//            bad_image = true;
//#endif
//            *ptr = 0;
//        }
//    }
//#ifndef NDEBUG
//    LOG_IF(bad_image, ERROR) << "Bad image for TM_CCORR_NORMED: " << filename;
//#endif
//}

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
                    XMat templImageU = im.templImageU;
                    if (channels == 1 && templImageU.channels() != 1) {
                        XMat grayImage;
                        cv::cvtColor(templImageU, grayImage, cv::COLOR_BGR2GRAY);
                        templImageU = grayImage;
                    }
                    ImageMatrix im_prep = prepareImageMatrix(env, filters, templImageU, scale, angle, im.name);
                    imagesPrepared.push_back(im_prep);
                }
            }
        }
    }
}

void ImageTemplate::matchTemplates(int method, const XMat& image, std::vector<ImageMatrix>& templates, MatchResult& out) {
    if (!templates.size())
        return;
    const bool use_float = useOpenCL() || templates.size() > 1;
    XMat preparedImage;
    if (use_float && image.depth() != CV_32F) {
        assert (image.depth() == CV_8U);
        image.convertTo(preparedImage, CV_32F, 1.0/255.0);
    } else {
        preparedImage = image;
    }
    assert (use_float && preparedImage.type() == templates[0].templImageF.type() || !use_float && preparedImage.type() == templates[0].templImageU.type());

    bool not_normed = (method == cv::TM_CCORR || method == cv::TM_CCOEFF || method == cv::TM_SQDIFF);
    if (not_normed && templates[0].u_norm == 0) {
        for (auto& im : templates) {
            im.f_norm = DBL_EPSILON;
            auto sum = cv::sum(im.templImageF);
            for (int i = 0; i < image.channels(); i++)
                im.f_norm += sum[i];
            im.u_norm = DBL_EPSILON;
            sum = cv::sum(im.templImageU);
            for (int i = 0; i < image.channels(); i++)
                im.u_norm += sum[i];
        }
    }

//    {
//        cv::Mat img = preparedImage.getMat(cv::ACCESS_READ);
//        cv::Mat templU = templates[0].templImageU.getMat(cv::ACCESS_READ);
//        cv::Mat templF = templates[0].templImageF.getMat(cv::ACCESS_READ);
//        if (img.empty())
//            return;
//    }

    XMat result;
    for (int idx=0; idx < templates.size(); idx++) {
        auto& im = templates[idx];
        XMat& templImage = use_float ? im.templImageF : im.templImageU;
        int result_cols = preparedImage.cols - templImage.cols + 1;
        int result_rows = preparedImage.rows - templImage.rows + 1;
        if (result_cols <= 0 || result_rows <= 0)
            continue;
        cv::matchTemplate(preparedImage, templImage, result, method);
        double minVal, maxVal;
        cv::Point minLoc, maxLoc;
        cv::minMaxLoc(result, &minVal, &maxVal, &minLoc, &maxLoc);
        if (not_normed) {
            minVal /= use_float ? im.f_norm : im.u_norm;
            maxVal /= use_float ? im.f_norm : im.u_norm;
        }
        //LOG(DEBUG) << "ImageTemplate match result: " << std::setprecision(4) << maxVal << " for " << im.name << " scale:" << im.scale;
        if (method == cv::TM_SQDIFF || method == cv::TM_SQDIFF_NORMED) {
            if ((1-minVal) > out.value) {
                out.value = (1-minVal);
                out.loc = minLoc + cv::Point(im.opt_l, im.opt_t);
                out.im = &im;
                out.index = idx;
            }
        } else {
            if (maxVal > out.value) {
                out.value = maxVal;
                out.loc = maxLoc + cv::Point(im.opt_l, im.opt_t);
                out.im = &im;
                out.index = idx;
            }
        }
    }
}


cv::Rect ImageTemplate::makeOptimalMatchRect(cv::Rect r) {
    int optimalH = cv::getOptimalDFTSize(r.height);
    int optimalW = cv::getOptimalDFTSize(r.width);
    int addTop = std::min(r.y, (optimalH - r.height) / 2);
    int addBottom = optimalH - r.height - addTop;
    int addLeft = std::min(r.x, (optimalW - r.width) / 2);
    int addRight = optimalW - r.width - addLeft;
    cv::Rect out {r.x - addLeft, r.y - addTop, r.width + addLeft + addRight, r.height + addTop + addBottom};
    return out;
}

double ImageTemplate::match(ClassifyEnv &env) {
    lastMatch = 0;
    if (!withRefRect.empty())
        refOrig = withRefRect.tl();
    else if (refEvalRect)
        refOrig = refEvalRect->calcReferenceRect(env).tl();
    if (imagesOrig.empty() || refSize.empty() || !channels)
        return 0;
    XMat gameImage = env.getColorImage();
    if (gameImage.empty())
        return 0;
    prepareImages(env);

    auto startTime = std::chrono::high_resolution_clock::now();

    int ext = Master::getInstance().getSearchRegionExtent();
    cv::Rect refRect(refOrig, refSize);
    captureRect = env.cvtReferenceToCaptured(refRect);
    matchRect = cv::Rect(captureRect.tl() - env.scaleToCaptured(extendLT + cv::Point(ext, ext)),
                         captureRect.br() + env.scaleToCaptured(extendRB + cv::Point(ext, ext)));
    matchRect = makeOptimalMatchRect(matchRect);
    matchRect &= cv::Rect(0,0,gameImage.cols,gameImage.rows);

    XMat gameImagePrepared = applyFilters(filters, gameImage(matchRect), {.convertToFloat=useOpenCL()});
    if (gameImagePrepared.empty())
        return 0;

    lastMatchResult = {};
    auto& mr = lastMatchResult;
    matchTemplates(matchMethod, gameImagePrepared, imagesPrepared, mr);
    auto elapsedTime = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - startTime);
    if (mr.value >= threshold_min) {
        LOG(DEBUG) << std::format("ImageTemplate match result: {:.4f} ({:.4f}) for {} scale {:.4f}, took {}us", mr.value, toResult(mr.value), mr.im->name, mr.im->scale, elapsedTime.count());
        matchedCaptureOffset = mr.loc - (captureRect.tl() - matchRect.tl());
        captureRect = {captureRect.tl() + matchedCaptureOffset, captureRect.br() + matchedCaptureOffset};
        if (mr.im->scale != 1) {
            captureRect.width = (int)std::round(captureRect.width * mr.im->scale);
            captureRect.height = (int)std::round(captureRect.height * mr.im->scale);
        }
    } else {
        LOG(DEBUG) << std::format("ImageTemplate not found: {:.4f} ({:.4f}) for '{}', took {}us", mr.value, toResult(mr.value), filename, elapsedTime.count());
    }
    lastMatch = mr.value;
    if (!name.empty() && mr.value >= threshold_min) {
        env.classified.emplace_back(ClsDetType::Detected, name,
                                    refRect + env.scaleToReference(matchedCaptureOffset));
        auto& tdet = env.classified.back().u.tdet;
        tdet.referenceRect = refRect;
        tdet.scale = mr.im->scale;
        tdet.angle = mr.im->angle;
        tdet.match = lastMatch;
        tdet.matchRect = matchRect;
    }
    return toResult(mr.value);
}

} // detect