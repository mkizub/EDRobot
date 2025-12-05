//
// Created by mkizub on 17.08.2025.
//

#pragma once

#ifndef EDROBOT_NAVPANEL_H
#define EDROBOT_NAVPANEL_H

#include "Detector.h"

namespace detect {

class NavPanelDetector : public Detector {
public:
    static void standaloneTest(std::string image, std::string screen_name);

    struct Tab {
        cv::Rect rect;
        std::string name;
    };

    NavPanelDetector(std::string panel_name,
            std::vector<std::unique_ptr<LineDetector>>&& lines,
            std::vector<std::unique_ptr<AnchorDetector>>&& anchors,
            std::vector<Tab>&& tabs);
    ~NavPanelDetector() override = default;

    double match(ClassifyEnv& env) override;
    bool match_flt_line(const char* lineName, cv::Line& detectedLine, ClassifyEnv& env, float roughAngle, XMat& roughImage, AnchorDetector *lan, cv::Mat& debugRough);
    double match_dialog(ClassifyEnv& env, float roughAngle, XMat& roughImage, AnchorDetector *lan, cv::Mat& debugImage);
    void approximate_bottom_line(ClassifyEnv& env);

    LineDetector* getLineDetector(const char* name);
    AnchorDetector* getAnchorDetector(const char* name);
    const Tab* getTab(const char* name);
    ConstTransform* getTransform();

    cv::RotatedRect lastRotRect;
    cv::Line lastTopLine;
    cv::Line lastBottomLine;
    const Tab* lastSelectedTab {};
private:
    friend struct NavPanelDetectLock;
    static std::string forceDetect;

    const std::string mPanelName;
    std::vector<std::unique_ptr<LineDetector>> mLines;
    std::vector<std::unique_ptr<AnchorDetector>> mAnchors;
    std::vector<Tab> mTabs;

    float deltaScale;
    float deltaAngle;
    cv::Point2f topLeftOffset;
    cv::Line topRefLine;
    cv::Mat roughAffineMatrix;
};

struct NavPanelDetectLock {
    NavPanelDetectLock(std::string force) {
        NavPanelDetector::forceDetect = force;
    }
    ~NavPanelDetectLock() {
        NavPanelDetector::forceDetect.clear();
    }
};

}

#endif //EDROBOT_NAVPANEL_H
