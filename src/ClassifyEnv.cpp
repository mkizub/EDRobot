//
// Created by mkizub on 16.06.2025.
//

#include "pch.h"

#include "ClassifyEnv.h"

ResolvedEnv& ResolvedEnv::operator=(const ResolvedEnv& other) {
    const_cast<cv::Rect&>(monitorRect) = other.monitorRect;
    const_cast<cv::Rect&>(captureRect) = other.captureRect;
    const_cast<cv::Rect&>(captureCrop) = other.captureCrop;
    classified = other.classified;
    inWarpMode_ = false;
    needScaling_ = other.needScaling_;
    scaleToCaptured_ = other.scaleToCaptured_;
    captureCenter = other.captureCenter;
    return *this;
}

void ResolvedEnv::init(const cv::Rect& monRect, const cv::Rect& captRect) {
    const_cast<cv::Rect&>(monitorRect) = monRect;
    const_cast<cv::Rect&>(captureRect) = captRect;
    const_cast<cv::Rect&>(captureCrop) = cv::Rect(cv::Point(), captureRect.size());
    classified.clear();
    inWarpMode_ = false;
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
    warpRect = {};
    warpMatrix = cv::Matx33d::eye();
    unWarpMatrix = cv::Matx33d::eye();
}

class FrameTmp : public Frame {
public:
    FrameTmp(cv::Size size, cv::Mat* cImage, cv::Mat* gImage)
        : Frame(nullptr, size)
    {
        if (cImage) {
            colorImage = *cImage;
            colorImageValid = true;
        }
        if (gImage) {
            grayImage = *gImage;
            grayImageValid = true;
        }
    }
    ~FrameTmp() override = default;
    bool valid() const override { return true; }
    const cv::UMat& getColorTexture() const override { return colorTexture; }
    const cv::Mat& getColorImage() const override { return colorImage; }
    const cv::Mat& getGrayImage() const override { return grayImage; }

    void cleanup();

    mutable cv::UMat colorTexture;
    mutable cv::Mat colorImage;
    mutable cv::Mat grayImage;
    mutable bool colorTextureValid {false};
    mutable bool colorImageValid {false};
    mutable bool grayImageValid {false};
};

void ClassifyEnv::init(const ResolvedEnv& rEnv, cv::Mat* colorImage, cv::Mat* grayImage) {
    cv::Size sz;
    if (colorImage)
        sz = {colorImage->cols, colorImage->rows};
    else if (grayImage)
        sz = {grayImage->cols, grayImage->rows};
    else
        throw std::runtime_error("Both color and gray image empty");
    upFrame fr(new FrameTmp(sz, colorImage, grayImage));
    mFrame.swap(fr);
    mDebugOverlay = cv::Mat();
    ResolvedEnv::init(rEnv.monitorRect, rEnv.captureRect);
    mWarpTransform.reset();
    mWarpedColorImage = cv::Mat();
    mWarpedGrayImage = cv::Mat();
}

void ClassifyEnv::init(const cv::Rect& monRect, const cv::Rect& captRect, upFrame&& frame) {
    assert (frame);
    assert (captRect.size() == frame->size);
    mFrame.swap(frame);
    mDebugOverlay = cv::Mat();
    ResolvedEnv::init(monRect, captRect);
    mWarpTransform.reset();
    mWarpedColorImage = cv::Mat();
    mWarpedGrayImage = cv::Mat();
}

void ResolvedEnv::clear() {
    const_cast<cv::Rect&>(monitorRect) = cv::Rect();
    const_cast<cv::Rect&>(captureRect) = cv::Rect();
    const_cast<cv::Rect&>(captureCrop) = cv::Rect();
    inWarpMode_ = false;
    needScaling_ = false;
    scaleToCaptured_ = 1;
    captureCenter = ReferenceScreenCenter;
    classified.clear();
    warpRect = {};
    warpMatrix = cv::Matx33d::eye();
    unWarpMatrix = cv::Matx33d::eye();
}

void ClassifyEnv::clear() {
    ResolvedEnv::clear();
    mFrame.reset();
    mDebugOverlay = cv::Mat();
    mWarpTransform.reset();
    mWarpedColorImage = cv::Mat();
    mWarpedGrayImage = cv::Mat();
}

cv::Mat& ClassifyEnv::getDebugImage() const {
    if (!mDebugOverlay.empty())
        return mDebugOverlay;
    mDebugOverlay = cv::Mat(captureRect.size(), CV_8UC4, cv::Vec4b::zeros());
    return mDebugOverlay;
}

