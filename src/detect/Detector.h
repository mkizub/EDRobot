//
// Created by mkizub on 23.05.2025.
//

#pragma once

#include "../pch.h"

#ifndef EDROBOT_TEMPLATE_H
#define EDROBOT_TEMPLATE_H

namespace detect {

class Detector {
public:
    Detector() = default;
    virtual ~Detector() = default;

    // return matching value, i.e. how much a template image is similar to given image
    virtual double match(ClassifyEnv& env) = 0;
    // classifies evaluated matching value, i.e. classifies by returning probability of being the same class
    // returns value in range [0..1]
    virtual double classify(ClassifyEnv& env) = 0;

    virtual double debugMatch(ClassifyEnv& env) = 0;

    double classifierWeight {1};
};

class Sequence : public Detector {
public:
    Sequence(std::vector<std::unique_ptr<Detector>>&& oracles)
        : oracles(std::move(oracles))
    {}
    ~Sequence() override = default;

    double match(ClassifyEnv& env) override;
    double classify(ClassifyEnv& env) override;
    double debugMatch(ClassifyEnv& env) override;
private:
    std::vector<std::unique_ptr<Detector>> oracles;
};

class BestOf : public Detector {
public:
    BestOf(std::vector<std::unique_ptr<Detector>>&& oracles)
            : oracles(std::move(oracles))
    {}
    ~BestOf() override = default;

    double match(ClassifyEnv& env) override;
    double classify(ClassifyEnv& env) override;
    double debugMatch(ClassifyEnv& env) override;
private:
    std::vector<std::unique_ptr<Detector>> oracles;
};

class Histogram : public Detector {
public:
    enum class CompareMode {
        Gray, Hsv, Luv, BGR
    };
    Histogram(CompareMode mode, const cv::Rect& rect, const std::array<cv::Vec3b,4>& colors);
    ~Histogram() override = default;

    double match(ClassifyEnv& env) override;
    double classify(ClassifyEnv& env) override;
    double debugMatch(ClassifyEnv& env) override;

    cv::Vec3b mLastColorBGR;
    std::array<double,4> mLastDistance;
    std::array<double,4> mLastValues;
    cv::Rect mRect;

private:
    const std::array<cv::Vec3b,4> mColors;
    const CompareMode mMode;
};

class ImageFilter {
public:
    virtual ~ImageFilter() = default;
    virtual cv::Mat apply(cv::Mat image) = 0;
};
class GaussFilter : public ImageFilter {
public:
    GaussFilter(int kern_x, int kern_y) : kernX(kern_x), kernY(kern_y) {}
    cv::Mat apply(cv::Mat image) final;
    const int kernX;
    const int kernY;
    const bool disabled {false};
};
class LaplacianFilter : public ImageFilter {
public:
    LaplacianFilter(int kern, double scale) : kern(kern), scale(scale) {}
    cv::Mat apply(cv::Mat image) final;
    const int kern;
    const double scale;
};
class HsvColorCropFilter : public ImageFilter {
public:
    HsvColorCropFilter() {}
    cv::Mat apply(cv::Mat image) final;
    std::vector<std::pair<cv::Vec3b,cv::Vec3b>> ranges;
};


class BaseImageTemplate : public Detector {
public:
    BaseImageTemplate(const std::string& filename, cv::Mat image, spEvalRect refRect);
    ~BaseImageTemplate() override = default;

    double classify(ClassifyEnv& env) override;
    double debugMatch(ClassifyEnv& env) override;

    static bool loadImageAndMask(const std::string& filename, cv::Mat& image, cv::Mat& mask);
    static bool extractImageMask(cv::Mat& image, cv::Mat& mask);

    double toResult(double matchValue); // something like logistic regression, S-curve
    cv::Mat applyFilters(cv::Mat image);
    cv::Mat scaleImage(cv::Mat image, double scaleX, double scaleY);
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

class TilesDetector : public Detector {
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

class LineDetector : public BaseImageTemplate {
public:
    LineDetector( std::vector<std::string> anchors, spEvalRect anchorAt, cv::Point p0, cv::Point p1);
    ~LineDetector() override = default;

    double match(ClassifyEnv& env) override;
    double debugMatch(ClassifyEnv& env) override;

    void normalizeRotatedRect(cv::RotatedRect& rr);
    void tryCannyParamsGUI(ClassifyEnv &env);

    const std::vector<std::string> anchorFiles;
    const cv::Point referenceP0;
    const cv::Point referenceP1;
    // maybe scale (speedup and a kind of blur
    double imageScaleX {1};
    double imageScaleY {1};
    // cv::threshold
    int binaryThreshold {127};

    struct AnchorMatrix {
        std::string name;
        cv::Mat templImage;
    };
    std::vector<AnchorMatrix> anchorSource;
    std::vector<AnchorMatrix> anchorScaled;

    cv::Point captureP0;
    cv::Point captureP1;
    cv::Rect lineMatchRect;
    float lastLineAngle;  // in degrees, -90 <= angle <= +90

};

} // namespace detect

#endif //EDROBOT_TEMPLATE_H
