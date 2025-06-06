//
// Created by mkizub on 23.05.2025.
//

#pragma once

#include "pch.h"

#ifndef EDROBOT_TEMPLATE_H
#define EDROBOT_TEMPLATE_H

struct ClassifyEnv;

// Rect evaluator
class EvalRect {
public:
    EvalRect() = default;
    virtual cv::Rect calcRect(const ClassifyEnv& detectorState) = 0;
};
typedef std::shared_ptr<EvalRect> spEvalRect;

class ConstRect : public EvalRect {
public:
    ConstRect(cv::Rect rect) : mRect(rect) {}
    cv::Rect calcRect(const ClassifyEnv& detectorState) override { return mRect; };

    cv::Rect mRect;
};

extern spEvalRect makeEvalRect(json5pp::value jv, int width=0, int height=0);

struct ClassifyEnv {
    // in configuration all numbers are specified for reference screen size
    const cv::Size ReferenceScreenSize {1920, 1080};
    const cv::Point ReferenceScreenCenter {1920/2, 1080/2};
    // actual window size and position on image (screenshot)
    const cv::Rect captureRect;
    const cv::Rect captureCrop;
    // RGB color window image
    const cv::Mat imageColor;
    // grayscale window image
    const cv::Mat imageGray;
    // image for debug drawing
    cv::Mat debugImage;

    void init(const cv::Rect& captRect, const cv::Mat& imgColor, const cv::Mat& imgGray);
    void clear();

private:
    // reference-to-captured scale
    bool needScaling_ {false};
    double scaleToCaptured_ {1};
    cv::Point captureCenter;

public:
    struct ResultRect {
        ResultRect() = default;
        ResultRect(double rat, const std::string& nm, cv::Rect refRect, cv::Rect detRect)
            : rate(rat), name(nm), referenceRect(refRect), detectedRect(detRect)
        {}
        double rate{};          // [0..1] probability returned by Template::classify()
        std::string name;       // name of Template that detected this rect
        cv::Rect referenceRect; // original rect in reference coordinates
        cv::Rect detectedRect;  // detected rect in reference coordinates
    };
    // a set of named detected rects
    std::vector<ResultRect> classifiedRects;
    std::unordered_map<std::string,ButtonState> classifiedButtonStates;

    cv::Point scaleToCaptured(const cv::Point& point) const {
        return needScaling_ ? point * scaleToCaptured_ : point;
    }
    cv::Size  scaleToCaptured(const cv::Size& size) const {
        if (!needScaling_)
            return size;
        return {int(size.width * scaleToCaptured_), int(size.height * scaleToCaptured_)};
    }
    cv::Point scaleToReference(const cv::Point& point) const {
        return needScaling_ ? point / scaleToCaptured_ : point;
    }
    cv::Size  scaleToReference(const cv::Size& size) const {
        if (!needScaling_)
            return size;
        return {int(size.width / scaleToCaptured_), int(size.height / scaleToCaptured_)};
    }

    cv::Point cvtReferenceToDesktop(const cv::Point& point) const;

    cv::Point cvtReferenceToCaptured(const cv::Point& point) const;
    cv::Rect  cvtReferenceToCaptured(const cv::Rect& rect) const;

    cv::Point cvtCapturedToReference(const cv::Point& point) const;
    cv::Rect  cvtCapturedToReference(const cv::Rect& rect) const;
};

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

class ImageTemplate : public Template {
public:
    ImageTemplate(const std::string& name, const std::string& filename, cv::Mat image, spEvalRect lt, cv::Point extLT, cv::Point extRB, double tmin, double tmax);
    ~ImageTemplate() override = default;

    double match(ClassifyEnv& env) override;
    double classify(ClassifyEnv& env) override;
    double debugMatch(ClassifyEnv& env) override;
private:
    double toResult(double matchValue); // something like logistic regression, S-curve
    const std::string name;
    const std::string filename;
    spEvalRect referenceRect;
    cv::Point extendLT;
    cv::Point extendRB;
    const double threshold_min;
    const double threshold_max;
    cv::Mat templGray;
    cv::Mat templMask;
    cv::Mat templGrayScaled;
    cv::Mat templMaskScaled;
    cv::Rect captureRect;
    cv::Rect matchRect;
    cv::Point matchedCaptureOffset;
    double lastScale;
};

#endif //EDROBOT_TEMPLATE_H
