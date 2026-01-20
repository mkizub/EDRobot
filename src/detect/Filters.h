//
// Created by mkizub on 23.12.2025.
//

#pragma once

#ifndef EDROBOT_FILTERS_H
#define EDROBOT_FILTERS_H

namespace detect {

class ImageFilter {
public:
    struct Params {
        bool convertToFloat;
        bool cropOptimalSize;
    };
    virtual ~ImageFilter() = default;
    virtual XMat apply(XMat image, Params params) = 0;
};
class ThresholdFilter : public ImageFilter {
public:
    explicit ThresholdFilter(double thr=127, double max=255) : thr(thr), max(max) {}
    XMat apply(XMat image, Params params) final;
    const double thr;
    const double max;
};
class ChannelFilter : public ImageFilter {
public:
    enum Channel {red, green, blue, gray, hue, sat, value};
    explicit ChannelFilter(Channel channel) : channel(channel) {}
    XMat apply(XMat image, Params params) final;
    const Channel channel;
};
class GainBiasFilter : public ImageFilter {
public:
    explicit GainBiasFilter(double gain, double bias=0) : gain(gain), bias(bias) {}
    XMat apply(XMat image, Params params) final;
    const double gain;
    const double bias;
};
class GaussFilter : public ImageFilter {
public:
    GaussFilter(int kern_x, int kern_y) : kernX(kern_x), kernY(kern_y) {}
    XMat apply(XMat image, Params params) final;
    const int kernX;
    const int kernY;
};
class LaplacianFilter : public ImageFilter {
public:
    explicit LaplacianFilter(int kern=3, double scale=1, double delta=0) : kern(kern), scale(scale), delta(delta) {}
    XMat apply(XMat image, Params params) final;
    const int kern;
    const double scale;
    const double delta;
};
class SobelFilter : public ImageFilter {
public:
    explicit SobelFilter(int kern=3, double scale=1, double delta=0) : kern(kern), scale(scale), delta(delta) {}
    XMat apply(XMat image, Params params) final;
    const int kern;
    const double scale;
    const double delta;
};
class ScharrFilter : public ImageFilter {
public:
    explicit ScharrFilter(bool vert, double scale=1) : vert(vert), scale(scale) {}
    XMat apply(XMat image, Params params) final;
    const bool vert;
    const double scale;
};
class GradientFilter : public ImageFilter {
public:
    explicit GradientFilter(bool vert, double scale=1) : vert(vert), scale(scale) {}
    XMat apply(XMat image, Params params) final;
    const bool vert;
    const double scale;
};
class ColorGradientFilter : public ImageFilter {
public:
    explicit ColorGradientFilter(double scale=1, double delta=0)  : scale(scale), delta(delta) {}
    XMat apply(XMat image, Params params) final;
    const double scale;
    const double delta;
};
class LinesFilter : public ImageFilter {
public:

    explicit LinesFilter(bool vert, double gradient_scale=1, double gradient_threshold=45, int dilatePos=2, int dilateNeg=2, int erode=0);
    XMat apply(XMat image, Params params) final;

    const bool vert;
    double gradient_scale;
    double gradient_threshold;
    int dilatePos; // dilate down for positive gradients
    int dilateNeg; // dilate up for negative gradients
    int erode; // erode after positive and negative (dilated) masks merged (bitwise and)

    cv::Mat kernel_2;
    cv::Mat kernel_3;
};
class CompassFilter : public ImageFilter {
public:
    explicit CompassFilter() = default;
    XMat apply(XMat image, Params params) final;
};
class EdgeByBoxFilter : public ImageFilter {
public:
    EdgeByBoxFilter(int kern=5, double scale=2.0, double thr=0) : kern(kern), scale(scale), threshold(thr)  {}
    XMat apply(XMat image, Params params) final;
    const int kern;
    const double scale;
    const double threshold;
};
class DilateFilter : public ImageFilter {
public:
    DilateFilter(int kX, int kY, int iter=1) : kernX(kX), kernY(kY), iterations(iter) {}
    XMat apply(XMat image, Params params) final;
    const int kernX;
    const int kernY;
    const int iterations;
};
class ErodeFilter : public ImageFilter {
public:
    ErodeFilter(int kX, int kY, int iter=1) : kernX(kX), kernY(kY), iterations(iter) {}
    XMat apply(XMat image, Params params) final;
    const int kernX;
    const int kernY;
    const int iterations;
};
class HsvMaskFilter : public ImageFilter {
public:
    HsvMaskFilter() {}
    std::vector<std::pair<cv::Vec3b,cv::Vec3b>> rangesU;
    std::vector<std::pair<cv::Vec3f,cv::Vec3f>> rangesF;
    void calcMask(XMat image, XMat& mask, XMat& hsv);
    XMat apply(XMat image, Params params) override;
};
class HsvColorCropFilter : public HsvMaskFilter {
public:
    HsvColorCropFilter() {}
    XMat apply(XMat image, Params params) final;
};
class HsvGrayCropFilter : public HsvMaskFilter {
public:
    HsvGrayCropFilter() {}
    XMat apply(XMat image, Params params) final;
};
class HsvValueCropFilter : public HsvMaskFilter {
public:
    HsvValueCropFilter() {}
    XMat apply(XMat image, Params params) final;
};


} // namespace detect

#endif //EDROBOT_FILTERS_H
