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
    // actual window size and position on image (screenshot)
    cv::Rect captureRect;
    // RGB color window image
    cv::Mat imageColor;
    // grayscale window image
    cv::Mat imageGray;
    // image for debug drawing
    cv::Mat debugImage;

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

    cv::Point cvtReferenceToImage(const cv::Point& point);
    cv::Size  cvtReferenceToImage(const cv::Size& size);
    cv::Rect  cvtReferenceToImage(const cv::Rect& rect);

    cv::Point cvtImageToReference(const cv::Point& point);
    cv::Size  cvtImageToReference(const cv::Size& size);
    cv::Rect  cvtImageToReference(const cv::Rect& rect);

    void clear() {
        captureRect = cv::Rect();
        imageColor = cv::Mat();
        imageGray = cv::Mat();
        debugImage = cv::Mat();
        classifiedRects.clear();
    }
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
    HistogramTemplate(unsigned value, bool gray, spEvalRect rect);
    HistogramTemplate(cv::Vec3b luv, spEvalRect rect);
    ~HistogramTemplate() override = default;

    double match(ClassifyEnv& env) override;
    double classify(ClassifyEnv& env) override;
    double debugMatch(ClassifyEnv& env) override;

    cv::Vec3b mLastColorRGB;
    cv::Vec3b mLastColorLuv;
    double mLastValue;

private:
    spEvalRect mRect;
    unsigned mValueGray;
    cv::Vec3b mValueRGB;
    cv::Vec3b mValueLuv;
    bool mGray;
};

class ImageTemplate : public Template {
public:
    ImageTemplate(const std::string& name, const std::string& filename, cv::Mat image, spEvalRect lt, cv::Point extLT, cv::Point extRB, double tmin, double tmax);
    ~ImageTemplate() override = default;

    double match(ClassifyEnv& env) override;
    double classify(ClassifyEnv& env) override;
    double debugMatch(ClassifyEnv& env) override;
private:
    cv::Rect getMatchRect(ClassifyEnv& env, int imageWidth, int imageHeight);
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
    cv::Point scaledScreenLT; // for debug
    cv::Point scaledScreenRB; // for debug
    cv::Point matchedLoc;     // for debug
    double lastScale;
};

#endif //EDROBOT_TEMPLATE_H
