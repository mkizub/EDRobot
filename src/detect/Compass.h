//
// Created by mkizub on 23.12.2025.
//

#pragma once

#ifndef EDROBOT_COMPASS_H
#define EDROBOT_COMPASS_H

namespace detect {

class CompassDetector : public Detector {
public:
    CompassDetector();
    ~CompassDetector() override = default;
    void clear() {
        lastHemisphere = 0;
        lastTgtPitch = 0;
        lastTgtYaw = 0;
        lastTgtRoll = 0;
        lastTgtAngle = 0;
        dotCaptureRect = {};
        dotSpherePosition = {};
        navTargetFound = false;
        lastNavTargetOffset = {};
        lastNavDist = {};
    }

    void loadCompass();

    double match(ClassifyEnv& env) override;

    cv::Rect targetReferenceRect;
    cv::Rect targetRemapRect;

    cv::Size compassRefSize;
    std::string compassImageName;

    std::unique_ptr<ImageTemplate>  compassDetector;

    std::vector<std::unique_ptr<ImageFilter>> dotsFilters;
    std::vector<std::unique_ptr<ImageFilter>> navTargetFilters;
    std::vector<std::unique_ptr<ImageFilter>> distOCRFilters;
    std::vector<ImageTemplate::ImageMatrix> compassDotsOrig;
    std::vector<ImageTemplate::ImageMatrix> compassDotsPrepared;
    std::vector<ImageTemplate::ImageMatrix> navTargetOrig;
    std::vector<ImageTemplate::ImageMatrix> navTargetPrepared;
    int navTargetReferenceRadius;
    cv::Mat navTargetRemapXY;
    XMat navTargetRemap1;
    XMat navTargetRemap2;

    double preprocessedDotsScale = 0;
    double preprocessedFOV = 0; // config fov
    double captureFovX = 0;
    double captureFovY = 0;
    std::string preprocessedShip;
    std::vector<double> navTargetScales;

    const double threshold_dot;

    int8_t lastHemisphere; // -1: back, 0: not detected, +1: front
    double lastTgtPitch;
    double lastTgtYaw;
    double lastTgtRoll;
    double lastTgtAngle;

    cv::Rect dotCaptureRect;
    cv::Point2d dotSpherePosition;

    bool navTargetFound;
    cv::Point lastNavTargetOffset;
    dist_t lastNavDist;

    static void tryLowerUpperBoundsGUI(ClassifyEnv &env, cv::Rect referenceRect);
};

} // namespace detect

#endif //EDROBOT_COMPASS_H
