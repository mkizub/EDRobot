//
// Created by mkizub on 31.05.2025.
//

#pragma once

#ifndef EDROBOT_EDWIDGET_H
#define EDROBOT_EDWIDGET_H

#include "../detect/Detector.h"

namespace widget {

enum class WidgetType {
    Label, Button, TileBtn, Spinner, List, Mode, Dialog, Screen, Root
};

struct Widget {
    Widget(WidgetType tp, const std::string& name, Widget* parent);
    virtual ~Widget();

    void addSubItem(Widget* sub);
    void setRect(const char* name, const json5pp::value& value, FovScale* fov_scale);
    cv::Rect calcReferenceRect(const ClassifyEnv& env) const;

    struct DetectParams {
        ClassifyEnv& env;
        ClassifyEnv* warpedEnv;
        UIState& uiState;
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
    bool detectWidgets(DetectParams& params);

    bool checkStatus() const;
    const json5pp::value status;

    spEvalTransform transform;
};

struct Root : public Widget {
    Root() : Widget(WidgetType::Root, "", nullptr) {}
    bool detect(DetectParams& params) final;
};

extern Widget* widget_from_json(const json5pp::value& j, Widget* parent, FovScale* fov_scale);
extern void debugNavPanel();

};

#endif //EDROBOT_EDWIDGET_H
