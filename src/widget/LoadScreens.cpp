//
// Created by mkizub on 26.12.2025.
//
#include "../pch.h"

#include "EDWidget.h"
#include "Button.h"
#include "List.h"

#include "../detect/Detector.h"
#include "../detect/Lines.h"
#include "../detect/Tiles.h"
#include "../detect/NavPanel.h"
#include "../detect/TextDetector.h"

#include "../ClassifyEnv.h"
#include "../FuzzyMatch.h"

using namespace widget;
using namespace detect;

namespace widget {

void debugNavPanel() {
#ifndef NDEBUG
    //detect::NavPanelDetectLock lock("flt-line");
    //detect::NavPanelDetector::standaloneTest("nav-panel-test-9.png", "scr-left-panel");
    //detect::NavPanelDetectLock lock("flt-line");
    //detect::NavPanelDetector::standaloneTest("nav-panel-left-filter.png", "scr-left-panel");
#endif
}

static cv::Vec3b color_from_json(const js::value& v) {
    unsigned bgr = 0;
    if (v.is_number())
        bgr = v.as_unsigned();
    else if (v.is_array()) {
        unsigned r = v.as_array().at(0).as_unsigned();
        unsigned g = v.as_array().at(1).as_unsigned();
        unsigned b = v.as_array().at(2).as_unsigned();
        bgr = (r&0xFF) | ((g&0xFF)<<8) | ((b&0xFF)<<16);
    }
    else if (v.is_string()) {
        auto& s = v.as_string();
        if (s.size() == 7 && s[0] == '#')
            bgr = std::stol(s.substr(1), nullptr, 16);
    }
    return encodeBGR(bgr);
}

static double value_from_json(const js::value& v, FovScale* fov_scale) {
    if (v.is_number())
        return v.as_real();
    cv::Size sz;
    sz.width = v[0].as_int();
    sz.height = sz.width;
    if (fov_scale && v[1].is_number())
        sz = fov_scale->apply(sz, v[1].as_real());
    return sz.width;
}

static cv::Point point_from_json(const js::value& v) {
    cv::Point p;
    p.x = v[0].as_int();
    p.y = v[1].as_int();
    return p;
}

static cv::Size size_from_json(const js::value& v, FovScale* fov_scale) {
    cv::Size sz;
    sz.width = v[0].as_int();
    sz.height = v[1].as_int();
    if (fov_scale && v[2].is_number())
        sz = fov_scale->apply(sz, v[2].as_real());
    return sz;
}

static cv::Rect rect_from_json(const js::value& v) {
    cv::Rect rect;
    rect.x = v.at(0,0).as_int();
    rect.y = v.at(1,0).as_int();
    rect.width = v.at(2,0).as_int();
    rect.height = v.at(3,0).as_int();
    return rect;
}

static cv::Rect fov_rect_from_json(const js::value& v, double& fov) {
    cv::Rect rect;
    rect.x = v.at(0,0).as_int();
    rect.y = v.at(1,0).as_int();
    rect.width = v.at(2,0).as_int();
    rect.height = v.at(3,0).as_int();
    fov = v.at(4,0.0).as_real();
    return rect;
}

static cv::Rect rect_from_json(const js::value& v, FovScale* fov_scale) {
    cv::Rect rect;
    rect.x = v.at(0,0).as_int();
    rect.y = v.at(1,0).as_int();
    rect.width = v.at(2,0).as_int();
    rect.height = v.at(3,0).as_int();
    if (fov_scale && v[4].is_real())
        rect = fov_scale->apply(rect, v[4].as_real());
    return rect;
}

static cv::Rect mark_from_json(const js::value& v, FovScale* fov_scale) {
    cv::Rect rect;
    rect.x = v.at(0,0).as_int();
    rect.y = v.at(1,0).as_int();
    rect.width = v.at(2,0).as_int();
    rect.height = v.at(3,0).as_int();
    if (fov_scale && v[4].is_number()) {
        cv::Size offs = fov_scale->apply(cv::Size(rect.tl()), v[4].as_real());
        cv::Size size = fov_scale->apply(rect.size(), v[4].as_real());
        rect = {cv::Point(offs), size};
    }
    return rect;
}

template<class Tp>
static void minmax_from_json(const js::value& v, Tp& vmin, Tp& vmax) {
    Tp tmin = vmin;
    Tp tmax = vmax;
    if (v.is_number())
        tmin = tmax = v.as_real();
    else if (v.is_array()) {
        auto& jt = v.as_array();
        if (!jt.empty())
            tmin = jt[0].as_real();
        if (jt.size() > 1)
            tmax = jt[1].as_real();
        else
            tmax = tmin;
    }
    if (tmax < tmin)
        std::swap(tmin, tmax);
    vmin = tmin;
    vmax = tmax;
}

static void ext_from_json(const js::value& v, cv::Point& extendLT, cv::Point& extendRB) {
    if (!v)
        return;
    int extL = 0;
    int extT = 0;
    int extR = 0;
    int extB = 0;
    if (v.is_number())
        extL = extT = extR = extB = v.as_int();
    else if (v.is_array()) {
        auto& jext = v.as_array();
        if (!jext.empty())
            extL = extT = extR = extB = jext[0].as_int();
        if (jext.size() > 1)
            extT = extB = jext[1].as_int();
        if (jext.size() > 2)
            extR = jext[2].as_int();
        if (jext.size() > 3)
            extB = jext[3].as_int();
    }
    extendLT = {extL, extT};
    extendRB = {extR, extB};
}

static detect::ImageFilter* filter_from_json(const js::value& jf) {
    if (!jf.is_object())
        return nullptr;
    if (jf["threshold"].is_number()) {
        double thr = jf["threshold"].as_real();
        double max = thr < 1 ? 0.5 : 255.0;
        if (jf["max"].is_number())
            max = jf["max"].as_real();
        return new ThresholdFilter(thr, max);
    }
    if (jf["channel"].is_string()) {
        std::string chn = toLower(jf["channel"].as_string());
        ChannelFilter::Channel channel = enum_cast<ChannelFilter::Channel>(chn).value();
        return new ChannelFilter(channel);
    }
    if (jf["gauss"].is_object()) {
        int kernX = 3;
        int kernY = 3;
        if (jf["gauss"]["kern"].is_array()) {
            kernX = jf["gauss"]["kern"][0].as_int();
            kernY = jf["gauss"]["kern"][1].as_int();
        } else {
            kernX = jf["gauss"]["kern"].as_int_or(3);
            kernY = kernX;
        }
        kernX = (kernX & ~1) + 1;
        kernY = (kernY & ~1) + 1;
        return new GaussFilter(kernX, kernY);
    }
    if (jf["laplacian"].is_object()) {
        int kern = jf["laplacian"]["kern"].as_int_or(3);
        double scale = jf["laplacian"]["scale"].as_real_or(1);
        double delta = jf["laplacian"]["delta"].as_real_or(0);
        return new LaplacianFilter(kern, scale, delta);
    }
    if (jf["sobel"].is_object()) {
        int kern = jf["sobel"]["kern"].as_int_or(3);
        double scale = jf["sobel"]["scale"].as_real_or(1);
        double delta = jf["sobel"]["delta"].as_real_or(0);
        return new SobelFilter(kern, scale, delta);
    }
    if (jf["grad"].is_object()) {
        double scale = jf["grad"]["scale"].as_real_or(1);
        double delta = jf["grad"]["delta"].as_real_or();
        return new ColorGradientFilter(scale, delta);
    }
    if (jf["scharr"].is_object()) {
        bool vert = false;
        double scale = jf["scharr"]["scale"].as_real_or(1);
        if (jf["scharr"]["vert"].is_bool())
            vert = jf["scharr"]["vert"].as_bool();
        else if (jf["scharr"]["horz"].is_bool())
            vert = !jf["scharr"]["horz"].as_bool();
        return new ScharrFilter(vert, scale);
    }
    if (jf["lines"].is_object()) {
        bool vert = false;
        double scale = jf["lines"]["scale"].as_real_or(1);
        double threshold = jf["lines"]["thr"].as_real_or(45);
        int dilatePos = 2;
        int dilateNeg = 2;
        int erode = jf["lines"]["erode"].as_int_or(0);
        if (jf["lines"]["vert"].is_bool())
            vert = jf["lines"]["vert"].as_bool();
        else if (jf["lines"]["horz"].is_bool())
            vert = !jf["lines"]["horz"].as_bool();
        dilatePos = dilateNeg = jf["lines"]["dilate"].as_int_or(2);
        dilatePos = jf["lines"]["dilate_pos"].as_int_or(dilatePos);
        dilateNeg = jf["lines"]["dilate_neg"].as_int_or(dilateNeg);
        return new LinesFilter(vert, scale, threshold, dilatePos, dilateNeg, erode);
    }
    if (jf["edge_box"].is_object()) {
        int kern = jf["edge_box"]["kern"].as_int_or(5);
        double scale = jf["edge_box"]["scale"].as_real_or(2.0);
        double thr = jf["edge_box"]["thr"].as_real_or(0);
        return new EdgeByBoxFilter(kern, scale, thr);
    }
    if (jf["gain"].is_number()) {
        double gain = jf["gain"].as_real_or(1);
        double bias = jf["bias"].as_real_or(0);
        return new GainBiasFilter(gain, bias);
    }
    if (!jf["dilate"].empty()) {
        int kernX = 3;
        int kernY = 3;
        int iter = jf["iter"].as_int_or(1);
        if (jf["dilate"].is_int())
            kernX = kernY = jf["dilate"].as_int();
        else if (jf["dilate"].is_array()) {
            kernX = jf["dilate"][0].as_int();
            kernY = jf["dilate"][1].as_int();
        }
        return new DilateFilter(kernX, kernY, iter);
    }
    if (!jf["erode"].empty()) {
        int kernX = 3;
        int kernY = 3;
        int iter = jf["iter"].as_int_or(1);
        if (jf["erode"].is_int())
            kernX = kernY = jf["erode"].as_int();
        else if (jf["erode"].is_array()) {
            kernX = jf["erode"][0].as_int();
            kernY = jf["erode"][1].as_int();
        }
        return new ErodeFilter(kernX, kernY, iter);
    }
    bool has_hsv_crop = !jf["hsv_crop"].empty();
    bool has_hsv_gray = !jf["hsv_gray"].empty();
    bool has_hsv_mask = !jf["hsv_mask"].empty();
    bool has_hsv_cval = !jf["hsv_cval"].empty();
    if (has_hsv_crop || has_hsv_gray || has_hsv_mask) {
        js::value jhsv;
        HsvMaskFilter* filter;
        if (has_hsv_crop) {
            jhsv = jf["hsv_crop"];
            filter = new HsvColorCropFilter();
        }
        else if (has_hsv_gray) {
            jhsv = jf["hsv_gray"];
            filter = new HsvGrayCropFilter();
        }
        else if (has_hsv_cval) {
            jhsv = jf["hsv_gray"];
            filter = new HsvValueCropFilter();
        }
        else {
            jhsv = jf["hsv_mask"];
            filter = new HsvMaskFilter();
        }
        std::vector<js::value> jarr;
        if (jhsv.is_array())
            jarr = jhsv.as_array();
        else if (jhsv.is_object())
            jarr.push_back(jhsv);

        for (const auto& jv : jarr) {
            cv::Vec3b min = {0,0,0};
            cv::Vec3b max = {255,255,255};
            if (jv["h"].is_array()) {
                min[0] = jv["h"][0].as_int();
                max[0] = jv["h"][1].as_int();
            }
            if (jv["s"].is_array()) {
                min[1] = jv["s"][0].as_int();
                max[1] = jv["s"][1].as_int();
            }
            if (jv["v"].is_array()) {
                min[2] = jv["v"][0].as_int();
                max[2] = jv["v"][1].as_int();
            }
            filter->rangesU.emplace_back(min,max);
        }
        return filter;
    }
    return nullptr;
}

static std::vector<std::unique_ptr<detect::ImageFilter>> filters_from_json(const js::value& v) {
    std::vector<std::unique_ptr<detect::ImageFilter>> filters;
    if (v.is_object()) {
        auto f = filter_from_json(v);
        if (f)
            filters.emplace_back(f);
    }
    else if (v.is_array()) {
        for (auto& jf : v.as_array()) {
            auto f = filter_from_json(jf);
            if (f)
                filters.emplace_back(f);
        }
    }
    return filters;
}

static void image_template_from_json(const js::value& j, ImageTemplate* templ) {
    if (j.at("name").is_string()) {
        templ->name = j.at("name").as_string();
    }

    if (j.at("scale").is_number())
        templ->testScales.push_back(j["scale"].as_real());
    else if (j.at("scale").is_array()) {
        for (auto& scl : j.at("scale").as_array())
            templ->testScales.push_back(scl.as_real());
    }

    if (j.at("angle").is_int())
        templ->testAngles.push_back(j["angle"].as_int());
    else if (j.at("angle").is_array()) {
        for (auto& angle : j.at("angle").as_array())
            templ->testAngles.push_back(angle.as_int());
    }

    ext_from_json(j["ext"], templ->extendLT, templ->extendRB);
    minmax_from_json(j["t"], templ->threshold_min, templ->threshold_max);

    if (j.at("method").is_string()) {
        const std::string method = j.at("method").as_string();
        if (method == "coeff")
            templ->matchMethod = cv::TM_CCOEFF_NORMED;
        else if (method == "corr")
            templ->matchMethod = cv::TM_CCORR_NORMED;
        else if (method == "sqdiff")
            templ->matchMethod = cv::TM_SQDIFF_NORMED;
    }

    if (!j["filter"].empty())
        templ->filters = filters_from_json(j["filter"]);
}

static Detector* detector_from_json(const js::value& j, Widget& widget, FovScale* fov_scale) {
    if (j.is_null())
        return nullptr;
    if (j.is_bool()) {
        if (j.as_bool())
            return new ConstDetector(1);
        return new ConstDetector(0);
    }
    if (j.is_number()) {
        double value = j.as_real();
        value = std::clamp(value, 0.0, 1.0);
        return new ConstDetector(value);
    }
    if (j.is_string()) {
        const auto& referred = j.as_string();
        return new ReferDetector(referred);
    }
    if (j.is_array()) {
        std::vector<std::unique_ptr<Detector>> oracles;
        for (auto& jo : j.as_array()) {
            Detector *oracle = detector_from_json(jo, widget, fov_scale);
            if (oracle)
                oracles.emplace_back(oracle);
        }
        return new Sequence(std::move(oracles));
    }
    if (j.is_object()) {
        if (j["const"].is_number()) {
            double value = j["const"].as_real();
            value = std::clamp(value, 0.0, 1.0);
            auto* cdet = new ConstDetector(value);
            cdet->classifierWeight = j["weight"].as_real_or(1);
            return cdet;
        }
        if (j["ref"].is_string()) {
            const auto referred = j["ref"].as_string();
            auto* rdet = new ReferDetector(referred);
            rdet->classifierWeight = j["weight"].as_real_or(1);
            return rdet;
        }
        if (j["best"].is_array()) {
            std::vector<std::unique_ptr<Detector>> oracles;
            for (auto& jo : j["best"].as_array()) {
                Detector *oracle = detector_from_json(jo, widget, fov_scale);
                if (oracle)
                    oracles.emplace_back(oracle);
            }
            auto* bdet = new BestOf(std::move(oracles));
            bdet->classifierWeight = j["weight"].as_real_or(1);
            return bdet;
        }
        if (j["seq"].is_array()) {
            std::vector<std::unique_ptr<Detector>> oracles;
            for (auto& jo : j["seq"].as_array()) {
                Detector *oracle = detector_from_json(jo, widget, fov_scale);
                if (oracle)
                    oracles.emplace_back(oracle);
            }
            auto* sdet = new Sequence(std::move(oracles));
            sdet->classifierWeight = j["weight"].as_real_or(1);
            return sdet;
        }
        if (j.as_object().contains("anchor")) {
            std::string filename = "templates/"+j.at("img").as_string();
            spEvalRect rect = makeEvalRect(widget, "rect", j["rect"], fov_scale, true);
            cv::Point anchor = size_from_json(j["anchor"], fov_scale);
            auto* templ = new AnchorDetector(filename, rect, anchor);
            image_template_from_json(j, templ);
            templ->classifierWeight = j["weight"].as_real_or(1);
            return templ;
        }
        if (j.as_object().contains("img")) {
            std::string filename = "templates/"+j.at("img").as_string();
            spEvalRect rect = makeEvalRect(widget, "rect", j["rect"], fov_scale, false);
            auto* templ = new ImageTemplate(filename, rect);
            image_template_from_json(j, templ);
            templ->classifierWeight = j["weight"].as_real_or(1);
            return templ;
        }
        if (j.as_object().contains("line")) {
            spEvalLine line = makeEvalLine(widget, "line", j["line"], fov_scale);
            auto* ldet = new detect::LineDetector(line);

            ldet->name = j["name"].as_string_or();
            ldet->classifierWeight = j["weight"].as_real_or(1);

            ext_from_json(j["ext"], ldet->extendLT, ldet->extendRB);
            if (j.at("delta"))
                minmax_from_json(j["delta"], ldet->extendAngleMin, ldet->extendAngleMax);
            if (j.at("votes")) {
                ldet->houghThreshold = j["votes"].as_int();
                if (fov_scale && j["line"][4].is_number())
                    ldet->houghThreshold *= fov_scale->getScaleForFOV(j["line"][4].as_real());
            }
            ldet->angleStep = j["prec"].as_real_or(1);

            if (!j["filter"].empty())
                ldet->filters = filters_from_json(j["filter"]);
            return ldet;
        }
        if (j.as_object().contains("tiles")) {
            cv::Rect tilesRect = rect_from_json(j["tiles"], fov_scale);
            cv::Rect marksRect = mark_from_json(j["rect"], fov_scale);

            std::string name = j["name"].as_string();
            int rows_min = 1;
            int rows_max = 1;
            minmax_from_json(j["rows"], rows_min, rows_max);
            int cols_min = 1;
            int cols_max = 1;
            minmax_from_json(j["cols"], cols_min, cols_max);

            int gap = j["gap"].as_int();

            auto* tiles = new TilesDetector(name, tilesRect, marksRect,
                                            rows_min, rows_max, cols_min, cols_max, gap);

            if (j["size"].is_array())
                tiles->mTileSize = size_from_json(j["size"], fov_scale);
            tiles->mTryMerge = j["merge"].as_bool_or(true);
            tiles->classifierWeight = j["weight"].as_real_or(1);

            if (j["icons"].is_string()) {
                std::string icons = "templates/" + j["icons"].as_string();
                auto* templ = new ImageTemplate(icons, spEvalRect(new ConstRect(marksRect)));
                image_template_from_json(j, templ);
                if (j["icon_align"].is_string()) {
                    auto& align = j["icon_align"].as_string();
                    if (toLower(align) == "center")
                        tiles->mIconAlign = TilesDetector::IconAlign::Center;
                    else if (toLower(align) == "top-left")
                        tiles->mIconAlign = TilesDetector::IconAlign::TopLeft;
                }
                tiles->icons_detector = std::unique_ptr<ImageTemplate>(templ);
            }
            if (j["labels"].is_object()) {
                if (!j["font_height"].empty())
                    tiles->mFontHeight = value_from_json(j["font_height"], fov_scale);
                FuzzyMatch fm;
                for (auto& p : j["labels"].as_object()) {
                    std::vector<std::wstring>& texts = tiles->labels[p.first];
                    if (p.second.is_string()) {
                        auto& txt = p.second.as_string();
                        texts.push_back(fm.toOCR(toUtf16(txt)));
                    } else if (p.second.is_array()) {
                        for (auto& t : p.second.as_array()) {
                            auto& txt = t.as_string();
                            texts.push_back(fm.toOCR(toUtf16(txt)));
                        }
                    }
                }
            }

            return tiles;
        }
        if (j.as_object().contains("texts")) {
            std::string name = j["name"].as_string_or();
            spEvalRect textsRect = makeEvalRect(widget, name.c_str(), j["rect"], fov_scale, false);

            auto* tdet = new TextDetector(name, textsRect);

            FuzzyMatch fm;
            for (auto& p : j["texts"].as_object()) {
                std::vector<std::wstring>& texts = tdet->labels[p.first];
                if (p.second.is_string()) {
                    auto& txt = p.second.as_string();
                    texts.push_back(fm.toOCR(toUtf16(txt)));
                } else if (p.second.is_array()) {
                    for (auto& t : p.second.as_array()) {
                        auto& txt = t.as_string();
                        texts.push_back(fm.toOCR(toUtf16(txt)));
                    }
                }
            }

            minmax_from_json(j["t"], tdet->mThresholdMin, tdet->mThresholdMax);
            tdet->classifierWeight = j["weight"].as_real_or(1);
            if (!j["font_height"].empty())
                tdet->mFontHeight = value_from_json(j["font_height"], fov_scale);
            if (j["psm"].is_int())
                tdet->mOcrPSM = (int)j["psm"].as_int();
            tdet->mMultiLine = j["multiline"].as_bool_or(false);

            return tdet;
        }
        if (j.as_object().contains("nav_panel")) {
            std::vector<std::unique_ptr<LineDetector>> lines;
            std::vector<std::unique_ptr<AnchorDetector>> anchors;
            std::vector<NavPanelDetector::Tab> tabs;
            for (auto& jo : j["nav_panel"]["lines"].as_array()) {
                Detector *oracle = detector_from_json(jo, widget, fov_scale);
                if (auto ldet = dynamic_cast<LineDetector*>(oracle))
                    lines.emplace_back(ldet);
            }
            for (auto& jo : j["nav_panel"]["anchors"].as_array()) {
                Detector *oracle = detector_from_json(jo, widget, fov_scale);
                if (auto adet = dynamic_cast<AnchorDetector*>(oracle))
                    anchors.emplace_back(adet);
            }
            for (auto& jo : j["nav_panel"]["tabs"].as_array()) {
                cv::Rect rect = rect_from_json(jo["rect"]);
                const std::string& name = jo["name"].as_string();
                tabs.emplace_back(NavPanelDetector::Tab{rect, name});
            }
            auto* pdet = new NavPanelDetector(widget.path, std::move(lines), std::move(anchors), std::move(tabs));
            pdet->classifierWeight = j["weight"].as_real_or(1);
            return pdet;
        }
        return nullptr;
    }
    return nullptr;
}

static void from_json(const js::value& j, std::map<std::string,std::vector<BaseDialog::Vars>>& varSetMap) {
    for (auto& varSet_it : j.as_object()) {
        std::string varSetName = varSet_it.first;
        for (auto& vars_it : varSet_it.second.as_array()) {
            BaseDialog::Vars& vars = varSetMap[varSetName].emplace_back();
            if (vars_it.at("key").is_string())
                vars.keys.push_back(vars_it.at("key").as_string());
            else if (vars_it.at("key").is_array()) {
                for (auto& js : vars_it.at("key").as_array())
                    vars.keys.push_back(js.as_string());
            }
            for (auto& jv_it : vars_it.as_object()) {
                if (jv_it.first == "key")
                    continue;
                if (jv_it.second.is_array()) {
                    for (auto jv : jv_it.second.as_array())
                        vars.values[jv_it.first].push_back(jv.as_real());
                }
                else if (jv_it.second.is_number())
                    vars.values[jv_it.first].push_back(jv_it.second.as_real());
            }
        }
    }
}

Widget* widget_from_json(const js::value& j, Widget* parent, FovScale* fov_scale) {
    if (j.is_null()) {
        return nullptr;
    }
    FovScale scr_fov_scale;
    Widget* widget = nullptr;
    auto& jo = j.as_object();
    auto name = jo.at("name").as_string();
    if (name.starts_with("scr-")) {
        auto scr = new Screen(name, parent, j["status"]);
        if (auto jfscl=j["fov_size"]; jfscl.is_array() || jfscl.is_number()) {
            if (jfscl.is_number()) {
                fov_scale = &scr_fov_scale;
                fov_scale->scale60 = jfscl.as_real();
            } else {
                double fov0, fov1;
                cv::Rect rect0 = fov_rect_from_json(jfscl[0], fov0);
                cv::Rect rect1 = fov_rect_from_json(jfscl[1], fov1);
                scr_fov_scale = FovScale(fov0, fov1, rect0, rect1);
                fov_scale = &scr_fov_scale;
            }
        } else {
            fov_scale = nullptr;
        }
        if (auto jvars = j["vars"]; jvars.is_object()) {
            from_json(jvars, scr->varSetMap);
        }
        if (auto jt = j["transform"]; jt.is_object()) {
            // transform: { tl: [212,256], tr: [1276,242], br: [1296,800], bl: [270,912] }
            // transform: { line: "lpline", tl: [0,-50], tr: [0,-50], ratio: 1.77777777 }
            spEvalPoint tl = makeEvalPoint(*scr, "tl", jt["tl"], fov_scale);
            spEvalPoint tr = makeEvalPoint(*scr, "tr", jt["tr"], fov_scale);
            spEvalPoint br = makeEvalPoint(*scr, "br", jt["br"], fov_scale);
            spEvalPoint bl = makeEvalPoint(*scr, "bl", jt["bl"], fov_scale);
            cv::Size sz = point_from_json(jt["size"]);
            if (jt["line"]) {
                std::vector<std::string> lines;
                if (jt["line"].is_array()) {
                    for (auto& l : jt["line"].as_array())
                        lines.push_back(l.as_string());
                } else {
                    lines.push_back(jt["line"].as_string());
                }
                scr->transform = spEvalTransform(new LineTransform(lines, tl, tr, br, bl, sz));
            }
            else {
                scr->transform = spEvalTransform(new ConstTransform(tl, tr, br, bl, sz));
            }
        }
        widget = scr;
    }
    else if (name.starts_with("dlg-")) {
        auto dlg = new Dialog(name, parent);
        widget = dlg;
    }
    else if (name.starts_with("mod-")) {
        auto mode = new Mode(name, parent);
        widget = mode;
    }
    else if (name.starts_with("btn-")) {
        auto btn = new Button(name, parent);
        widget = btn;
        widget->setRect("rect", j, fov_scale);
        if (j["icon"].is_string())
            btn->icon = "templates/"+j["icon"].as_string();
        ext_from_json(j["ext"], btn->extendLT, btn->extendRB);
        btn->filters = filters_from_json(j["filter"]);
        btn->force_present = j["force_present"].as_bool_or(false);
    }
    else if (name.starts_with("spn-")) {
        auto btn = new Spinner(name, parent);
        widget = btn;
        widget->setRect("rect", j, fov_scale);
        ext_from_json(j["ext"], btn->extendLT, btn->extendRB);
        btn->filters = filters_from_json(j["filter"]);
        btn->force_present = j["force_present"].as_bool_or(false);
    }
    else if (name.starts_with("til-")) {
        std::string icon;
        if (jo.contains("icon"))
            icon = jo.at("icon").as_string();
        auto btn = new TileBtn(name, parent, icon);
        widget = btn;
    }
    else if (name.starts_with("lbl-")) {
        auto lbl = new Label(name, parent);
        widget = lbl;
        widget->setRect("rect", j, fov_scale);
        if (jo.contains("font_height"))
            lbl->mFontHeight = value_from_json(j["font_height"], fov_scale);
    }
    else if (name.starts_with("lst-")) {
        List* lst;
        if (name.starts_with("lst-pp-"))
            lst = new ListPPC(name, parent);
        else
            lst = new List(name, parent);
        widget = lst;
        widget->setRect("rect", j, fov_scale);
        lst->row_height = j["row_height"].as_real_or(0);
        lst->row_gap = j["row_gap"].as_real_or(0);
        lst->header = j["header"].as_real_or(0);
        if (j["row_test"].is_array()) {
            lst->row_test_bgn = j["row_test"][0].as_int();
            lst->row_test_end = j["row_test"][1].as_int();
        }
        lst->filters = filters_from_json(j["filter"]);
        if (jo.contains("tabs")) {
            auto& jtabs = jo.at("tabs").as_array();
            for (auto& jt : jtabs) {
                List::Tab tab;
                if (jt.at("name"))
                    tab.name = jt.at("name").as_string();
                tab.tab_left = jt.at("left").as_int();
                tab.tab_right = jt.at("right").as_int();
                if (!jt["font_height"].empty())
                    tab.ocr_height = value_from_json(jt["font_height"], fov_scale);
                if (jt["ocr_psm"].is_int())
                    tab.ocr_psm = jt["ocr_psm"].as_int();
                lst->tabs.push_back(tab);
            }
        }
    }
    else {
        LOG(ERROR) << "Unknown widget type: " << name;
        return nullptr;
    }
    if (jo.contains("have") && jo.at("have").is_array()) {
        for (auto &h: jo.at("have").as_array()) {
            widget->addSubItem(widget_from_json(h, widget, fov_scale));
        }
    }
    if (jo.contains("detect")) {
        Detector* oracle = detector_from_json(jo.at("detect"), *widget, fov_scale);
        widget->oracle.reset(oracle);
    }
    return widget;
}

} // namespace widget