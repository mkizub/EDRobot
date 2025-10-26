//
// Created by mkizub on 31.05.2025.
//

#pragma once

#ifndef EDROBOT_EDWIDGET_H
#define EDROBOT_EDWIDGET_H

namespace widget {

enum class WidgetType {
    Label, Button, TileBtn, Spinner, List, Mode, Dialog, Screen, Root
};

struct Widget {
    Widget(WidgetType tp, const std::string& name, Widget* parent);
    virtual ~Widget();

    void addSubItem(Widget* sub);
    void setRect(const char* name, const json5pp::value& value);
    cv::Rect calcReferenceRect(const ClassifyEnv& env) const;

    struct DetectParams {
        ClassifyEnv& env;
        UIState& uiState;
        Master& master;
        DetectLevel level;
    };
    virtual bool detect(DetectParams& params) = 0;

    const WidgetType tp;
    const std::string name;
    const Widget* parent;
    const std::string path;

    spEvalRect rect;
    std::unique_ptr<detect::Detector> oracle;
    std::vector<Widget*> have;
};

struct Label : public Widget {
    Label(const std::string& name, Widget* parent) : Widget(WidgetType::Label, name, parent) {}
    bool detect(DetectParams& params) final;

    // we'll add ocr leading above the 'ocr_top'
    int ocr_top {0}; // top of 'H' from single-line label top
    int ocr_bot {0}; // bottom of 'p/g' from single-line label top
};

struct BaseButton : public Widget {
    BaseButton(WidgetType tp, const std::string &name, Widget *parent) : Widget(tp, name, parent) {}
    bool detect(DetectParams& params);
    cv::Point extendLT;
    cv::Point extendRB;
};

struct Button : public BaseButton {
    Button(const std::string& name, Widget* parent) : BaseButton(WidgetType::Button, name, parent) {}
};

struct TileBtn : public BaseButton {
    TileBtn(const std::string& name, Widget* parent, const std::string& icon)
        : BaseButton(WidgetType::TileBtn, name, parent)
        , icon(icon)
    {
        rect = std::shared_ptr<EvalRect>(new TileRect(icon));
    }

    const std::string icon;
};

struct Spinner : public BaseButton {
    Spinner(const std::string& name, Widget* parent) : BaseButton(WidgetType::Spinner, name, parent) {}

    int button_width {}; // by default spinner button is square, i.e. width is the same as spinner height
};

struct List : public Widget {
    List(const std::string& name, Widget* parent) : Widget(WidgetType::List, name, parent) {}
    bool detect(DetectParams& params) final;

    float row_height {0};
    float row_gap {0};
    float header {0};
    struct Tab {
        std::string name;
        int tab_left {0};
        int tab_right {0};
        // we'll add ocr leading above the 'ocr_top'
        int ocr_top {0}; // top of 'H' from single-line label top
        int ocr_bot {0}; // bottom of 'p/g' from single-line label top
    };
    std::vector<Tab> tabs;
};

struct BaseDialog : public Widget {
    BaseDialog(WidgetType tp, const std::string &name, Widget *parent) : Widget(tp, name, parent) {}
    struct Vars {
        std::vector<std::string> keys;
        std::map<std::string,std::vector<double>> values;
    };
    std::map<std::string,std::vector<Vars>> varSetMap;
};

struct Mode : public BaseDialog {
    Mode(const std::string& name, Widget* parent) : BaseDialog(WidgetType::Mode, name, parent) {}
    bool detect(DetectParams& params) final;
};

struct Dialog : public BaseDialog {
    Dialog(const std::string& name, Widget* parent) : BaseDialog(WidgetType::Dialog, name, parent) {}
    bool detect(DetectParams& params) final;
};

struct Screen : public BaseDialog {
    Screen(const std::string& name, Widget* parent, json5pp::value status)
        : BaseDialog(WidgetType::Screen, name, parent)
        , status(std::move(status))
    {}
    bool detect(DetectParams& params) final;

    bool checkStatus() const;
    const json5pp::value status;

    spEvalTransform transform;
};

struct Root : public Widget {
    Root() : Widget(WidgetType::Root, "", nullptr) {}
    bool detect(DetectParams& params) final;
};

};

#endif //EDROBOT_EDWIDGET_H
