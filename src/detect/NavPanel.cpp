//
// Created by mkizub on 17.08.2025.
//

#include "../pch.h"

#include "Detector.h"
#include "Lines.h"
#include "NavPanel.h"
#include "../EDWidget.h"

namespace detect {

std::string NavPanelDetector::forceDetect;


void NavPanelDetector::standaloneTest(std::string image_filename, std::string screen_name) {
    cv::Mat colorImage = cv::imread(image_filename, cv::IMREAD_UNCHANGED); // assume GRAY/BGR/BGRA
    ResolvedEnv rEnv;
    cv::Rect monitorRect(cv::Point(), rEnv.ReferenceScreenSize);
    rEnv.init(monitorRect, monitorRect);
    ClassifyEnv cEnv;
    cEnv.init(rEnv, &colorImage, nullptr);

    const widget::Screen *screen = (const widget::Screen *) Master::getInstance().getCfgItem(screen_name);
    auto det = dynamic_cast<NavPanelDetector*>(screen->oracle.get());
    double match = det->match(cEnv);
    LOG(INFO) << std::format("NavPanelDetector::standaloneTest: {:.1f}", match);
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

static bool checkAnchors(ClassifyEnv &env, AnchorDetector *lan, AnchorDetector *ran, cv::Point& offset, XMat& roughImage) {
    lan->extendLT = offset;
    lan->extendRB = offset;
    double match = lan->match(env, roughImage, offset);
    if (match < 0.4)
        return false;
    if (!ran)
        return true;
    ran->extendLT = offset;
    ran->extendRB = offset;
    match = ran->match(env, roughImage, offset);
    if (match < 0.4)
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
            roughAngle = dl.angle;
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
            LOG(DEBUG) << std::format("Pre-detected panel angle: {:.1f}deg", roughAngle);
        } else {
            LOG(WARNING) << std::format("Panel lines not found");
            return 0;
        }
    }

    LineDetector *top_line = getLineDetector("top-line");

