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
    std::vector<std::unique_ptr<Detector>> detectors;
};

}

#endif //EDROBOT_NAVPANEL_H
