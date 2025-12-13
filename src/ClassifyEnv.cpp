//
// Created by mkizub on 16.06.2025.
//

#include "pch.h"

#include "ClassifyEnv.h"

ResolvedEnv& ResolvedEnv::operator=(const ResolvedEnv& other) {
    const_cast<cv::Rect&>(monitorRect) = other.monitorRect;
    const_cast<cv::Rect&>(clientRect) = other.clientRect;
    const_cast<cv::Size&>(frameSize) = other.frameSize;
    const_cast<cv::Rect&>(frameCrop) = other.frameCrop;
    classified = other.classified;
    inWarpMode_ = false;
    needScaling_ = other.needScaling_;
    scaleToCaptured_ = other.scaleToCaptured_;
    frameCenter = other.frameCenter;
    return *this;
}

ResolvedEnv::ResolvedEnv() {
    cv::Rect r {0, 0, ReferenceScreenSize.width, ReferenceScreenSize.height};
    init(r, r, ReferenceScreenSize);
}

void ResolvedEnv::init(const cv::Rect& monRect, const cv::Rect& clntRect, const cv::Size& frmSize) {
    const_cast<cv::Rect&>(monitorRect) = monRect;
    const_cast<cv::Rect&>(clientRect) = clntRect;
    const_cast<cv::Size &>(frameSize) = frmSize;
    const_cast<cv::Rect&>(frameCrop) = cv::Rect(cv::Point(), frmSize);
    classified.clear();
    inWarpMode_ = false;
    isDebugMatch_ = false;
    if (frmSize != ReferenceScreenSize) {
        double x_scale = double(frmSize.width) / ReferenceScreenSize.width;
        double y_scale = double(frmSize.height) / ReferenceScreenSize.height;
        scaleToCaptured_ = std::min(x_scale, y_scale);
        needScaling_ = true;
    } else {
        needScaling_ = false;
        scaleToCaptured_ = 1;
    }
    frameCenter = cv::Point(frmSize) / 2;
    warpRect = {};
    warpMatrix = cv::Matx33d::eye();
    unWarpMatrix = cv::Matx33d::eye();
}

class FrameTmp : public Frame {
public:
    FrameTmp(cv::Size size, cv::Mat* cImage)
        : Frame(nullptr, size)
    {
        if (cImage) {
#ifdef EDROBOT_USE_OPENCL
            colorImage = cImage->getUMat(cv::ACCESS_READ);
#else
            colorImage = *cImage;
#endif
        }
    }
    FrameTmp(cv::Size size, XMat cImage)
            : Frame(nullptr, size)
    {
        colorImage = cImage;
    }
    ~FrameTmp() override = default;
    bool valid() const override { return true; }
    const XMat& getImage() const override { return colorImage; }

    XMat colorImage;
};

void ClassifyEnv::init(const ResolvedEnv& rEnv, cv::Mat* colorImage, cv::Mat* grayImage) {
    cv::Size sz;
    if (colorImage)
        sz = {colorImage->cols, colorImage->rows};
    else if (grayImage)
        sz = {grayImage->cols, grayImage->rows};
    else
        throw std::runtime_error("Both color and gray image empty");
    upFrame fr(new FrameTmp(sz, colorImage));
    mFrame.swap(fr);
    mColorImage = colorImage ? toXMat(*colorImage) : XMat();
    mGrayImage = grayImage ? toXMat(*grayImage) : XMat();
    if (!mDebugOverlay.empty())
        mDebugOverlay = cv::Mat();
    ResolvedEnv::init(rEnv.monitorRect, rEnv.clientRect, rEnv.frameSize);
    mWarpTransform.reset();
    if (!mWarpedColorImage.empty())
        mWarpedColorImage.release();
    if (!mWarpedGrayImage.empty())
        mWarpedGrayImage.release();
}

void ClassifyEnv::init(XMat warpedImage) {
    cv::Size sz {warpedImage.cols, warpedImage.rows};
    upFrame fr(new FrameTmp(sz, warpedImage));
    mFrame.swap(fr);
    mColorImage = warpedImage;
    mGrayImage.release();
    if (!mDebugOverlay.empty())
        mDebugOverlay.release();
    cv::Rect rect(0,0,warpedImage.cols,warpedImage.rows);
    ResolvedEnv::init(rect, rect, rect.size());
    needScaling_ = false;
    scaleToCaptured_ = 1;
    mWarpTransform.reset();
    if (!mWarpedColorImage.empty())
        mWarpedColorImage.release();
    if (!mWarpedGrayImage.empty())
        mWarpedGrayImage.release();
}

void ClassifyEnv::init(const cv::Rect& monRect, const cv::Rect& captRect, const cv::Size& frmSize, upFrame&& frame) {
    assert (frame);
    assert (frame->size == frmSize);
    mFrame.swap(frame);
    mColorImage.release();
    mGrayImage.release();
    if (!mDebugOverlay.empty())
        mDebugOverlay.release();
    ResolvedEnv::init(monRect, captRect, frmSize);
    mWarpTransform.reset();
    if (!mWarpedColorImage.empty())
        mWarpedColorImage.release();
    if (!mWarpedGrayImage.empty())
        mWarpedGrayImage.release();
}

