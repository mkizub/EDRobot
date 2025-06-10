//
// Created by mkizub on 31.05.2025.
//

#pragma once

#ifndef EDROBOT_EDWIDGET_H
#define EDROBOT_EDWIDGET_H

namespace widget {

enum class WidgetState {
    Normal, Focused, Activated, Disabled
};

enum class WidgetType {
    Button, Spinner, List, Mode, Dialog, Screen, Root
};

struct Widget {
    Widget(WidgetType tp, const std::string& name, Widget* parent);
    virtual ~Widget();

    void addSubItem(Widget* sub);
    void setRect(json5pp::value value);
    cv::Rect calcReferenceRect(const ClassifyEnv& env) const;

    const WidgetType tp;
    const std::string name;
    const Widget* parent;
    const std::string path;

    spEvalRect rect;
    std::unique_ptr<Template> oracle;
    std::vector<Widget*> have;

};

struct Button : public Widget {
    Button(const std::string& name, Widget* parent) : Widget(WidgetType::Button, name, parent) {}
};

struct Spinner : public Widget {
    Spinner(const std::string& name, Widget* parent) : Widget(WidgetType::Spinner, name, parent) {}
    int button_width {}; // by default spinner button is square, i.e. width is the same as spinner height
};

struct List : public Widget {
    List(const std::string& name, Widget* parent) : Widget(WidgetType::List, name, parent) {}
    int row_height {36};
    int row_gap {2};
    bool ocr {false};
};

struct Mode : public Widget {
    Mode(const std::string& name, Widget* parent) : Widget(WidgetType::Mode, name, parent) {}
};

struct Dialog : public Widget {
    Dialog(const std::string& name, Widget* parent) : Widget(WidgetType::Dialog, name, parent) {}
};

struct Screen : public Widget {
    Screen(const std::string& name, Widget* parent) : Widget(WidgetType::Screen, name, parent) {}
};

struct Root : public Widget {
    Root() : Widget(WidgetType::Root, "", nullptr) {}
};

};

#endif //EDROBOT_EDWIDGET_H
