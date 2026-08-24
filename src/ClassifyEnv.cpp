//
// Created by mkizub on 16.06.2025.
//

#include "pch.h"

#include "ClassifyEnv.h"

ResolvedEnv::ResolvedEnv() {
    init(cv::Rect(cv::Point(), ReferenceScreenSize), false, 1);
}

void ResolvedEnv::init(const cv::Rect& frmRect, bool warped, double scale) {
    frameRect = frmRect;
    frameCrop = cv::Rect(cv::Point(), frmRect.size());
    classified.clear();
    isDebugMatch_ = false;
    needScale_ = (scale != 1);
    needCrop_ = frameRect.tl() != cv::Point();
    isWarped_ = warped;
    scaleToCaptured_ = scale;
}

void ClassifyEnv::init(upFrame&& frame) {
    assert (frame);
    mFrame.swap(frame);
    mColorImage.release();

    double scale = std::min(double(mFrame->size.width) / ReferenceScreenSize.width,
                            double(mFrame->size.height) / ReferenceScreenSize.height);
    cv::Point frameCenter = mFrame->size / 2;
    cv::Point tl = cv::Point() - cv::Point(ReferenceScreenCenter);
    tl *= scale;
    tl += frameCenter;
    cv::Point br = cv::Point(ReferenceScreenSize) - cv::Point(ReferenceScreenCenter);
    br *= scale;
    br += frameCenter;
    cv::Rect frameRect {tl, br};
    ResolvedEnv::init(frameRect, false, scale);
    unWarpMatrix = cv::Matx33d::eye();
}

void ClassifyEnv::init(XMat warpedImage, bool warped, double scale) {
    cv::Rect frameRect {0, 0, warpedImage.cols, warpedImage.rows};
    upFrame fr(new FrameWarp(frameRect.size(), warpedImage));
    mFrame.swap(fr);
    mColorImage = warpedImage;
    ResolvedEnv::init(frameRect, warped, scale);
    unWarpMatrix = cv::Matx33d::eye();
}

void ClassifyEnv::init(XMat warpedImage, const cv::Matx33d& unWM) {
    cv::Rect frameRect {0, 0, warpedImage.cols, warpedImage.rows};
    upFrame fr(new FrameWarp(frameRect.size(), warpedImage));
    mFrame.swap(fr);
    mColorImage = warpedImage;
    ResolvedEnv::init(frameRect, true, 1.0);
    unWarpMatrix = unWM;
}

void ClassifyEnv::init(XMat warpedImage, const cv::Matx23d& unWMaffine) {
    auto& m = unWMaffine.val;
    cv::Matx33d unWMperspective = {
            m[0], m[1], m[2],
            m[3], m[4], m[5],
            0.00, 0.00, 1.00
    };
    init(warpedImage, unWMperspective);
}

void ResolvedEnv::clear() {
    frameRect = {};
    frameCrop = {};
    isDebugMatch_ = false;
    isWarped_ = false;
    needScale_ = false;
    needCrop_ = false;
    scaleToCaptured_ = 1;
    classified.clear();
}

void ClassifyEnv::clear() {
    ResolvedEnv::clear();
    mColorImage.release();
    unWarpMatrix = cv::Matx33d::eye();
    mFrame.reset();
}

const XMat& ClassifyEnv::getColorImage() const {
    if (!mColorImage.empty())
        return mColorImage;
    if (!mFrame) {
        static XMat empty;
        return empty;
    }
    if (!needCrop_) {
        mColorImage = mFrame->getImage();
    } else {
        mColorImage = mFrame->getImage()(frameRect);
    }
    return mColorImage;
}

template<typename _Tp>
cv::Point_<_Tp> ClassifyEnv::unWarp(const cv::Point_<_Tp> point) const {
    std::vector<cv::Point_<_Tp>> in {point};
    std::vector<cv::Point_<_Tp>> out;
    cv::perspectiveTransform(in, out, unWarpMatrix);
    //auto& m = warpMatrix.val;
    //out.x = (m[0] * point.x + m[1] * point.y + m[2]) / (m[6] * point.x + m[7] * point.y + m[8]);
    //out.y = (m[3] * point.x + m[4] * point.y + m[5]) / (m[6] * point.x + m[7] * point.y + m[8]);
    //out.x = (warpMatrix(0,0) * point.x + warpMatrix(0,1) * point.y + warpMatrix(0,2)) / (warpMatrix(2,0) * point.x + warpMatrix(2,1) * point.y + warpMatrix(2,2));
    //out.y = (warpMatrix(1,0) * point.x + warpMatrix(1,1) * point.y + warpMatrix(1,2)) / (warpMatrix(2,0) * point.x + warpMatrix(2,1) * point.y + warpMatrix(2,2));
    return out[0];
}
template<typename _Tp>
std::array<cv::Point_<_Tp>,4> ClassifyEnv::unWarp(const cv::Rect_<_Tp>& rect) const {
    std::vector<cv::Point_<_Tp>> in {rect.tl(), cv::Point_<_Tp>(rect.x+rect.width, rect.y), rect.br(), cv::Point_<_Tp>(rect.x, rect.y+rect.height)};
    std::vector<cv::Point_<_Tp>> out;
    cv::perspectiveTransform(in, out, unWarpMatrix);
    return {out[0], out[1], out[2], out[3]};
}

