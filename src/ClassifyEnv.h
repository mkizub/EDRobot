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

enum class ClsDetType {
    TemplateDetected,   // rect is detected by Template, text is the name of the template
    Widget,             // rect is assumed to be a widget
    ListRow,            // rect is a list row (maybe commodity list)
};

struct ClassifiedRect {
    ClassifiedRect() = default;
    ClassifiedRect(ClsDetType cdt, std::string txt, cv::Rect detRect)
            : cdt(cdt), text(std::move(txt)), detectedRect(detRect), u{}
    {}
    ClsDetType cdt;
    std::string text;         // name of Template that detected this rect, or a text recognized by OCR, etc
    cv::Rect detectedRect;    // actually detected rect in reference coordinates
    union {
        struct {
            cv::Rect referenceRect;   // originally expected rect in reference coordinates
            double scale; // detected scale for multi-scale templates, environment (screen) scale is not counted
        } templ;
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
    const cv::Rect captureCrop;

    ResolvedEnv& operator=(const ResolvedEnv& other);
    void init(const cv::Rect& monitorRect, const cv::Rect& captRect);
    void clear();

    // a list of classified detected rects
    std::vector<ClassifiedRect> classified;

    bool needScaling() const { return needScaling_; }
    double getScale() const { return scaleToCaptured_; }

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

protected:
    friend class Master;
    // reference-to-captured scale
    bool needScaling_ {false};
    double scaleToCaptured_ {1};
    cv::Point captureCenter;
};

struct ClassifyEnv : public ResolvedEnv {
    void init(const cv::Rect& monitorRect, const cv::Rect& captRect, upFrame&& frame);
    void clear();

    [[nodiscard]] const cv::UMat& getColorTexture() const { return mFrame->getColorTexture(); };
    [[nodiscard]] const cv::Mat&  getColorImage()   const { return mFrame->getColorImage();   };
    [[nodiscard]] const cv::Mat&  getGrayImage()    const { return mFrame->getGrayImage();    };
    [[nodiscard]] cv::Mat&        getDebugImage()   const;

private:
    friend class Master;
    upFrame mFrame;
    mutable cv::Mat mDebugOverlay;
};

class TileRect : public EvalRect {
public:
    TileRect(const std::string& name) : name(name) {}
    cv::Rect calcReferenceRect(const ResolvedEnv& env) const override;

    const std::string name;
};

#endif //EDROBOT_CLASSIFYENV_H
