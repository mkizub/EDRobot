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

    // classifies evaluated matching value, i.e. classifies by returning probability of being the same class
    // returns value in range [0..1]
    virtual double match(ClassifyEnv& env) = 0;

    double classifierWeight {1};
};

class Sequence : public Detector {
public:
    Sequence(std::vector<std::unique_ptr<Detector>>&& oracles)
        : oracles(std::move(oracles))
    {}
    ~Sequence() override = default;

    double match(ClassifyEnv& env) override;
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
private:
    std::vector<std::unique_ptr<Detector>> oracles;
};

class ReferDetector : public Detector {
public:
    ReferDetector(std::string referred)
            : referred(referred)
    {}
    ~ReferDetector() override = default;

    double match(ClassifyEnv& env) override;
private:
    string referred;
};

class Histogram {
public:
    enum class Mode {
        Gray, Hsv, Luv, BGR
    };
    Histogram(Mode mode, const cv::Rect rect) : mMode(mode), mRect(rect) {}

    bool calc(ClassifyEnv& env);

    Mode mMode;
    cv::Vec3b mLastColor;
    cv::Rect mRect;
};

class ImageFilter {
public:
    struct Params {
        bool convertToFloat;
        bool cropToGray;
    };
    virtual ~ImageFilter() = default;
    virtual XMat apply(XMat image, Params params) = 0;
};
class GaussFilter : public ImageFilter {
public:
    GaussFilter(int kern_x, int kern_y) : kernX(kern_x), kernY(kern_y) {}
    XMat apply(XMat image, Params params) final;
    const int kernX;
    const int kernY;
};
class LaplacianFilter : public ImageFilter {
public:
    LaplacianFilter(int kern, double scale) : kern(kern), scale(scale) {}
    XMat apply(XMat image, Params params) final;
    const int kern;
    const double scale;
};
class DilateFilter : public ImageFilter {
public:
    DilateFilter(int kX, int kY, int iter=1) : kernX(kX), kernY(kY), iterations(iter) {}
    XMat apply(XMat image, Params params) final;
    const int kernX;
    const int kernY;
    const int iterations;
};
class ErodeFilter : public ImageFilter {
public:
    ErodeFilter(int kX, int kY, int iter=1) : kernX(kX), kernY(kY), iterations(iter) {}
    XMat apply(XMat image, Params params) final;
    const int kernX;
    const int kernY;
    const int iterations;
};
class HsvColorCropFilter : public ImageFilter {
public:
    HsvColorCropFilter() {}
    XMat apply(XMat image, Params params) final;
    std::vector<std::pair<cv::Vec3b,cv::Vec3b>> rangesU;
    std::vector<std::pair<cv::Vec3f,cv::Vec3f>> rangesF;
};


class ImageTemplate : public Detector {
public:
    struct ImageMatrix {
        double scale;
        double angle;
        std::string name;
        XMat templImageU;
        XMat templImageF;
        uint16_t org_w;
        uint16_t org_h;
        uint16_t opt_w;
        uint16_t opt_h;
        uint16_t opt_l;
        uint16_t opt_t;
        uint16_t opt_r;
        uint16_t opt_b;
    };
    struct MatchResult {
        ImageMatrix* im {nullptr};
        int index {-1};
        double value {-1};
        cv::Point loc;
    };

    ImageTemplate(const std::string& filename, spEvalRect rect);
    ~ImageTemplate() override = default;

    double match(ClassifyEnv& env) override;

    static bool loadImageAndMask(const std::string& filename, XMat& image);

    double toResult(double matchValue); // something like logistic regression, S-curve

    static ImageMatrix prepareImageMatrix(const ClassifyEnv& env, const std::vector<std::unique_ptr<ImageFilter>>& filters, XMat image, double scale, int angle, const std::string& name, ImageFilter::Params params={});
    static XMat applyFilters(const std::vector<std::unique_ptr<ImageFilter>>& filters, XMat image, ImageFilter::Params params={});
    static XMat scaleImage(XMat image, double scaleX, double scaleY = 0);
    static XMat rotateImage(XMat image, int angle, double scale);

