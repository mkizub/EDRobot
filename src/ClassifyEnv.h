//
// Created by mkizub on 16.06.2025.
//

#pragma once

#ifndef EDROBOT_CLASSIFYENV_H
#define EDROBOT_CLASSIFYENV_H

struct ResolvedEnv;

//
// Frame returned by Capturer, manages allocated memory and textures
//
class Frame {
protected:
    Frame(Capturer* owner, cv::Size size) : owner(owner), size(size) {}
    virtual ~Frame() = default;
public:
    virtual bool valid() const = 0;
    virtual const XMat& getImage() const = 0;

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

// Point evaluator
class EvalPoint {
public:
    EvalPoint() = default;
    virtual ~EvalPoint() = default;
    virtual cv::Point calcReferencePoint(const ResolvedEnv& detectorState) const = 0;
};

class ConstPoint : public EvalPoint {
public:
    ConstPoint(cv::Point point) : mPoint(point) {}
    cv::Point calcReferencePoint(const ResolvedEnv& detectorState) const override { return mPoint; };

    cv::Point mPoint;
};

class RefPoint : public EvalPoint {
public:
    RefPoint(const widget::BaseDialog& dlg, std::string name, std::string scope) : mDlg(dlg), mName(name), mScope(scope) {}
    cv::Point calcReferencePoint(const ResolvedEnv& detectorState) const override;

    const widget::BaseDialog& mDlg;
    std::string mName;
    std::string mScope;
};

// Rect evaluator
class EvalRect {
public:
    EvalRect() = default;
    virtual ~EvalRect() = default;
    virtual cv::Rect calcReferenceRect(const ResolvedEnv& detectorState) const = 0;
};

class ConstRect : public EvalRect {
public:
    ConstRect(int x, int y, int w, int h) : mRect(x,y,w,h) {}
    ConstRect(cv::Rect rect) : mRect(rect) {}
    cv::Rect calcReferenceRect(const ResolvedEnv& detectorState) const override { return mRect; };

    cv::Rect mRect;
};

class RefRect : public EvalRect {
public:
    RefRect(const widget::BaseDialog& dlg, std::string name, std::string scope) : mDlg(dlg), mName(name), mScope(scope) {}
    cv::Rect calcReferenceRect(const ResolvedEnv& detectorState) const override;

    const widget::BaseDialog& mDlg;
    std::string mName;
    std::string mScope;
};

// Line evaluator
class EvalLine {
public:
    EvalLine() = default;
    virtual ~EvalLine() = default;
    virtual cv::Line calcReferenceLine(const ResolvedEnv& detectorState) const = 0;
};

class ConstLine : public EvalLine {
public:
    ConstLine(int x1, int y1, int x2, int y2) : mLine(x1,y1,x2,y2) {}
    ConstLine(cv::Line line) : mLine(line) {}
    cv::Line calcReferenceLine(const ResolvedEnv& detectorState) const override { return mLine; };

    cv::Line mLine;
};

class RefLine : public EvalLine {
public:
    RefLine(const widget::BaseDialog& dlg, std::string name, std::string scope) : mDlg(dlg), mName(name), mScope(scope) {}
    cv::Line calcReferenceLine(const ResolvedEnv& detectorState) const override;

    const widget::BaseDialog& mDlg;
    std::string mName;
    std::string mScope;
};

typedef std::shared_ptr<EvalPoint> spEvalPoint;
typedef std::shared_ptr<EvalRect> spEvalRect;
typedef std::shared_ptr<EvalLine> spEvalLine;
extern spEvalPoint makeEvalPoint(const widget::Widget& widget, const char* name, const json5pp::value& jv);
extern spEvalRect makeEvalRect(const widget::Widget& widget, const char* name, const json5pp::value& jv);
extern spEvalLine makeEvalLine(const widget::Widget& widget, const char* name, const json5pp::value& jv);

// Transform evaluator
class EvalTransform {
public:
    EvalTransform(spEvalPoint tl, spEvalPoint tr, spEvalPoint br, spEvalPoint bl, cv::Size size)
        : orig {tl, tr, br, bl}
        , origSize(size)
    {
    }
    virtual ~EvalTransform() = default;
    virtual bool calcTransform(const ResolvedEnv& detectorState) = 0;
    virtual XMat transformImage(const XMat& image) const;

    const std::array<spEvalPoint,4> orig;   // tl, tr, br, bl
    const cv::Size origSize {};             // warped image always scaled to reference size
    std::array<cv::Point2f,4> transformSrc; // in captured coordinates
    cv::Mat transformMatrix {};
    bool valid {false};
};
typedef std::shared_ptr<EvalTransform> spEvalTransform;

class ConstTransform : public EvalTransform {
public:
    ConstTransform(spEvalPoint tl, spEvalPoint tr, spEvalPoint br, spEvalPoint bl, cv::Size size);
    bool calcTransform(const ResolvedEnv& detectorState) override;
    bool useCaptured;
};

class LineTransform : public EvalTransform {
public:
    LineTransform(std::vector<std::string> lines, spEvalPoint tl, spEvalPoint tr, spEvalPoint br, spEvalPoint bl, cv::Size size);
    bool calcTransform(const ResolvedEnv& detectorState) override;

