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

class ConstDetector : public Detector {
public:
    ConstDetector(double value)
            : value(value)
    {}
    ~ConstDetector() override = default;

    double match(ClassifyEnv& env) override { return value; }
private:
    double value;
};

class Histogram {
public:
    enum class Mode {
        Gray, Hsv, Luv, BGR
    };
    Histogram(Mode mode, const cv::Rect rect, int bin=8) : mMode(mode), mRect(rect), mBin(bin) {}

    bool calc(ClassifyEnv& env);
    bool calc(cv::Mat image);

    Mode mMode;
    cv::Vec3b mLastColor;
    cv::Rect mRect;
    int mBin;
};

class ImageFilter {
public:
    struct Params {
        bool convertToFloat;
    };
    virtual ~ImageFilter() = default;
    virtual XMat apply(XMat image, Params params) = 0;
};
class ThresholdFilter : public ImageFilter {
public:
    explicit ThresholdFilter(double thr=127, double max=255) : thr(thr), max(max) {}
    XMat apply(XMat image, Params params) final;
    const double thr;
    const double max;
};
class ChannelFilter : public ImageFilter {
public:
    enum Channel {red, green, blue, gray, hue, sat, value};
    explicit ChannelFilter(Channel channel) : channel(channel) {}
    XMat apply(XMat image, Params params) final;
    const Channel channel;
};
class GainBiasFilter : public ImageFilter {
public:
    explicit GainBiasFilter(double gain, double bias=0) : gain(gain), bias(bias) {}
    XMat apply(XMat image, Params params) final;
    const double gain;
    const double bias;
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
    explicit LaplacianFilter(int kern=3, double scale=1, double delta=0) : kern(kern), scale(scale), delta(delta) {}
    XMat apply(XMat image, Params params) final;
    const int kern;
    const double scale;
    const double delta;
};
class SobelFilter : public ImageFilter {
public:
    explicit SobelFilter(int kern=3, double scale=1, double delta=0) : kern(kern), scale(scale), delta(delta) {}
    XMat apply(XMat image, Params params) final;
    const int kern;
    const double scale;
    const double delta;
};
class ScharrFilter : public ImageFilter {
public:
    explicit ScharrFilter(bool vert, double scale=1) : vert(vert), scale(scale) {}
    XMat apply(XMat image, Params params) final;
    const bool vert;
    const double scale;
};
class GradientFilter : public ImageFilter {
public:
    explicit GradientFilter(bool vert, double scale=1) : vert(vert), scale(scale) {}
    XMat apply(XMat image, Params params) final;
    const bool vert;
    const double scale;
};
class LinesFilter : public ImageFilter {
public:

    explicit LinesFilter(bool vert, double gradient_scale=1, int gradient_threshold=45, int dilatePos=2, int dilateNeg=2, int erode=0);
    XMat apply(XMat image, Params params) final;

    const bool vert;
    double gradient_scale;
    int gradient_threshold;
    int dilatePos; // dilate down for positive gradients
    int dilateNeg; // dilate up for negative gradients
    int erode; // erode after positive and negative (dilated) masks merged (bitwise and)

    cv::Mat kernel_2;
    cv::Mat kernel_3;
};
class CompassFilter : public ImageFilter {
public:
    explicit CompassFilter() = default;
    XMat apply(XMat image, Params params) final;
};
class EdgeByBoxFilter : public ImageFilter {
public:
    EdgeByBoxFilter(int kern=5, double scale=2.0, double thr=0) : kern(kern), scale(scale), threshold(thr)  {}
    XMat apply(XMat image, Params params) final;
    const int kern;
    const double scale;
    const double threshold;
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
class HsvMaskFilter : public ImageFilter {
public:
    HsvMaskFilter() {}
    std::vector<std::pair<cv::Vec3b,cv::Vec3b>> rangesU;
    std::vector<std::pair<cv::Vec3f,cv::Vec3f>> rangesF;
    void calcMask(XMat image, XMat& mask, XMat& hsv);
    XMat apply(XMat image, Params params) override;
};
class HsvColorCropFilter : public HsvMaskFilter {
public:
    HsvColorCropFilter() {}
    XMat apply(XMat image, Params params) final;
};
class HsvGrayCropFilter : public HsvMaskFilter {
public:
    HsvGrayCropFilter() {}
    XMat apply(XMat image, Params params) final;
};
class HsvValueCropFilter : public HsvMaskFilter {
public:
    HsvValueCropFilter() {}
    XMat apply(XMat image, Params params) final;
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
        double u_norm;
        double f_norm;
    };
    struct MatchResult {
        ImageMatrix* im {nullptr};
        int index {-1};
        double value {-1};
        cv::Point loc;
    };

    ImageTemplate(const std::string& filename, spEvalRect rect);
    ~ImageTemplate() override = default;
    void setTemplate(const std::string& filename);

    double match(ClassifyEnv& env) override;
    double match(ClassifyEnv& env, XMat gameImage, cv::Point* gameImageOffset);

