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
    void setRect(json5pp::value value);
    cv::Rect calcReferenceRect(const ClassifyEnv& env) const;

    struct DetectParams {
        ClassifyEnv& env;
        UIState& uiState;
        Master& master;
        Configuration& cfg;
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

    std::optional<int> row_height;
    std::optional<bool> invert;
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
    TileBtn(const std::string& name, Widget* parent, const std::string& icon, int row, int col)
        : BaseButton(WidgetType::TileBtn, name, parent)
        , icon(icon)
        , row(row)
        , col(col)
    {
        rect = std::shared_ptr<EvalRect>(new TileRect(icon, row, col));
    }

    const std::string icon;
    const int row;
    const int col;
};

struct Spinner : public BaseButton {
    Spinner(const std::string& name, Widget* parent) : BaseButton(WidgetType::Spinner, name, parent) {}

    int button_width {}; // by default spinner button is square, i.e. width is the same as spinner height
};

struct List : public Widget {
    List(const std::string& name, Widget* parent) : Widget(WidgetType::List, name, parent) {}
    bool detect(DetectParams& params) final;

    int row_height {36};
    int row_gap {2};
    bool ocr {false};
};

struct BaseDialog : public Widget {
    BaseDialog(WidgetType tp, const std::string &name, Widget *parent) : Widget(tp, name, parent) {}
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
    Screen(const std::string& name, Widget* parent, json::value status)
        : BaseDialog(WidgetType::Screen, name, parent)
        , status(std::move(status))
    {}
    bool detect(DetectParams& params) final;

    bool checkStatus(Configuration& cfg) const;
    const json::value status;

    spEvalTransform transform;
};

struct Root : public Widget {
    Root() : Widget(WidgetType::Root, "", nullptr) {}
    bool detect(DetectParams& params) final;
};

};

#endif //EDROBOT_EDWIDGET_H
