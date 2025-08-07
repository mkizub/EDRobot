//
// Created by mkizub on 31.05.2025.
//

// conflicts with _() of gettext, have to include before pch.h
#include <peglib/peglib.h>

#include "pch.h"

#include "EDWidget.h"
#include "OCR.h"
#include "detect/Detector.h"

#ifndef NDEBUG
//#include <cpptrace/cpptrace.hpp>
#include "cpptrace/from_current.hpp"
#include "Master.h"

#endif

#ifdef CPPTRACE_TRY
# define TRY CPPTRACE_TRY
# define CATCH(param) CPPTRACE_CATCH(param)
# define GET_EXCEPTION_STACK_TRACE cpptrace::from_current_exception().to_string()
#else
# define TRY try
# define CATCH(param) catch(param)
# ifdef _GLIBCXX_HAVE_STACKTRACE
#  include <stacktrace>
#  define GET_EXCEPTION_STACK_TRACE std::stacktrace::current()
# else
#  define GET_EXCEPTION_STACK_TRACE "(stack trace unavailable)"
# endif
#endif


namespace widget {

typedef std::shared_ptr<peg::Ast> spAst;

void Widget::addSubItem(Widget *sub) {
    if (!sub)
        return;
    if (!sub->parent)
        sub->parent = this;
    have.push_back(sub);
}

Widget::~Widget() {
    oracle.reset();
}

Widget::Widget(WidgetType tp, const std::string &name, Widget *parent)
    : tp(tp)
    , name(name)
    , parent(nullptr)
    , path((parent && parent->tp != WidgetType::Root) ? parent->path + ":" + name : name)
    , oracle(nullptr)
{
    //if (parent)
    //    parent->addSubItem(this);
}

void Widget::setRect(const char* name, const json5pp::value& value) {
    rect = makeEvalRect(*this, name, value);
}

cv::Rect Widget::calcReferenceRect(const ClassifyEnv& env) const {
    if (!rect)
        return {};
    return rect->calcReferenceRect(env);
}

static bool safeDetect(Widget* widget, Widget::DetectParams& params) {
    if (!widget)
        return false;
    bool detected = false;
    TRY {
        detected = widget->detect(params);
    } CATCH(const std::exception& e) {
        LOG(ERROR) << "Exception in widget '" << widget->path << "' detection: " << e.what() << std::endl << GET_EXCEPTION_STACK_TRACE;
#ifndef NDEBUG
        throw;
#endif
    }
    return detected;
}

bool Root::detect(DetectParams& params) {
    if (params.level < DetectLevel::Screen)
        return true;
    for (auto widget: this->have) {
        if (!widget || widget->tp != WidgetType::Screen)
            continue;
        if (safeDetect(widget, params))
            return true;
    }
    return false;
}

bool Screen::detect(DetectParams& params) {
    if (!this->checkStatus(params.cfg))
        return false;

    if (oracle) {
        double match = oracle->match(params.env);
        if (match < 0.5)
            return false;
    }

    if (transform) {
        params.env.warpPerspective(transform);
        if (transform->valid)
            params.env.setWarpMode(transform->valid);
    }

    bool modeMatch = true;
    for (auto mode: this->have) {
        if (!mode || !(mode->tp == WidgetType::Mode || mode->tp == WidgetType::Dialog))
            continue;
        modeMatch = safeDetect(mode, params);
        if (modeMatch)
            break;
    }
    if (!modeMatch) {
        params.env.setWarpMode(false);
        return false;
    }
    if (!params.uiState.screen)
        params.uiState.screen = this;
    if (!params.uiState.widget)
        params.uiState.widget = this;

    if (params.level <= DetectLevel::Screen) {
        params.env.setWarpMode(false);
        return true;
    }

    for (auto widget: this->have) {
        if (!widget || widget->tp == WidgetType::Mode || widget->tp == WidgetType::Dialog)
            continue;
        safeDetect(widget, params);
    }

    params.env.setWarpMode(false);
    return true;
}

bool Dialog::detect(DetectParams& params) {
    if (oracle) {
        double match = oracle->match(params.env);
        if (match < 0.5)
            return false;
        if (!params.uiState.widget || params.uiState.widget == parent)
            params.uiState.widget = this;
    }

    bool modeMatch = true;
    for (auto mode: this->have) {
        if (!mode || mode->tp != WidgetType::Mode)
            continue;
        modeMatch = safeDetect(mode, params);
        if (modeMatch)
            break;
    }
    if (!modeMatch)
        return false;

    for (auto widget: this->have) {
        if (!widget || widget->tp == WidgetType::Mode)
            continue;
        safeDetect(widget, params);
    }

    return true;
}

bool Mode::detect(DetectParams& params) {
    if (!oracle)
        return false;
    double match = oracle->match(params.env);
    if (match < 0.5)
        return false;
    if (!params.uiState.widget || params.uiState.widget == parent || params.uiState.widget == parent->parent)
        params.uiState.widget = this;

    for (auto widget: this->have) {
        safeDetect(widget, params);
    }

    return true;
}

bool Label::detect(DetectParams& params) {
    cv::Rect r = params.env.calcReferenceRect(this->rect);
    if (ocr_bot > 0) {
        int reference_line_height = (int) std::round(ocr::LINE_HEIGHT * double(ocr_bot - ocr_top) / double(ocr::ASCENT+ocr::DESCENT));
        int lines = (int) std::round(double(r.height) / double(reference_line_height));
        r.height = lines * reference_line_height;
    }
    params.env.classified.emplace_back(ClsDetType::Widget, params.env.isWarpMode(), this->name, r);
    ClassifiedRect& clsLblRect = params.env.classified.back();
    clsLblRect.u.widg.referenceRect = r;
    clsLblRect.u.widg.ws = WState::Unknown;
    clsLblRect.u.widg.widget = this;
    return true;
}

bool BaseButton::detect(DetectParams& params) {
    ClassifyEnv& env = params.env;
    cv::Rect expectedR = env.calcReferenceRect(this->rect);
    if (expectedR.empty())
        return false;
    cv::Rect detectedR = expectedR;
    cv::Rect captureR = env.cvtCapturedToReference(expectedR);
    if (captureR.empty())
        return false;

    if (extendLT != cv::Point() || extendRB != cv::Point()) {
        cv::Rect extendR = {expectedR.tl() - extendLT, expectedR.br() + extendRB};
        cv::Rect matchR = env.cvtReferenceToCaptured(extendR);

        cv::Vec3b hsvColorMin {0, 127, 30};
        cv::Vec3b hsvColorMax {30, 255, 255};
        XMat hsvImage;
        cv::cvtColor(env.getColorImage()(matchR), hsvImage, cv::COLOR_BGR2HSV);
        XMat thrImage;
        cv::inRange(hsvImage, hsvColorMin, hsvColorMax, thrImage);

        std::vector<std::vector<cv::Point>> contours;
        cv::findContoursLinkRuns(thrImage, contours);
        for (auto &cont: contours) {
            std::vector<cv::Point> convex;
            cv::convexHull(cont, convex);
            if (convex.size() >= 4) {
                std::vector<cv::Point> approx;
                cv::approxPolyN(convex, approx, 4, 5, true);
                cv::Rect bbox = cv::boundingRect(approx);
                bbox &= cv::Rect(cv::Point(),matchR.size());
                if (bbox.width > captureR.width*0.9 && bbox.height > captureR.height*0.9 &&
                    bbox.width < captureR.width*1.1 && bbox.height < captureR.height*1.2)
                {
                    captureR = {matchR.tl() + bbox.tl(), bbox.size()};
                    detectedR = env.cvtCapturedToReference(captureR);
                    break;
                }
            }
        }
    }

    env.classified.emplace_back(ClsDetType::Widget, env.isWarpMode(), this->name, detectedR);
    ClassifiedRect& clsBtnRect = env.classified.back();
    clsBtnRect.u.widg.referenceRect = expectedR;
    clsBtnRect.u.widg.ws = WState::Unknown;
    clsBtnRect.u.widg.widget = this;

    detect::Histogram histDet(detect::Histogram::Mode::Hsv, detectedR);
    if (!histDet.calc(env))
        return false;
    WState ws = WState::Unknown;
    if (histDet.mLastColor[2] > 10) { // not black
        if (histDet.mLastColor[1] < 80) // desaturated = disabled
            ws = WState::Disabled;
        else if (histDet.mLastColor[0] < 30) {// hue is near red = known color
            if (histDet.mLastColor[2] > 180) // bright = focused
                ws = WState::Focused;
            else
                ws = WState::Normal;
        }
    }
    clsBtnRect.u.widg.ws = ws;
    LOG_IF(ws == WState::Focused, INFO) << "Focused: " << this->path;
    LOG_IF(ws == WState::Disabled, INFO) << "Disabld: " << this->path;
    if (ws == WState::Focused && !params.uiState.focused)
        params.uiState.focused = this;

    return true;
}

bool List::detect(DetectParams& params) {
    ClassifyEnv& env = params.env;
    cv::Rect listReferenceRect = env.calcReferenceRect(this->rect);
    cv::Rect listCapturedRect =  env.cvtReferenceToCaptured(listReferenceRect);
    env.cropToCapture(listCapturedRect);
    if (listCapturedRect.empty())
        return false;

    env.classified.emplace_back(ClsDetType::Widget, env.isWarpMode(), this->name, listReferenceRect);
    ClassifiedRect& clsListRect = env.classified.back();
    clsListRect.u.widg.referenceRect = listReferenceRect;
    clsListRect.u.widg.ws = WState::Unknown;
    clsListRect.u.widg.widget = this;

    //unsigned buttonGrayColor = mConfiguration->getLstRowGrayColor(WState::Normal);
    cv::Vec3b hsvColorMin {0, 127, 30};
    cv::Vec3b hsvColorMax {30, 255, 255};
    XMat hsvImage;
    cv::cvtColor(env.getColorImage()(listCapturedRect), hsvImage, cv::COLOR_BGR2HSV);
    XMat thrImage;
    cv::inRange(hsvImage, hsvColorMin, hsvColorMax, thrImage);
    if (true) {
        cv::Mat kernel = (cv::Mat_<float>(15, 3) <<
                1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
                2, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 2,
                1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1);
        kernel = kernel.t();
        kernel /= cv::sum(kernel)[0];
        cv::Mat erodedImage;
        cv::filter2D(thrImage, erodedImage, -1, kernel, {-1,-1}, 0, cv::BORDER_DEFAULT);
        cv::threshold(erodedImage, thrImage, 253, 255, cv::THRESH_BINARY);
        //cv::Mat kernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(7, 3));
        //cv::erode(thrImage, erodedImage, kernel, cv::Point(-1, -1), 1, cv::BORDER_CONSTANT, cv::Scalar::all(0));
        //cv::threshold(erodedImage, thrImage, 250, 255, cv::THRESH_BINARY);
    }

    auto expected_row_width = env.getScale() * listReferenceRect.width;
    auto expected_row_height = env.getScale() * this->row_height;
    auto expected_row_gap = env.getScale() * this->row_gap;
    double minArea =  expected_row_width * expected_row_height * 0.75;

    std::vector<double> detectedRows;
    std::vector<std::vector<cv::Point>> contours;
    cv::findContoursLinkRuns(thrImage, contours);
    for (const auto &contour: contours) {
        std::vector<cv::Point> convex;
        cv::convexHull(contour, convex);
        if (convex.size() >= 4) {
            std::vector<cv::Point> approx;
            cv::approxPolyN(convex, approx, 4, 5, true);
            if (cv::contourArea(approx) > minArea) {
                cv::Rect bbox = cv::boundingRect(approx);
                bbox &= cv::Rect(cv::Point(),listCapturedRect.size());
                if (bbox.height < expected_row_height * 0.9)
                    continue;
                if (bbox.height < expected_row_height * 1.5) {
                    detectedRows.push_back(bbox.y);
                } else {
                    int count = (int)std::round(double(bbox.height) / double(expected_row_height+expected_row_gap));
                    double split_height = double(bbox.height) / double(count);
                    for (int i=0; i < count; i++)
                        detectedRows.push_back(bbox.y + i*split_height);
                }
            }
        }
    }

    if (detectedRows.size() > 3) {
        double expectedDist = this->row_height + this->row_gap;
        while (cleanBadRows(detectedRows, expectedDist) && detectedRows.size() > 3)
            ;
        alignDetectedRows(detectedRows, expectedDist);
    }

    for (auto& row_top : detectedRows) {
        cv::Rect rowCapturedRect {0, (int)std::round(row_top), (int)std::round(expected_row_width), (int)std::round(expected_row_height)};
        rowCapturedRect += listCapturedRect.tl();
        cv::Rect rowReferenceRect = env.cvtCapturedToReference(rowCapturedRect);

        detect::Histogram histDet(detect::Histogram::Mode::Hsv, rowReferenceRect);
        if (!histDet.calc(env))
            return false;
        WState ws = WState::Unknown;
        if (histDet.mLastColor[2] > 10) { // not black
            if (histDet.mLastColor[1] < 80) // desaturated = disabled
                ws = WState::Disabled;
            else if (histDet.mLastColor[0] <= 40) {// hue is near red = known color
                if (histDet.mLastColor[2] > 180) // bright = focused
                    ws = WState::Focused;
                else
                    ws = WState::Normal;
            }
        }

        env.classified.emplace_back(ClsDetType::ListRow, env.isWarpMode(), "", rowReferenceRect);
        ClassifiedRect& clsRowRect = env.classified.back();
        clsRowRect.u.lrow.capturedRect = rowCapturedRect;
        clsRowRect.u.lrow.list = this;
        clsRowRect.u.lrow.ws = ws;
        clsRowRect.u.lrow.text_confidence = -1;
        if (ws == WState::Focused) {
            clsListRect.u.widg.ws = WState::Focused;
            clsRowRect.u.lrow.capturedRect.y += 1;
            clsRowRect.u.lrow.capturedRect.height -= 1;

            if (!params.uiState.focused)
                params.uiState.focused = this;
        }
        if (params.level >= DetectLevel::ListOcrAllRows || ws == WState::Focused && params.level >= DetectLevel::ListOcrFocusedRow) {
            int conf;
            if (env.isWarpMode())
                conf = ocr::ocrRowText(toMat(env.getGrayImage()), env, clsRowRect, 0, clsRowRect.text);
            else
                conf = ocr::ocrRowText(toMat(env.getGrayImage()), env, clsRowRect, 1, clsRowRect.text);
            clsRowRect.u.lrow.text_confidence = conf;
        }
    }
    return true;
}

bool List::cleanBadRows(std::vector<double>& detectedRows, double expectedDist) {
    int distCount = 1;
    double avgrDist = expectedDist;
    for (int i = 1; i < detectedRows.size(); i++) {
        double dist = detectedRows[i] - detectedRows[i - 1];
        if (std::abs(dist - expectedDist) / expectedDist < 0.1f) {
            avgrDist += detectedRows[i] - detectedRows[i - 1];
            distCount += 1;
        }
    }
    avgrDist /= double(distCount);
    int rowIdx = -1;
    std::vector<int> indices;
    std::vector<double> offsets;
    for (int i = 0; i < detectedRows.size(); i++) {
        int idx = (int) std::round(detectedRows[i] / avgrDist);
        if (idx < rowIdx)
            idx = rowIdx;
        indices.push_back(idx);
        double offset = detectedRows[i] - (idx * avgrDist);
        offsets.push_back(offset);
        rowIdx = idx + 1;
    }

    cv::Scalar meanOffs, stdDev;
    cv::meanStdDev(offsets, meanOffs, stdDev);
    double baseOffset = meanOffs[0];

    for (int i = 0; i < detectedRows.size(); i++) {
        int idx = indices[i];
        double offset = detectedRows[i] - baseOffset - (idx * avgrDist);
        if (std::abs(offset) > avgrDist*0.1) {
            detectedRows.erase(detectedRows.begin()+i);
            return true;
        }
    }
    return false;
}

bool List::alignDetectedRows(std::vector<double>& detectedRows, double expectedDist) {
    int distCount = 1;
    double avgrDist = expectedDist;
    for (int i = 1; i < detectedRows.size(); i++) {
        double dist = detectedRows[i] - detectedRows[i - 1];
        if (std::abs(dist - expectedDist) / expectedDist < 0.1f) {
            avgrDist += detectedRows[i] - detectedRows[i - 1];
            distCount += 1;
        }
    }
    avgrDist /= double(distCount);
    int rowIdx = 0;
    std::vector<int> indices;
    std::vector<double> offsets;
    for (int i = 0; i < detectedRows.size(); i++) {
        int idx = (int) std::round(detectedRows[i] / avgrDist);
        if (idx < rowIdx)
            idx = rowIdx;
        indices.push_back(idx);
        double offset = detectedRows[i] - (idx * avgrDist);
        offsets.push_back(offset);
        rowIdx = idx + 1;
    }

    cv::Scalar meanOffs, stdDev;
    cv::meanStdDev(offsets, meanOffs, stdDev);
    double baseOffset = meanOffs[0];

    offsets.clear();
    for (int i = 0; i < detectedRows.size(); i++) {
        int idx = indices[i];
        double offset = detectedRows[i] - baseOffset - (idx * avgrDist);
        offsets.push_back(offset);
    }

    cv::meanStdDev(offsets, meanOffs, stdDev);
    baseOffset += meanOffs[0];
    for (int i = 0; i < detectedRows.size(); i++) {
        detectedRows[i] -= offsets[i];
    }
    return true;
}


bool Screen::checkStatus(Configuration& cfg) const {
    if (!status.is_object())
        return false;
    for (auto& kv : status.as_object()) {
        auto& key = kv.first;
        auto& val = kv.second;
        if (key == "gui" || key == "focus") {
            auto gf = enum_cast<GuiFocus>(val.as_string());
            LOG_IF(!gf.has_value(),ERROR) << "Bad gui focus name: " << val;
            if (gf.value() != cfg.getGuiFocus())
                return false;
            continue;
        }
        if (key == "ship") {
            std::string ship = toLower(cfg.getShipType());
            bool ok = false;
            if (val.is_string()) {
                ok = (val.as_string() == ship);
            }
            else if (val.is_array()) {
                for (auto& s : val.as_array()) {
                    if (s.as_string() == ship) {
                        ok = true;
                        break;
                    }
                }
            }
            if (!ok)
                return false;
            continue;
        }
        if (key == "docked") {
            if (val.as_boolean() != cfg.getCurrentStatus()->flags.docked)
                return false;
            continue;
        }
        LOG(ERROR) << "Unknown or unimplemented status key: " << key;
        return false;
    }
    return true;
}

class ExprPoint : public EvalPoint {
public:
    ExprPoint(const json5pp::value& source);
    cv::Point calcReferencePoint(const ResolvedEnv& env) const override;

private:
    int eval(const spAst& ast, const ResolvedEnv& env) const;

