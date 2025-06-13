//
// Created by mkizub on 31.05.2025.
//

// conflicts with _() of gettext, have to include before pch.h
#include <peglib/peglib.h>

#include "pch.h"

namespace widget {

typedef std::shared_ptr<peg::Ast> spAst;

void Widget::addSubItem(widget::Widget *sub) {
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


class ExprRect : public EvalRect {
public:
    ExprRect(const json5pp::value& source);
    cv::Rect calcReferenceRect(const ClassifyEnv& env) const override;

private:
    int eval(const spAst& ast, const ClassifyEnv& env) const;

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

cv::Rect ExprRect::calcReferenceRect(const ClassifyEnv& env) const {
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

static int getIntValue(const std::string_view& view, const ClassifyEnv& env) {
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
        if (it.cdt == ClsDetType::TemplateDetected && name == it.text) {
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
        return cr->detectedRect.x - cr->u.templ.referenceRect.x;
    if (field == "oy" || field == "offset_y")
        return cr->detectedRect.y - cr->u.templ.referenceRect.y;
    LOG(ERROR) << "Field " << field << " not known, use x,y,w,h,l,t,r,b and cx,cy, ox, oy";
    return 0;
}

int ExprRect::eval(const spAst& ast, const ClassifyEnv& env) const {
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

