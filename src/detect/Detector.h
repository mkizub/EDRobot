//
// Created by mkizub on 23.05.2025.
//

#pragma once

#ifndef EDROBOT_TEMPLATE_H
#define EDROBOT_TEMPLATE_H

#include "Filters.h"

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
    Histogram(Mode mode, int bin=8) : mMode(mode), mBin(bin) {}

#ifdef EDROBOT_USE_OPENCL
    bool calc(XMat image);
#endif
    bool calc(cv::Mat image);

    WState guessWState();
#ifdef EDROBOT_USE_OPENCL
    WState guessWState(XMat image) {
        if (!calc(image))
            return WState::Unknown;
        return guessWState();
    }
#endif
    WState guessWState(cv::Mat image) {
        if (!calc(image))
            return WState::Unknown;
        return guessWState();
    }

    Mode mMode;
    cv::Vec3b mLastColor;
    int mBin;
};

class ImageTemplate : public Detector {
public:
    struct ImageMatrix {
        ~ImageMatrix() {
            templImageF.release();
            templImageU.release();
        }
        double scale;
        double angle;
        std::string name;
        XMat templImageU;
        XMat templImageF;
        int16_t org_w;
        int16_t org_h;
        int16_t opt_w;
        int16_t opt_h;
        int16_t opt_l;
        int16_t opt_t;
        int16_t opt_r;
        int16_t opt_b;
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
    ~ImageTemplate() override;
    void setTemplate(const std::string& filename);

    double match(ClassifyEnv& env) override;

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
    cv::Rect withRefRect;
    cv::Rect captureRect;
    cv::Rect matchRect;
    cv::Point matchedCaptureOffset;
    std::vector<double> testScales;
    std::vector<int> testAngles;
    bool cropOptimalSize {false};
    MatchResult lastMatchResult;
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

} // namespace detect

#endif //EDROBOT_TEMPLATE_H
