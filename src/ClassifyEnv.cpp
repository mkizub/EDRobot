//
// Created by mkizub on 16.06.2025.
//

#include "pch.h"

#include "ClassifyEnv.h"

void ClassifyEnv::init(const cv::Rect& monRect, const cv::Rect& captRect, upFrame&& frame) {
    const_cast<cv::Rect&>(monitorRect) = monRect;
    const_cast<cv::Rect&>(captureRect) = captRect;
    const_cast<cv::Rect&>(captureCrop) = cv::Rect(cv::Point(), captureRect.size());
    assert (frame);
    assert (captureRect.size() == frame->size);
    mFrame.swap(frame);
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
    mFrame.reset();
    mDebugOverlay = cv::Mat();
    needScaling_ = false;
    scaleToCaptured_ = 1;
    captureCenter = ReferenceScreenCenter;
    classified.clear();
}

cv::Mat& ClassifyEnv::getDebugImage() const {
    if (!mDebugOverlay.empty())
        return mDebugOverlay;
    mDebugOverlay = cv::Mat(captureRect.size(), CV_8UC4, cv::Vec4b::zeros());
    return mDebugOverlay;
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
cv::Rect ClassifyEnv::cvtCapturedToReference(const cv::Rect& rect) const {
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
