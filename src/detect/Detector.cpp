//
// Created by mkizub on 23.05.2025.
//

#include "../pch.h"

#include "Detector.h"

#include <iomanip>

namespace detect {

double Sequence::match(ClassifyEnv &env) {
    double sumWeights = 0;
    for (auto &oracle: oracles) {
        if (oracle->classifierWeight <= 0)
            continue;
        sumWeights += oracle->classifierWeight;
    }
    double sum = 0;
    for (auto &oracle: oracles) {
        if (oracle->classifierWeight <= 0)
            continue;
        double value = oracle->classify(env);
        double weight = oracle->classifierWeight / sumWeights;
        sum += weight * (2 * value - 1);
    }
    return (sum + 1) / 2;
}

double Sequence::classify(ClassifyEnv &env) {
    const auto sz = env.classified.size();
    double result = match(env);
    if (result < 0.5) {
        while (sz < env.classified.size())
            env.classified.pop_back();
    }
    return result;
}

double Sequence::debugMatch(ClassifyEnv &env) {
    json5pp::value j_arr = json5pp::array({});
    double sumWeights = 0;
    for (auto &oracle: oracles) {
        if (oracle->classifierWeight <= 0)
            continue;
        sumWeights += oracle->classifierWeight;
    }
    double sum = 0;
    for (auto &oracle: oracles) {
        if (oracle->classifierWeight <= 0)
            continue;
        double value = oracle->classify(env);
        j_arr.as_array().emplace_back(value);
        double weight = oracle->classifierWeight / sumWeights;
        sum += weight * (2 * value - 1);
    }
    double result = (sum + 1) / 2;
    LOG(INFO) << "match result: " << result << " for " << j_arr;
    return result;
}


double BestOf::match(ClassifyEnv &env) {
    int bestIdx = -1;
    double bestVal = 0;
    for (int i = 0; i < oracles.size(); i++) {
        double value = oracles[i]->classify(env);
        if (value > bestVal) {
            bestVal = value;
            bestIdx = i;
        }
    }
    return bestVal;
}

double BestOf::classify(ClassifyEnv &env) {
    const auto sz = env.classified.size();
    double result = match(env);
    if (result < 0.5) {
        while (sz < env.classified.size())
            env.classified.pop_back();
    }
    return result;
}

double BestOf::debugMatch(ClassifyEnv &env) {
    json5pp::value j_arr = json5pp::array({});
    int bestIdx = -1;
    double bestVal = 0;
    for (int i = 0; i < oracles.size(); i++) {
        double value = oracles[i]->classify(env);
        j_arr.as_array().emplace_back(value);
        if (value > bestVal) {
            bestVal = value;
            bestIdx = i;
        }
    }
    LOG(INFO) << "match result: " << bestVal << " index " << bestIdx << " between " << j_arr;
    return bestVal;
}


Histogram::Histogram(CompareMode mode, const cv::Rect &rect, const std::array<cv::Vec3b, 4> &colors)
        : mMode(mode), mRect(rect), mColors(colors) {
}

double gaussian(double x) {
    return exp(-x * x / 2) / (sqrt(2 * M_PI));
}

double xxx(double x, double downscale) {
    return gaussian(x / downscale) / gaussian(0);
}

double Histogram::match(ClassifyEnv &env) {
    cv::Rect rect = mRect;
    rect = env.cvtReferenceToCaptured(rect);
    env.cropToCapture(rect);
    if (rect.empty())
        return 0;
    int colorPlanes;
    std::vector<cv::Mat> imagePlanes;
    if (mMode == CompareMode::Gray) {
        colorPlanes = 1;
        imagePlanes.push_back(env.getGrayImage());
    } else {
        colorPlanes = 3;
        cv::split(env.getColorImage(), imagePlanes);
    }
    unsigned resultColor = 0;
    for (auto i = 0; i < colorPlanes; i++) {
        int histSize = 256;
        float range[]{0, 256}; //the upper boundary is exclusive
        const float *histRange[]{range};
        cv::Mat subImage(imagePlanes[i], rect);
        cv::Mat hist;
        cv::calcHist(&subImage, 1, nullptr, cv::Mat(), hist, 1, &histSize, histRange);
        int maxLoc[4]{};
        cv::minMaxIdx(hist, nullptr, nullptr, nullptr, maxLoc);
        resultColor |= maxLoc[0] << (i * 8);
    }
    mLastColorBGR = encodeBGR(resultColor);
    cv::Vec3b cmpColor;
    switch (mMode) {
    case CompareMode::Gray:
        mLastColorBGR = sGray2sBgr(resultColor);
        for (size_t i = 0; i < mColors.size(); i++) {
            mLastDistance[i] = std::abs(int(resultColor) - int(mColors[i][0]));
            mLastValues[i] = xxx(mLastDistance[i], 15);
        }
        LOG(DEBUG) << "Colors result: " << std::fixed << std::setprecision(3) << mLastValues << " for gray level "
                   << resultColor << " and colors " << mColors << " with distance " << mLastDistance;
        break;
    case CompareMode::Hsv:
        cmpColor = sBgr2Hsv(mLastColorBGR);
        for (size_t i = 0; i < mColors.size(); i++) {
            mLastDistance[i] = distanceHsv(cmpColor, mColors[i]);
            mLastValues[i] = xxx(mLastDistance[i], 50);
        }
        LOG(DEBUG) << "Colors result: " << std::fixed << std::setprecision(3) << mLastValues << " for hsv color "
                   << cmpColor << " and colors " << mColors << " with distance " << mLastDistance;
        break;
    case CompareMode::Luv:
        cmpColor = sBgr2Luv(mLastColorBGR);
        for (size_t i = 0; i < mColors.size(); i++) {
            mLastDistance[i] = distanceLuv(cmpColor, mColors[i]);
            mLastValues[i] = xxx(mLastDistance[i], 40);
        }
        LOG(DEBUG) << "Colors result: " << std::fixed << std::setprecision(3) << mLastValues << " for luv color "
                   << cmpColor << " and colors " << mColors << " with distance " << mLastDistance;
        break;
    case CompareMode::BGR:
        for (size_t i = 0; i < mColors.size(); i++) {
            mLastDistance[i] = distanceBGR(mLastColorBGR, mColors[i]);
            mLastValues[i] = xxx(mLastDistance[i], 50);
        }
        LOG(DEBUG) << "Colors result: " << std::fixed << std::setprecision(3) << mLastValues << " for bgr color "
                   << mLastColorBGR << " and colors " << mColors << " with distance " << mLastDistance;
        break;
    }
    imagePlanes.clear();
    return *std::max_element(mLastValues.begin(), mLastValues.end());
}

double Histogram::classify(ClassifyEnv &env) {
    return match(env) >= 0.8;
}

double Histogram::debugMatch(ClassifyEnv &env) {
    return match(env);
}

} // namespace detect
