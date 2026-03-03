//
// Created by mkizub on 31.05.2025.
//

#include "../pch.h"

#include <peglib/peglib.h>

#include "EDWidget.h"
#include "../OCR.h"
#include "../State.h"
#include "../detect/Detector.h"

#ifndef NDEBUG
#include "cpptrace/from_current.hpp"
#endif

#ifdef CPPTRACE_TRY
# define TRY CPPTRACE_TRY
# define CATCH(param) CPPTRACE_CATCH(param)
# define GET_EXCEPTION_STACK_TRACE cpptrace::from_current_exception().to_string()
#else
# define TRY try
# define CATCH(param) catch(param)
# include <stacktrace>
# define GET_EXCEPTION_STACK_TRACE std::stacktrace::current()
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

void Widget::setRect(const char* name, const js::value& value, FovScale* fov_scale) {
    rect = makeEvalRect(*this, name, value[name], fov_scale, false);
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
    if (!this->checkStatus())
        return false;

    if (oracle) {
        double match = oracle->match(params.env);
        if (match < 0.5)
            return false;
    }

    if (transform && transform->calcTransform(params.env)) {
        XMat frameImage = transform->transformImage(params.env.getColorImage());
        cv::Matx33d unWarpMat = transform->transformMatrix;
        unWarpMat = unWarpMat.inv();
        params.warpedEnv->init(frameImage, unWarpMat);
        DetectParams wared_params{*params.warpedEnv, nullptr, params.uiState, params.level};
        for (auto& cr : params.env.classified) {
            if (cr.cdt == ClsDetType::Detected && cr.text.starts_with("nav_panel:")) {
                params.warpedEnv->classified.push_back(cr);
            }
        }
        return detectWidgets(wared_params);
    }
    return detectWidgets(params);
}

bool Screen::detectWidgets(DetectParams& params) {
    bool modeMatch = true;
    for (auto mode: this->have) {
        if (!mode || !(mode->tp == WidgetType::Mode || mode->tp == WidgetType::Dialog))
            continue;
        modeMatch = safeDetect(mode, params);
        if (modeMatch)
            break;
    }
    if (!modeMatch)
        return false;
    if (!params.uiState.screen)
        params.uiState.screen = this;
    if (!params.uiState.widget)
        params.uiState.widget = this;

    if (params.level <= DetectLevel::Screen)
        return true;

    for (auto widget: this->have) {
        if (!widget || widget->tp == WidgetType::Mode || widget->tp == WidgetType::Dialog)
            continue;
        safeDetect(widget, params);
    }

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
    if (r.empty())
        return false;
    params.env.classified.emplace_back(ClsDetType::Widget, this->name, r);
    ClassifiedRect& clsLblRect = params.env.classified.back();
    clsLblRect.u.widg.referenceRect = r;
    clsLblRect.u.widg.ws = WState::Unknown;
    clsLblRect.u.widg.widget = this;
    return true;
}

