//
// Created by mkizub on 31.05.2025.
//

#pragma once

#ifndef EDROBOT_EDWIDGET_H
#define EDROBOT_EDWIDGET_H

#include "json5pp.hpp"
#include <opencv2/opencv.hpp>

class Template;
class HistogramTemplate;

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
    virtual const cv::Rect* getRect() const = 0;

    const WidgetType tp;
    const std::string name;
    const Widget* parent;
    const std::string path;

    std::unique_ptr<Template> oracle;
    std::vector<Widget*> have;
};

struct Button : public Widget {
    Button(const std::string& name, Widget* parent) : Widget(WidgetType::Button, name, parent), rect{} {}
    cv::Rect rect;
    const cv::Rect* getRect() const override { return &rect; }
};

struct Spinner : public Widget {
    Spinner(const std::string& name, Widget* parent) : Widget(WidgetType::Spinner, name, parent), rect{} {}
    cv::Rect rect;
    const cv::Rect* getRect() const override { return &rect; }
};

struct List : public Widget {
    List(const std::string& name, Widget* parent) : Widget(WidgetType::List, name, parent), rect{} {}
    cv::Rect rect;
    const cv::Rect* getRect() const override { return &rect; }
};

struct Mode : public Widget {
    Mode(const std::string& name, Widget* parent) : Widget(WidgetType::Mode, name, parent) {}
    const cv::Rect* getRect() const override { return nullptr; }
};

struct Dialog : public Widget {
    Dialog(const std::string& name, Widget* parent) : Widget(WidgetType::Dialog, name, parent) {}
    const cv::Rect* getRect() const override { return nullptr; }
};

struct Screen : public Widget {
    Screen(const std::string& name, Widget* parent) : Widget(WidgetType::Screen, name, parent) {}
    const cv::Rect* getRect() const override { return nullptr; }
};

struct Root : public Widget {
    Root() : Widget(WidgetType::Root, "", nullptr) {}
    const cv::Rect* getRect() const override { return nullptr; }
};

};

#endif //EDROBOT_EDWIDGET_H
