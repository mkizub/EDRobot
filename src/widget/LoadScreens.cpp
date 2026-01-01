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

static cv::Vec3b color_from_json(const json5pp::value& v) {
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

static double value_from_json(const json5pp::value& v, FovScale* fov_scale) {
    if (v.is_number())
        return v.as_number();
    cv::Size sz;
    sz.width = v[0].as_integer();
    sz.height = sz.width;
    if (fov_scale && v[1].is_number())
        sz = fov_scale->apply(sz, v[1].as_number());
    return sz.width;
}

static cv::Point point_from_json(const json5pp::value& v) {
    cv::Point p;
    p.x = v[0].as_integer();
    p.y = v[1].as_integer();
    return p;
}

static cv::Size size_from_json(const json5pp::value& v, FovScale* fov_scale) {
    cv::Size sz;
    sz.width = v[0].as_integer();
    sz.height = v[1].as_integer();
    if (fov_scale && v[2].is_number())
        sz = fov_scale->apply(sz, v[2].as_number());
    return sz;
}

static cv::Rect rect_from_json(const json5pp::value& v) {
    cv::Rect rect;
    rect.x = v.at(0,0).as_integer();
    rect.y = v.at(1,0).as_integer();
    rect.width = v.at(2,0).as_integer();
    rect.height = v.at(3,0).as_integer();
    return rect;
}

static cv::Rect fov_rect_from_json(const json5pp::value& v, double& fov) {
    cv::Rect rect;
    rect.x = v.at(0,0).as_integer();
    rect.y = v.at(1,0).as_integer();
    rect.width = v.at(2,0).as_integer();
    rect.height = v.at(3,0).as_integer();
    fov = v.at(4,0.0).as_number();
    return rect;
}

static cv::Rect rect_from_json(const json5pp::value& v, FovScale* fov_scale) {
    cv::Rect rect;
    rect.x = v.at(0,0).as_integer();
    rect.y = v.at(1,0).as_integer();
    rect.width = v.at(2,0).as_integer();
    rect.height = v.at(3,0).as_integer();
    if (fov_scale && v[4].is_number())
        rect = fov_scale->apply(rect, v[4].as_number());
    return rect;
}

static cv::Rect mark_from_json(const json5pp::value& v, FovScale* fov_scale) {
    cv::Rect rect;
    rect.x = v.at(0,0).as_integer();
    rect.y = v.at(1,0).as_integer();
    rect.width = v.at(2,0).as_integer();
    rect.height = v.at(3,0).as_integer();
    if (fov_scale && v[4].is_number()) {
        cv::Size offs = fov_scale->apply(cv::Size(rect.tl()), v[4].as_number());
        cv::Size size = fov_scale->apply(rect.size(), v[4].as_number());
        rect = {cv::Point(offs), size};
    }
    return rect;
}

template<class Tp>
static void minmax_from_json(const json5pp::value& v, Tp& vmin, Tp& vmax) {
    Tp tmin = vmin;
    Tp tmax = vmax;
    if (v.is_number())
        tmin = tmax = v.as_number();
    else if (v.is_array()) {
        auto& jt = v.as_array();
        if (!jt.empty())
            tmin = jt[0].as_number();
        if (jt.size() > 1)
            tmax = jt[1].as_number();
        else
            tmax = tmin;
    }
    if (tmax < tmin)
        std::swap(tmin, tmax);
    vmin = tmin;
    vmax = tmax;
}

static void ext_from_json(const json5pp::value& v, cv::Point& extendLT, cv::Point& extendRB) {
    if (!v)
        return;
    int extL = 0;
    int extT = 0;
    int extR = 0;
    int extB = 0;
    if (v.is_number())
        extL = extT = extR = extB = v.as_integer();
    else if (v.is_array()) {
        auto& jext = v.as_array();
        if (!jext.empty())
            extL = extT = extR = extB = jext[0].as_integer();
        if (jext.size() > 1)
            extT = extB = jext[1].as_integer();
        if (jext.size() > 2)
            extR = jext[2].as_integer();
        if (jext.size() > 3)
            extB = jext[3].as_integer();
    }
    extendLT = {extL, extT};
    extendRB = {extR, extB};
}

static void from_json(const json5pp::value& jf, std::unique_ptr<detect::ImageFilter>& f) {
    if (!jf.is_object())
        return;
    auto& jo = jf.as_object();
    if (jo.contains("threshold") && jf["threshold"].is_number()) {
        double thr = jf["threshold"].as_number();
        double max = thr < 1 ? 0.5 : 255.0;
        if (jf["max"].is_number())
            max = jf["max"].as_number();
        f.reset(new ThresholdFilter(thr, max));
        return;
    }
    if (jo.contains("channel") && jf["channel"].is_string()) {
        std::string chn = toLower(jf["channel"].as_string());
        ChannelFilter::Channel channel = enum_cast<ChannelFilter::Channel>(chn).value();
        f.reset(new ChannelFilter(channel));
        return;
    }
    if (jo.contains("gauss") && jf["gauss"].is_object()) {
        int kernX = 3;
        int kernY = 3;
        if (jf["gauss"]["kern"].is_array()) {
            kernX = jf["gauss"]["kern"][0].as_integer();
            kernY = jf["gauss"]["kern"][1].as_integer();
        } else {
            kernX = jf["gauss"]["kern"].as_integer();
            kernY = kernX;
        }
        kernX = (kernX & ~1) + 1;
        kernY = (kernY & ~1) + 1;
        f.reset(new GaussFilter(kernX, kernY));
        return;
    }
    if (jo.contains("laplacian") && jf["laplacian"].is_object()) {
        int kern = 3;
        double scale = 1;
        double delta = 0;
        if (jf["laplacian"]["kern"].is_integer())
            kern = jf["laplacian"]["kern"].as_integer();
        if (jf["laplacian"]["scale"].is_number())
            scale = jf["laplacian"]["scale"].as_number();
        if (jf["laplacian"]["delta"].is_number())
            delta = jf["laplacian"]["delta"].as_number();
        f.reset(new LaplacianFilter(kern, scale, delta));
        return;
    }
    if (jo.contains("scharr") && jf["scharr"].is_object()) {
        bool vert = false;
        double scale = 1;
        if (jf["scharr"]["vert"].is_boolean())
            vert = jf["scharr"]["vert"].as_boolean();
        else if (jf["scharr"]["horz"].is_boolean())
            vert = !jf["scharr"]["horz"].as_boolean();
        if (jf["scharr"]["scale"].is_number())
            scale = jf["scharr"]["scale"].as_number();
        f.reset(new ScharrFilter(vert, scale));
        return;
    }
    if (jo.contains("lines") && jf["lines"].is_object()) {
        bool vert = false;
        double scale = 1;
        double threshold = 45;
        int dilatePos = 2;
        int dilateNeg = 2;
        int erode = 0;
        if (jf["lines"]["vert"].is_boolean())
            vert = jf["lines"]["vert"].as_boolean();
        else if (jf["lines"]["horz"].is_boolean())
            vert = !jf["lines"]["horz"].as_boolean();
        if (jf["lines"]["scale"].is_number())
            scale = jf["lines"]["scale"].as_number();
        if (jf["lines"]["thr"].is_number())
            threshold = jf["lines"]["thr"].as_number();
        if (jf["lines"]["dilate"].is_integer())
            dilatePos = dilateNeg = jf["lines"]["dilate"].as_integer();
        if (jf["lines"]["dilate_pos"].is_integer())
            dilatePos = jf["lines"]["dilate_pos"].as_integer();
        if (jf["lines"]["dilate_neg"].is_integer())
            dilateNeg = jf["lines"]["dilate_neg"].as_integer();
        if (jf["lines"]["erode"].is_integer())
            erode = jf["lines"]["erode"].as_integer();
        f.reset(new LinesFilter(vert, scale, threshold, dilatePos, dilateNeg, erode));
        return;
    }
    if (jo.contains("edge_box") && jf["edge_box"].is_object()) {
        int kern = 5;
        double scale = 2.0;
        double thr = 0;
        if (jf["edge_box"]["kern"].is_integer())
            kern = jf["edge_box"]["kern"].as_integer();
        if (jf["edge_box"]["scale"].is_number())
            scale = jf["edge_box"]["scale"].as_number();
        if (jf["edge_box"]["thr"].is_number())
            thr = jf["edge_box"]["thr"].as_number();
        f.reset(new EdgeByBoxFilter(kern, scale, thr));
        return;
    }
    if (jo.contains("gain") && jf["gain"].is_number()) {
        double gain = jf["gain"].as_number();
        double bias = 0;
        if (jf["bias"].is_number())
            bias = jf["bias"].as_number();
        f.reset(new GainBiasFilter(gain, bias));
        return;
    }
    if (jo.contains("dilate")) {
        int kernX = 3;
        int kernY = 3;
        int iter = 1;
        if (jf["dilate"].is_integer())
            kernX = kernY = jf["dilate"].as_integer();
        else if (jf["dilate"].is_array()) {
            kernX = jf["dilate"][0].as_integer();
            kernY = jf["dilate"][1].as_integer();
        }
        if (jf["iter"].is_integer())
            iter = jf["iter"].as_integer();
        f.reset(new DilateFilter(kernX, kernY, iter));
        return;
    }
    if (jo.contains("erode")) {
        int kernX = 3;
        int kernY = 3;
        int iter = 1;
        if (jf["erode"].is_integer())
            kernX = kernY = jf["erode"].as_integer();
        else if (jf["erode"].is_array()) {
            kernX = jf["erode"][0].as_integer();
            kernY = jf["erode"][1].as_integer();
        }
        if (jf["iter"].is_integer())
            iter = jf["iter"].as_integer();
        f.reset(new ErodeFilter(kernX, kernY, iter));
        return;
    }
    bool has_hsv_crop = jo.contains("hsv_crop");
    bool has_hsv_gray = jo.contains("hsv_gray");
    bool has_hsv_mask = jo.contains("hsv_mask");
    bool has_hsv_cval = jo.contains("hsv_cval");
    if (has_hsv_crop || has_hsv_gray || has_hsv_mask) {
        json5pp::value jhsv;
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
        std::vector<json5pp::value> jarr;
        if (jhsv.is_array())
            jarr = jhsv.as_array();
        else if (jhsv.is_object())
            jarr.push_back(jhsv);

        for (auto& jv_ : jarr) {
            cv::Vec3b min = {0,0,0};
            cv::Vec3b max = {255,255,255};
            auto& jv = jv_.as_object();
            if (jv.contains("h")) {
                min[0] = jv["h"][0].as_integer();
                max[0] = jv["h"][1].as_integer();
            }
            if (jv.contains("s")) {
                min[1] = jv["s"][0].as_integer();
                max[1] = jv["s"][1].as_integer();
            }
            if (jv.contains("v")) {
                min[2] = jv["v"][0].as_integer();
                max[2] = jv["v"][1].as_integer();
            }
            filter->rangesU.emplace_back(min,max);
        }
        if (filter->rangesU.empty())
            delete filter;
        else
            f.reset(filter);
        return;
    }
}

static void image_template_from_json(const json5pp::value& j, ImageTemplate* templ) {
    if (j.at("name").is_string()) {
        templ->name = j.at("name").as_string();
    }

    if (j.at("scale").is_number())
        templ->testScales.push_back(j["scale"].as_number());
    else if (j.at("scale").is_array()) {
        for (auto& scl : j.at("scale").as_array())
            templ->testScales.push_back(scl.as_number());
    }

    if (j.at("angle").is_integer())
        templ->testAngles.push_back(j["angle"].as_integer());
    else if (j.at("angle").is_array()) {
        for (auto& angle : j.at("angle").as_array())
            templ->testAngles.push_back(angle.as_integer());
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

    if (j.at("filter")) {
        if (j.at("filter").is_object()) {
            std::unique_ptr<ImageFilter> f;
            from_json(j.at("filter"), f);
            if (f)
                templ->filters.push_back(std::move(f));
        }
        else if (j.at("filter").is_array()) {
            for (auto& jf : j.at("filter").as_array()) {
                std::unique_ptr<ImageFilter> f;
                from_json(jf, f);
                if (f)
                    templ->filters.push_back(std::move(f));
            }
        }
    }
}

static Detector* detector_from_json(const json5pp::value& j, Widget& widget, FovScale* fov_scale) {
    if (j.is_null())
        return nullptr;
    if (j.is_boolean()) {
        if (j.as_boolean())
            return new ConstDetector(1);
        return new ConstDetector(0);
    }
    if (j.is_number()) {
        double value = j.as_number();
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
            double value = j["const"].as_number();
            value = std::clamp(value, 0.0, 1.0);
            auto* cdet = new ConstDetector(value);
            if (j["weight"].is_number())
                cdet->classifierWeight = j["weight"].as_number();
            return cdet;
        }
        if (j["ref"].is_string()) {
            const auto referred = j["ref"].as_string();
            auto* rdet = new ReferDetector(referred);
            if (j["weight"].is_number())
                rdet->classifierWeight = j["weight"].as_number();
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
            if (j["weight"].is_number())
                bdet->classifierWeight = j["weight"].as_number();
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
            if (j["weight"].is_number())
                sdet->classifierWeight = j["weight"].as_number();
            return sdet;
        }
        if (j.as_object().contains("anchor")) {
            std::string filename = "templates/"+j.at("img").as_string();
            spEvalRect rect = makeEvalRect(widget, "rect", j["rect"], fov_scale, true);
            cv::Point anchor = size_from_json(j["anchor"], fov_scale);
            auto* templ = new AnchorDetector(filename, rect, anchor);
            image_template_from_json(j, templ);
            if (j["weight"].is_number())
                templ->classifierWeight = j["weight"].as_number();
            return templ;
        }
        if (j.as_object().contains("img")) {
            std::string filename = "templates/"+j.at("img").as_string();
            spEvalRect rect = makeEvalRect(widget, "rect", j["rect"], fov_scale, false);
            auto* templ = new ImageTemplate(filename, rect);
            image_template_from_json(j, templ);
            if (j["weight"].is_number())
                templ->classifierWeight = j["weight"].as_number();
            return templ;
        }
        if (j.as_object().contains("line")) {
            spEvalLine line = makeEvalLine(widget, "line", j["line"], fov_scale);
            auto* ldet = new detect::LineDetector(line);

            if (j["name"].is_string())
                ldet->name = j["name"].as_string();
            if (j["weight"].is_number())
                ldet->classifierWeight = j["weight"].as_number();

            ext_from_json(j["ext"], ldet->extendLT, ldet->extendRB);
            if (j.at("delta"))
                minmax_from_json(j["delta"], ldet->extendAngleMin, ldet->extendAngleMax);
            if (j.at("votes")) {
                ldet->houghThreshold = j["votes"].as_integer();
                if (fov_scale && j["line"][4].is_number())
                    ldet->houghThreshold *= fov_scale->getScaleForFOV(j["line"][4].as_number());
            }
            if (j.at("prec"))
                ldet->angleStep = j["prec"].as_number();

            if (j.at("filter")) {
                if (j.at("filter").is_object()) {
                    std::unique_ptr<ImageFilter> f;
                    from_json(j.at("filter"), f);
                    if (f)
                        ldet->filters.push_back(std::move(f));
                }
                else if (j.at("filter").is_array()) {
                    for (auto& jf : j.at("filter").as_array()) {
                        std::unique_ptr<ImageFilter> f;
                        from_json(jf, f);
                        if (f)
                            ldet->filters.push_back(std::move(f));
                    }
                }
            }
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

            int gap = j["gap"].as_integer();

            auto* tiles = new TilesDetector(name, tilesRect, marksRect,
                                            rows_min, rows_max, cols_min, cols_max, gap);

            if (j["size"].is_array())
                tiles->mTileSize = size_from_json(j["size"], fov_scale);
            if (j["merge"].is_boolean())
                tiles->mTryMerge = j["merge"].as_boolean();
            if (j["weight"].is_number())
                tiles->classifierWeight = j["weight"].as_number();

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
                if (!j["ocr_height"].empty())
                    tiles->mOcrHeight = value_from_json(j["ocr_height"], fov_scale);
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
            std::string name = j["name"].asif_string();
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
            if (j["weight"].is_number())
                tdet->classifierWeight = j["weight"].as_number();
            if (j["line_height"].is_integer())
                tdet->mLineHeight = j["line_height"].as_integer();
            if (j["psm"].is_integer())
                tdet->mOcrPSM = j["psm"].as_integer();

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
            if (j["weight"].is_number())
                pdet->classifierWeight = j["weight"].as_number();
            return pdet;
        }
        return nullptr;
    }
    return nullptr;
}

static void from_json(const json5pp::value& j, std::map<std::string,std::vector<BaseDialog::Vars>>& varSetMap) {
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
                        vars.values[jv_it.first].push_back(jv.as_number());
                }
                else if (jv_it.second.is_number())
                    vars.values[jv_it.first].push_back(jv_it.second.as_number());
            }
        }
    }
}

