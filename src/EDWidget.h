//
// Created by mkizub on 31.05.2025.
//

#pragma once

#ifndef EDROBOT_EDWIDGET_H
#define EDROBOT_EDWIDGET_H

namespace widget {

enum class WidgetType {
    Label, Button, TileBtn, Spinner, List, ListRow, Mode, Dialog, Screen, Root
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
    std::unique_ptr<detect::Detector> oracle;
    std::vector<Widget*> have;

};

struct Label : public Widget {
    Label(const std::string& name, Widget* parent) : Widget(WidgetType::Label, name, parent) {}
    std::optional<int> row_height;
    std::optional<bool> invert;
};

struct Button : public Widget {
    Button(const std::string& name, Widget* parent) : Widget(WidgetType::Button, name, parent) {}
};

struct TileBtn : public Widget {
    TileBtn(const std::string& name, Widget* parent, const std::string& icon, int row, int col)
        : Widget(WidgetType::TileBtn, name, parent)
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

struct ListRow : public Widget {
    ListRow(const std::string& name, List* parent) : Widget(WidgetType::ListRow, name, parent) {}
};

struct Mode : public Widget {
    Mode(const std::string& name, Widget* parent) : Widget(WidgetType::Mode, name, parent) {}
};

struct Dialog : public Widget {
    Dialog(const std::string& name, Widget* parent) : Widget(WidgetType::Dialog, name, parent) {}
};

struct Screen : public Widget {
    Screen(const std::string& name, Widget* parent, json::value status)
        : Widget(WidgetType::Screen, name, parent)
        , status(std::move(status))
    {}
    bool checkStatus(Master& master, Configuration& cfg) const;
    const json::value status;

    spEvalTransform transform;
};

struct Root : public Widget {
    Root() : Widget(WidgetType::Root, "", nullptr) {}
};

};

#endif //EDROBOT_EDWIDGET_H