template cv::Point2f ClassifyEnv::unWarp(const cv::Point2f point) const;
template cv::Point2d ClassifyEnv::unWarp(const cv::Point2d point) const;
template std::array<cv::Point2f,4> ClassifyEnv::unWarp(const cv::Rect2f& point) const;
template std::array<cv::Point2d,4> ClassifyEnv::unWarp(const cv::Rect2d& point) const;

template<>
cv::Point2i ClassifyEnv::unWarp(const cv::Point2i point) const {
    std::vector<cv::Point2f> in {point};
    std::vector<cv::Point2f> out;
    cv::perspectiveTransform(in, out, unWarpMatrix);
    return out[0];
}
template<>
std::array<cv::Point2i,4> ClassifyEnv::unWarp(const cv::Rect2i& rect) const {
    std::vector<cv::Point2f> in {rect.tl(), cv::Point2f(rect.x+rect.width, rect.y), rect.br(), cv::Point2f(rect.x, rect.y+rect.height)};
    std::vector<cv::Point2f> out;
    cv::perspectiveTransform(in, out, unWarpMatrix);
    return {out[0], out[1], out[2], out[3]};
}

cv::Rect TileRect::calcReferenceRect(const ResolvedEnv& env) const {
    for (auto& cr : env.classified) {
        if (cr.cdt != ClsDetType::Tile)
            continue;
        if (!name.empty()) {
            if (cr.text.starts_with(name))
                return cr.detectedRect;
        }
    }
    return {};
}

ConstTransform::ConstTransform(spEvalPoint tl, spEvalPoint tr, spEvalPoint br, spEvalPoint bl, cv::Size sz)
    : EvalTransform(tl, tr, br, bl, sz)
    , useCaptured(false)
{
}

XMat EvalTransform::transformImage(const XMat& image) const {
    assert (valid);
    XMat warped_image;
    cv::warpPerspective(image, warped_image, transformMatrix, origSize);
    return warped_image;
}

bool ConstTransform::calcTransform(const ResolvedEnv& env) {
    if (!useCaptured) {
        for (int i = 0; i < 4; i++) {
            auto p = orig[i]->calcReferencePoint(env);
            transformSrc[i] = env.cvtReferenceToCaptured(p);
        }
    }

    std::array<cv::Point2f,4> dst;
    dst[0] = {0.f,0.f};
    dst[1] = {(float)origSize.width,0.f};
    dst[2] = {(float)origSize.width,(float)origSize.height};
    dst[3] = {0.f,(float)origSize.height};
    transformMatrix = cv::getPerspectiveTransform(transformSrc, dst);

    valid = true;
    return true;
}

LineTransform::LineTransform(std::vector<std::string> line, spEvalPoint tl, spEvalPoint tr, spEvalPoint br, spEvalPoint bl, cv::Size sz)
    : EvalTransform(tl, tr, br, bl, sz)
    , lineDetector(std::move(line))
{
}

bool LineTransform::calcTransform(const ResolvedEnv& env) {
    valid = false;
    const ClassifiedRect* crld = nullptr;
    for (auto& cr : env.classified) {
        if (cr.cdt == ClsDetType::LineDetected) {
            std::string_view name = cr.text;
            int sep = name.find(':');
            if (sep != std::string_view::npos)
                name = name.substr(0, sep);
            auto it = std::find(lineDetector.begin(), lineDetector.end(), name);
            if (it != lineDetector.end()) {
                crld = &cr;
                break;
            }
        }
    }
    if (!crld) {
        valid = false;
        return false;
    }
    cv::Point2f detectedAnchor = (crld->detectedRect.tl()+crld->detectedRect.br()) * 0.5f;
    cv::Matx23d affineMatrix = cv::getRotationMatrix2D_(detectedAnchor, -crld->u.ldet.angle, crld->u.ldet.scale);
    affineMatrix.val[2] += crld->u.ldet.offset.x;
    affineMatrix.val[5] += crld->u.ldet.offset.y;
    for (int i=0; i < 4; i++) {
        auto p = orig[i]->calcReferencePoint(env);
        transformSrc[i] = env.cvtReferenceToCaptured(p);
    }
    cv::transform(transformSrc, transformSrc, affineMatrix);

    std::array<cv::Point2f,4> transformDst;
    transformDst[0] = {0.f,0.f};
    transformDst[1] = {(float)origSize.width,0.f};
    transformDst[2] = {(float)origSize.width,(float)origSize.height};
    transformDst[3] = {0.f,(float)origSize.height};

    transformMatrix = cv::getPerspectiveTransform(transformSrc, transformDst);
    valid = true;
    return true;
}
