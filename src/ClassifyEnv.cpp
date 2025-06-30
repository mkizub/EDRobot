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
    needScaling_ = false;
    scaleToCaptured_ = 1;
    captureCenter = ReferenceScreenCenter;
    classified.clear();
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
    mWarpTransform = transform;
    if (mWarpTransform && mWarpTransform->calcTransform(*this)) {
        mWarpedColorImage = mWarpTransform->transformImage(getColorImage());
        mWarpedGrayImage = mWarpTransform->transformImage(getGrayImage());
    }
}

cv::Point ResolvedEnv::cvtReferenceToDesktop(const cv::Point& point) const {
    return monitorRect.tl() + captureRect.tl() + cvtReferenceToCaptured(point);
}

cv::Point ResolvedEnv::cvtReferenceToCaptured(const cv::Point& point) const {
    cv::Point screenPoint(point);
    if (needScaling_) {
        cv::Point relative = screenPoint - ReferenceScreenCenter;
        relative *= scaleToCaptured_;
        screenPoint = relative + captureCenter;
    }
    return screenPoint;
}
cv::Rect  ResolvedEnv::cvtReferenceToCaptured(const cv::Rect& rect) const {
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

cv::Point ResolvedEnv::cvtCapturedToReference(const cv::Point& point) const {
    cv::Point referencePoint(point);
    if (needScaling_) {
        cv::Point lt_rel = referencePoint - captureCenter;
        lt_rel /= scaleToCaptured_;
        cv::Point lt = lt_rel + ReferenceScreenCenter;
        referencePoint = lt;
    }
    return referencePoint;
}
cv::Rect ResolvedEnv::cvtCapturedToReference(const cv::Rect& rect) const {
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
    float t_w = (float)cv::norm(src_tl - src_tr);
    float b_w = (float)cv::norm(src_bl - src_br);
    float l_h = (float)cv::norm(src_tl - src_bl);
    float r_h = (float)cv::norm(src_tr - src_br);
    float d_w = (t_w + b_w) * 0.5f;
    float d_h = (l_h + r_h) * 0.5f;
    d_w = std::round(d_w);
    d_h = std::round(d_h);
    transformedSize = { int(d_w), int(d_h) };
    cv::Point2f dst_tl = {0.f,0.f};
    cv::Point2f dst_tr = {d_w,0.f};
    cv::Point2f dst_br = {d_w,d_h};
    cv::Point2f dst_bl = {0.f,d_h};

    transfromSrc[0] = src_tl;
    transfromSrc[1] = src_tr;
    transfromSrc[2] = src_br;
    transfromSrc[3] = src_bl;
    transfromDst[0] = dst_tl;
    transfromDst[1] = dst_tr;
    transfromDst[2] = dst_br;
    transfromDst[3] = dst_bl;

    transfromMatrix = cv::getPerspectiveTransform(transfromSrc, transfromDst);
}

cv::Mat EvalTransform::transformImage(const cv::Mat& image) const {
    assert (!transformedSize.empty());
    cv::Mat warped_image;
    cv::warpPerspective(image, warped_image, transfromMatrix, transformedSize);
    //if (image.channels() == 1)
    //    cv::imwrite("warped-screen-gray.png", warped_image);
    //else
    //    cv::imwrite("warped-screen-color.png", warped_image);
    return warped_image;
}

bool ConstTransform::calcTransform(const ResolvedEnv& env) {
    return !transformedSize.empty();
}

LineTransform::LineTransform(std::string line, cv::Point src_tl, cv::Point src_tr, cv::Point src_br, cv::Point src_bl)
    : EvalTransform(src_tl, src_tr, src_br, src_bl)
    , lineDetector(std::move(line))
{
}

bool LineTransform::calcTransform(const ResolvedEnv& env) {
    const ClassifiedRect* crld = nullptr;
    for (auto& cr : env.classified) {
        if (cr.cdt == ClsDetType::LineDetected && cr.text == lineDetector) {
            crld = &cr;
            break;
        }
    }
    if (!crld) {
        transformedSize = {};
        return false;
    }
    float cos_a = (float)std::cos(crld->u.ldet.angle * M_PI / 180);
    float sin_a = (float)std::sin(crld->u.ldet.angle * M_PI / 180);
    float x0 = crld->u.ldet.offset.x + orig_tl.x * cos_a - orig_tl.y * sin_a;
    float y0 = crld->u.ldet.offset.y + orig_tl.x * sin_a + orig_tl.y * cos_a;
    float x1 = crld->u.ldet.offset.x + orig_tr.x * cos_a - orig_tr.y * sin_a;
    float y1 = crld->u.ldet.offset.y + orig_tr.x * sin_a + orig_tr.y * cos_a;
    cv::Point2f src_tl = {x0, y0};
    cv::Point2f src_tr = {x1, y1};

    float x2 = crld->u.ldet.offset.x + orig_bl.x * cos_a - orig_bl.y * sin_a;
    float y2 = crld->u.ldet.offset.y + orig_bl.x * sin_a + orig_bl.y * cos_a;
    float x3 = crld->u.ldet.offset.x + orig_br.x * cos_a - orig_br.y * sin_a;
    float y3 = crld->u.ldet.offset.y + orig_br.x * sin_a + orig_br.y * cos_a;
    cv::Point2f src_bl = {x2, y2};
    cv::Point2f src_br = {x3, y3};

    std::vector<cv::Point2f> orig {orig_tl, orig_tr, orig_br, orig_bl};
    std::vector<cv::Point2f> src;
    cv::Mat affine_matrix = (cv::Mat_<float>(2,3) << cos_a, -sin_a, crld->u.ldet.offset.x, sin_a, cos_a, crld->u.ldet.offset.y);
    cv::transform(orig, src, affine_matrix);

    float width = std::round((float)cv::norm(src_tl - src_tr));
    float height = std::round((float)cv::norm(src_tl - src_bl));
    transformedSize = { int(width), int(height) };

    transfromSrc[0] = src[0]; //src_tl;
    transfromSrc[1] = src[1]; //src_tr;
    transfromSrc[2] = src[2]; //src_br;
    transfromSrc[3] = src[3]; //src_bl;
    transfromDst[0] = {0.f,   0.f};
    transfromDst[1] = {width, 0.f};
    transfromDst[2] = {width, height};
    transfromDst[3] = {0.f,   height};

    transfromMatrix = cv::getPerspectiveTransform(transfromSrc, transfromDst);
    return true;
}
