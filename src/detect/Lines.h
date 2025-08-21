//
// Created by mkizub on 17.08.2025.
//

#pragma once

#ifndef EDROBOT_LINES_H
#define EDROBOT_LINES_H

#include "Detector.h"

namespace detect {

class LineDetector : public Detector {
public:
    LineDetector(spEvalLine line);
    ~LineDetector() override = default;

    double match(ClassifyEnv& env) override = 0;

    std::vector<std::unique_ptr<ImageFilter>> filters;

    std::string name;
    float extendAngleMin;
    float extendAngleMax;
    float angleStep; // angle step in degrees
    int houghThreshold;
    const spEvalLine referenceLine;

    cv::Line expectedLine;
    cv::Line detectedLine;
    cv::Rect lineMatchRect;
    float lastLineAngle;  // in degrees, -90 <= angle <= +90
    float lastDeltaAngle;  // in degrees
    float lastDeltaScale;

};

class AnchoredLineDetector : public LineDetector {
public:

    AnchoredLineDetector(ImageTemplate* anchor, spEvalLine line);
    ~AnchoredLineDetector() override = default;

    double match(ClassifyEnv& env) override;

    std::unique_ptr<ImageTemplate> anchorDetector;
    cv::Point2f captureAnchor;
};

class SimpleLineDetector : public LineDetector {
public:

    SimpleLineDetector(spEvalLine line);
    ~SimpleLineDetector() override = default;

    double match(ClassifyEnv& env) override;

    cv::Point extendLT;
    cv::Point extendRB;
    spEvalLine altReferenceLine;
};

}

#endif //EDROBOT_LINES_H
