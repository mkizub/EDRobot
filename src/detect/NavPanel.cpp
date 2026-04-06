//
// Created by mkizub on 17.08.2025.
//

#include "../pch.h"

#include "Detector.h"
#include "Lines.h"
#include "NavPanel.h"
#include "../widget/EDWidget.h"

namespace detect {

void NavPanelDetector::standaloneTest(std::string image_filename, std::string screen_name) {
    cv::Mat fileImage = cv::imread(image_filename, cv::IMREAD_UNCHANGED); // assume GRAY/BGR/BGRA
    XMat debugImage = toXMat(fileImage);
    ClassifyEnv debugEnv;
    debugEnv.init(debugImage, 1);

    const widget::Screen *screen = (const widget::Screen *) Master::getInstance().getCfgItem(screen_name);
    auto det = dynamic_cast<NavPanelDetector*>(screen->oracle.get());
    double match = det->match(debugEnv);
    LOG_INFO("NavPanelDetector::standaloneTest: {:.1f}", match);
}

NavPanelDetector::NavPanelDetector(
        std::string panel_name,
        std::vector<std::unique_ptr<LineDetector>>&& lines,
        std::vector<std::unique_ptr<AnchorDetector>>&& anchors,
        std::vector<Tab>&& tabs)
    : mPanelName(std::move(panel_name))
    , mLines(std::move(lines))
    , mAnchors(std::move(anchors))
    , mTabs(std::move(tabs))
{
}

LineDetector* NavPanelDetector::getLineDetector(const char* name) {
    for (auto& ldet : mLines) {
        if (ldet->name == name)
            return ldet.get();
    }
    throw std::runtime_error("LineDetector not found");
}
AnchorDetector* NavPanelDetector::getAnchorDetector(const char* name) {
    for (auto& adet : mAnchors) {
        if (adet->name == name)
            return adet.get();
    }
    throw std::runtime_error("AnchorDetector not found");
}
const NavPanelDetector::Tab* NavPanelDetector::getTab(const char* name) {
    for (auto &tab: mTabs) {
        if (tab.name == name)
            return &tab;
    }
    throw std::runtime_error("Tab not found");
}
ConstTransform* NavPanelDetector::getTransform() {
    const widget::Screen *screen = (const widget::Screen *) Master::getInstance().getCfgItem(mPanelName);
    if (!screen)
        return nullptr;
    ConstTransform *transform = dynamic_cast<ConstTransform *>(screen->transform.get());
    return transform;
}

//#define DEBUG_DETECTOR 1
#if defined(DEBUG_DETECTOR) && defined(NDEBUG)
# error "DEBUG_NAV_PANEL_DETECTOR in release build"
#endif

static cv::Point2f rotateAround(cv::Point2f point, cv::Point2f anchor, float angle, float scale) {
    cv::Point2f delta = point - anchor;
    double cos_a = std::cos(angle*M_PI/180);
    double sin_a = std::sin(angle*M_PI/180);
    cv::Point2f rot;
    rot.x = delta.x * cos_a - delta.y * sin_a;
    rot.y = delta.x * sin_a + delta.y * cos_a;
    rot *= scale;
    return rot + anchor;
}

static cv::Line2f rotateAround(cv::Line2f line, cv::Point2f anchor, float angle, float scale) {
    cv::Point2f p0 = rotateAround(line.p0(), anchor, angle, scale);
    cv::Point2f p1 = rotateAround(line.p1(), anchor, angle, scale);
    return {p0, p1};
}

static bool checkAnchors(ClassifyEnv &env, double thr, AnchorDetector *lan, AnchorDetector *ran, cv::Point& offset) {
    lan->extendLT = offset;
    lan->extendRB = offset;
    lan->withRefRect = lan->refEvalRect->calcReferenceRect(env);
    lan->withRefRect += offset;
    double match = lan->match(env);
    if (match < thr)
        return false;
    if (!ran)
        return true;
    ran->extendLT = offset;
    ran->extendRB = offset;
    ran->withRefRect = ran->refEvalRect->calcReferenceRect(env);
    ran->withRefRect += offset;
    match = ran->match(env);
    if (match < thr)
        return false;
    return true;
}

double NavPanelDetector::match(ClassifyEnv &env) {
    ConstTransform *transform = getTransform();
    if (!transform)
        return 0;
    transform->valid = false;
    transform->useCaptured = true;

#ifdef DEBUG_DETECTOR
    cv::Mat debugImage = toMat(env.getColorImage()).clone();
#else
    cv::Mat debugImage;
#endif

    float roughAngle;
    cv::Point2f roughCenter;
    {
        LineDetector *detect_angle = getLineDetector("detect-angle");
        double lineMatch = detect_angle->match(env);
#ifdef DEBUG_DETECTOR
        cv::rectangle(debugImage, detect_angle->lineMatchRect, {128,128,128});
#endif
        if (lineMatch > 0) {
            auto& dl = detect_angle->detectedLines[0];
            roughAngle = detect_angle->lastAvrgAngle;
            cv::Line2f line = dl.line;
            line += cv::Point2f(detect_angle->lineMatchRect.tl());
            roughCenter = (line.p0() + line.p1()) * 0.5f;
#ifdef DEBUG_DETECTOR
            for (auto& dl : detect_angle->detectedLines) {
                cv::Line line = dl.line;
                line += detect_angle->lineMatchRect.tl();
                cv::line(debugImage, line.p0(), line.p1(), {128,128,128}, 1, cv::LINE_AA);
            }
#endif
            LOG_DEBUG("Pre-detected panel angle: {:.1f}deg", roughAngle);
        } else {
            LOG_WARNING("Panel lines not found");
            return 0;
        }
    }

    LineDetector *top_line = getLineDetector("top-line");

    lastTopLine = {};
    lastBottomLine = {};
    lastSelectedTab = {};
    topRefLine = top_line->referenceLine->calcReferenceLine(env);
    auto topCaptLine = env.cvtReferenceToCaptured(topRefLine);
    {
        // rotate for anchor detection
        const int roughExtW = 300;
        const int roughExtH = 120;
        float refRoughW = topRefLine.length() + roughExtW;
        float captRoughW = topCaptLine.length() + roughExtW*env.getScale();
        float captRoughH = roughExtH*env.getScale();
        lastRotRect = {roughCenter, {captRoughW, captRoughH}, roughAngle};
        cv::Point2f pointsSrc[4];
        lastRotRect.points(pointsSrc); // bottomLeft, topLeft, topRight, bottomRight
        cv::Point2f pointsDst[4] = { {0,roughExtH}, {0,0}, {refRoughW,0}, {refRoughW,roughExtH} };
        roughAffineMatrix = cv::getAffineTransform(pointsSrc, pointsDst);
        cv::invertAffineTransform(roughAffineMatrix, roughAffineInverted);
#ifdef DEBUG_DETECTOR
        for (int j = 0; j < 4; j++)
            cv::line(debugImage, pointsSrc[j], pointsSrc[(j+1) % 4], {160,160,160}, 1, cv::LINE_AA);
#endif
        XMat roughImage;
        cv::warpAffine(env.getColorImage(), roughImage, roughAffineMatrix, {int(refRoughW), roughExtH});
        cv::Point offset{roughExtW/2, roughExtH/2};
        ClassifyEnv roughEnv;
        roughEnv.init(roughImage, roughAffineInverted);
        AnchorDetector *lan {};
        AnchorDetector *ran {};
        if (!st::autopilot.expect_screen.empty() && mPanelName == "scr-left-panel") {
            if (st::autopilot.expect_screen == "dlg-filters") {
                lan = getAnchorDetector("flt-left");
                ran = nullptr;
                if (checkAnchors(roughEnv, 0.2, lan, ran, offset))
                    if (match_dialog(roughEnv, roughAngle, lan, debugImage))
                        return fillResult(env);
                return 0;
            }
            if (st::autopilot.expect_screen == "dlg-nav-select") {
                lan = getAnchorDetector("nav-left");
                ran = nullptr;
                if (checkAnchors(roughEnv, 0.2, lan, ran, offset))
                    if (match_dialog(roughEnv, roughAngle, lan, debugImage))
                        return fillResult(env);
                return 0;
            }
        } else {
            bool anchorMatch = false;
            if (!anchorMatch) {
                lan = getAnchorDetector("top-left");
                ran = getAnchorDetector("top-right");
                anchorMatch = checkAnchors(roughEnv, 0.4, lan, ran, offset);
            }
            if (!anchorMatch && mPanelName == "scr-right-panel") {
                lan = getAnchorDetector("trn-left");
                ran = getAnchorDetector("trn-right");
                anchorMatch = checkAnchors(roughEnv, 0.4, lan, ran, offset);
                if (anchorMatch)
                    lastSelectedTab = getTab("transfer");
            }
            if (!anchorMatch && mPanelName == "scr-left-panel") {
                lan = getAnchorDetector("flt-left");
                ran = nullptr;
                if (checkAnchors(roughEnv, 0.6, lan, ran, offset))
                    if (match_dialog(roughEnv, roughAngle, lan, debugImage))
                        return fillResult(env);
            }
            if (!anchorMatch && mPanelName == "scr-left-panel") {
                lan = getAnchorDetector("top-left");
                ran = nullptr;
                anchorMatch = checkAnchors(roughEnv, 0.4, lan, ran, offset);
            }
            if (!anchorMatch) {
                LOG_WARNING("Anchors for top line not found");
                return 0;
            }
        }
#ifdef DEBUG_DETECTOR
        cv::Mat debugRough = toMat(roughImage).clone();
        if (lan)
            cv::rectangle(debugRough, lan->captureRect, {160, 160, 160});
        if (ran)
            cv::rectangle(debugRough, ran->captureRect, {160, 160, 160});
#endif
        cv::Line2f roughTopLine;
        if (lan && ran) {
            cv::Line refLine {lan->refEvalRect->calcReferenceRect(env).tl(), ran->refEvalRect->calcReferenceRect(env).tl()};
            cv::Line detLine {lan->captureRect.tl(), ran->captureRect.tl()};
            double len1 = detLine.length();
            double len2 = refLine.length();
            deltaScale = len1 / len2;
            roughTopLine = {lan->captureRect.tl() + lan->anchor_of * deltaScale,
                            ran->captureRect.tl() + ran->anchor_of * deltaScale};
            deltaAngle = roughAngle + roughTopLine.angle() - topCaptLine.angle();
        }
        else if (lan) {
            cv::Point2f p0 = lan->captureRect.tl() + lan->anchor_of;
            cv::Point2f p1 = topCaptLine.p1() - topCaptLine.p0();
            p1 = p0 + rotateAround(p1, cv::Point2f(), -roughAngle, 1);
            roughTopLine = {p0, p1};
            deltaScale = 1;
            deltaAngle = roughAngle + roughTopLine.angle() - topCaptLine.angle();
        }
        else {
            LOG_WARNING("Anchors for top line not found");
            return 0;
        }
#ifdef DEBUG_DETECTOR
        if (lan) {
            cv::rectangle(debugRough, lan->captureRect, {160, 160, 160});
            cv::drawMarker(debugRough, roughTopLine.p0(), {255, 255, 255}, 10);
        }
        if (ran) {
            cv::rectangle(debugRough, ran->captureRect, {160, 160, 160});
            cv::drawMarker(debugRough, roughTopLine.p1(), {255, 255, 255}, 10);
        }
        cv::line(debugRough, roughTopLine.p0(), roughTopLine.p1(), {96,96,96}, 1, cv::LINE_AA);
#endif
        if (!lastSelectedTab) {
            uchar selectedTabValue = 0;
            double sin_a = std::sin(roughTopLine.angle() * M_PI / 180);
            cv::Rect tabsRect {roughTopLine.p0(), cv::Point(roughTopLine.x1, roughTopLine.y1+40)};
            tabsRect &= cv::Rect(0,0,roughImage.cols,roughImage.rows);
            cv::Mat tabImage;
            cv::cvtColor(roughImage(tabsRect), tabImage, cv::COLOR_BGR2GRAY);
            for (auto &tab: mTabs) {
                cv::Rect r = tab.rect;
                if (r.empty())
                    continue;
                r.x *= deltaScale;
                r.y *= deltaScale;
                r.y += r.x * sin_a;
                //r.x += 0;
                r &= cv::Rect(0, 0, tabImage.cols, tabImage.rows);
                detect::Histogram hist(Histogram::Mode::Gray);
                if (!r.empty() && hist.calc(tabImage(r))) {
                    uchar bg = hist.mLastColor[0];
                    if (bg >= 140 && bg > selectedTabValue) {
                        lastSelectedTab = &tab;
                        selectedTabValue = bg;
                    }
                }
#ifdef DEBUG_DETECTOR
                cv::rectangle(debugRough, r+cv::Point(roughTopLine.p0()), {64, 64, 64},
                              lastSelectedTab == &tab ? 3 : 1);
#endif
            }
            if (!lastSelectedTab)
                LOG_WARNING("Nav panel tab not recognized");
        }
        cv::Matx23d backMatrix;
        cv::invertAffineTransform(roughAffineMatrix, backMatrix);
        std::array<cv::Point2f,2> arr {roughTopLine.p0(), roughTopLine.p1()};
        cv::transform(arr, arr, backMatrix);
        lastTopLine = env.cvtCapturedToReference(cv::Line2f{arr[0], arr[1]});
        topLeftOffset = env.scaleToReference(cv::Point2f(topCaptLine.p0()) - arr[0]);
#ifdef DEBUG_DETECTOR
        auto captTopLine = env.cvtReferenceToCaptured(lastTopLine);
        cv::line(debugImage, captTopLine.p0(), captTopLine.p1(), {200,200,200}, 1, cv::LINE_AA);
        cv::drawMarker(debugImage, captTopLine.p0(), {255,255,255}, 10);
        cv::drawMarker(debugImage, captTopLine.p1(), {255,255,255}, 10);
#endif
    }

    approximate_bottom_line(env);
    auto captBottomLine = env.cvtReferenceToCaptured(lastBottomLine);
#ifdef DEBUG_DETECTOR
    cv::line(debugImage, captBottomLine.p0(), captBottomLine.p1(), {64,64,64}, 1, cv::LINE_AA);
#endif
    LineDetector *btm_line = getLineDetector("btm-line");
    btm_line->detectEdgesMode = -1;
    btm_line->withRefLine = lastBottomLine;
    double btmMatch = btm_line->match(env);
    if (btmMatch < 0.4) {
        btm_line = getLineDetector("btm-line-weak");
        btm_line->detectEdgesMode = -1;
        btm_line->withRefLine = lastBottomLine;
        btmMatch = btm_line->match(env);
    }
    if (btmMatch > 0) {
        captBottomLine = btm_line->detectedLine;
        lastBottomLine = env.cvtCapturedToReference(btm_line->detectedLine);
        transform->transformSrc[2] = captBottomLine.p1();
        if (mPanelName == "scr-left-panel")
            transform->transformSrc[2] += cv::Point2f(10*env.getScale(),0);
        transform->transformSrc[3] = captBottomLine.p0();
#ifdef DEBUG_DETECTOR
        for (int j = 0; j < 4; j++)
            cv::line(debugImage, transform->transformSrc[j], transform->transformSrc[(j+1) % 4], {160,160,160}, 1, cv::LINE_AA);
#endif
    } else {
#ifdef DEBUG_DETECTOR
        captBottomLine = env.cvtReferenceToCaptured(lastBottomLine);
        cv::line(debugImage, lastTopLine.p0(), captBottomLine.p0(), {64,64,64}, 1, cv::LINE_AA);
        cv::line(debugImage, lastTopLine.p1(), captBottomLine.p1(), {64,64,64}, 1, cv::LINE_AA);
#endif
    }

    return fillResult(env);
}

double NavPanelDetector::fillResult(ClassifyEnv& env) {
    std::string name = "nav_panel:";
    if (lastSelectedTab)
        name += lastSelectedTab->name;
    env.classified.emplace_back(ClsDetType::Detected, name, cv::Rect());
    auto& tdet = env.classified.back().u.tdet;
    tdet.referenceRect = {};
    tdet.scale = deltaScale;
    tdet.angle = deltaAngle;
    tdet.match = 1;
    tdet.matchRect = {};
    return 1;
}

bool NavPanelDetector::match_flt_line(const char* lineName, cv::Line& detectedLine, ClassifyEnv& env, float roughAngle, AnchorDetector *lan, cv::Mat& debugRough) {
    LineDetector *flt_line = getLineDetector(lineName);
    cv::Line fltRefLine = flt_line->referenceLine->calcReferenceLine(env);
    cv::Line fltAltLine = {cv::Point(), fltRefLine.p1()-fltRefLine.p0()};
    fltAltLine = rotateAround(fltAltLine, cv::Point(), -roughAngle, 1);
    fltAltLine += env.scaleToReference(lan->captureRect.tl()) - lan->refEvalRect->calcReferenceRect(env).tl();
    if (strcmp(lineName, "flt-line2") == 0)
        fltAltLine += cv::Point(0,38*env.getScale());

#ifdef DEBUG_DETECTOR
    auto captAltLine = env.scaleToCaptured(fltAltLine);
    cv::line(debugRough, captAltLine.p0(), captAltLine.p1(), {160, 160, 160});
#endif
    flt_line->detectEdgesMode = 1;
    flt_line->withRefLine = fltAltLine;
    double lineMatch = flt_line->match(env);
    if (lineMatch >= 0.5 && !flt_line->detectedLine.empty()) {
        detectedLine = flt_line->detectedLine;
        deltaScale = detectedLine.length() / (fltRefLine.length()*env.getScale());
    } else {
        detectedLine = env.scaleToCaptured(fltAltLine);
        deltaScale = 1;
    }
    deltaAngle = roughAngle + detectedLine.angle() - topRefLine.angle();
#ifdef DEBUG_DETECTOR
    cv::line(debugRough, detectedLine.p0(), detectedLine.p1(), {160, 160, 160});
#endif

    cv::Point tl = lan->captureRect.tl() + lan->anchor_of*deltaScale*env.getScale();
    cv::Point tr_anchor = topRefLine.p1() - fltRefLine.p1();
    tr_anchor = rotateAround(tr_anchor, {}, -roughAngle, deltaScale);
    cv::Point tr = detectedLine.p1() + env.scaleToCaptured(tr_anchor);

    cv::Matx23d backMatrix;
    cv::invertAffineTransform(roughAffineMatrix, backMatrix);
    std::array<cv::Point2f,2> points {tl, tr};
    cv::transform(points, points, backMatrix);
    lastTopLine = env.cvtCapturedToReference(cv::Line2f{points[0], points[1]});
    topLeftOffset = cv::Point2f(topRefLine.p0() - lastTopLine.p0());

    return lineMatch >= 0.5;
}

bool NavPanelDetector::match_dialog(ClassifyEnv& env, float roughAngle, AnchorDetector *lan, cv::Mat& debugImage) {
#ifdef DEBUG_DETECTOR
    cv::Mat debugRough = toMat(env.getColorImage()).clone();
    cv::rectangle(debugRough, lan->captureRect, {160,160,160});
#else
    cv::Mat debugRough;
#endif

    if (lan->name == "flt-left")
        lastSelectedTab = getTab("filters");
    else if (lan->name == "nav-left")
        lastSelectedTab = getTab("select");

    cv::Line detectedLine;
    if (!match_flt_line("flt-line", detectedLine, env, roughAngle, lan, debugRough)) {
        if (mPanelName == "scr-left-panel") {
            if (!match_flt_line("flt-line2", detectedLine, env, roughAngle, lan, debugRough))
                return false;
        }
    }

#ifdef DEBUG_DETECTOR
    auto captTopLine = env.cvtReferenceToCaptured(lastTopLine);
    cv::line(debugImage, captTopLine.p0(), captTopLine.p1(), {200,200,200}, 1, cv::LINE_AA);
    cv::drawMarker(debugImage, captTopLine.p0(), {255,255,255}, 10);
    cv::drawMarker(debugImage, captTopLine.p1(), {255,255,255}, 10);
#endif
    approximate_bottom_line(env);
#ifdef DEBUG_DETECTOR
    captTopLine = env.cvtReferenceToCaptured(lastTopLine);
    auto captBotLine = env.cvtReferenceToCaptured(lastBottomLine);
    cv::line(debugImage, captTopLine.p0(), captBotLine.p0(), {64,64,64}, 1, cv::LINE_AA);
    cv::line(debugImage, captTopLine.p1(), captBotLine.p1(), {64,64,64}, 1, cv::LINE_AA);
    cv::line(debugImage, captBotLine.p0(), captBotLine.p1(), {64,64,64}, 1, cv::LINE_AA);
#endif

    return true;
}

void NavPanelDetector::approximate_bottom_line(ClassifyEnv& env) {
    ConstTransform *transform = getTransform();
    transform->valid = true;
    transform->useCaptured = true;
    LineDetector *btm_line = getLineDetector("btm-line");
    auto btmRefLine = btm_line->referenceLine->calcReferenceLine(env);
    // rotate/scale around anchor top-left and top-right transform points and expect bottom line
    cv::Point2f btmLeft = cv::Point2f(btmRefLine.p0()) - topLeftOffset;
    cv::Point2f btmRight = cv::Point2f(btmRefLine.p1()) - topLeftOffset;
    btmLeft = rotateAround(btmLeft, lastTopLine.p0(), deltaAngle, deltaScale);
    btmRight = rotateAround(btmRight, lastTopLine.p0(), deltaAngle, deltaScale);
    lastBottomLine = {btmLeft, btmRight};
    auto captTopLine = env.cvtReferenceToCaptured(lastTopLine);
    auto captBotLine = env.cvtReferenceToCaptured(lastBottomLine);
    transform->transformSrc[0] = captTopLine.p0();
    transform->transformSrc[1] = captTopLine.p1();
    transform->transformSrc[2] = captBotLine.p1();
    if (mPanelName == "scr-left-panel")
        transform->transformSrc[2] += cv::Point2f(10*env.getScale(),0);
    transform->transformSrc[3] = captBotLine.p0();
}

} // namespace detect