void ResolvedEnv::clear() {
    const_cast<cv::Rect&>(monitorRect) = cv::Rect();
    const_cast<cv::Rect&>(clientRect) = cv::Rect();
    const_cast<cv::Size&>(frameSize) = cv::Size();
    const_cast<cv::Rect&>(frameCrop) = cv::Rect();
    inWarpMode_ = false;
    needScaling_ = false;
    scaleToCaptured_ = 1;
    frameCenter = ReferenceScreenCenter;
    classified.clear();
    warpRect = {};
    warpMatrix = cv::Matx33d::eye();
    unWarpMatrix = cv::Matx33d::eye();
}

void ClassifyEnv::clear() {
    ResolvedEnv::clear();
    mFrame.reset();
    mColorImage.release();
    mGrayImage.release();
    if (!mDebugOverlay.empty())
        mDebugOverlay.release();
    mWarpTransform.reset();
    if (!mWarpedColorImage.empty())
        mWarpedColorImage.release();
    if (!mWarpedGrayImage.empty())
        mWarpedGrayImage.release();
}

const XMat& ClassifyEnv::getColorImage() const {
    if (inWarpMode_)
        return mWarpedColorImage;
    if (!mColorImage.empty())
        return mColorImage;
    if (!mFrame) {
        static XMat empty;
        return empty;
    }
    mColorImage = mFrame->getImage();
    return mColorImage;
}

const XMat& ClassifyEnv::getGrayImage() const {
    if (inWarpMode_) {
        if (mWarpedGrayImage.empty())
            cv::cvtColor(mWarpedColorImage, mWarpedGrayImage, cv::COLOR_BGR2GRAY);
        return mWarpedGrayImage;
    }
    if (mGrayImage.empty())
        cv::cvtColor(getColorImage(), mGrayImage, cv::COLOR_BGR2GRAY);
    return mGrayImage;
}

cv::Mat& ClassifyEnv::getDebugImage() const {
    if (!mDebugOverlay.empty())
        return mDebugOverlay;
    mDebugOverlay = cv::Mat(frameSize, CV_8UC4, cv::Vec4b::zeros());
    return mDebugOverlay;
}

const XMat& ClassifyEnv::getWarpedColorImage() const {
    return mWarpedColorImage;
}
const XMat& ClassifyEnv::getWarpedGrayImage() const {
    return mWarpedGrayImage;
}

void ClassifyEnv::warpPerspective(const spEvalTransform& transform) {
    if (mWarpTransform)
        LOG(WARNING) << "Duplicated warping";
    mWarpTransform = transform;
    if (mWarpTransform && mWarpTransform->calcTransform(*this)) {
        warpRect = cv::Rect(cv::Point(0,0), mWarpTransform->origSize);
        warpMatrix = mWarpTransform->transformMatrix;
        unWarpMatrix = warpMatrix.inv();
        bool saveWarp = inWarpMode_;
        inWarpMode_ = false;
        mWarpedColorImage = mWarpTransform->transformImage(getColorImage());
        mWarpedGrayImage = mWarpTransform->transformImage(getGrayImage());
        inWarpMode_ = saveWarp;
    }
}

cv::Point ResolvedEnv::cvtReferenceToDesktop(const cv::Point& point) const {
    return monitorRect.tl() + clientRect.tl() + cvtReferenceToCaptured(point);
}

template<typename _Tp>
cv::Point_<_Tp> ResolvedEnv::unWarp(const cv::Point_<_Tp> point) const {
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
std::array<cv::Point_<_Tp>,4> ResolvedEnv::unWarp(const cv::Rect_<_Tp>& rect) const {
    std::vector<cv::Point_<_Tp>> in {rect.tl(), cv::Point_<_Tp>(rect.x+rect.width, rect.y), rect.br(), cv::Point_<_Tp>(rect.x, rect.y+rect.height)};
    std::vector<cv::Point_<_Tp>> out;
    cv::perspectiveTransform(in, out, unWarpMatrix);
    return {out[0], out[1], out[2], out[3]};
}

template cv::Point2f ResolvedEnv::unWarp(const cv::Point2f point) const;
template cv::Point2d ResolvedEnv::unWarp(const cv::Point2d point) const;
template std::array<cv::Point2f,4> ResolvedEnv::unWarp(const cv::Rect2f& point) const;
template std::array<cv::Point2d,4> ResolvedEnv::unWarp(const cv::Rect2d& point) const;

template<>
cv::Point2i ResolvedEnv::unWarp(const cv::Point2i point) const {
    std::vector<cv::Point2f> in {point};
    std::vector<cv::Point2f> out;
    cv::perspectiveTransform(in, out, unWarpMatrix);
    return out[0];
}
template<>
std::array<cv::Point2i,4> ResolvedEnv::unWarp(const cv::Rect2i& rect) const {
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