    lastTopLine = {};
    lastBottomLine = {};
    lastSelectedTab = {};
    topRefLine = top_line->referenceLine->calcReferenceLine(env);
    {
        // rotate for anchor detection
        const int roughExtW = 300;
        const int roughExtH = 120;
        int topWidth = topRefLine.length();
        float roughW = topWidth + roughExtW;
        float roughH = roughExtH;
        lastRotRect = {roughCenter, {roughW, roughH}, roughAngle};
        cv::Point2f pointsSrc[4];
        lastRotRect.points(pointsSrc); // bottomLeft, topLeft, topRight, bottomRight
        cv::Point2f pointsDst[4] = { {0,roughH}, {0,0}, {roughW,0}, {roughW,roughH} };
        roughAffineMatrix = cv::getAffineTransform(pointsSrc, pointsDst);
#ifdef DEBUG_DETECTOR
        for (int j = 0; j < 4; j++)
            cv::line(debugImage, pointsSrc[j], pointsSrc[(j+1) % 4], {160,160,160}, 1, cv::LINE_AA);
#endif
        XMat roughImage;
        cv::warpAffine(env.getColorImage(), roughImage, roughAffineMatrix, {topWidth+roughExtW, roughExtH});
        cv::Point offset{roughExtW/2, roughExtH/2};
        AnchorDetector *lan;
        AnchorDetector *ran;
        bool anchorMatch = false;
        if (!forceDetect.empty()) {
            if (mPanelName == "scr-left-panel" && forceDetect == "flt-line") {
                lan = getAnchorDetector("flt-left");
                ran = nullptr;
                if (checkAnchors(env, lan, ran, offset, roughImage))
                    return match_dialog(env, roughAngle, roughImage, lan, debugImage);
            }
            if (mPanelName == "scr-left-panel" && forceDetect == "nav-line") {
                lan = getAnchorDetector("nav-left");
                ran = nullptr;
                if (checkAnchors(env, lan, ran, offset, roughImage))
                    return match_dialog(env, roughAngle, roughImage, lan, debugImage);
            }
        } else {
            if (!anchorMatch && mPanelName == "scr-left-panel") {
                lan = getAnchorDetector("flt-left");
                ran = nullptr;
                if (checkAnchors(env, lan, ran, offset, roughImage))
                    return match_dialog(env, roughAngle, roughImage, lan, debugImage);
            }
            if (!anchorMatch) {
                lan = getAnchorDetector("top-left");
                ran = getAnchorDetector("top-right");
                anchorMatch = checkAnchors(env, lan, ran, offset, roughImage);
            }
            if (!anchorMatch && mPanelName == "scr-right-panel") {
                lan = getAnchorDetector("trn-left");
                ran = getAnchorDetector("trn-right");
                anchorMatch = checkAnchors(env, lan, ran, offset, roughImage);
                if (anchorMatch)
                    lastSelectedTab = getTab("transfer");
            }
        }
        if (!anchorMatch) {
            LOG(WARNING) << "Anchors for top line not found";
            return 0;
        }
#ifdef DEBUG_DETECTOR
        cv::Mat debugRough = toMat(roughImage).clone();
        cv::rectangle(debugRough, lan->captureRect, {160, 160, 160});
        cv::rectangle(debugRough, ran->captureRect, {160, 160, 160});
#endif
        cv::Line roughTopLine;
        if (lan && ran) {
            float topRefAnchorDist = ran->refOrig.x - lan->refOrig.x;
            float topDetAnchorDX = ran->matchedCaptureOffset.x - lan->matchedCaptureOffset.x;
            float topDetAnchorDY = ran->matchedCaptureOffset.y - lan->matchedCaptureOffset.y;
            deltaScale = 1 + topDetAnchorDX / topRefAnchorDist;
            roughTopLine = {lan->captureRect.tl() + lan->anchor_of * deltaScale,
                            ran->captureRect.tl() + ran->anchor_of * deltaScale};
            deltaAngle = roughAngle + roughTopLine.angle() - topRefLine.angle();
        } else {
            deltaScale = 1;
            deltaAngle = roughAngle - topRefLine.angle();
        }
#ifdef DEBUG_DETECTOR
        cv::rectangle(debugRough, lan->captureRect, {160,160,160});
        cv::drawMarker(debugRough, roughTopLine.p0(), {255,255,255}, 10);
        cv::rectangle(debugRough, ran->captureRect, {160,160,160});
        cv::drawMarker(debugRough, roughTopLine.p1(), {255,255,255}, 10);
#endif
        if (!lastSelectedTab) {
            uchar selectedTabValue = 0;
            double sin_a = std::sin(roughTopLine.angle() * M_PI / 180);
            cv::Rect tabsRect {roughTopLine.p0(), roughTopLine.p1()+cv::Point(0, 40*env.getScale())};
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
#ifdef DEBUG_DETECTOR
                cv::rectangle(debugRough, r+roughTopLine.p0(), {64, 64, 64});
#endif
                detect::Histogram hist(Histogram::Mode::Gray, {0, 0, r.width, r.height});
                if (!r.empty() && hist.calc(tabImage(r))) {
                    uchar bg = hist.mLastColor[0];
                    if (bg >= 130 && bg > selectedTabValue) {
                        lastSelectedTab = &tab;
                        selectedTabValue = bg;
                    }
                }
            }
            if (!lastSelectedTab)
                LOG(WARNING) << "Nav panel tab not recognized";
        }
        cv::Matx23d backMatrix;
        cv::invertAffineTransform(roughAffineMatrix, backMatrix);
        std::array<cv::Point2f,2> arr {
                lan->captureRect.tl()+lan->anchor_of*deltaScale,
                ran->captureRect.tl()+ran->anchor_of*deltaScale,
        };
        cv::transform(arr, arr, backMatrix);
        lastTopLine = {arr[0], arr[1]};
        topLeftOffset = cv::Point2f(topRefLine.p0()) - arr[0];
#ifdef DEBUG_DETECTOR
        cv::line(debugImage, lastTopLine.p0(), lastTopLine.p1(), {200,200,200}, 1, cv::LINE_AA);
        cv::drawMarker(debugImage, lastTopLine.p0(), {255,255,255}, 10);
        cv::drawMarker(debugImage, lastTopLine.p1(), {255,255,255}, 10);
#endif
    }

    approximate_bottom_line(env);
#ifdef DEBUG_DETECTOR
    cv::line(debugImage, lastBottomLine.p0(), lastBottomLine.p1(), {64,64,64}, 1, cv::LINE_AA);
#endif
    LineDetector *btm_line = getLineDetector("btm-line");
    btm_line->detectEdgesMode = -1;
    double btmMatch = btm_line->match(env, lastBottomLine, XMat());
    if (btmMatch > 0) {
        lastBottomLine = btm_line->detectedLine;
        transform->transformSrc[2] = lastBottomLine.p1();
        if (mPanelName == "scr-left-panel")
            transform->transformSrc[2] += cv::Point2f(10,0);
        transform->transformSrc[3] = lastBottomLine.p0();
#ifdef DEBUG_DETECTOR
        for (int j = 0; j < 4; j++)
            cv::line(debugImage, transform->transformSrc[j], transform->transformSrc[(j+1) % 4], {160,160,160}, 1, cv::LINE_AA);
#endif
    } else {
#ifdef DEBUG_DETECTOR
        cv::line(debugImage, lastTopLine.p0(), lastBottomLine.p0(), {64,64,64}, 1, cv::LINE_AA);
        cv::line(debugImage, lastTopLine.p1(), lastBottomLine.p1(), {64,64,64}, 1, cv::LINE_AA);
#endif
    }

    std::string name = "nav_panel:";
    if (lastSelectedTab)
        name += lastSelectedTab->name;
    env.classified.emplace_back(ClsDetType::Detected, true, name, cv::Rect(cv::Point(), transform->origSize));
    env.classified.back().u.tdet.referenceRect = {};
    env.classified.back().u.tdet.scale = deltaScale;
    env.classified.back().u.tdet.angle = deltaAngle;
    env.classified.back().u.tdet.match = 1;
    return 1;
}

