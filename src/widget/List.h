//
// Created by mkizub on 26.12.2025.
//

#pragma once

#ifndef EDROBOT_LIST_H
#define EDROBOT_LIST_H

#include "EDWidget.h"

namespace widget {

struct List : public Widget {
    List(const std::string& name, Widget* parent) : Widget(WidgetType::List, name, parent) {}
    bool detect(DetectParams& params) override;

    float row_height {0};
    float row_gap {0};
    float header {0};
    int row_test_bgn {0};
    int row_test_end {0};
    std::vector<std::unique_ptr<detect::ImageFilter>> filters;
    struct Tab {
        std::string name;
        int tab_left {0};
        int tab_right {0};
        float ocr_height {0}; // ascending + descending
        int ocr_psm {7}; // see OCR.h
    };
    std::vector<Tab> tabs;
    const Tab& getTab(std::string_view name) const;
};

struct ListPPC : public List {
    ListPPC(const std::string& name, Widget* parent) : List(name, parent) {}

    bool detect(DetectParams& params) override;

    std::unique_ptr<detect::ImageTemplate> icon_detector;
};

} // namespace widget
#endif //EDROBOT_LIST_H
