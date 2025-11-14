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
    NavPanelDetector(std::vector<std::unique_ptr<Detector>>&& detectors);
    ~NavPanelDetector() override = default;

    double match(ClassifyEnv& env) override;

private:
    friend struct NavPanelDetectLock;
    static std::string forceDetect;

    std::vector<std::unique_ptr<Detector>> detectors;
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
