//
// Created by mkizub on 26.12.2025.
//

#include "../pch.h"

#include "EDWidget.h"
#include "Button.h"

namespace widget {

bool BaseButton::detect(DetectParams &params) {
    ClassifyEnv &env = params.env;
    cv::Rect expectedR = env.calcReferenceRect(this->rect);
    if (expectedR.empty())
        return false;
    cv::Rect detectedR = expectedR;
    cv::Rect captureR = env.cvtReferenceToCaptured(expectedR);

    if (!icon.empty()) {
        if (!detector) {
            detector = std::make_unique<detect::ImageTemplate>(icon, nullptr);
            detector->threshold_min = 0.4;
            detector->threshold_max = 0.8;
            detector->extendLT = extendLT;
            detector->extendRB = extendRB;
            std::swap(detector->filters, this->filters);
        }
        // assume icon is at the center of this bgutton
        auto &orig = detector->refOrig;
        auto &size = detector->refSize;
        orig.x = expectedR.x + expectedR.width / 2 - size.width / 2;
        orig.y = expectedR.y + expectedR.height / 2 - size.height / 2;
        if (detector->match(params.env) >= 0.5) {
            int cx = detector->captureRect.x + detector->captureRect.width / 2;
            int cy = detector->captureRect.y + detector->captureRect.height / 2;
            captureR.x = cx - captureR.width / 2;
            captureR.y = cy - captureR.height / 2;
            detectedR = env.cvtCapturedToReference(captureR);
        }
        else if (!force_present)
            return false;
    } else if (extendLT != cv::Point() || extendRB != cv::Point()) {
        cv::Rect extendR = {expectedR.tl() - extendLT, expectedR.br() + extendRB};
        cv::Rect matchR = env.cvtReferenceToCaptured(extendR);

        if (filters.empty()) {
            filters.emplace_back(new detect::ChannelFilter(detect::ChannelFilter::gray));
            filters.emplace_back(new detect::LaplacianFilter(1));
            filters.emplace_back(new detect::ThresholdFilter(5));
        }
        XMat buttonImage = env.getColorImage()(matchR);
        XMat thrImage = detect::ImageTemplate::applyFilters(filters, buttonImage, {});

        bool detected = false;
        std::vector<std::vector<cv::Point>> contours;
        cv::findContoursLinkRuns(thrImage, contours);
        for (auto &cont: contours) {
            std::vector<cv::Point> convex;
            cv::convexHull(cont, convex);
            if (convex.size() >= 4) {
                std::vector<cv::Point> approx;
                cv::approxPolyN(convex, approx, 4, 5, true);
                cv::Rect bbox = cv::boundingRect(approx);
                bbox &= cv::Rect(cv::Point(), matchR.size());
                if ((bbox.width + 2) > captureR.width * 0.95 && (bbox.height + 2) > captureR.height * 0.95 &&
                    (bbox.width - 4) < captureR.width * 1.08 && (bbox.height - 4) < captureR.height * 1.08) {
                    captureR = {matchR.tl() + bbox.tl(), bbox.size()};
                    detectedR = env.cvtCapturedToReference(captureR);
                    detected = true;
                    break;
                }
            }
        }
        if (!detected && !force_present)
            return false;
    }

    env.classified.emplace_back(ClsDetType::Widget, this->name, detectedR);
    ClassifiedRect &clsBtnRect = env.classified.back();
    clsBtnRect.u.widg.referenceRect = expectedR;
    clsBtnRect.u.widg.ws = WState::Unknown;
    clsBtnRect.u.widg.widget = this;

    detect::Histogram histDet(detect::Histogram::Mode::Hsv);
    XMat detectImage = env.getColorImage()(captureR);
    if (!histDet.calc(detectImage))
        return false;
    WState ws = WState::Unknown;
    if (histDet.mLastColor[2] > 10) { // not black
        if (histDet.mLastColor[1] < 80) // desaturated = disabled
            ws = WState::Disabled;
        else if (histDet.mLastColor[0] < 30) {// hue is near red = known color
            if (histDet.mLastColor[2] > 180) // bright = focused
                ws = WState::Focused;
            else
                ws = WState::Normal;
        }
    }
    clsBtnRect.u.widg.ws = ws;
    LOG_IF(ws == WState::Focused, INFO) << "Focused: " << this->path;
    LOG_IF(ws == WState::Disabled, INFO) << "Disabld: " << this->path;
    if (ws == WState::Focused && !params.uiState.focused)
        params.uiState.focused = this;

    return true;
}

} // namespace widget