bool Screen::checkStatus() const {
    if (!status.is_object())
        return false;
    if (st::isDead && !status["dead"])
        return false;
    for (auto [key,val] : status.key_value()) {
        if (key == "gui" || key == "focus") {
            auto gf = enum_cast<GuiFocus>(val.as_string());
            LOG_IF(!gf.has_value(),ERROR) << "Bad gui focus name: " << val;
            if (gf.value() != st::guiFocus)
                return false;
            continue;
        }
        if (key == "ship") {
            std::string ship = toLower(st::shipInfo.shipType);
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
            if (val.as_bool() != st::ship.flags.docked)
                return false;
            continue;
        }
        if (key == "expect") {
            if (val.is_bool() && val.as_bool()) {
                if (this->name != st::autopilot.expect_screen)
                    return false;
            }
            if (val.is_string()) {
                if (val.as_string() != st::autopilot.expect_screen)
                    return false;
            }
            continue;
        }
        if (key == "dead") {
            if (val.as_bool() != st::isDead)
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
    ExprPoint(const js::value& source);
    cv::Point calcReferencePoint(const ResolvedEnv& env) const override;

private:
    int eval(const spAst& ast, const ResolvedEnv& env) const;

    static peg::parser& initParser();

    const js::value source;
    std::array<std::variant<int,spAst>,2> astPoint;
};

class ExprRect : public EvalRect {
public:
    ExprRect(const js::value& source);
    cv::Rect calcReferenceRect(const ResolvedEnv& env) const override;

private:
    int eval(const spAst& ast, const ResolvedEnv& env) const;

    static peg::parser& initParser();

    const js::value source;
    std::array<std::variant<int,spAst>,4> astRect;
};

class ExprLine : public EvalLine {
public:
    ExprLine(const js::value& source);
    cv::Line calcReferenceLine(const ResolvedEnv& env) const override;

private:
    int eval(const spAst& ast, const ResolvedEnv& env) const;

    static peg::parser& initParser();

    const js::value source;
    std::array<std::variant<int,spAst>,4> astLine;
};


ExprPoint::ExprPoint(const js::value& src)
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
        if (v.is_int()) {
            astPoint[i] = (int)v.as_int();
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

ExprRect::ExprRect(const js::value& src)
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
        if (v.is_int()) {
            astRect[i] = (int)v.as_int();
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

ExprLine::ExprLine(const js::value& src)
        : source(src)
{
    if (!src.is_array() || src.as_array().size() != 4) {
        LOG(ERROR) << "Bad line: " << src;
        return;
    }
    peg::parser& parser = initParser();
    if (!parser)
        return;

    for (int i=0; i < 4; i++) {
        auto& v = source.at(i);
        if (v.is_int()) {
            astLine[i] = (int)v.as_int();
            continue;
        }
        else if (v.is_string()) {
            spAst ast;
            bool ok = parser.parse(v.as_string(), ast);
            if (ok) {
                astLine[i] = parser.optimize_ast(ast);
                continue;
            }
        }
        LOG(ERROR) << "Bad value: " << v << " in rect " << source;
    }
}

cv::Point ExprPoint::calcReferencePoint(const ResolvedEnv& env) const {
    try {
        cv::Point point;
        for (int i = 0; i < 2; i++) {
            int *ptr = &point.x;
            if (holds_alternative<int>(astPoint[i]))
                ptr[i] = std::get<int>(astPoint[i]);
            else
                ptr[i] = eval(std::get<spAst>(astPoint[i]), env);
        }
        return point;
    } catch (...) {
        return {};
    }
}

cv::Rect ExprRect::calcReferenceRect(const ResolvedEnv& env) const {
    try {
        cv::Rect rect;
        for (int i = 0; i < 4; i++) {
            int *ptr = &rect.x;
            if (holds_alternative<int>(astRect[i]))
                ptr[i] = std::get<int>(astRect[i]);
            else
                ptr[i] = eval(std::get<spAst>(astRect[i]), env);
        }
        return rect;
    } catch (...) {
        return {};
    }
}

cv::Line ExprLine::calcReferenceLine(const ResolvedEnv& env) const {
    try {
        cv::Line line;
        for (int i = 0; i < 4; i++) {
            int *ptr = &line.x0;
            if (holds_alternative<int>(astLine[i]))
                ptr[i] = std::get<int>(astLine[i]);
            else
                ptr[i] = eval(std::get<spAst>(astLine[i]), env);
        }
        return line;
    } catch (...) {
        return {};
    }
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

peg::parser& ExprLine::initParser() {
    return init_parser();
}

static int getIntValue(const std::string_view& view, const ResolvedEnv& env) {
    size_t dot = view.find('.');
    if (dot == std::string_view::npos) {
        if (equalsIgnoreCase(view, "ScreenWidth"))
            return ReferenceScreenSize.width;
        if (equalsIgnoreCase(view, "ScreenHeight"))
            return ReferenceScreenSize.height;
        LOG(ERROR) << "Unknown identifier in expression: " << view;
        throw std::runtime_error("Bad identifier");
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
        throw std::runtime_error("No widget found");
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
    throw std::runtime_error("Field not known");
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

int ExprLine::eval(const spAst& ast, const ResolvedEnv& env) const {
    return eval_ast(ast, env);
}

}

FovScale::FovScale(double fov0, double fov1, cv::Rect rect0, cv::Rect rect1)
    : fov54(fov0)
    , fov60(fov1)
{
    double scale_x0 = double(rect0.width) / double(ReferenceScreenSize.width);
    double scale_x1 = double(rect1.width) / double(ReferenceScreenSize.width);
    double scale_x = scale_x1 / scale_x0;
    double scale_y0 = double(rect0.height) / double(ReferenceScreenSize.height);
    double scale_y1 = double(rect1.height) / double(ReferenceScreenSize.height);
    double scale_y = scale_y1 / scale_y0;
    scale60 = std::sqrt(scale_x * scale_y);
}

double FovScale::getScaleForFOV(double fov) {
    double scl_current = std::lerp(1.0, scale60, (Cfg.getConfigFOV()-fov54)/(fov60-fov54));
    double scl_screens = std::lerp(1.0, scale60, (fov-fov54)/(fov60-fov54));
    double scl = scl_current / scl_screens;
    return scl;
}

cv::Size FovScale::apply(cv::Size s, double fov) {
    double scl = getScaleForFOV(fov);
    return {(int)std::round(s.width*scl), (int)std::round(s.height*scl)};
}

cv::Point FovScale::apply(cv::Point p, double fov) {
    double scl = getScaleForFOV(fov);
    cv::Point np = p;
    np -= ReferenceScreenCenter;
    np = {(int)std::round(np.x*scl), (int)std::round(np.y*scl)};
    np += ReferenceScreenCenter;
    return np;
}

cv::Rect FovScale::apply(cv::Rect r, double fov) {
    return {apply(r.tl(), fov), apply(r.size(), fov)};
}

cv::Line FovScale::apply(cv::Line l, double fov) {
    return {apply(l.p0(), fov), apply(l.p1(), fov)};
}


spEvalPoint makeEvalPoint(const widget::Widget& widget, const char* name, const js::value& jv, FovScale* fov_scale) {
    if (jv.is_string()) {
        std::vector<std::string> scope_name = split(jv.as_string(), ':');
        if (scope_name.size() != 2) {
            LOG(ERROR) << "Reference must be at form 'scope:name', but is " << jv;
            return {};
        }
        const std::string& scope = scope_name[0];
        for (const widget::Widget* p=&widget; p; p = p->parent) {
            if (p->tp == widget::WidgetType::Screen || p->tp == widget::WidgetType::Mode || p->tp == widget::WidgetType::Dialog) {
                auto dlg = (const widget::BaseDialog*) p;
                if (dlg->varSetMap.contains(scope)) {
                    return std::make_shared<RefPoint>(*dlg, scope_name[1], scope);
                }
            }
        }
        LOG(ERROR) << "Cannot resolve var scope " << scope << " in parents of widget " << widget.path;
        return {};
    }
    if (!jv.is_array() || jv.as_array().size() < 2) {
        LOG(ERROR) << "For point '" << name << "' expecting array of 2 ints, but got: " << jv;
        return {};
    }
    bool simple = true;
    for (int i=0; i < 2; i++) {
        if (!jv[i].is_int()) {
            simple = false;
            if (!jv[i].is_string())
                return {};
        }
    }
    if (simple) {
        cv::Point p;
        p.x = jv[0].as_int();
        p.y = jv[1].as_int();
        if (fov_scale && jv[3].is_number()) {
            p = fov_scale->apply(p, jv[3].as_real());
        }
        return std::make_shared<ConstPoint>(p);
    }
    return std::make_shared<widget::ExprPoint>(jv);
}

spEvalRect makeEvalRect(const widget::Widget& widget, const char* name, const js::value& jv, FovScale* fov_scale, bool relative) {
    if (jv.is_string()) {
        std::vector<std::string> scope_name = split(jv.as_string(), ':');
        if (scope_name.size() != 2) {
            LOG(ERROR) << "Reference must be at form 'scope:name', but is " << jv;
            return {};
        }
        const std::string& scope = scope_name[0];
        for (const widget::Widget* p=&widget; p; p = p->parent) {
            if (p->tp == widget::WidgetType::Screen || p->tp == widget::WidgetType::Mode || p->tp == widget::WidgetType::Dialog) {
                auto dlg = (const widget::BaseDialog*) p;
                if (dlg->varSetMap.contains(scope)) {
                    return std::make_shared<RefRect>(*dlg, scope_name[1], scope);
                }
            }
        }
        LOG(ERROR) << "Cannot resolve var scope " << scope << " in parents of widget " << widget.path;
        return {};
    }
    if (!jv.is_array() || jv.as_array().size() < 4) {
        LOG(ERROR) << "For rect '" << name << "' expecting array of 4 ints, but got: " << jv;
        return {};
    }
    bool simple = true;
    for (int i=0; i < 4; i++) {
        if (!jv[i].is_int()) {
            simple = false;
            if (!jv[i].is_string())
                return {};
        }
    }
    if (simple) {
        cv::Rect r;
        r.x = jv[0].as_int();
        r.y = jv[1].as_int();
        r.width = jv[2].as_int();
        r.height = jv[3].as_int();
        if (fov_scale && jv[4].is_number()) {
            if (!relative) {
                r = fov_scale->apply(r, jv[4].as_real());
            } else {
                cv::Point tl = fov_scale->apply(cv::Size(r.tl()), jv[4].as_real());
                cv::Size sz = fov_scale->apply(r.size(), jv[4].as_real());
                r = {tl, sz};
            }
        }
        return std::make_shared<ConstRect>(r);
    }
    return std::make_shared<widget::ExprRect>(jv);
}

spEvalLine makeEvalLine(const widget::Widget& widget, const char* name, const js::value& jv, FovScale* fov_scale) {
    if (jv.is_string()) {
        std::vector<std::string> scope_name = split(jv.as_string(), ':');
        if (scope_name.size() != 2) {
            LOG(ERROR) << "Reference must be at form 'scope:name', but is " << jv;
            return {};
        }
        const std::string& scope = scope_name[0];
        for (const widget::Widget* p=&widget; p; p = p->parent) {
            if (p->tp == widget::WidgetType::Screen || p->tp == widget::WidgetType::Mode || p->tp == widget::WidgetType::Dialog) {
                auto dlg = (const widget::BaseDialog*) p;
                if (dlg->varSetMap.contains(scope)) {
                    return std::make_shared<RefLine>(*dlg, scope_name[1], scope);
                }
            }
        }
        LOG(ERROR) << "Cannot resolve var scope " << scope << " in parents of widget " << widget.path;
        return {};
    }
    if (!jv.is_array() || jv.as_array().size() < 4) {
        LOG(ERROR) << "For rect '" << name << "' expecting array of 4 ints, but got: " << jv;
        return {};
    }
    bool simple = true;
    for (int i=0; i < 4; i++) {
        if (!jv[i].is_int()) {
            simple = false;
            if (!jv[i].is_string())
                return {};
        }
    }
    if (simple) {
        cv::Line ln;
        ln.x0 = jv[0].as_int();
        ln.y0 = jv[1].as_int();
        ln.x1 = jv[2].as_int();
        ln.y1 = jv[3].as_int();
        if (fov_scale && jv[4].is_number()) {
            ln = fov_scale->apply(ln, jv[4].as_real());
        }
        return std::make_shared<ConstLine>(ln);
    }
    return std::make_shared<widget::ExprLine>(jv);
}

cv::Point RefPoint::calcReferencePoint(const ResolvedEnv& detectorState) const {
    const std::string& ship = st::shipInfo.shipType;
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
    const std::string& ship = st::shipInfo.shipType;
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

cv::Line RefLine::calcReferenceLine(const ResolvedEnv& detectorState) const {
    const std::string& ship = st::shipInfo.shipType;
    auto& varSet = mDlg.varSetMap.at(mScope);
    for (auto& vars : varSet) {
        if (vars.keys.empty() || std::count(vars.keys.begin(),vars.keys.end(), ship)) {
            auto& vals = vars.values.at(mName);
            cv::Line line {(int)vals[0],(int)vals[1],(int)vals[2],(int)vals[3]};
            return line;
        }
    }
    return {};
}