    const std::vector<std::string> lineDetector;
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
            int angle;  // detected rotation angle for multi-scale templates
            double match; // detector's match value
        } tdet;
        struct {
            cv::Line2f referenceLine;
            float scale;
            float angle; // angle difference, in degrees, -90 <= angle <= +90
            double match; // detector's match value
            cv::Point2f offset;
            detect::LineDetector* detector;
        } ldet;
        struct {
            int row;
            int col;
            int span; // column span
        } tile;
        struct {
            cv::Rect referenceRect;     // originally expected rect in reference coordinates
            WState ws;                  // detected state for widgets
            const widget::Widget* widget;
        } widg;
        struct {
            cv::Rect capturedRect;      // in captured coordinates
            WState ws;                  // detected state for commodity row
            const widget::List* list;
            const Commodity* commodity;
            int text_confidence;        // OCR confidence for OCR text
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

    ResolvedEnv();
    ResolvedEnv& operator=(const ResolvedEnv& other);
    void init(const cv::Rect& monitorRect, const cv::Rect& captRect);
    void clear();
    bool isWarpMode() const { return inWarpMode_; }
    void setWarpMode(bool on) { inWarpMode_ = on; }

    // a list of classified detected rects
    std::vector<ClassifiedRect> classified;

    bool needScaling() const { return needScaling_ && !inWarpMode_; }
    double getScale() const { return needScaling() ? scaleToCaptured_ : 1.0; }

    void cropToCapture(cv::Rect& rect) {
        if (inWarpMode_)
            rect &= warpRect;
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
    cv::Rect scaleToCaptured(const cv::Rect& rect) const {
        if (!needScaling())
            return rect;
        return {rect.tl() * scaleToCaptured_, rect.br() * scaleToCaptured_};
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

    template<typename _Tp>
    cv::Point_<_Tp> cvtReferenceToCaptured(const cv::Point_<_Tp>& point) const {
        if (!needScaling_ || inWarpMode_)
            return point;
        cv::Point_<_Tp> relative = point - cv::Point_<_Tp>(ReferenceScreenCenter);
        relative *= scaleToCaptured_;
        return relative + cv::Point_<_Tp>(captureCenter);
    }
    template<typename _Tp>
    cv::Rect_<_Tp>  cvtReferenceToCaptured(const cv::Rect_<_Tp>& rect) const {
        return cv::Rect_<_Tp>(cvtReferenceToCaptured(rect.tl()), cvtReferenceToCaptured(rect.br()));
    }
    template<typename _Tp>
    cv::Line_<_Tp>  cvtReferenceToCaptured(const cv::Line_<_Tp>& line) const {
        return cv::Line_<_Tp>(cvtReferenceToCaptured(line.p0()), cvtReferenceToCaptured(line.p1()));
    }

    template<typename _Tp>
    cv::Point_<_Tp> cvtCapturedToReference(const cv::Point_<_Tp>& point) const{
        if (!needScaling_ || inWarpMode_)
            return point;
        cv::Point_<_Tp> relative = point - cv::Point_<_Tp>(captureCenter);
        relative /= scaleToCaptured_;
        return relative + cv::Point_<_Tp>(ReferenceScreenCenter);
    }
    template<typename _Tp>
    cv::Rect_<_Tp>  cvtCapturedToReference(const cv::Rect_<_Tp>& rect) const {
        return cv::Rect_<_Tp>(cvtCapturedToReference(rect.tl()), cvtCapturedToReference(rect.br()));
    }
    template<typename _Tp>
    cv::Line_<_Tp>  cvtCapturedToReference(const cv::Line_<_Tp>& line) const {
        return cv::Line_<_Tp>(cvtCapturedToReference(line.p0()), cvtCapturedToReference(line.p1()));
    }

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

    template<typename _Tp>
    cv::Point_<_Tp> unWarp(const cv::Point_<_Tp> point) const;
    template<typename _Tp>
    std::array<cv::Point_<_Tp>,4> unWarp(const cv::Rect_<_Tp>& rect) const;

    bool isDebugMatch() const { return isDebugMatch_; }
protected:
    friend class Master;
    cv::Rect captureCrop;
    // reference-to-captured scale
    bool inWarpMode_ {false};
    bool needScaling_ {false};
    bool isDebugMatch_ {false};
    double scaleToCaptured_ {1};
    cv::Point captureCenter;
    cv::Rect warpRect;
    cv::Matx33d warpMatrix;
    cv::Matx33d unWarpMatrix;
};

struct ClassifyEnv : public ResolvedEnv {
    ClassifyEnv() = default;
    void init(const ResolvedEnv& rEnv, cv::Mat* colorImage, cv::Mat* grayImage);
    void init(const cv::Rect& monitorRect, const cv::Rect& captRect, upFrame&& frame);
    void warpPerspective(const spEvalTransform& transform);
    void clear();

    [[nodiscard]] const XMat& getColorImage() const;

    [[nodiscard]] const XMat& getGrayImage() const;

    [[nodiscard]] const XMat& getWarpedColorImage() const;
    [[nodiscard]] const XMat& getWarpedGrayImage() const;

    [[nodiscard]] cv::Mat&       getDebugImage()   const;
private:
    friend class Master;
    upFrame mFrame;
    mutable XMat mColorImage;
    mutable XMat mGrayImage;
    spEvalTransform mWarpTransform;
    mutable XMat mWarpedColorImage;
    mutable XMat mWarpedGrayImage;

    mutable cv::Mat mDebugOverlay;
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
