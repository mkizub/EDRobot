//
// Created by mkizub on 23.12.2025.
//

#pragma once

#ifndef EDROBOT_TEXTDETECTOR_H
#define EDROBOT_TEXTDETECTOR_H

#include "Detector.h"

namespace detect {

class TextDetector : public Detector {
public:
    TextDetector(const std::string& name, spEvalRect rect);
    ~TextDetector() override = default;

    double match(ClassifyEnv& env) override;
    double toResult(double matchValue); // something like logistic regression, S-curve

    const std::string name;
    spEvalRect refEvalRect;
    int mOcrConfThreshold = 50;
    double mThresholdMin = 0.5;
    double mThresholdMax = 0.9;
    std::optional<int> mLineHeight;
    std::optional<int> mOcrPSM; // see OCR.h
    std::map<std::string,std::vector<std::wstring>> labels;
};

} // namespace detect

#endif //EDROBOT_TEXTDETECTOR_H
