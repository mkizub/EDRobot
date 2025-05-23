//
// Created by mkizub on 23.05.2025.
//

#pragma once

#ifndef EDROBOT_TEMPLATE_H
#define EDROBOT_TEMPLATE_H

#include <opencv2/core/mat.hpp>

class Master;

class Template {
public:
    Template() = default;
    virtual ~Template() = default;

    virtual bool match(Master* master) = 0;
};

struct ImagePlace {
    cv::Rect screenRect;
    const char* filename;
    ImagePlace(int x, int y, const char* filename) : screenRect(x, y, 0, 0), filename(filename) {}
    ImagePlace(int x, int y, int w, const char* filename) : screenRect(x, y, w, 0), filename(filename) {}
    ImagePlace(int x, int y, int w, int h, const char* filename) : screenRect(x, y, w, h), filename(filename) {}
};

class ImageTemplate : public Template {
public:
    ImageTemplate(ImagePlace place, double threshold = 0.8);
    ~ImageTemplate() override = default;

    bool match(Master* master) override;
private:
    const double threshold;
    const ImagePlace place;
    cv::Rect screenRect;
    cv::Mat templ;
};



#endif //EDROBOT_TEMPLATE_H
