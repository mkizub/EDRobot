//
// Created by mkizub on 16.06.2025.
//

#pragma once

#ifndef EDROBOT_CLASSIFYENV_H
#define EDROBOT_CLASSIFYENV_H

struct ResolvedEnv;

namespace widget {
class Widget;
class List;
class Screen;
}

class Capturer;

//
// Frame returned by Capturer, manages allocated memory and textures
//
class Frame {
protected:
    Frame(Capturer* owner, cv::Size size) : owner(owner), size(size) {}
    virtual ~Frame() = default;
public:
    virtual       bool valid() const = 0;
    virtual const cv::UMat& getColorTexture() const = 0;
    virtual const cv::Mat& getColorImage() const = 0;
    virtual const cv::Mat& getGrayImage() const = 0;

    static void recycle(Frame* p);

    const Capturer* const owner;
    const cv::Size size;
};
struct FrameRecycler {
    FrameRecycler() = default;
    void operator()(Frame* p) const {
        Frame::recycle(p);
    };
};
typedef std::unique_ptr<Frame,FrameRecycler> upFrame;

// Rect evaluator
class EvalRect {
public:
    EvalRect() = default;
    virtual ~EvalRect() = default;
    virtual cv::Rect calcReferenceRect(const ResolvedEnv& detectorState) const = 0;
};
typedef std::shared_ptr<EvalRect> spEvalRect;

class ConstRect : public EvalRect {
public:
    ConstRect(cv::Rect rect) : mRect(rect) {}
    cv::Rect calcReferenceRect(const ResolvedEnv& detectorState) const override { return mRect; };

    cv::Rect mRect;
};

extern spEvalRect makeEvalRect(json5pp::value jv, int width=0, int height=0);

// Transform evaluator
class EvalTransform {
public:
    EvalTransform(cv::Point tl, cv::Point tr, cv::Point br, cv::Point bl)
        : orig {tl, tr, br, bl}
    {
        float t_w = (float)cv::norm(orig[0] - orig[1]);
        float b_w = (float)cv::norm(orig[2] - orig[3]);
        float l_h = (float)cv::norm(orig[0] - orig[3]);
        float r_h = (float)cv::norm(orig[1] - orig[2]);
        float d_w = std::round(std::max(t_w, b_w));
        float d_h = std::round(std::max(l_h, r_h));
        const_cast<cv::Size&>(origSize) = { int(d_w), int(d_h) };
    }
    virtual ~EvalTransform() = default;
    virtual bool calcTransform(const ResolvedEnv& detectorState) = 0;
    virtual cv::Mat transformImage(const cv::Mat& image) const;

    const std::array<cv::Point2f,4> orig;   // tl, tr, br, bl
    const cv::Size origSize {};             // warped image always scaled to reference size
    std::array<cv::Point2f,4> transfromSrc; // in captured coordinates
    cv::Mat transfromMatrix {};
    bool valid {false};
};
typedef std::shared_ptr<EvalTransform> spEvalTransform;

class ConstTransform : public EvalTransform {
public:
    ConstTransform(cv::Point tl, cv::Point tr, cv::Point br, cv::Point bl);
    bool calcTransform(const ResolvedEnv& detectorState) override;
};

class LineTransform : public EvalTransform {
public:
    LineTransform(std::string line, cv::Point tl, cv::Point tr, cv::Point br, cv::Point bl);
    bool calcTransform(const ResolvedEnv& detectorState) override;

    const std::string lineDetector;
};

enum class ClsDetType {
    Detected,           // rect is detected by Template, text is the name of the template
    LineDetected,       // rect is detected by LineDetector, text is the name of the detector
    Tile,               // rect is detected as tile button
    Widget,             // rect is assumed to be a widget
    ListRow,            // rect is a list row (maybe commodity list)
};

struct ClassifiedRect {
    ClassifiedRect() = default;
    ClassifiedRect(ClsDetType cdt, bool warped, std::string txt, cv::Rect detRect)
            : cdt(cdt), warped(warped), text(std::move(txt)), detectedRect(detRect), u{}
    {}
    ClsDetType cdt;
    bool warped;
    std::string text;         // name of Template that detected this rect, or a text recognized by OCR, etc
    cv::Rect detectedRect;    // actually detected rect in reference coordinates
    union {
        struct {
            cv::Rect referenceRect;   // originally expected rect in reference coordinates
            double scale; // detected scale for multi-scale templates, environment (screen) scale is not counted
        } tdet;
        struct {
            cv::Point2f offset;
            float angle; // angle difference, in degrees, -90 <= angle <= +90
            float scale;
            cv::Point referenceP0;
            cv::Point referenceP1;
        } ldet;
        struct {
            int row;
            int col;
            int span; // column span
        } tile;
        struct {
            mutable WState ws;        // detected state for widgets
            mutable const widget::Widget* widget;
        } widg;
        struct {
            mutable WState ws;        // detected state for commodity row
            mutable const widget::List* list;
            mutable const Commodity* commodity;
        } lrow;
    } u;
};

