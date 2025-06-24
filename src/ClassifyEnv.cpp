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
    ResolvedEnv::init(monRect, captRect);
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
}

cv::Mat& ClassifyEnv::getDebugImage() const {
    if (!mDebugOverlay.empty())
        return mDebugOverlay;
    mDebugOverlay = cv::Mat(captureRect.size(), CV_8UC4, cv::Vec4b::zeros());
    return mDebugOverlay;
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