    static cv::Rect makeOptimalMatchRect(ClassifyEnv& env, cv::Rect);
    static void matchTemplates(int matchMethod, const XMat& imagePrepared, std::vector<ImageMatrix>& templPrepared, MatchResult& result);
    //static void fixNaNinResult(cv::Mat& result, const std::string& filename);

//protected:
    void prepareImages(ClassifyEnv& env);
    std::string name;
    std::string filename;
    spEvalRect referenceRect;
    int channels;
    cv::Point extendLT;
    cv::Point extendRB;
    double threshold_min;
    double threshold_max;
    std::vector<std::unique_ptr<ImageFilter>> filters;
    int matchMethod = cv::TM_CCOEFF_NORMED;

    std::vector<ImageMatrix> imagesOrig;
    std::vector<ImageMatrix> imagesPrepared;

    double preprocessedTemplateScale = 0;

    cv::Rect refRect;
    cv::Rect captureRect;
    cv::Rect matchRect;
    cv::Point matchedCaptureOffset;
    std::vector<double> testScales;
    std::vector<int> testAngles;
    int lastTemplatedx;
    double lastMatch {0};
};

class CompassDetector : public Detector {
public:
    CompassDetector();
    ~CompassDetector() override = default;

    double match(ClassifyEnv& env) override;

    cv::Rect targetReferenceRect;
    cv::Rect targetRemapRect;
    int targetReferenceRadius;

    std::unique_ptr<ImageTemplate>  compassDetector;

    std::vector<std::unique_ptr<ImageFilter>> dotsFilters;
    std::vector<std::unique_ptr<ImageFilter>> navTargetFilters;
    std::vector<std::unique_ptr<ImageFilter>> distOCRFilters;
    std::vector<ImageTemplate::ImageMatrix> compassDotsOrig;
    std::vector<ImageTemplate::ImageMatrix> compassDotsPrepared;
    std::vector<ImageTemplate::ImageMatrix> navTargetOrig;
    std::vector<ImageTemplate::ImageMatrix> navTargetPrepared;
    XMat navTargetRemap1;
    XMat navTargetRemap2;

    double preprocessedDotsScale = 0;
    double preprocessedFOV = 0;
    double shipCompassScale = 1;
    std::string preprocessedShip;
    std::vector<double> baseTestScales;
    std::vector<double> navTargetScales;

    const double threshold_dot;

    int lastScaleIdx;
    double lastScale;
    int lastHemisphere; // -1: back, 0: not detected, +1: front
    double lastTgtPitch;
    double lastTgtYaw;
    double lastTgtRoll;

    cv::Rect dotCaptureRect;
    cv::Point2d dotSpherePosition;
    double lastDotValue;

    bool navTargetFound;
    cv::Point lastNavTargetOffset;
    std::string lastNavTargetText;
    dist_t lastNavDist;

    static void tryLowerUpperBoundsGUI(ClassifyEnv &env, cv::Rect referenceRect);
};

class TilesDetector : public ImageTemplate {
public:
    TilesDetector(const std::string& name, cv::Rect& tilesRect, const std::string& icons, cv::Rect& iconsRect,
                  int min_rows, int max_rows, int min_cols, int max_cols, int gap);
    ~TilesDetector() override = default;

    double match(ClassifyEnv& env) override;

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
    LineDetector(ImageTemplate* anchor, spEvalPoint p0, spEvalPoint p1);
    ~LineDetector() override = default;

    double match(ClassifyEnv& env) override;

    void normalizeRotatedRect(cv::RotatedRect& rr);
    void tryCannyParamsGUI(ClassifyEnv &env);

    std::unique_ptr<ImageTemplate> anchorDetector;
    std::vector<std::unique_ptr<ImageFilter>> filters;

    std::string name;
    cv::Point extendLT;
    cv::Point extendRB;
    const spEvalPoint referenceP0;
    const spEvalPoint referenceP1;
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
