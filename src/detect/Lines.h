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

    double match(ClassifyEnv& env);

    std::vector<std::unique_ptr<ImageFilter>> filters;

    std::string name;
    float extendAngleMin {10};
    float extendAngleMax {10};
    float angleStep {1}; // angle step in degrees
    int houghThreshold {0};
    int detectEdgesMode {0}; // +1 for top line, -1 for bottom line, 0 to not detect
    const spEvalLine referenceLine;
    cv::Line withRefLine;

    cv::Point extendLT;
    cv::Point extendRB;

    struct DetectedLine {
        float rho;
        float angle; // in degrees
        float dist_to_center;
        int count;
        cv::Line2d line;
    };
    cv::Line expectedLine;
    cv::Line detectedLine;
    std::vector<DetectedLine> detectedLines;
    cv::Rect lineMatchRect;
    float lastAvrgAngle;  // in degrees, -90 <= angle <= +90
    float lastDeltaAngle;  // in degrees
    float lastDeltaScale;

};

}

#endif //EDROBOT_LINES_H
