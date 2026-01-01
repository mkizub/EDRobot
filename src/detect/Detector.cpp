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
        double value = oracle->match(env);
        if (oracle->classifierWeight > 0) {
            if (value < 0.2)
                return 0;
            double weight = oracle->classifierWeight / sumWeights;
            sum += weight * (2 * value - 1);
        }
    }
    return (sum + 1) / 2;
}

//double Sequence::debugMatch(ClassifyEnv &env) {
//    json5pp::value j_arr = json5pp::array({});
//    double sumWeights = 0;
//    for (auto &oracle: oracles) {
//        if (oracle->classifierWeight <= 0)
//            continue;
//        sumWeights += oracle->classifierWeight;
//    }
//    double sum = 0;
//    for (auto &oracle: oracles) {
//        if (oracle->classifierWeight <= 0)
//            continue;
//        double value = oracle->debugMatch(env);
//        j_arr.as_array().emplace_back(value);
//        double weight = oracle->classifierWeight / sumWeights;
//        sum += weight * (2 * value - 1);
//    }
//    double result = (sum + 1) / 2;
//    LOG(INFO) << "Sequence match result: " << result << " for " << j_arr;
//    return result;
//}


double BestOf::match(ClassifyEnv &env) {
    double bestMatch = 0;
    for (int i = 0; i < oracles.size(); i++) {
        double value = oracles[i]->match(env);
        if (value > bestMatch) {
            bestMatch = value;
            if (value >= 1)
                break;
        }
    }
    return bestMatch;
}

//double BestOf::debugMatch(ClassifyEnv &env) {
//    json5pp::value j_arr = json5pp::array({});
//    int bestIdx = -1;
//    double bestMatch = 0;
//    for (int i = 0; i < oracles.size(); i++) {
//        double value = oracles[i]->debugMatch(env);
//        j_arr.as_array().emplace_back(value);
//        if (value > bestMatch) {
//            bestMatch = value;
//            bestIdx = i;
//        }
//    }
//    LOG(INFO) << "BestOf match result: " << bestMatch << " index " << bestIdx << " between " << j_arr;
//    return bestMatch;
//}

double ReferDetector::match(ClassifyEnv &env) {
    for (auto& cr : env.classified) {
        if (cr.cdt == ClsDetType::Detected && cr.text == referred)
            return cr.u.tdet.match;
        if (cr.cdt == ClsDetType::LineDetected && cr.text.starts_with(referred) && cr.text[referred.size()] == ':')
            return cr.u.ldet.match;
    }
    return 0;
}

#ifdef EDROBOT_USE_OPENCL
bool Histogram::calc(XMat image) {
    if (mMode == Mode::Gray && image.channels() > 1) {
        XMat grayImage;
        cv::cvtColor(image, grayImage, cv::COLOR_BGR2GRAY);
        return calc(toMat(grayImage));
    } else {
        return calc(toMat(image));
    }
}
#endif

bool Histogram::calc(cv::Mat image) {
    // TODO: optimize for OpenCL (split to channels and use InputArrayOfArrays)
    int colorPlanes;
    if (mMode == Mode::Gray) {
        if (image.channels() > 1) {
            cv::Mat grayImage;
            cv::cvtColor(image, grayImage, cv::COLOR_BGR2GRAY);
            image = grayImage;
        }
        colorPlanes = 1;
        int histSize = 256/mBin;
        float range[]{0, 256}; //the upper boundary is exclusive
        const float *histRange[]{range};
        if (image.channels() != 1)
            return false;
        cv::Mat hist;
        cv::calcHist(&image, 1, nullptr, cv::Mat(), hist, 1, &histSize, histRange);
        int maxLoc[4]{};
        cv::minMaxIdx(hist, nullptr, nullptr, nullptr, maxLoc);
        uchar gray = maxLoc[0] * mBin + mBin/2;
        mLastColor = {gray, gray, gray};
    }
    else if (mMode == Mode::Hsv) {
        if (image.channels() < 3)
            return false;
        cv::Mat hsvSubImage;
        cv::cvtColor(image, hsvSubImage, cv::COLOR_BGR2HSV_FULL);
        int channels[3] {0, 1, 2};
        int histSize[3] {256/mBin, 256/mBin, 256/mBin};
        float range[]{0, 256}; //the upper boundary is exclusive
        const float *histRange[3]{range,range,range};
        cv::Mat hist;
        cv::calcHist(&hsvSubImage, 1, channels, cv::Mat(), hist, 3, histSize, histRange);
        int maxLoc[4]{};
        cv::minMaxIdx(hist, nullptr, nullptr, nullptr, maxLoc);
        uchar h = maxLoc[0] * mBin + mBin/2;
        h = h * 179. / 255.;
        uchar s = maxLoc[1] * mBin + mBin/2;
        uchar v = maxLoc[2] * mBin + mBin/2;
        mLastColor = {h, s, v};
    }
    else if (mMode == Mode::Luv) {
        if (image.channels() < 3)
            return false;
        cv::Mat luvSubImage;
        cv::cvtColor(image, luvSubImage, cv::COLOR_BGR2Luv);
        int channels[3] {0, 1, 2};
        int histSize[3] {256/mBin, 256/mBin, 256/mBin};
        float range[2] {0, 256};
        const float *histRange[3]{range,range,range};
        cv::Mat hist;
        cv::calcHist(&luvSubImage, 1, channels, cv::Mat(), hist, 3, histSize, histRange);
        int maxLoc[4]{};
        cv::minMaxIdx(hist, nullptr, nullptr, nullptr, maxLoc);
        uchar l = maxLoc[0] * mBin + mBin/2;
        uchar u = maxLoc[1] * mBin + mBin/2;
        uchar v = maxLoc[2] * mBin + mBin/2;
        mLastColor = {l, u, v};
    }
    else /*if (mMode == Mode::BGR)*/ {
        if (image.channels() < 3)
            return false;
        std::vector<cv::Mat> imagePlanes;
        cv::split(image, imagePlanes);
        int channels[3] {0, 1, 2};
        int histSize[3] {256/mBin, 256/mBin, 256/mBin};
        float range[2] {0, 256};
        const float *histRange[3]{range,range,range};
        cv::Mat hist;
        cv::calcHist(&image, 1, channels, cv::Mat(), hist, 3, histSize, histRange);
        int maxLoc[4]{};
        cv::minMaxIdx(hist, nullptr, nullptr, nullptr, maxLoc);
        uchar b = maxLoc[0] * mBin + mBin/2;
        uchar g = maxLoc[1] * mBin + mBin/2;
        uchar r = maxLoc[2] * mBin + mBin/2;
        mLastColor = {b, g, r};
    }
    return true;
}

WState Histogram::guessWState() {
    if (mMode == Mode::Hsv) {
        if (mLastColor[1] < 80) // desaturated = disabled
            return WState::Disabled;
        else if (mLastColor[0] < 30) {// hue is near red = known color
            if (mLastColor[2] > 180) // bright = focused
                return WState::Focused;
            else
                return WState::Normal;
        }
    }
    if (mMode == Mode::Gray) {
        if (mLastColor[0] > 120)
            return WState::Focused;
        if (mLastColor[0] > 20)
            return WState::Normal;
    }
    return WState::Unknown;
}

} // namespace detect