const cv::Mat& ClassifyEnv::getWarpedColorImage() const {
    return mWarpedColorImage;
}
const cv::Mat& ClassifyEnv::getWarpedGrayImage() const {
    return mWarpedGrayImage;
}

void ClassifyEnv::warpPerspective(const spEvalTransform& transform) {
    if (mWarpTransform)
        LOG(WARNING) << "Duplicated warping";
    mWarpTransform = transform;
    if (mWarpTransform && mWarpTransform->calcTransform(*this)) {
        warpRect = cv::Rect(cv::Point(0,0), mWarpTransform->origSize);
        warpMatrix = mWarpTransform->transfromMatrix;
        unWarpMatrix = warpMatrix.inv();
        mWarpedColorImage = mWarpTransform->transformImage(getColorImage());
        mWarpedGrayImage = mWarpTransform->transformImage(getGrayImage());
    }
}

cv::Point ResolvedEnv::cvtReferenceToDesktop(const cv::Point& point) const {
    return monitorRect.tl() + captureRect.tl() + cvtReferenceToCaptured(point);
}

cv::Point2f ResolvedEnv::cvtReferenceToCaptured(const cv::Point2f& point) const {
    if (!needScaling_ || inWarpMode_)
        return point;
    cv::Point2f screenPoint(point);
    cv::Point2f relative = screenPoint - cv::Point2f(ReferenceScreenCenter);
    relative *= scaleToCaptured_;
    screenPoint = relative + cv::Point2f(captureCenter);
    return screenPoint;
}
cv::Point ResolvedEnv::cvtReferenceToCaptured(const cv::Point& point) const {
    if (!needScaling_ || inWarpMode_)
        return point;
    cv::Point screenPoint(point);
    cv::Point relative = screenPoint - ReferenceScreenCenter;
    relative *= scaleToCaptured_;
    screenPoint = relative + captureCenter;
    return screenPoint;
}
cv::Rect  ResolvedEnv::cvtReferenceToCaptured(const cv::Rect& rect) const {
    if (!needScaling_ || inWarpMode_)
        return rect;
    cv::Rect screenRect(rect);
    cv::Point lt_rel = screenRect.tl() - ReferenceScreenCenter;
    cv::Point rb_rel = screenRect.br() - ReferenceScreenCenter;
    lt_rel *= scaleToCaptured_;
    rb_rel *= scaleToCaptured_;
    cv::Point lt = lt_rel + captureCenter;
    cv::Point rb = rb_rel + captureCenter;
    screenRect = cv::Rect(lt, rb);
    return screenRect;
}

cv::Point2f ResolvedEnv::cvtCapturedToReference(const cv::Point2f& point) const {
    if (!needScaling_ || inWarpMode_)
        return point;
    cv::Point2f referencePoint(point);
    cv::Point2f lt_rel = referencePoint - cv::Point2f(captureCenter);
    lt_rel /= scaleToCaptured_;
    cv::Point2f lt = lt_rel + cv::Point2f(ReferenceScreenCenter);
    referencePoint = lt;
    return referencePoint;
}
cv::Point ResolvedEnv::cvtCapturedToReference(const cv::Point& point) const {
    if (!needScaling_ || inWarpMode_)
        return point;
    cv::Point referencePoint(point);
    cv::Point lt_rel = referencePoint - captureCenter;
    lt_rel /= scaleToCaptured_;
    cv::Point lt = lt_rel + ReferenceScreenCenter;
    referencePoint = lt;
    return referencePoint;
}
cv::Rect ResolvedEnv::cvtCapturedToReference(const cv::Rect& rect) const {
    if (!needScaling_ || inWarpMode_)
        return rect;
    cv::Rect referenceRect(rect);
    cv::Point lt_rel = referenceRect.tl() - captureCenter;
    cv::Point rb_rel = referenceRect.br() - captureCenter;
    lt_rel /= scaleToCaptured_;
    rb_rel /= scaleToCaptured_;
    cv::Point lt = lt_rel + ReferenceScreenCenter;
    cv::Point rb = rb_rel + ReferenceScreenCenter;
    referenceRect = cv::Rect(lt,rb);
    return referenceRect;
}