    static bool loadImageAndMask(const std::string& filename, XMat& image);

    double toResult(double matchValue); // something like logistic regression, S-curve

    static ImageMatrix prepareImageMatrix(const ClassifyEnv& env, const std::vector<std::unique_ptr<ImageFilter>>& filters, XMat image, double scale, int angle, const std::string& name, ImageFilter::Params params={});
    static XMat applyFilters(const std::vector<std::unique_ptr<ImageFilter>>& filters, XMat image, ImageFilter::Params params={});
    static XMat scaleImage(XMat image, double scaleX, double scaleY = 0);
    static XMat rotateImage(XMat image, int angle, double scale);

    static cv::Rect makeOptimalMatchRect(cv::Rect);
    static void matchTemplates(int matchMethod, const XMat& imagePrepared, std::vector<ImageMatrix>& templPrepared, MatchResult& result);
    //static void fixNaNinResult(cv::Mat& result, const std::string& filename);

//protected:
    void prepareImages(ClassifyEnv& env);
    std::string name;
    std::string filename;
    spEvalRect refEvalRect;
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

    cv::Point refOrig;
    cv::Size refSize;
    cv::Rect captureRect;
    cv::Rect matchRect;
    cv::Point matchedCaptureOffset;
    std::vector<double> testScales;
    std::vector<int> testAngles;
    int lastTemplatedx;
    double lastMatch {0};
};

class AnchorDetector : public ImageTemplate {
public:
    AnchorDetector(const std::string& filename, spEvalRect rect, cv::Point anchor_of)
        : ImageTemplate(filename, rect)
        , anchor_of(anchor_of)
        {}
    ~AnchorDetector() override = default;

    const cv::Point anchor_of;
};

class CompassDetector : public Detector {
public:
    CompassDetector();
    ~CompassDetector() override = default;
    void clear() {
        lastHemisphere = 0;
        lastTgtPitch = 0;
        lastTgtYaw = 0;
        lastTgtRoll = 0;
        lastTgtAngle = 0;
        dotCaptureRect = {};
        dotSpherePosition = {};
        navTargetFound = false;
        lastNavTargetOffset = {};
        lastNavDist = {};
    }

    void loadCompass();

    double match(ClassifyEnv& env) override;

    cv::Rect targetReferenceRect;
    cv::Rect targetRemapRect;

    cv::Size compassRefSize;
    std::string compassImageName;

    std::unique_ptr<ImageTemplate>  compassDetector;

    std::vector<std::unique_ptr<ImageFilter>> dotsFilters;
    std::vector<std::unique_ptr<ImageFilter>> navTargetFilters;
    std::vector<std::unique_ptr<ImageFilter>> distOCRFilters;
    std::vector<ImageTemplate::ImageMatrix> compassDotsOrig;
    std::vector<ImageTemplate::ImageMatrix> compassDotsPrepared;
    std::vector<ImageTemplate::ImageMatrix> navTargetOrig;
    std::vector<ImageTemplate::ImageMatrix> navTargetPrepared;
    int navTargetReferenceRadius;
    cv::Mat navTargetRemapXY;
    XMat navTargetRemap1;
    XMat navTargetRemap2;

    double preprocessedDotsScale = 0;
    double preprocessedFOV = 0; // config fov
    double captureFovX = 0;
    double captureFovY = 0;
    std::string preprocessedShip;
    std::vector<double> navTargetScales;

    const double threshold_dot;

    int8_t lastHemisphere; // -1: back, 0: not detected, +1: front
    double lastTgtPitch;
    double lastTgtYaw;
    double lastTgtRoll;
    double lastTgtAngle;

    cv::Rect dotCaptureRect;
    cv::Point2d dotSpherePosition;

    bool navTargetFound;
    cv::Point lastNavTargetOffset;
    dist_t lastNavDist;

    static void tryLowerUpperBoundsGUI(ClassifyEnv &env, cv::Rect referenceRect);
};

class TilesDetector : public ImageTemplate {
public:
    TilesDetector(const std::string& name, cv::Rect& tilesRect, const std::string& icons, cv::Rect& iconsRect,
                  int min_rows, int max_rows, int min_cols, int max_cols, int gap);
    ~TilesDetector() override = default;

    double match(ClassifyEnv& env) override;
    std::vector<cv::Rect> detectColumns(XMat& roiImage, int gap);
    std::vector<cv::Rect> detectRows(XMat& roiImage, int gap);

    const std::string name;
    cv::Rect mTilesRect;
    std::vector<std::string> mIconFiles;
    enum IconAlign {
        Center, TopLeft
    } mIconAlign;
    bool hudTryHard {false};

    std::vector<ClassifiedRect> mDetectedTiles;

private:
    struct Range {
        int bgn, end, val;
    };
    std::vector<Range> split(bool columns, uchar* reduced, int size, int gap, int& threshold);

    int mMinRows;
    int mMaxRows;
    int mMinCols;
    int mMaxCols;
    int mGap;
};

} // namespace detect

#endif //EDROBOT_TEMPLATE_H
