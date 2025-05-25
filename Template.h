//
// Created by mkizub on 23.05.2025.
//

#pragma once

#ifndef EDROBOT_TEMPLATE_H
#define EDROBOT_TEMPLATE_H

#include <opencv2/core/mat.hpp>

class Master;

class Template {
public:
    Template() = default;
    virtual ~Template() = default;

    virtual double evaluate(Master* master) = 0;
    virtual double match(Master* master) = 0;
};

class SequenceTemplate : public Template {
public:
    SequenceTemplate(std::vector<std::unique_ptr<Template>>&& oracles)
        : oracles(std::move(oracles))
    {}
    ~SequenceTemplate() override = default;

    double evaluate(Master* master) override;
    double match(Master* master) override;
private:
    std::vector<std::unique_ptr<Template>> oracles;
};

class ImageTemplate : public Template {
public:
    ImageTemplate(const std::string& filename, int x, int y, int l, int t, int r, int b, double tmin, double tmax);
    ~ImageTemplate() override = default;

    double evaluate(Master* master) override;
    double match(Master* master) override;
private:
    const std::string filename;
    cv::Point screenPoint;
    cv::Size extLT;
    cv::Size extRB;
    const double threshold_min;
    const double threshold_max;
    cv::Rect screenRect;
    cv::Mat templGray;
    cv::Mat templMask;
    cv::Rect screenRectScaled;
    cv::Mat templGrayScaled;
    cv::Mat templMaskScaled;
};



#endif //EDROBOT_TEMPLATE_H
