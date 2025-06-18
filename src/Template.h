//
// Created by mkizub on 23.05.2025.
//

#pragma once

#include <utility>

#include "pch.h"

#ifndef EDROBOT_TEMPLATE_H
#define EDROBOT_TEMPLATE_H

class Template {
public:
    Template() = default;
    virtual ~Template() = default;

    // return matching value, i.e. how much a template image is similar to given image
    virtual double match(ClassifyEnv& env) = 0;
    // classifies evaluated matching value, i.e. classifies by returning probability of being the same class
    // returns value in range [0..1]
    virtual double classify(ClassifyEnv& env) = 0;

    virtual double debugMatch(ClassifyEnv& env) = 0;
};

class SequenceTemplate : public Template {
public:
    SequenceTemplate(std::vector<std::unique_ptr<Template>>&& oracles)
        : oracles(std::move(oracles))
    {}
    ~SequenceTemplate() override = default;

    double match(ClassifyEnv& env) override;
    double classify(ClassifyEnv& env) override;
    double debugMatch(ClassifyEnv& env) override;
private:
    std::vector<std::unique_ptr<Template>> oracles;
};

class HistogramTemplate : public Template {
public:
    enum class CompareMode {
        Gray, Luv, RGB
    };
    HistogramTemplate(CompareMode mode, const cv::Rect& rect, const cv::Vec3b& colors);
    HistogramTemplate(CompareMode mode, const cv::Rect& rect, const std::vector<cv::Vec3b>& colors);
    ~HistogramTemplate() override = default;

    double match(ClassifyEnv& env) override;
    double classify(ClassifyEnv& env) override;
    double debugMatch(ClassifyEnv& env) override;

    cv::Vec3b mLastColor;
    std::vector<double> mLastDistance;
    std::vector<double> mLastValues;
    cv::Rect mRect;

private:
    const std::vector<cv::Vec3b> mColors;
    const CompareMode mMode;
};

class BaseImageTemplate : public Template {
public:
    BaseImageTemplate(const std::string& name, const std::string& filename, cv::Mat& image, bool edge,
                      spEvalRect& refRect, cv::Point extLT, cv::Point extRB, double tmin, double tmax);
    ~BaseImageTemplate() override = default;

    double classify(ClassifyEnv& env) override;
    double debugMatch(ClassifyEnv& env) override;
protected:
    static bool loadImageAndMask(const std::string& filename, cv::Mat& image, cv::Mat& mask);
    static bool extractImageMask(cv::Mat& image, cv::Mat& mask);
    double toResult(double matchValue); // something like logistic regression, S-curve
    cv::Mat makeLaplacian(cv::Mat m);
    cv::Mat makeGaussianBlur(cv::Mat m, int kernelSize=3, double sigma=0);
    void fixNaNinResult(cv::Mat& result);
    const std::string name;
    const std::string filename;
    bool edgeLaplacian;
    spEvalRect referenceRect;
    cv::Point extendLT;
    cv::Point extendRB;
    const double threshold_min;
    const double threshold_max;

    cv::Mat templImage;
    cv::Mat templMask;
    double preprocessedTemplateScale = 1;
    bool preprocessedLaplacian = false;

    cv::Rect captureRect;
    cv::Rect matchRect;
    cv::Point matchedCaptureOffset;

    struct ScaledMatrix {
        double scale;
        cv::Mat templImage;
        cv::Mat templMask;
    };
};

class ImageTemplate : public BaseImageTemplate {
public:
    ImageTemplate(const std::string& name, const std::string& filename, cv::Mat& image, bool edge,
                  spEvalRect& refRect, cv::Point extLT, cv::Point extRB, double tmin, double tmax);
    ~ImageTemplate() override = default;

    double match(ClassifyEnv& env) override;
    cv::Mat templImageScaled;
    cv::Mat templMaskScaled;
};

class ImageMultiScaleTemplate : public BaseImageTemplate {
public:
    ImageMultiScaleTemplate(const std::string& name, const std::string& filename, cv::Mat& image,
                            double scaleMin, double scaleMax, double scaleStep, bool edge,
                            spEvalRect& refRect, cv::Point extLT, cv::Point extRB, double tmin, double tmax);
    ~ImageMultiScaleTemplate() override = default;

    double match(ClassifyEnv& env) override;
    double debugMatch(ClassifyEnv& env) override;
private:
    double generateScaleMin;
    double generateScaleMax;
    double generateScaleStep;
    std::vector<ScaledMatrix> scales;
    int lastScaleIdx;
    double lastScale;
};

class CompassDetector : public BaseImageTemplate {
public:
    CompassDetector(cv::Mat& image, spEvalRect& refRect);
    ~CompassDetector() override = default;

    double match(ClassifyEnv& env) override;
    double debugMatch(ClassifyEnv& env) override;

    std::vector<ScaledMatrix> compassScales;
    std::vector<ScaledMatrix> compassDots;
    cv::Vec3b luvLower;
    cv::Vec3b luvUpper;

    const double threshold_dot;

    int lastScaleIdx;
    double lastScale;
    int lastDotIdx;
    double lastTgtPitch;
    double lastTgtYaw;
    double lastTgtRoll;

    cv::Rect dotCaptureRect;
    cv::Point2d dotSpherePosition;
    double lastDotValue;

private:
    static void tryLowerUpperBoundsGUI(ClassifyEnv &env, cv::Rect referenceRect);
};

#endif //EDROBOT_TEMPLATE_H
