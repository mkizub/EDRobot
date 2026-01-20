//
// Created by mkizub on 23.12.2025.
//


#include "../pch.h"

#include "Detector.h"
#include "Filters.h"

namespace detect {

XMat ThresholdFilter::apply(XMat image, Params params) {
    XMat out;
    cv::threshold(image, out, thr, max, cv::THRESH_BINARY);
    return out;
}

XMat ChannelFilter::apply(XMat image, Params params) {
    if (image.channels() == 1)
        return image;
    if (channel == red || channel == green || channel == blue) {
        std::vector<XMat> channels;
        cv::split(image, channels);
        if (channel == blue)
            return channels[0];
        if (channel == green)
            return channels[1];
        return channels[2];
    }
    if (channel == hue || channel == sat || channel == value) {
        XMat hsv;
        cv::cvtColor(image, hsv, cv::COLOR_BGR2HSV);
        std::vector<XMat> channels;
        cv::split(image, channels);
        if (channel == hue)
            return channels[0];
        if (channel == sat)
            return channels[1];
        return channels[2];
    }
    XMat gray;
    cv::cvtColor(image, gray, cv::COLOR_BGR2GRAY);
    return gray;
}

XMat GainBiasFilter::apply(XMat image, Params params) {
    XMat out;
    cv::convertScaleAbs(image, out, gain, bias);
    return out;
}

XMat GaussFilter::apply(XMat image, Params params) {
    XMat out;
    cv::GaussianBlur(image, out, cv::Size(kernX, kernY), 0, 0);
    return out;
}

XMat LaplacianFilter::apply(XMat image, Params params) {
    assert(image.depth() == CV_8U || image.depth() == CV_32F);
    double scale;
    if (kern == 1)
        scale = this->scale / 4.;
    else if (kern == 3)
        scale = this->scale / 8.;
    else
        scale = this->scale;
    if (image.channels() == 4)
        cv::cvtColor(image, image, cv::COLOR_BGRA2BGR);
    if (image.depth() == CV_8U) {
        XMat out16S;
        XMat out8U;
        cv::Laplacian(image, out16S, CV_16S, kern, scale, delta);
        cv::convertScaleAbs(out16S, out8U);
        return out8U;
    }
    else if (image.depth() == CV_32F) {
        XMat out32F;
        cv::Laplacian(image, out32F, CV_32F, kern, scale, delta);
        cv::max(out32F, 0.0f, out32F);
        cv::min(out32F, 1.0f, out32F);
        return out32F;
    }
    return image;
}

XMat SobelFilter::apply(XMat image, Params params) {
    XMat grad_x, grad_y, grad;
    cv::Sobel(image, grad_x, CV_32F, 1, 0, kern, scale, delta);
    cv::Sobel(image, grad_y, CV_32F, 0, 1, kern, scale, delta);
    addWeighted(grad_x, 0.5, grad_y, 0.5, 0, grad);
    if (image.depth() == CV_8U && !params.convertToFloat) {
        XMat out8U;
        convertScaleAbs(grad, out8U);
        if (out8U.channels() == 4)
            cv::cvtColor(out8U, out8U, cv::COLOR_BGRA2BGR);
        return out8U;
    }
    cv::max(grad, 0.0f, grad);
    cv::min(grad, 1.0f, grad);
    if (grad.channels() == 4)
        cv::cvtColor(grad, grad, cv::COLOR_BGRA2BGR);
    return grad;
}

XMat ScharrFilter::apply(XMat image, Params params) {
    assert(image.depth() == CV_8U || image.depth() == CV_32F);
    if (image.depth() == CV_8U) {
        XMat out16S;
        XMat out8U;
        cv::Scharr(image, out16S, CV_16S, 1, 1, scale);
        cv::convertScaleAbs(out16S, out8U);
        return out8U;
    }
    else if (image.depth() == CV_32F) {
        XMat out32F;
        cv::Scharr(image, out32F, CV_32F, 1, 1, scale);
        cv::max(out32F, 0.0f, out32F);
        cv::min(out32F, 1.0f, out32F);
        return out32F;
    }
    return image;
}

XMat GradientFilter::apply(XMat image, Params params) {
    assert(image.depth() == CV_8U || image.depth() == CV_32F);
    bool restore8U = false;
    if (image.depth() == CV_8U) {
        restore8U = !params.convertToFloat;
        XMat out32f;
        image.convertTo(out32f, CV_32F, 1.0 / 255.0);
        image = out32f;
    }
    XMat grad;
    if (vert)
        cv::Scharr(image, grad, CV_32F, 0, 1, 0.5*scale*0.0625, 0.5);
    else
        cv::Scharr(image, grad, CV_32F, 1, 0, 0.5*scale*0.0625, 0.5);
    if (grad.channels() == 4)
        cv::cvtColor(grad, grad, cv::COLOR_BGRA2BGR);
    if (!restore8U)
        return grad;
    //cv::max(grad, 0.0f, grad);
    //cv::min(grad, 1.0f, grad);
    XMat out8U;
    cv::convertScaleAbs(grad, out8U, 255);
    return out8U;
}

XMat ColorGradientFilter::apply(XMat image, Params params) {
    if (image.channels() < 3)
        return image;
    std::vector<XMat> channel;
    cv::split(image, channel);
    std::vector<XMat> merge;
    merge.reserve(4);
    XMat grad_gv;
    cv::Scharr(channel[1], grad_gv, CV_32F, 0, 1, scale*0.0625 / 255., delta);
    merge.push_back(grad_gv);
    XMat grad_gh;
    cv::Scharr(channel[1], grad_gh, CV_32F, 1, 0, scale*0.0625 / 255., delta);
    merge.push_back(grad_gh);
    XMat grad_rv;
    cv::Scharr(channel[2], grad_rv, CV_32F, 0, 1, scale*0.0625 / 255., delta);
    merge.push_back(grad_rv);
    XMat grad_rh;
    cv::Scharr(channel[2], grad_rh, CV_32F, 1, 0, scale*0.0625 / 255., delta);
    merge.push_back(grad_rh);

    XMat out;
    cv::merge(merge, out);
    return out;
}

LinesFilter::LinesFilter(bool vert, double gradient_scale, double gradient_threshold, int dilatePos, int dilateNeg, int erode)
        : vert(vert)
        , gradient_scale(gradient_scale)
        , gradient_threshold(gradient_threshold)
        , dilatePos(dilatePos)
        , dilateNeg(dilateNeg)
        , erode(erode)
{
    if (!vert) {
        kernel_2 = cv::getStructuringElement(cv::MORPH_RECT, {1, 2});
        kernel_3 = cv::getStructuringElement(cv::MORPH_RECT, {1, 3});
    } else {
        kernel_2 = cv::getStructuringElement(cv::MORPH_RECT, {2, 1});
        kernel_3 = cv::getStructuringElement(cv::MORPH_RECT, {3, 1});
    }
}

XMat LinesFilter::apply(XMat image, Params params) {
    XMat grad;
    cv::Point anchor;
    if (!vert) {
        cv::Scharr(image, grad, CV_32F, 0, 1, gradient_scale * 0.0625 / 255., 0);
        anchor = {0, 1};
    } else {
        cv::Scharr(image, grad, CV_32F, 1, 0, gradient_scale * 0.0625 / 255., 0);
        anchor = {1, 0};
    }

    XMat grad_posF;
    cv::max(grad, 0.0, grad_posF);
    XMat grad_posU;
    cv::convertScaleAbs(grad_posF, grad_posU, 255);
    cv::threshold(grad_posU, grad_posU, gradient_threshold, 255, cv::THRESH_BINARY);
    if (dilatePos > 0)
        cv::dilate(grad_posU, grad_posU, kernel_2, anchor, dilatePos);

    XMat grad_negF;
    cv::min(grad, 0.0, grad_negF);
    XMat grad_negU;
    cv::convertScaleAbs(grad_negF, grad_negU, 255);
    cv::threshold(grad_negU, grad_negU, gradient_threshold, 255, cv::THRESH_BINARY);
    if (dilateNeg > 0) {
        cv::flip(grad_negU, grad_negU, vert ? 1 : 0);
        cv::dilate(grad_negU, grad_negU, kernel_2, anchor, dilateNeg);
        cv::flip(grad_negU, grad_negU, vert ? 1 : 0);
    }

    XMat out;
    if (dilatePos > 0 || dilateNeg > 0)
        cv::bitwise_and(grad_posU, grad_negU, out);
    else
        cv::bitwise_or(grad_posU, grad_negU, out);
    if (erode > 0)
        cv::erode(out, out, kernel_3, anchor, erode);
    return out;
}

XMat CompassFilter::apply(XMat image, Params params) {
    if (image.channels() < 3)
        return image;
    std::vector<XMat> channel;
    cv::split(image, channel);
    std::vector<XMat> merge;
    merge.reserve(4);
    XMat grad_gv;
    cv::Scharr(channel[1], grad_gv, CV_32F, 0, 1, 0.0625 / 255., 0);
    merge.push_back(grad_gv);
    XMat grad_gh;
    cv::Scharr(channel[1], grad_gh, CV_32F, 1, 0, 0.0625 / 255., 0);
    merge.push_back(grad_gh);
    XMat grad_rv;
    cv::Scharr(channel[2], grad_rv, CV_32F, 0, 1, 0.0625 / 255., 0);
    merge.push_back(grad_rv);
    XMat grad_rh;
    cv::Scharr(channel[2], grad_rh, CV_32F, 1, 0, 0.0625 / 255., 0);
    merge.push_back(grad_rh);

    XMat out;
    cv::merge(merge, out);
    return out;
}

XMat EdgeByBoxFilter::apply(XMat image, Params params) {
    assert(image.depth() == CV_8U || image.depth() == CV_32F);
    if (image.depth() == CV_8U) {
        XMat smooth;
        cv::boxFilter(image, smooth, -1, {kern,kern});
        XMat out;
        cv::addWeighted(image, scale, smooth, -scale, 0, out, CV_8UC1);
        if (threshold > 0)
            cv::threshold(out, out, threshold, 255, cv::THRESH_BINARY);
        return out;
    }
    else if (image.depth() == CV_32F) {
        XMat smooth;
        cv::boxFilter(image, smooth, -1, {kern,kern});
        XMat out;
        cv::addWeighted(image, scale, smooth, -scale, 0, out, CV_32F);
        cv::max(out, 0.0f, out);
        if (threshold > 0)
            cv::threshold(out, out, threshold, 1, cv::THRESH_BINARY);
        else
            cv::min(out, 1.0f, out);
        return out;
    }
    return image;
}

XMat DilateFilter::apply(XMat image, Params params) {
    cv::Mat kernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(kernX, kernY));
    XMat out;
    cv::dilate(image, out, kernel, cv::Point(-1, -1), iterations);
    return out;
}