    static peg::parser& initParser();

    const json5pp::value source;
    std::array<std::variant<int,spAst>,2> astPoint;
};

class ExprRect : public EvalRect {
public:
    ExprRect(const json5pp::value& source);
    cv::Rect calcReferenceRect(const ResolvedEnv& env) const override;

private:
    int eval(const spAst& ast, const ResolvedEnv& env) const;

    static peg::parser& initParser();

    const json5pp::value source;
    std::array<std::variant<int,spAst>,4> astRect;
};


ExprPoint::ExprPoint(const json5pp::value& src)
        : source(src)
{
    if (!src.is_array() || src.as_array().size() != 4) {
        LOG(ERROR) << "Bad point: " << src;
        return;
    }
    peg::parser& parser = initParser();
    if (!parser)
        return;

    for (int i=0; i < 4; i++) {
        auto& v = source.at(i);
        if (v.is_integer()) {
            astPoint[i] = v.as_integer();
            continue;
        }
        else if (v.is_string()) {
            spAst ast;
            bool ok = parser.parse(v.as_string(), ast);
            if (ok) {
                astPoint[i] = parser.optimize_ast(ast);
                continue;
            }
        }
        LOG(ERROR) << "Bad value: " << v << " in point " << source;
    }
}

ExprRect::ExprRect(const json5pp::value& src)
    : source(src)
{
    if (!src.is_array() || src.as_array().size() != 4) {
        LOG(ERROR) << "Bad rect: " << src;
        return;
    }
    peg::parser& parser = initParser();
    if (!parser)
        return;

    for (int i=0; i < 4; i++) {
        auto& v = source.at(i);
        if (v.is_integer()) {
            astRect[i] = v.as_integer();
            continue;
        }
        else if (v.is_string()) {
            spAst ast;
            bool ok = parser.parse(v.as_string(), ast);
            if (ok) {
                astRect[i] = parser.optimize_ast(ast);
                continue;
            }
        }
        LOG(ERROR) << "Bad value: " << v << " in rect " << source;
    }
}

cv::Point ExprPoint::calcReferencePoint(const ResolvedEnv& env) const {
    cv::Point point;
    for (int i=0; i < 2; i++) {
        int* ptr = &point.x;
        if (holds_alternative<int>(astPoint[i]))
            ptr[i] = std::get<int>(astPoint[i]);
        else
            ptr[i] = eval(std::get<spAst>(astPoint[i]), env);
    }
    return point;
}

cv::Rect ExprRect::calcReferenceRect(const ResolvedEnv& env) const {
    cv::Rect rect;
    for (int i=0; i < 4; i++) {
        int* ptr = &rect.x;
        if (holds_alternative<int>(astRect[i]))
            ptr[i] = std::get<int>(astRect[i]);
        else
            ptr[i] = eval(std::get<spAst>(astRect[i]), env);
    }
    return rect;
}

static peg::parser& init_parser() {
    static peg::parser parser;
    if (!parser) {
        parser.load_grammar(R"(
        Expr        <-  Term (TermOp Term)*
        Term        <-  Factor (FactorOp Factor)*
        Factor      <-  Num / Ident / '(' Expr ')'

        TermOp      <-  < [-+] >
        FactorOp    <-  < [/*] >

        Num         <- < '-'? [0-9]+ >
        Ident       <- < [a-zA-Z] [a-zA-Z0-9-_$.]* >
        %whitespace <- [ \t\r\n]*
        )");
        if (parser)
            parser.enable_ast();
        else
            LOG(ERROR) << "Expression parser initialization error";
    }
    return parser;
}

peg::parser& ExprPoint::initParser() {
    return init_parser();
}

peg::parser& ExprRect::initParser() {
    return init_parser();
}

static int getIntValue(const std::string_view& view, const ResolvedEnv& env) {
    size_t dot = view.find('.');
    if (dot == std::string_view::npos) {
        if (equalsIgnoreCase(view, "ScreenWidth"))
            return env.ReferenceScreenSize.width;
        if (equalsIgnoreCase(view, "ScreenHeight"))
            return env.ReferenceScreenSize.height;
        LOG(ERROR) << "Unknown identifier in expression: " << view;
        return 0;
    }
    const ClassifiedRect* cr = nullptr;
    const std::string_view& name = view.substr(0,dot);
    cv::Point offset;
    for (auto& it : env.classified) {
        if (it.cdt == ClsDetType::Detected && name == it.text) {
            cr = &it;
            offset = cr->detectedRect.tl() - cr->u.tdet.referenceRect.tl();
            break;
        }
        if (it.cdt == ClsDetType::LineDetected && it.text.starts_with(name) && it.text[name.size()] == ':') {
            cr = &it;
            offset = cr->u.ldet.offset;
            break;
        }
        if (it.cdt == ClsDetType::Widget && name == it.text) {
            cr = &it;
            offset = cr->detectedRect.tl() - cr->u.widg.referenceRect.tl();
            break;
        }
    }
    if (!cr) {
        LOG(ERROR) << "Identifier for detector '" << name << "' not found in classified rects";
        return 0;
    }
    const std::string_view& field = view.substr(dot+1);
    if (field == "x" || field == "l" || field == "left")
        return cr->detectedRect.x;
    if (field == "y" || field == "t" || field == "top")
        return cr->detectedRect.y;
    if (field == "w" || field == "width")
        return cr->detectedRect.width;
    if (field == "h" || field == "height")
        return cr->detectedRect.height;
    if (field == "r" || field == "right")
        return cr->detectedRect.br().x;
    if (field == "b" || field == "bottom")
        return cr->detectedRect.br().y;
    if (field == "cx" || field == "center_x")
        return cr->detectedRect.x + cr->detectedRect.width/2;
    if (field == "cy" || field == "center_y")
        return cr->detectedRect.y + cr->detectedRect.height/2;
    if (field == "ox" || field == "offset_x")
        return offset.x;
    if (field == "oy" || field == "offset_y")
        return offset.y;
    LOG(ERROR) << "Field " << field << " not known, use x,y,w,h,l,t,r,b and cx,cy, ox, oy";
    return 0;
}

static int eval_ast(const spAst& ast, const ResolvedEnv& env) {
    if (ast->name == "Num") {
        return ast->token_to_number<int>();
    }
    else if (ast->name == "Ident") {
        return getIntValue(ast->token, env);
    }
    else {
        const auto &nodes = ast->nodes;
        auto result = eval_ast(nodes[0], env);
        for (auto i = 1u; i < nodes.size(); i += 2) {
            auto num = eval_ast(nodes[i + 1], env);
            auto ope = nodes[i]->token[0];
            switch (ope) {
            case '+': result += num; break;
            case '-': result -= num; break;
            case '*': result *= num; break;
            case '/': result /= num; break;
            default:
                LOG(ERROR) << "Bad operator '" << ope << "'";
            }
        }
        return result;
    }
}

int ExprPoint::eval(const spAst& ast, const ResolvedEnv& env) const {
    return eval_ast(ast, env);
}

int ExprRect::eval(const spAst& ast, const ResolvedEnv& env) const {
    return eval_ast(ast, env);
}

}

spEvalPoint makeEvalPoint(const widget::Widget& widget, const char* name, const json5pp::value& j) {
    auto& jv = j.at(name);
    if (jv.is_string()) {
        std::string scope = jv.as_string();
        for (const widget::Widget* p=&widget; p; p = p->parent) {
            if (p->tp == widget::WidgetType::Screen || p->tp == widget::WidgetType::Mode || p->tp == widget::WidgetType::Dialog) {
                auto dlg = (const widget::BaseDialog*) p;
                if (dlg->varSetMap.contains(scope)) {
                    return std::make_shared<RefPoint>(*dlg, name, jv.as_string());
                }
            }
        }
        LOG(ERROR) << "Cannot resolve var scope " << scope << " in parents of widget " << widget.path;
        return {};
    }
    if (!jv.is_array() || jv.as_array().size() != 2) {
        LOG(ERROR) << "For point '" << name << "' expecting array of 2 ints, but got: " << jv;
        return {};
    }
    auto& j_arr = jv.as_array();
    bool simple = true;
    for (int i=0; i < 4; i++) {
        if (!j_arr[i].is_integer()) {
            simple = false;
            if (!j_arr[i].is_string())
                return {};
        }
    }
    if (simple) {
        cv::Point p;
        p.x = j_arr[0].as_integer();
        p.y = j_arr[1].as_integer();
        return std::make_shared<ConstPoint>(p);
    }
    return std::make_shared<widget::ExprPoint>(jv);
}

spEvalRect makeEvalRect(const widget::Widget& widget, const char* name, const json5pp::value& j) {
    auto& jv = j.at(name);
    if (jv.is_string()) {
        std::string scope = jv.as_string();
        for (const widget::Widget* p=&widget; p; p = p->parent) {
            if (p->tp == widget::WidgetType::Screen || p->tp == widget::WidgetType::Mode || p->tp == widget::WidgetType::Dialog) {
                auto dlg = (const widget::BaseDialog*) p;
                if (dlg->varSetMap.contains(scope)) {
                    return std::make_shared<RefRect>(*dlg, name, jv.as_string());
                }
            }
        }
        LOG(ERROR) << "Cannot resolve var scope " << scope << " in parents of widget " << widget.path;
        return {};
    }
    if (!jv.is_array() || jv.as_array().size() != 4) {
        LOG(ERROR) << "For rect '" << name << "' expecting array of 4 ints, but got: " << jv;
        return {};
    }
    auto& j_arr = jv.as_array();
    bool simple = true;
    for (int i=0; i < 4; i++) {
        if (!j_arr[i].is_integer()) {
            simple = false;
            if (!j_arr[i].is_string())
                return {};
        }
    }
    if (simple) {
        cv::Rect r;
        r.x = j_arr[0].as_integer();
        r.y = j_arr[1].as_integer();
        r.width = j_arr[2].as_integer();
        r.height = j_arr[3].as_integer();
        return std::make_shared<ConstRect>(r);
    }
    return std::make_shared<widget::ExprRect>(jv);
}

cv::Point RefPoint::calcReferencePoint(const ResolvedEnv& detectorState) const {
    const std::string& ship = Master::getInstance().getConfiguration()->getShipType();
    auto& varSet = mDlg.varSetMap.at(mScope);
    for (auto& vars : varSet) {
        if (vars.keys.empty() || std::count(vars.keys.begin(),vars.keys.end(), ship)) {
            auto& vals = vars.values.at(mName);
            cv::Point point {(int)vals[0],(int)vals[1]};
            return point;
        }
    }
    return {};
}

cv::Rect RefRect::calcReferenceRect(const ResolvedEnv& detectorState) const {
    const std::string& ship = Master::getInstance().getConfiguration()->getShipType();
    auto& varSet = mDlg.varSetMap.at(mScope);
    for (auto& vars : varSet) {
        if (vars.keys.empty() || std::count(vars.keys.begin(),vars.keys.end(), ship)) {
            auto& vals = vars.values.at(mName);
            cv::Rect rect {(int)vals[0],(int)vals[1],(int)vals[2],(int)vals[3]};
            return rect;
        }
    }
    return {};
}

