//
// Created by mkizub on 26.12.2025.
//

#pragma once

#ifndef EDROBOT_BUTTON_H
#define EDROBOT_BUTTON_H

#include "EDWidget.h"

namespace widget {

struct BaseButton : public Widget {
    BaseButton(WidgetType tp, const std::string &name, Widget *parent) : Widget(tp, name, parent) {}

    bool detect(DetectParams &params);

    cv::Point extendLT;
    cv::Point extendRB;
    std::string icon;
    std::unique_ptr<detect::ImageTemplate> detector{};
    std::vector<std::unique_ptr<detect::ImageFilter>> filters;
    bool force_present {};
};

struct Button : public BaseButton {
    Button(const std::string &name, Widget *parent) : BaseButton(WidgetType::Button, name, parent) {}
};

struct TileBtn : public BaseButton {
    TileBtn(const std::string &name, Widget *parent, const std::string &tile_icon)
            : BaseButton(WidgetType::TileBtn, name, parent) {
        rect = std::shared_ptr<EvalRect>(new TileRect(tile_icon));
    }
};

struct Spinner : public BaseButton {
    Spinner(const std::string &name, Widget *parent) : BaseButton(WidgetType::Spinner, name, parent) {}

    int button_width{}; // by default spinner button is square, i.e. width is the same as spinner height
};

} // namespace widget

#endif //EDROBOT_BUTTON_H