XMat ErodeFilter::apply(XMat image, Params params) {
    cv::Mat kernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(kernX, kernY));
    XMat out;
    cv::erode(image, out, kernel, cv::Point(-1, -1), iterations);
    return out;
}

void HsvMaskFilter::calcMask(XMat image, XMat& mask, XMat& hsv) {
    if (rangesU.empty())
        return;
    if (rangesF.empty()) {
        for (auto& r : rangesU) {
            cv::Vec3b min = r.first;
            cv::Vec3b max = r.second;
            cv::Vec3f minF (min[0]*2.0, min[1]>=255 ? 1.1f : min[1]/255.f, min[2] >= 255 ? 1.1f : min[2]/255.f); // 0 < H < 360 for floating values!
            cv::Vec3f maxF (max[0]*2.0, max[1]>=255 ? 1.1f : max[1]/255.f, max[2] >= 255 ? 1.1f : max[2]/255.f); // 0 < H < 360 for floating values!
            rangesF.emplace_back(minF, maxF);
        }
    }
    cv::cvtColor(image, hsv, cv::COLOR_BGR2HSV);
    //cv::Mat tmp_hsv = hsv.getMat(cv::ACCESS_READ).clone();
    mask.create(image.rows, image.cols, CV_8UC1);
    if (hsv.depth() == CV_8U)
        cv::inRange(hsv, rangesU.front().first, rangesU.front().second, mask);
    else
        cv::inRange(hsv, rangesF.front().first, rangesF.front().second, mask);
    //cv::Mat tmp_mask = mask.getMat(cv::ACCESS_READ).clone();
    if (rangesU.size() > 1) {
#ifdef EDROBOT_USE_OPENCL
        cv::Mat accum = mask.getMat(cv::ACCESS_RW);
#else
        cv::Mat& accum = mask;
#endif
        for (int r=1; r < rangesU.size(); r++) {
            XMat m;
            if (hsv.depth() == CV_8U)
                cv::inRange(hsv, rangesU[r].first, rangesU[r].second, m);
            else
                cv::inRange(hsv, rangesF[r].first, rangesF[r].second, m);
            cv::bitwise_or(accum, toMat(m), accum);
        }
    }
}

XMat HsvMaskFilter::apply(XMat image, Params params) {
    XMat mask, hsv;
    calcMask(image, mask, hsv);
    return mask;
}

XMat HsvColorCropFilter::apply(XMat image, Params params) {
    XMat mask, hsv;
    calcMask(image, mask, hsv);
    if (mask.empty())
        return {};
    XMat masked;
    image.copyTo(masked, mask);
    return masked;
}

XMat HsvGrayCropFilter::apply(XMat image, Params params) {
    XMat mask, hsv;
    calcMask(image, mask, hsv);
    if (mask.empty())
        return {};
    XMat gray;
    cv::cvtColor(image, gray, cv::COLOR_BGR2GRAY);
    XMat masked;
    gray.copyTo(masked, mask);
    return masked;
}

XMat HsvValueCropFilter::apply(XMat image, Params params) {
    XMat mask, hsv;
    calcMask(image, mask, hsv);
    if (mask.empty())
        return {};
    std::vector<XMat> channels;
    cv::split(hsv, channels);
    XMat masked;
    channels[2].copyTo(masked, mask);
    return masked;
}

} // namespace detect
