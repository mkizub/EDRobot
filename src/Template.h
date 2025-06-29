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

    double classifierWeight {1};
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
        Gray, Hsv, Luv, BGR
    };
    HistogramTemplate(CompareMode mode, const cv::Rect& rect, const cv::Vec3b& colors);
    HistogramTemplate(CompareMode mode, const cv::Rect& rect, const std::vector<cv::Vec3b>& colors);
    ~HistogramTemplate() override = default;

    double match(ClassifyEnv& env) override;
    double classify(ClassifyEnv& env) override;
    double debugMatch(ClassifyEnv& env) override;

    cv::Vec3b mLastColorBGR;
    std::vector<double> mLastDistance;
    std::vector<double> mLastValues;
    cv::Rect mRect;

private:
    const std::vector<cv::Vec3b> mColors;
    const CompareMode mMode;
};

class ImageFilter {
public:
    virtual ~ImageFilter() = default;
    virtual cv::Mat apply(cv::Mat& image) = 0;
};
class GaussFilter : public ImageFilter {
public:
    GaussFilter(int kern, double sigma) : kern(kern), sigma(sigma) {}
    cv::Mat apply(cv::Mat& image) final;
    const int kern;
    const double sigma;
};
class LaplacianFilter : public ImageFilter {
public:
    LaplacianFilter(int kern, double scale) : kern(kern), scale(scale) {}
    cv::Mat apply(cv::Mat& image) final;
    const int kern;
    const double scale;
};
class HsvColorCropFilter : public ImageFilter {
public:
    HsvColorCropFilter() {}
    cv::Mat apply(cv::Mat& image) final;
    std::vector<std::pair<cv::Vec3b,cv::Vec3b>> ranges;
};


class BaseImageTemplate : public Template {
public:
    BaseImageTemplate(const std::string& filename, cv::Mat image, spEvalRect refRect);
    ~BaseImageTemplate() override = default;

    double classify(ClassifyEnv& env) override;
    double debugMatch(ClassifyEnv& env) override;

    static bool loadImageAndMask(const std::string& filename, cv::Mat& image, cv::Mat& mask);
    static bool extractImageMask(cv::Mat& image, cv::Mat& mask);

    double toResult(double matchValue); // something like logistic regression, S-curve
    cv::Mat applyFilters(cv::Mat& image);
    void fixNaNinResult(cv::Mat& result);
    std::string name;
    std::string filename;
    spEvalRect referenceRect;
    cv::Point extendLT;
    cv::Point extendRB;
    double threshold_min;
    double threshold_max;
    std::vector<std::unique_ptr<ImageFilter>> filters;

    cv::Mat templImage;
    cv::Mat templMask;
    double preprocessedTemplateScale = 0;

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
    ImageTemplate(const std::string& filename, cv::Mat image, spEvalRect refRect);
    ~ImageTemplate() override = default;

    double match(ClassifyEnv& env) override;
    double debugMatch(ClassifyEnv& env) override;
    cv::Mat templImageScaled;
    cv::Mat templMaskScaled;
};

class ImageMultiScaleTemplate : public BaseImageTemplate {
public:
    ImageMultiScaleTemplate(const std::string& filename, cv::Mat image, spEvalRect refRect, std::vector<double> scales);
    ~ImageMultiScaleTemplate() override = default;

    double match(ClassifyEnv& env) override;
    double debugMatch(ClassifyEnv& env) override;
private:
    std::vector<double> scales;
    std::vector<ScaledMatrix> scaledImages;
    int lastScaleIdx;
    double lastScale;
};

class CompassDetector : public ImageMultiScaleTemplate {
public:
    CompassDetector();
    ~CompassDetector() override = default;

    double match(ClassifyEnv& env) override;
    double debugMatch(ClassifyEnv& env) override;

    std::vector<ScaledMatrix> compassScales;
    std::vector<ScaledMatrix> compassDots;

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

    static void tryLowerUpperBoundsGUI(ClassifyEnv &env, cv::Rect referenceRect);
};

class TilesDetector : public Template {
public:
    TilesDetector(const std::string& name, spEvalRect& rect, int rows, int cols, int gap, double tmin, double tmax, std::vector<std::string> icon_files);
    ~TilesDetector() override = default;

    double match(ClassifyEnv& env) override;
    double classify(ClassifyEnv& env) override;
    double debugMatch(ClassifyEnv& env) override;

    const std::string name;
    spEvalRect mRect;
    std::vector<std::string> mIconFiles;

    std::vector<ClassifiedRect> mDetectedTiles;

private:
    const double threshold_min;
    const double threshold_max;
    struct IconMatrix {
        double scale;
        std::string name;
        cv::Mat templImage;
    };
    std::vector<IconMatrix> iconsSource;
    std::vector<IconMatrix> iconsScaled;

    bool getColSpan(int& col, int& span, cv::Rect& bbox, cv::Rect& captureRect, int gap) const;
    double mPreprocessedTemplateScale = 1;
    int mMaxRows;
    int mMaxCols;
    int mGap;

};

#endif //EDROBOT_TEMPLATE_H