Widget* widget_from_json(const json5pp::value& j, Widget* parent, FovScale* fov_scale) {
    if (j.is_null()) {
        return nullptr;
    }
    FovScale scr_fov_scale;
    Widget* widget = nullptr;
    auto& jo = j.as_object();
    auto name = jo.at("name").as_string();
    if (name.starts_with("scr-")) {
        auto scr = new Screen(name, parent, j["status"]);
        if (auto& jfscl=j["fov_size"]; jfscl.is_array() || jfscl.is_number()) {
            if (jfscl.is_number()) {
                fov_scale = &scr_fov_scale;
                fov_scale->scale60 = jfscl.as_number();
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
        if (auto& jvars = j["vars"]; jvars.is_object()) {
            from_json(jvars, scr->varSetMap);
        }
        if (auto& jt = j["transform"]; jt.is_object()) {
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
        if (jo.contains("ext"))
            ext_from_json(jo.at("ext"), btn->extendLT, btn->extendRB);
        if (j["icon"].is_string())
            btn->icon = "templates/"+j["icon"].as_string();
    }
    else if (name.starts_with("spn-")) {
        auto btn = new Spinner(name, parent);
        widget = btn;
        widget->setRect("rect", j, fov_scale);
        if (jo.contains("ext"))
            ext_from_json(jo.at("ext"), btn->extendLT, btn->extendRB);
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
        if (jo.contains("ocr_top") && jo.contains("ocr_bot")) {
            lbl->ocr_top = jo.at("ocr_top").as_integer();
            lbl->ocr_bot = jo.at("ocr_bot").as_integer();
        }
    }
    else if (name.starts_with("lst-")) {
        List* lst;
        if (name.starts_with("lst-pp-"))
            lst = new ListPPC(name, parent);
        else
            lst = new List(name, parent);
        widget = lst;
        widget->setRect("rect", j, fov_scale);
        lst->row_height = (float) j.at("row_height",0).as_number();
        lst->row_gap = (float) j.at("row_gap",0).as_number();
        lst->header = (float) j.at("header",0).as_number();
        if (j["row_test"].is_array()) {
            lst->row_test_bgn = j["row_test"][0].as_integer();
            lst->row_test_end = j["row_test"][1].as_integer();
        }
        if (jo.contains("tabs")) {
            auto& jtabs = jo.at("tabs").as_array();
            for (auto& jt : jtabs) {
                List::Tab tab;
                if (jt.at("name"))
                    tab.name = jt.at("name").as_string();
                tab.tab_left = jt.at("left").as_integer();
                tab.tab_right = jt.at("right").as_integer();
                if (jt["ocr_height"].is_integer())
                    tab.ocr_height = jt["ocr_height"].as_integer();
                if (jt["ocr_psm"].is_integer())
                    tab.ocr_psm = jt["ocr_psm"].as_integer();
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