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
    Frame(Capturer* owner, cv::Size size) : owner(owner), size(size), timestamp{} {}
    virtual ~Frame() = default;
public:
    virtual bool valid() const = 0;
    virtual const XMat& getImage() const = 0;

    static void recycle(Frame* p);

    const Capturer* const owner;
    const cv::Size size;
    Timestamp timestamp;
};
struct FrameRecycler {
    FrameRecycler() = default;
    void operator()(Frame* p) const {
        Frame::recycle(p);
    };
};
typedef std::unique_ptr<Frame,FrameRecycler> upFrame;

class FrameWarp : public Frame {
public:
    FrameWarp(cv::Size size, XMat cImage)
            : Frame(nullptr, size)
    {
        colorImage = cImage;
    }
    ~FrameWarp() override = default;
    bool valid() const override { return true; }
    const XMat& getImage() const override { return colorImage; }

    XMat colorImage;
};

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
extern spEvalPoint makeEvalPoint(const widget::Widget& widget, const char* name, const json5pp::value& jv, FovScale* fov_scale);
extern spEvalRect makeEvalRect(const widget::Widget& widget, const char* name, const json5pp::value& jv, FovScale* fov_scale, bool relative);
extern spEvalLine makeEvalLine(const widget::Widget& widget, const char* name, const json5pp::value& jv, FovScale* fov_scale);

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
            float angle;  // detected rotation angle for multi-scale templates
            double match; // detector's match value
            cv::Rect matchRect;   // template match rect in screen coordinates
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
            cv::Rect capturedRect;      // in captured coordinates
            WState ws;                  // detected state for this tile
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

const cv::Size ReferenceScreenSize {1920, 1080};
const cv::Point ReferenceScreenCenter {1920/2, 1080/2};

// in configuration all numbers are specified for reference screen size
// actual window size and position on image (screenshot)
struct ResolvedEnv {

    ResolvedEnv();
    ResolvedEnv& operator=(const ResolvedEnv& other) = default;
    void init(const cv::Rect& frameRect, double scale);
    void clear();

    // a list of classified detected rects
    mutable std::vector<ClassifiedRect> classified;

    double getScale() const { return scaleToCaptured_; }

    void cropToCapture(cv::Rect& rect) {
        rect &= frameCrop;
    }

    cv::Point2f scaleToCaptured(const cv::Point2f& point) const {
        if (!needScale_)
            return point;
        return point * scaleToCaptured_;
    }
    cv::Point scaleToCaptured(const cv::Point& point) const {
        if (!needScale_)
            return point;
        return point * scaleToCaptured_;
    }
    cv::Size  scaleToCaptured(const cv::Size& size) const {
        if (!needScale_)
            return size;
        return {int(size.width * scaleToCaptured_), int(size.height * scaleToCaptured_)};
    }
    cv::Rect scaleToCaptured(const cv::Rect& rect) const {
        if (!needScale_)
            return rect;
        return {rect.tl() * scaleToCaptured_, rect.br() * scaleToCaptured_};
    }
    cv::Line scaleToCaptured(const cv::Line& line) const {
        if (!needScale_)
            return line;
        return {line.p0() * scaleToCaptured_, line.p1() * scaleToCaptured_};
    }
    cv::Point scaleToReference(const cv::Point& point) const {
        if (!needScale_)
            return point;
        return point / scaleToCaptured_;
    }
    cv::Size  scaleToReference(const cv::Size& size) const {
        if (!needScale_)
            return size;
        return {int(size.width / scaleToCaptured_), int(size.height / scaleToCaptured_)};
    }

    template<typename _Tp>
    cv::Point_<_Tp> cvtReferenceToCaptured(const cv::Point_<_Tp>& point) const {
        if (!needScale_)
            return point;
        return point * scaleToCaptured_;
    }
    template<typename _Tp>
    cv::Rect_<_Tp>  cvtReferenceToCaptured(const cv::Rect_<_Tp>& rect) const {
        if (!needScale_)
            return rect;
        return cv::Rect_<_Tp>(cvtReferenceToCaptured(rect.tl()), cvtReferenceToCaptured(rect.br()));
    }
    template<typename _Tp>
    cv::Line_<_Tp>  cvtReferenceToCaptured(const cv::Line_<_Tp>& line) const {
        if (!needScale_)
            return line;
        return cv::Line_<_Tp>(cvtReferenceToCaptured(line.p0()), cvtReferenceToCaptured(line.p1()));
    }

    template<typename _Tp>
    cv::Point_<_Tp> cvtCapturedToReference(const cv::Point_<_Tp>& point) const{
        if (!needScale_)
            return point;
        return point / scaleToCaptured_;
    }
    template<typename _Tp>
    cv::Rect_<_Tp>  cvtCapturedToReference(const cv::Rect_<_Tp>& rect) const {
        if (!needScale_)
            return rect;
        return cv::Rect_<_Tp>(cvtCapturedToReference(rect.tl()), cvtCapturedToReference(rect.br()));
    }
    template<typename _Tp>
    cv::Line_<_Tp>  cvtCapturedToReference(const cv::Line_<_Tp>& line) const {
        if (!needScale_)
            return line;
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

    void setDebugMatch(bool on) { isDebugMatch_ = on; }
    bool isDebugMatch() const { return isDebugMatch_; }
protected:
    cv::Rect frameRect;
    cv::Rect frameCrop;
    // reference-to-captured scale
    bool needScale_ {false};
    bool needCrop_ {false};
    bool isDebugMatch_ {false};
    double scaleToCaptured_ {1};
};

struct ClassifyEnv : public ResolvedEnv {
    ClassifyEnv() = default;
    void init(upFrame&& frame);
    void init(XMat warpedImage, double scale=1.0);
    void init(XMat warpedImage, const cv::Matx33d& unWarpMatrix);
    void init(XMat warpedImage, const cv::Matx23d& unWarpMatrix);
    void clear();

    [[nodiscard]] const XMat& getColorImage() const;

    template<typename _Tp>
    cv::Point_<_Tp> unWarp(const cv::Point_<_Tp> point) const;
    template<typename _Tp>
    std::array<cv::Point_<_Tp>,4> unWarp(const cv::Rect_<_Tp>& rect) const;

private:
    friend class Master;
    upFrame mFrame;
    mutable XMat mColorImage;
    cv::Matx33d unWarpMatrix;
};

class TileRect : public EvalRect {
public:
    TileRect(const std::string& name)
        : name(name)
    {}
    cv::Rect calcReferenceRect(const ResolvedEnv& env) const override;

    const std::string name;
};

#endif //EDROBOT_CLASSIFYENV_H
