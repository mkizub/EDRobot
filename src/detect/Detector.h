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


class ImageTemplate : public Detector {
public:
    ImageTemplate(const std::string& filename, cv::Rect rect);
    ~ImageTemplate() override = default;

    double match(ClassifyEnv& env) override;
    double classify(ClassifyEnv& env) override;
    double debugMatch(ClassifyEnv& env) override;

    static bool loadImageAndMask(const std::string& filename, cv::Mat& image, cv::Mat& mask);
    static bool extractImageMask(cv::Mat& image, cv::Mat& mask);

    double toResult(double matchValue); // something like logistic regression, S-curve

    static cv::Mat applyFilters(const std::vector<std::unique_ptr<ImageFilter>>& filters, cv::Mat image);
    static cv::Mat scaleImage(cv::Mat image, double scaleX, double scaleY = 0);
    static cv::Mat rotateImage(cv::Mat image, int angle, double scale);
    static void fixNaNinResult(cv::Mat& result, const std::string& filename);

//protected:
    void prepareImages(ClassifyEnv& env);
    std::string name;
    std::string filename;
    cv::Rect referenceRect;
    int channels;
    cv::Point extendLT;
    cv::Point extendRB;
    double threshold_min;
    double threshold_max;
    std::vector<std::unique_ptr<ImageFilter>> filters;

    struct ImageMatrix {
        double scale;
        double angle;
        std::string name;
        cv::Mat templImage;
        cv::Mat templMask;
    };
    std::vector<ImageMatrix> imagesOrig;
    std::vector<ImageMatrix> imagesPrepared;

    double preprocessedTemplateScale = 0;

    cv::Rect captureRect;
    cv::Rect matchRect;
    cv::Point matchedCaptureOffset;
    std::vector<double> testScales;
    std::vector<int> testAngles;
    int lastTemplatedx;
};

class CompassDetector : public ImageTemplate {
public:
    CompassDetector();
    ~CompassDetector() override = default;

    double match(ClassifyEnv& env) override;
    double debugMatch(ClassifyEnv& env) override;

    std::vector<ImageMatrix> compassScales;
    std::vector<ImageMatrix> compassDots;

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

class TilesDetector : public ImageTemplate {
public:
    TilesDetector(const std::string& name, cv::Rect& tilesRect, const std::string& icons, cv::Rect& iconsRect,
                  int min_rows, int max_rows, int min_cols, int max_cols, int gap);
    ~TilesDetector() override = default;

    double match(ClassifyEnv& env) override;
    double classify(ClassifyEnv& env) override;
    double debugMatch(ClassifyEnv& env) override;

    const std::string name;
    cv::Rect mTilesRect;
    std::vector<std::string> mIconFiles;
    bool hudTryHard {false};

    std::vector<ClassifiedRect> mDetectedTiles;

private:
    bool getColSpan(int& col, int& span, cv::Rect& bbox, cv::Rect& captureRect, int gap) const;

    int mMinRows;
    int mMaxRows;
    int mMinCols;
    int mMaxCols;
    int mGap;
};

class LineDetector : public Detector {
public:
    LineDetector(ImageTemplate* anchor, cv::Point p0, cv::Point p1);
    ~LineDetector() override = default;

    double match(ClassifyEnv& env) override;
    double classify(ClassifyEnv &env) override;
    double debugMatch(ClassifyEnv& env) override;

    void normalizeRotatedRect(cv::RotatedRect& rr);
    void tryCannyParamsGUI(ClassifyEnv &env);

    std::unique_ptr<ImageTemplate> anchorDetector;
    std::vector<std::unique_ptr<ImageFilter>> filters;

    std::string name;
    cv::Point extendLT;
    cv::Point extendRB;
    const cv::Point referenceP0;
    const cv::Point referenceP1;
    // maybe scale (speedup and a kind of blur
    double imageScaleX {1};
    double imageScaleY {1};
    // cv::threshold
    int binaryThreshold {127};

    cv::Point captureP0;
    cv::Point captureP1;
    cv::Rect lineMatchRect;
    float lastLineAngle;  // in degrees, -90 <= angle <= +90

};

} // namespace detect

#endif //EDROBOT_TEMPLATE_H