cv::Point2f ResolvedEnv::unWarp(const cv::Point2f point) const {
    std::vector<cv::Point2f> in {point};
    std::vector<cv::Point2f> out;
    cv::perspectiveTransform(in, out, unWarpMatrix);
    //auto& m = warpMatrix.val;
    //out.x = (m[0] * point.x + m[1] * point.y + m[2]) / (m[6] * point.x + m[7] * point.y + m[8]);
    //out.y = (m[3] * point.x + m[4] * point.y + m[5]) / (m[6] * point.x + m[7] * point.y + m[8]);
    //out.x = (warpMatrix(0,0) * point.x + warpMatrix(0,1) * point.y + warpMatrix(0,2)) / (warpMatrix(2,0) * point.x + warpMatrix(2,1) * point.y + warpMatrix(2,2));
    //out.y = (warpMatrix(1,0) * point.x + warpMatrix(1,1) * point.y + warpMatrix(1,2)) / (warpMatrix(2,0) * point.x + warpMatrix(2,1) * point.y + warpMatrix(2,2));
    return out[0];
}
cv::Point ResolvedEnv::unWarp(const cv::Point point) const {
    cv::Point2f tmp = unWarp(cv::Point2f(point));
    return {(int)std::round(tmp.x), (int)std::round(tmp.y)};
}
std::array<cv::Point,4> ResolvedEnv::unWarp(const cv::Rect& rect) const {
    std::array<cv::Point,4> out;
    out[0] = unWarp(rect.tl());
    out[1] = unWarp(cv::Point(rect.x+rect.width, rect.y));
    out[2] = unWarp(rect.br());
    out[3] = unWarp(cv::Point(rect.x, rect.y+rect.height));
    return out;
}

cv::Rect TileRect::calcReferenceRect(const ResolvedEnv& env) const {
    for (auto& cr : env.classified) {
        if (cr.cdt != ClsDetType::Tile)
            continue;
        if (!name.empty()) {
            if (cr.text.starts_with(name))
                return cr.detectedRect;
        }
        if (col >= 0 && row >= 0) {
            if (col == cr.u.tile.col && row == cr.u.tile.row)
                return cr.detectedRect;
        }
    }
    return {};
}

ConstTransform::ConstTransform(cv::Point src_tl, cv::Point src_tr, cv::Point src_br, cv::Point src_bl)
    : EvalTransform(src_tl, src_tr, src_br, src_bl)
{
}

cv::Mat EvalTransform::transformImage(const cv::Mat& image) const {
    assert (valid);
    cv::Mat warped_image;
    cv::warpPerspective(image, warped_image, transfromMatrix, origSize);
    return warped_image;
}

bool ConstTransform::calcTransform(const ResolvedEnv& env) {
    for (int i=0; i < 4; i++)
        transfromSrc[i] = env.cvtReferenceToCaptured(orig[i]);

    std::array<cv::Point2f,4> dst;
    dst[0] = {0.f,0.f};
    dst[1] = {(float)origSize.width,0.f};
    dst[2] = {(float)origSize.width,(float)origSize.height};
    dst[3] = {0.f,(float)origSize.height};
    transfromMatrix = cv::getPerspectiveTransform(transfromSrc, dst);

    valid = true;
    return true;
}

LineTransform::LineTransform(std::string line, cv::Point src_tl, cv::Point src_tr, cv::Point src_br, cv::Point src_bl)
    : EvalTransform(src_tl, src_tr, src_br, src_bl)
    , lineDetector(std::move(line))
{
}

bool LineTransform::calcTransform(const ResolvedEnv& env) {
    valid = false;
    const ClassifiedRect* crld = nullptr;
    for (auto& cr : env.classified) {
        if (cr.cdt == ClsDetType::LineDetected && cr.text == lineDetector) {
            crld = &cr;
            break;
        }
    }
    if (!crld) {
        valid = false;
        return false;
    }
    float cos_a = (float)std::cos(crld->u.ldet.angle * M_PI / 180);
    float sin_a = (float)std::sin(crld->u.ldet.angle * M_PI / 180);
    std::array<cv::Point2f,4> capt_orig;
    for (int i=0; i < 4; i++)
        capt_orig[i] = env.cvtReferenceToCaptured(orig[i]);
    cv::Point offset = env.scaleToCaptured(crld->u.ldet.offset);
    cv::Mat affine_matrix = (cv::Mat_<float>(2,3) << cos_a, -sin_a, offset.x, sin_a, cos_a, offset.y);
    cv::transform(capt_orig, transfromSrc, affine_matrix);

    std::array<cv::Point2f,4> dst;
    dst[0] = {0.f,0.f};
    dst[1] = {(float)origSize.width,0.f};
    dst[2] = {(float)origSize.width,(float)origSize.height};
    dst[3] = {0.f,(float)origSize.height};

    transfromMatrix = cv::getPerspectiveTransform(transfromSrc, dst);
    valid = true;
    return true;
}
