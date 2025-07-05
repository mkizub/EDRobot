//
// Created by mkizub on 31.05.2025.
//

// conflicts with _() of gettext, have to include before pch.h
#include <peglib/peglib.h>

#include "pch.h"

#include "EDWidget.h"
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

void Widget::setRect(json5pp::value value) {
    if (value.is_null())
        return;
    rect = makeEvalRect(value);
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
        LOG(ERROR) << "Exception in widget '" << widget->path << "' detection: " << GET_EXCEPTION_STACK_TRACE;
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

    bool screenMatch = false;
    if (oracle) {
        screenMatch = oracle->classify(params.env);
        if (!screenMatch)
            return false;
    }

    if (transform) {
        params.env.warpPerspective(transform);
        if (transform->valid)
            params.env.setWarpMode(transform->valid);
    }

    bool modeMatch = true;
    for (auto mode: this->have) {
        if (!mode || mode->tp != WidgetType::Mode)
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
        if (!widget || widget->tp == WidgetType::Mode)
            continue;
        safeDetect(widget, params);
    }

    params.env.setWarpMode(false);
    return true;
}

bool Dialog::detect(DetectParams& params) {
    bool dialogMatch = false;
    if (oracle) {
        dialogMatch = oracle->classify(params.env);
        if (!dialogMatch)
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
    if (!oracle->classify(params.env))
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
    params.env.classified.emplace_back(ClsDetType::Widget, params.env.isWarpMode(), this->name, r);
    params.env.classified.back().u.widg.widget = this;

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
        unsigned buttonGrayColor = Master::getInstance().getConfiguration()->getButtonGrayColor(WState::Normal);
        cv::Mat extImage(env.getGrayImage(), matchR);

        cv::Mat thrImage;
        cv::threshold(extImage, thrImage, buttonGrayColor - 4, 255, cv::THRESH_BINARY);

        std::vector<std::vector<cv::Point>> contours;
        cv::findContoursLinkRuns(thrImage, contours);
        for (auto &cont: contours) {
            std::vector<cv::Point> convex;
            cv::convexHull(cont, convex);
            if (convex.size() >= 4) {
                std::vector<cv::Point> approx;
                cv::approxPolyN(convex, approx, 4, 5, true);
                cv::Rect bbox = cv::boundingRect(approx);
                if (bbox.width > captureR.width*0.9 && bbox.height > captureR.height*0.9 &&
                    bbox.width < captureR.width*1.1 && bbox.height < captureR.height*1.2)
                {
                    captureR = {matchR.tl() + bbox.tl(), bbox.size()};
                    cv::Mat tmpImage(env.getGrayImage(), captureR);
                    detectedR = env.cvtCapturedToReference(captureR);
                    break;
                }
            }
        }
    }

    env.classified.emplace_back(ClsDetType::Widget, env.isWarpMode(), this->name, detectedR);
    ClassifiedRect& clsBtnRect = env.classified.back();
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
    if (listCapturedRect.empty())
        return false;

    env.classified.emplace_back(ClsDetType::Widget, env.isWarpMode(), this->name, listReferenceRect);
    ClassifiedRect& clsListRect = env.classified.back();
    clsListRect.u.widg.ws = WState::Unknown;
    clsListRect.u.widg.widget = this;

    //unsigned buttonGrayColor = mConfiguration->getLstRowGrayColor(WState::Normal);
    cv::Vec3b hsvColorMin {0, 127, 25};
    cv::Vec3b hsvColorMax {30, 255, 255};
    cv::Mat hsvImage;
    cv::cvtColor(cv::Mat(env.getColorImage(), listCapturedRect), hsvImage, cv::COLOR_BGR2HSV);
    cv::Mat thrImage;
    cv::inRange(hsvImage, hsvColorMin, hsvColorMax, thrImage);

    auto expected_row_size = env.scaleToCaptured(cv::Size(listReferenceRect.width, this->row_height));
    double minArea =  expected_row_size.area() * 0.75;

    std::vector<int> detectedRows;
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
                if (bbox.height < expected_row_size.height * 0.9)
                    continue;
                if (bbox.height < expected_row_size.height * 1.5) {
                    int y = bbox.y + bbox.height/2 - expected_row_size.height/2;
                    detectedRows.push_back(y);
                } else {
                    int count = (int)std::floor(0.1 + double(bbox.height) / double(expected_row_size.height));
                    int split_height = bbox.height / count;
                    for (int i=0; i < count; i++) {
                        int y = bbox.y+(i*split_height) + split_height/2 - expected_row_size.height/2;
                        detectedRows.push_back(y);
                    }
                }
            }
        }
    }

    for (auto& row_top : detectedRows) {
        cv::Rect rowSubRect {0, row_top, expected_row_size.width, expected_row_size.height};
        cv::Rect rowReferenceRect = env.cvtCapturedToReference(rowSubRect+listCapturedRect.tl());

        detect::Histogram histDet(detect::Histogram::Mode::Hsv, rowReferenceRect);
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

        std::string text;
        if (ws == WState::Focused && params.level >= DetectLevel::ListOcrFocusedRow) {
            cv::Mat grayImage(env.getGrayImage(), listCapturedRect);
            Master::getInstance().ocrMarketText(grayImage, rowSubRect, text);
        }

        env.classified.emplace_back(ClsDetType::ListRow, env.isWarpMode(), text, rowReferenceRect);
        ClassifiedRect& clsRowRect = env.classified.back();
        clsRowRect.u.lrow.list = this;
        clsRowRect.u.lrow.ws = ws;
        if (ws == WState::Focused) {
            clsListRect.u.widg.ws = WState::Focused;
            if (!params.uiState.focused)
                params.uiState.focused = this;
        }
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

peg::parser& ExprRect::initParser() {
    static peg::parser parser;
    if (!parser) {
        parser.load_grammar(R"(
        Expr        <-  Term (TermOp Term)*
        Term        <-  Factor (FactorOp Factor)*
        Factor      <-  Num / Ident / '(' Expr ')'

        TermOp      <-  < [-+] >
        FactorOp    <-  < [/*] >

        Num         <- < '-'? [0-9]+ >
        Ident       <- < [a-zA-Z] [a-zA-Z0-9-_.]* >
        %whitespace <- [ \t\r\n]*
        )");
        if (parser)
            parser.enable_ast();
        else
            LOG(ERROR) << "Expression parser initialization error";
    }
    return parser;
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
    for (auto& it : env.classified) {
        if (it.cdt == ClsDetType::Detected && name == it.text) {
            cr = &it;
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
        return cr->detectedRect.x - cr->u.tdet.referenceRect.x;
    if (field == "oy" || field == "offset_y")
        return cr->detectedRect.y - cr->u.tdet.referenceRect.y;
    LOG(ERROR) << "Field " << field << " not known, use x,y,w,h,l,t,r,b and cx,cy, ox, oy";
    return 0;
}

int ExprRect::eval(const spAst& ast, const ResolvedEnv& env) const {
    if (ast->name == "Num") {
        return ast->token_to_number<int>();
    }
    else if (ast->name == "Ident") {
        return getIntValue(ast->token, env);
    }
    else {
        const auto &nodes = ast->nodes;
        auto result = eval(nodes[0], env);
        for (auto i = 1u; i < nodes.size(); i += 2) {
            auto num = eval(nodes[i + 1], env);
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

}


spEvalRect makeEvalRect(json5pp::value jv, int width, int height) {
    if (!jv.is_array())
        return {};
    auto& j_arr = jv.as_array();
    if (j_arr.size() < 2)
        return {};
    if (j_arr.size() == 2)
        j_arr.emplace_back(width);
    if (j_arr.size() == 3)
        j_arr.emplace_back(height);
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

