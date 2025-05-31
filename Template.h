//
// Created by mkizub on 23.05.2025.
//

#pragma once

#include "pch.h"

#ifndef EDROBOT_TEMPLATE_H
#define EDROBOT_TEMPLATE_H

class Template {
public:
    Template() = default;
    virtual ~Template() = default;

    // return matching value, i.e. how much a template image is similar to given image
    virtual double match() = 0;
    // classifies evaluated matching value, i.e. classifies by returning probability of being the same class
    // returns value in range [0..1]
    virtual double classify() = 0;

    virtual double debugMatch(cv::Mat drawToImage) = 0;
};

class SequenceTemplate : public Template {
public:
    SequenceTemplate(std::vector<std::unique_ptr<Template>>&& oracles)
        : oracles(std::move(oracles))
    {}
    ~SequenceTemplate() override = default;

    double match() override;
    double classify() override;
    double debugMatch(cv::Mat drawToImage) override;
private:
    std::vector<std::unique_ptr<Template>> oracles;
};

class HistogramTemplate : public Template {
public:
    HistogramTemplate(cv::Rect rect, unsigned value, bool gray);
    HistogramTemplate(cv::Vec3b luv);
    ~HistogramTemplate() override = default;

    double match() override;
    double classify() override;
    double debugMatch(cv::Mat drawToImage) override;

    double classify(cv::Rect& r);
    double match(cv::Rect& r);

    cv::Vec3b mLastColorRGB;
    cv::Vec3b mLastColorLuv;
    double mLastValue;

private:
    cv::Rect mRect;
    unsigned mValueGray;
    cv::Vec3b mValueRGB;
    cv::Vec3b mValueLuv;
    bool mGray;
};

class ImageTemplate : public Template {
public:
    ImageTemplate(const std::string& filename, cv::Point lt, cv::Point extLT, cv::Point extRB, double tmin, double tmax);
    ~ImageTemplate() override = default;

    double match() override;
    double classify() override;
    double debugMatch(cv::Mat drawToImage) override;
private:
    cv::Rect getMatchRect(int imageWidth, int imageHeight);
    double toResult(double matchValue); // something like logistic regression, S-curve
    const std::string filename;
    cv::Point screenLT;
    cv::Point screenRB;
    cv::Point extendLT;
    cv::Point extendRB;
    const double threshold_min;
    const double threshold_max;
    cv::Mat templGray;
    cv::Mat templMask;
    cv::Mat templGrayScaled;
    cv::Mat templMaskScaled;
    cv::Point scaledScreenLT; // for debug
    cv::Point scaledScreenRB; // for debug
    cv::Point matchedLoc;     // for debug
    double lastScale;
};



#endif //EDROBOT_TEMPLATE_H