struct ResolvedEnv {
    // in configuration all numbers are specified for reference screen size
    const cv::Size ReferenceScreenSize {1920, 1080};
    const cv::Point ReferenceScreenCenter {1920/2, 1080/2};
    // actual window size and position on image (screenshot)
    const cv::Rect monitorRect;
    const cv::Rect captureRect;

    ResolvedEnv& operator=(const ResolvedEnv& other);
    void init(const cv::Rect& monitorRect, const cv::Rect& captRect);
    void clear();
    bool isWarpMode() { return inWarpMode_; }
    void setWarpMode(bool on) { inWarpMode_ = on; }

    // a list of classified detected rects
    std::vector<ClassifiedRect> classified;

    bool needScaling() const { return needScaling_ && !inWarpMode_; }
    double getScale() const { return needScaling() ? scaleToCaptured_ : 1.0; }

    void cropToCapture(cv::Rect& rect) {
        if (!inWarpMode_)
            rect &= captureCrop;
        else
            rect &= captureCrop;
    }

    cv::Point2f scaleToCaptured(const cv::Point2f& point) const {
        return needScaling() ? point * scaleToCaptured_ : point;
    }
    cv::Point scaleToCaptured(const cv::Point& point) const {
        return needScaling() ? point * scaleToCaptured_ : point;
    }
    cv::Size  scaleToCaptured(const cv::Size& size) const {
        if (!needScaling())
            return size;
        return {int(size.width * scaleToCaptured_), int(size.height * scaleToCaptured_)};
    }
    cv::Point scaleToReference(const cv::Point& point) const {
        return needScaling() ? point / scaleToCaptured_ : point;
    }
    cv::Size  scaleToReference(const cv::Size& size) const {
        if (!needScaling())
            return size;
        return {int(size.width / scaleToCaptured_), int(size.height / scaleToCaptured_)};
    }

    cv::Point cvtReferenceToDesktop(const cv::Point& point) const;

    cv::Point2f cvtReferenceToCaptured(const cv::Point2f& point) const;
    cv::Point cvtReferenceToCaptured(const cv::Point& point) const;
    cv::Rect  cvtReferenceToCaptured(const cv::Rect& rect) const;

    cv::Point2f cvtCapturedToReference(const cv::Point2f& point) const;
    cv::Point cvtCapturedToReference(const cv::Point& point) const;
    cv::Rect  cvtCapturedToReference(const cv::Rect& rect) const;

    cv::Rect calcReferenceRect(const spEvalRect& er) const {
        if (!er)
            return {};
        return er->calcReferenceRect(*this);
    }
    cv::Rect calcCapturedRect(const spEvalRect& er) const {
        if (!er)
            return {};
        return cvtReferenceToCaptured(calcReferenceRect(er));
    }

    cv::Point2f unWarp(const cv::Point2f point) const;
    cv::Point unWarp(const cv::Point point) const;
    std::array<cv::Point,4> unWarp(const cv::Rect& rect) const;

protected:
    friend class Master;
    cv::Rect captureCrop;
    // reference-to-captured scale
    bool inWarpMode_ {false};
    bool needScaling_ {false};
    double scaleToCaptured_ {1};
    cv::Point captureCenter;
    cv::Rect warpRect;
    cv::Matx33d warpMatrix;
    cv::Matx33d unWarpMatrix;
};

struct ClassifyEnv : public ResolvedEnv {
    void init(const ResolvedEnv& rEnv, cv::Mat* colorImage, cv::Mat* grayImage);
    void init(const cv::Rect& monitorRect, const cv::Rect& captRect, upFrame&& frame);
    void warpPerspective(const spEvalTransform& transform);
    void clear();

    [[nodiscard]] const cv::UMat& getColorTexture() const {
        throw std::runtime_error("getColorTexture() not implemented");
        //return mFrame->getColorTexture();
    };
    [[nodiscard]] const cv::Mat&  getColorImage()   const {
        if (inWarpMode_)
            return mWarpedColorImage;
        return mFrame->getColorImage();
    };
    [[nodiscard]] const cv::Mat&  getGrayImage()    const {
        if (inWarpMode_)
            return mWarpedGrayImage;
        return mFrame->getGrayImage();
    };
    [[nodiscard]] cv::Mat&        getDebugImage()   const;
    [[nodiscard]] const cv::Mat&  getWarpedColorImage() const;
    [[nodiscard]] const cv::Mat&  getWarpedGrayImage()  const;

private:
    friend class Master;
    upFrame mFrame;
    mutable cv::Mat mDebugOverlay;
    spEvalTransform mWarpTransform;
    mutable cv::Mat mWarpedColorImage;
    mutable cv::Mat mWarpedGrayImage;
};

class TileRect : public EvalRect {
public:
    TileRect(const std::string& name, int row, int col)
        : name(name), row(row), col(col)
    {}
    cv::Rect calcReferenceRect(const ResolvedEnv& env) const override;

    const std::string name;
    const int row;
    const int col;
};

#endif //EDROBOT_CLASSIFYENV_H