double NavPanelDetector::match_dialog(ClassifyEnv& env, float roughAngle, XMat roughImage, AnchorDetector *lan, cv::Mat debugImage) {
#ifdef DEBUG_DETECTOR
    cv::Mat debugRough = toMat(roughImage).clone();
    cv::rectangle(debugRough, lan->captureRect, {160,160,160});
#endif

    if (lan->name == "flt-left")
        lastSelectedTab = getTab("filters");
    else if (lan->name == "nav-left")
        lastSelectedTab = getTab("select");
    LineDetector *flt_line = getLineDetector("flt-line");
    cv::Line fltRefLine = flt_line->referenceLine->calcReferenceLine(env);
    cv::Line fltAltLine = {cv::Point(), fltRefLine.p1()-fltRefLine.p0()};
    fltAltLine = rotateAround(fltAltLine, cv::Point(), -roughAngle, 1);
    fltAltLine += lan->captureRect.tl() - lan->refOrig;

#ifdef DEBUG_DETECTOR
    cv::line(debugRough, fltAltLine.p0(), fltAltLine.p1(), {160, 160, 160});
#endif
    flt_line->detectEdgesMode = 1;
    double lineMatch = flt_line->match(env, fltAltLine, roughImage);
    if (lineMatch < 0.5)
        return 0;

    if (!flt_line->detectedLine.empty()) {
        deltaScale = flt_line->detectedLine.length() / fltRefLine.length();
        deltaAngle = roughAngle + flt_line->detectedLine.angle() - topRefLine.angle();
    } else {
        deltaScale = 1;
        deltaAngle = roughAngle - topRefLine.angle();
    }

    cv::Point tl = lan->captureRect.tl() + lan->anchor_of*deltaScale;
    cv::Point tr_anchor = topRefLine.p1() - fltRefLine.p1();
    tr_anchor = rotateAround(tr_anchor, {}, -roughAngle, deltaScale);
    cv::Point tr = flt_line->detectedLine.p1() + tr_anchor;
#ifdef DEBUG_DETECTOR
    cv::line(debugRough, flt_line->detectedLine.p0(), flt_line->detectedLine.p1(), {160, 160, 160});
    cv::line(debugRough, tl, tr, {160, 160, 160});
#endif

    cv::Matx23d backMatrix;
    cv::invertAffineTransform(roughAffineMatrix, backMatrix);
    std::array<cv::Point2f,2> points {tl, tr};
    cv::transform(points, points, backMatrix);
    lastTopLine = {points[0], points[1]};
    topLeftOffset = cv::Point2f(topRefLine.p0()) - points[0];
#ifdef DEBUG_DETECTOR
    cv::line(debugImage, lastTopLine.p0(), lastTopLine.p1(), {200,200,200}, 1, cv::LINE_AA);
    cv::drawMarker(debugImage, lastTopLine.p0(), {255,255,255}, 10);
    cv::drawMarker(debugImage, lastTopLine.p1(), {255,255,255}, 10);
#endif
    approximate_bottom_line(env);
#ifdef DEBUG_DETECTOR
    cv::line(debugImage, lastTopLine.p0(), lastBottomLine.p0(), {64,64,64}, 1, cv::LINE_AA);
    cv::line(debugImage, lastTopLine.p1(), lastBottomLine.p1(), {64,64,64}, 1, cv::LINE_AA);
    cv::line(debugImage, lastBottomLine.p0(), lastBottomLine.p1(), {64,64,64}, 1, cv::LINE_AA);
#endif

    std::string name = "nav_panel:";
    if (lastSelectedTab)
        name += lastSelectedTab->name;
    env.classified.emplace_back(ClsDetType::Detected, true, name, cv::Rect(cv::Point(), getTransform()->origSize));
    env.classified.back().u.tdet.referenceRect = {};
    env.classified.back().u.tdet.scale = deltaScale;
    env.classified.back().u.tdet.angle = deltaAngle;
    env.classified.back().u.tdet.match = 1;
    return 1;
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
    transform->transformSrc[0] = lastTopLine.p0();
    transform->transformSrc[1] = lastTopLine.p1();
    transform->transformSrc[2] = lastBottomLine.p1();
    if (mPanelName == "scr-left-panel")
        transform->transformSrc[2] += cv::Point2f(10,0);
    transform->transformSrc[3] = lastBottomLine.p0();
}

} // namespace detect
