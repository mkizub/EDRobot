//
// Created by mkizub on 17.08.2025.
//

#include "../pch.h"

#include "Detector.h"
#include "Lines.h"
#include "NavPanel.h"
#include "../EDWidget.h"

namespace detect {

NavPanelDetector::NavPanelDetector(std::vector<std::unique_ptr<Detector>>&& sub_detectors)
    : detectors(std::move(sub_detectors))
{

}

double NavPanelDetector::match(ClassifyEnv &env) {
    AnchoredLineDetector* lpline = dynamic_cast<AnchoredLineDetector*>(detectors.at(0).get());
    SimpleLineDetector* btline = dynamic_cast<SimpleLineDetector*>(detectors.at(1).get());
    assert (lpline && lpline->name == "lpline");
    assert (btline && btline->name == "btline");

    const widget::Screen* screen = (const widget::Screen*)Master::getInstance().getCfgItem("scr-left-panel");
    ConstTransform* transform = dynamic_cast<ConstTransform*>(screen->transform.get());
    if (!transform)
        return 0;
    transform->valid = false;
    transform->useCaptured = true;

    double lineMatch = lpline->match(env);
    if (lineMatch > 0) {
        cv::Line btlineSrc = btline->referenceLine->calcReferenceLine(env);
        // rotate/scale around anchor top-left and top-right transform points and expect bottom line
        {
            cv::Point2f detectedAnchor = lpline->captureAnchor;
            cv::Matx23d affineMatrix = cv::getRotationMatrix2D_(detectedAnchor,
                                                                -lpline->lastDeltaAngle, lpline->lastDeltaScale);
            cv::Point offset = env.scaleToReference(lpline->anchorDetector->matchedCaptureOffset);
            affineMatrix(0,2) += offset.x * affineMatrix(0,0) + offset.y * affineMatrix(0,1);
            affineMatrix(1,2) += offset.x * affineMatrix(1,0) + offset.y * affineMatrix(1,1);
            std::array<cv::Point2f, 6> transformSrc;
            for (int i = 0; i < 4; i++)
                transformSrc[i] = env.cvtReferenceToCaptured(transform->orig[i]->calcReferencePoint(env));
            transformSrc[4] = env.cvtReferenceToCaptured(btlineSrc.p0());
            transformSrc[5] = env.cvtReferenceToCaptured(btlineSrc.p1());
            cv::transform(transformSrc, transformSrc, affineMatrix);
            for (int i = 0; i < 4; i++)
                transform->transformSrc[i] = env.cvtReferenceToCaptured(transformSrc[i]);
            transform->valid = true;

            // try to find bottom line
            btline->extendLT = {20, 20};
            btline->extendRB = {50, 50};
            cv::Line alt{transformSrc[4], transformSrc[5]};
            btline->altReferenceLine.reset(new ConstLine(alt));
        }

        lineMatch = btline->match(env);
        if (lineMatch > 0) {
            double scale = 1;
            if (std::abs(1 - btline->lastDeltaScale) < 0.05)
                scale = btline->lastDeltaScale;
            cv::Matx23d affineMatrix = cv::getRotationMatrix2D_(btline->expectedLine.p0(),
                                                                -btline->lastDeltaAngle, scale);
            cv::Point offset = btline->detectedLine.p0() - btline->expectedLine.p0();
            affineMatrix(0,2) += offset.x * affineMatrix(0,0) + offset.y * affineMatrix(0,1);
            affineMatrix(1,2) += offset.x * affineMatrix(1,0) + offset.y * affineMatrix(1,1);
            // transform bottom points
            std::array<cv::Point2f, 2> transformSrc;
            std::array<cv::Point2f, 2> transformXxx;
            transformSrc[0] = env.cvtReferenceToCaptured(transform->transformSrc[2]);
            transformSrc[1] = env.cvtReferenceToCaptured(transform->transformSrc[3]);
            cv::transform(transformSrc, transformXxx, affineMatrix);
            transform->transformSrc[2] = transformXxx[0];
            transform->transformSrc[3] = transformXxx[1];
        }

        return 1;
    }

    for (int l=2; l < detectors.size(); l++) {
        lineMatch = detectors[l]->match(env);
        if (lineMatch > 0.5)
            return 1;
    }

    return 0;
}

} // namespace detect
