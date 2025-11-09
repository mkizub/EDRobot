//
// Created by mkizub on 18.10.2025.
//

#include "../pch.h"

#include "NavList.h"
#include "Task.h"
#include "AIManager.h"
#include "../Keyboard.h"
#include "../Galaxy.h"
#include "../FuzzyMatch.h"
#include "../OCR.h"

using namespace std::chrono_literals;

namespace {
bool parseNavName(std::wstring text, nav::NavListEntry &nle) {
    nle.icon = 0;
    nle.isTarget = false;
    nle.isMarked = false;
    nle.portSize = 0;
    nle.portDanger = 0;
    nle.name.clear();

    text = trim(text);
    if (text.empty())
        return false;
    wchar_t ch = text.front();
    while (ch >= 0x2000 && ch <= 0x2FFF) {
        if (nle.icon == 0)
            nle.icon = ch;
        else
            nle.icon = nav::ERROR.charOCR;
        text = trim(text.substr(1));
        if (text.empty())
            return false;
        ch = text.front();
    }
    ch = text.back();
    if (ch == nav::LOCATION.charOCR) {
        text.pop_back();
        text = trim(text);
        nle.isMarked = true;
        ch = text.back();
    }
    if (text.contains(L'<') || text.contains(L'>')) {
        size_t ps = text.find(L'<');
        if (ps != std::string::npos && ps <= 8) {
            nle.isTarget = true;
            text = trim(text.substr(ps + 1));
        }
        size_t pe = text.find(L'>');
        if (pe != std::string::npos && pe >= text.size() - 8) {
            nle.isTarget = true;
            text = trim(text.substr(1, pe - 1));
        }
        if (text.empty())
            return false;
    }
    ch = text.back();
    if (ch == nav::SHIELD1.charOCR || ch == nav::SHIELD2.charOCR || ch == nav::SHIELD3.charOCR) {
        nle.portDanger = ch;
        text.pop_back();
        text = trim(text);
        if (text.empty())
            return false;
    }
    ch = text.back();
    while (ch == L'+') {
        nle.portSize += 1;
        text.pop_back();
        text = trim(text);
        if (text.empty())
            return false;
        ch = text.back();
    }
    nle.name = text;

    return true;
}

bool parseNavRow(const cv::Mat &grayImage, const ResolvedEnv& rEnv, const ClassifiedRect &cr, nav::NavListEntry &nle) {
    ai::check_interrupted();
    nle = {};
    cv::Rect rectOut;
    std::string text;
    int ocr_conf = ocr::ocrRowText(ocr::GENERIC, grayImage, rEnv, cr, 0, text, &rectOut);
    if (ocr_conf < 60)
        text.clear();
    std::wstring wtext = toUtf16(text);
    bool ok = parseNavName(wtext, nle);
    nle.parsed = true;
    nle.ocr_conf = ocr_conf;
    double indent = (rectOut.x - 3.0) / double(25);
    nle.indent = (int) std::round(indent);
    if (cr.u.lrow.ws == WState::Focused)
        nle.focused = true;
    return ok;
}

bool parseNavDist(const cv::Mat &grayImage, const ResolvedEnv& rEnv, const ClassifiedRect &cr, nav::NavListEntry &nle) {
    ai::check_interrupted();
    std::string dist;
    if (ocr::ocrRowText(ocr::DISTANCE, grayImage, rEnv, cr, 1, dist) < 60)
        return false;
    std::wstring wdist = toUtf16(dist);
    nle.dist = parseDist(wdist);
    return nle.dist.valid();
}

bool parseFocusedNavRow(const cv::Mat& grayImage, const ResolvedEnv& rEnv, nav::NavListEntry &nle) {
    nle = {};
    for (auto &cr: rEnv.classified) {
        if (cr.cdt != ClsDetType::ListRow)
            continue;
        if (cr.u.lrow.ws == WState::Focused) {
            bool ok = parseNavRow(grayImage, rEnv, cr, nle);
            ok &= parseNavDist(grayImage, rEnv, cr, nle);
            return ok;
        }
    }
    return false;
}
}

namespace nav {

bool NavList::init(st::NavPanelFilters filters) {
    this->list.clear();

    if (filters == st::navFilters)
        return true;

    st::autopilot.isDestBodyFocused = false;
    st::autopilot.isDestDockFocused = false;
    for (int retry=0; retry < 3; retry++) {
        if (filters == st::navFilters)
            return true;

        if (!ai::gotoNavPage("mod-navigation", false))
            continue;

        int delay = 300;
        kbd::send("UI_Left", 0, delay);
        for (int i = 0; i < 4; i++)
            kbd::send("UI_Up", 0, delay);
        kbd::send("UI_Select", 0, 1000);

        ai::detectEDState(DetectLevel::Buttons);
        if (!ai::uiState.match("scr-left-panel:dlg-filters")) {
            ai::throw_trouble("TaskDock expecting 'scr-left-panel:dlg-filters' but got " + ai::uiState.to_string());
        }
        // currently ED always opens filters at top position 'stars',
        // so just scroll down and select/deselect what we need
        if (filters.star != st::navFilters.star)
            kbd::send("UI_Select");
        kbd::send("UI_Down");

        if (filters.asteroidCluster != st::navFilters.asteroidCluster)
            kbd::send("UI_Select");
        kbd::send("UI_Down");

        if (filters.planetOrMoon != st::navFilters.planetOrMoon)
            kbd::send("UI_Select");
        kbd::send("UI_Down");

        if (filters.landablePlanetOrMoon != st::navFilters.landablePlanetOrMoon)
            kbd::send("UI_Select");
        kbd::send("UI_Down");

        if (filters.settlement != st::navFilters.settlement)
            kbd::send("UI_Select");
        kbd::send("UI_Down");

        if (filters.station != st::navFilters.station)
            kbd::send("UI_Select");
        kbd::send("UI_Down");

        if (filters.fleetCarrier != st::navFilters.fleetCarrier)
            kbd::send("UI_Select");
        kbd::send("UI_Down");

        if (filters.pointOfInterest != st::navFilters.pointOfInterest)
            kbd::send("UI_Select");
        kbd::send("UI_Down");

        if (filters.signalSource != st::navFilters.signalSource)
            kbd::send("UI_Select");
        kbd::send("UI_Down");

        if (filters.system != st::navFilters.system)
            kbd::send("UI_Select");
        kbd::send("UI_Down");

        kbd::send("UI_Back", 0, 1000); // wait for configuration changes
        kbd::send("UI_Right");
    }
    return (filters == st::navFilters);
}

std::vector<ClassifiedRect*> NavList::initNavList(cv::Mat& grayImage, int& focusIdx) {
    std::vector<ClassifiedRect*> rows;
    focusIdx = -1;
    list.clear();
    for (int retry=0; retry < 3; retry++) {
        if (!ai::uiState.match("scr-left-panel:mod-navigation"))
            if (!ai::gotoNavPage("mod-navigation", false))
                continue;
        if (ai::uiState.focused_name() != "lst-bodies")
            kbd::send("UI_Right");
        if (!ai::detectEDState(DetectLevel::ListRows, nullptr, &grayImage))
            continue;
        rows.clear();
        for (auto &cr: ai::rEnv.classified) {
            if (cr.cdt != ClsDetType::ListRow)
                continue;
            if (cr.u.lrow.ws == WState::Focused)
                focusIdx = rows.size();
            rows.push_back(&cr);
        }
        if (focusIdx >= 0) {
            list.resize(rows.size());
            break;
        }
        kbd::send("UI_Right");
    }
    if (focusIdx < 0 || focusIdx >= rows.size())
        return {};
    list[focusIdx].focused = true;
    return rows;
}

std::vector<ClassifiedRect*> NavList::recognizeWholePage(cv::Mat& grayImage, int& focusIdx) {
    std::vector<ClassifiedRect*> rows = initNavList(grayImage, focusIdx);
    for (int idx=0; idx < rows.size(); idx++) {
        parseNavRow(grayImage, ai::rEnv, *rows[idx], list[idx]);
        list[idx].item = guessNavItem(list[idx]);
    }
    return rows;
}

bool NavList::recognizeFocusedNavRow(nav::NavListEntry& nle) {
    if (!ai::uiState.match("scr-left-panel:mod-navigation"))
        ai::gotoNavPage("mod-navigation");

    cv::Mat grayImage;
    double dist_km = 10;
    for (int cnt=0; cnt < 3; cnt++) {
        ai::detectEDState(DetectLevel::ListRows, nullptr, &grayImage);
        if (ai::uiState.focused_name() != "lst-bodies") {
            kbd::send("UI_Right", 0, 500);
            continue;
        }
        if (!parseFocusedNavRow(grayImage, ai::rEnv, nle)) {
            LOG(INFO) << "Failed to parse nav row";
            continue;
        }
        return true;
    }
    return false;
}

gal::spItem NavList::guessNavItem(NavListEntry &nle) {
    gal::spStarSystem ss = gal::getCurrentStarSystem();
    gal::spItem bestItem;
    FuzzyMatch fm;
    double bestMatch = 0.5;
    {
        double match = fm.ratio(fm.toOCR(L"Не исследовано"), nle.name);
        if (nle.icon != nav::SIGNAL.charOCR)
            match -= 10;
        if (match >= 60) {
            bestMatch = match;
            bestItem = std::make_shared<gal::Item>();
            bestItem->name = "Не исследовано";
        }
    }
    {
        gal::spSite bestSite;
        for (auto& s : ss->stations) {
            bool duplicated = false;
            for (auto& ne : list)
                if (ne.confirmed >= 0 && ne.item.get() == s.get())
                    duplicated = true;
            if (duplicated)
                continue;
            bool typeMatch = false;
            if (s->typeNav == gal::TypeNav::SpacePort) {
                if (s->typeSite == gal::TypeSite::Orbis || s->typeSite == gal::TypeSite::Ocellus)
                    typeMatch = (nle.icon == nav::ORBIS.charOCR);
                else if (s->typeSite == gal::TypeSite::Coriolis)
                    typeMatch = (nle.icon == nav::CORIOLIS.charOCR);
                else if (s->typeSite == gal::TypeSite::AsteroidBase)
                    typeMatch = (nle.icon == nav::MINER_BASE.charOCR);
                else if (s->typeSite == gal::TypeSite::SpaceOutpost)
                    typeMatch = (nle.icon == nav::SPACE_OUTPOST.charOCR);
            }
            else if (s->typeNav == gal::TypeNav::SpaceInst) {
                typeMatch = (nle.icon == nav::SPACE_INSTALLATION.charOCR);
            }
            else if (s->typeNav == gal::TypeNav::Carrier) {
                if (s->typeSite == gal::TypeSite::FleetCarrier)
                    typeMatch = (nle.icon == nav::FLEET_CARRIER.charOCR || nle.icon == nav::SQUADRON_CARRIER.charOCR);
            }
            else if (s->typeNav == gal::TypeNav::MegashipDock) {
                typeMatch = (nle.icon == nav::STATION_MEGASHIP.charOCR);
            }
            double match = fm.ratio(fm.toOCR(toUtf16(s->name)), nle.name);
            if (!s->nloc.empty())
                match = std::max(match, fm.ratio(fm.toOCR(toUtf16(s->nloc)), nle.name));
            if (!typeMatch)
                match -= 10;
            if (match >= 60 && match > bestMatch) {
                bestMatch = match;
                bestSite = s;
            }
        }
        if (bestSite)
            bestItem = std::static_pointer_cast<gal::Item>(bestSite);
    }
    {
        gal::spBody bestBody;
        for (auto& b : ss->bodies) {
            bool duplicated = false;
            for (auto& ne : list)
                if (ne.confirmed > 0 && ne.item.get() == b.get())
                    duplicated = true;
            if (duplicated)
                continue;
            bool typeMatch = false;
            if (b->typeNav == gal::TypeNav::Star) {
                typeMatch = (nle.icon == nav::STAR.charOCR);
            }
            else if (b->typeNav == gal::TypeNav::Planet) {
                auto* planet = (gal::Planet*)b.get();
                if (planet->isLandable)
                    typeMatch = (nle.icon == nav::LAND.charOCR);
                else
                    typeMatch = (nle.icon == nav::BODY.charOCR);
            }
            double match = fm.ratio(fm.toOCR(toUtf16(b->name)), nle.name);
            if (!typeMatch)
                match -= 10;
            if (match >= 80 && match > bestMatch) {
                bestMatch = match;
                bestBody = b;
            }
        }
        if (bestBody)
            bestItem = std::static_pointer_cast<gal::Item>(bestBody);
    }
    return bestItem;
}

bool NavList::fixupNavList() {
    bool hasFixes = false;
    for (int idx=0; idx < list.size(); idx++) {
        if (!list[idx].parsed || list[idx].ocr_conf < 80 || !list[idx].item)
            continue;
        if (list[idx].item->isSite() && list[idx].item->parentBodyId > 0) {
            int bodyId = list[idx].item->parentBodyId;
            int bodyIndent = list[idx].indent - 1;
            int bodyIdx = -1;
            for (int b=idx-1; b >= 0; b--) {
                if (list[b].indent == bodyIndent) {
                    bodyIdx = b;
                    break;
                }
            }
            if (bodyIdx >= 0) {
                if (!list[bodyIdx].item || list[bodyIdx].ocr_conf < list[idx].ocr_conf) {
                    list[bodyIdx].item = gal::getCurrentStarSystem()->getBodyById(bodyId);
                    hasFixes = true;
                }
            }
        }
    }
    return hasFixes;
}

static inline void nextIdx(int& idx, int incr, size_t size) {
    idx = idx + incr;
    if (idx < 0)
        idx = int(size)-1;
    else if (idx >= size)
        idx = 0;
}
bool NavList::focusDestDock() {
    auto destBody = st::autopilot.destBody;
    auto destDock = st::autopilot.destDock;
    if (!destDock)
        return false;
    if (st::autopilot.isDestDockFocused)
        return true;
    int focusIdx;
    cv::Mat grayImage;
    std::vector<ClassifiedRect*> rows = initNavList(grayImage, focusIdx);
    if (rows.empty())
        return false;

    int startIdx = 0;
    if (st::autopilot.isDestBodyFocused && focusIdx >= 0)
        startIdx = focusIdx;

    // try current page
    int destBodyNavIdx = -1;
    int destDockNavIdx = -1;
    for (int idx=startIdx; !list[idx].parsed; nextIdx(idx, 1, list.size())) {
        parseNavRow(grayImage, ai::rEnv, *rows[idx], list[idx]);
        list[idx].item = guessNavItem(list[idx]);
        if (destBody && list[idx].item.get() == destBody.get()) {
            destBodyNavIdx = idx;
            parseNavDist(grayImage, ai::rEnv, *rows[idx], list[idx]);
            if (list[idx].dist.valid())
                st::autopilot.distanceToBody = list[idx].dist;
        }
        if (list[idx].item.get() == destDock.get()) {
            destDockNavIdx = idx;
            parseNavDist(grayImage, ai::rEnv, *rows[idx], list[idx]);
            if (list[idx].dist.valid())
                st::autopilot.distanceToDock = list[idx].dist;
            break;
        }
    }
    if (destDockNavIdx == focusIdx) {
        st::autopilot.isDestDockFocused = true;
        return true;
    }

    if (destDockNavIdx < 0 && destBodyNavIdx >= 0) {
        int bodyIndent = list[destBodyNavIdx].indent;
        for (int idx=destBodyNavIdx+1; idx < rows.size(); idx++) {
            if (!list[idx].parsed)
                break;
            if (list[idx].indent <= bodyIndent)
                break;
            if (list[idx].indent > bodyIndent+1)
                continue; // ??? should not happen
            if (list[idx].item && list[idx].item->isSite() && list[idx].item->parentBodyId > 0) {
                gal::spItem item = gal::getCurrentStarSystem()->getBodyById(list[idx].item->parentBodyId);
                if (item.get() != list[destBodyNavIdx].item.get()) {
                    st::autopilot.badBodyHierarchy = true;
                    break;
                }
            }
        }
    }

    st::autopilot.isDestDockFocused = false;
    st::autopilot.isDestBodyFocused = false;
    for (int retry=0; retry < 10; retry++) {
        startIdx = 0;
        if (destDockNavIdx >= 0) {
            // found in current page, scroll page down
            int count = destDockNavIdx - focusIdx;
            if (count > 0) {
                for (int i=0; i < count; i++)
                    kbd::send("UI_Down");
            } else {
                for (int i=0; i < -count; i++)
                    kbd::send("UI_Up");
            }
            startIdx = destDockNavIdx;
        } else {
            // not found in current page, scroll page down
            int count = int(rows.size()) - focusIdx - 1;
            for (int i = 0; i < count; i++)
                kbd::send("UI_Down");
            int hold = 300 + 8*50;
            kbd::send("UI_Down", hold);
        }
        ai::sleep(300);
        destDockNavIdx = -1;
        destBodyNavIdx = -1;

        rows = initNavList(grayImage, focusIdx);
        if (rows.empty() || focusIdx < 0)
            continue;
        for (int idx = startIdx; !list[idx].parsed; nextIdx(idx, 1, list.size())) {
            parseNavRow(grayImage, ai::rEnv, *rows[idx], list[idx]);
            list[idx].item = guessNavItem(list[idx]);
            if (list[idx].item.get() == destDock.get()) {
                destDockNavIdx = idx;
                parseNavDist(grayImage, ai::rEnv, *rows[idx], list[idx]);
                if (list[idx].dist.valid())
                    st::autopilot.distanceToDock = list[idx].dist;
                break;
            }
        }
        if (destDockNavIdx == focusIdx) {
            st::autopilot.isDestDockFocused = true;
            return true;
        }
    }
    return false;
}

bool NavList::focusDestBody() {
    auto destBody = st::autopilot.destBody;
    auto destDock = st::autopilot.destDock;
    if (!st::autopilot.destBody)
        return false;
    if (st::autopilot.isDestBodyFocused)
        return true;
    int focusIdx;
    cv::Mat grayImage;
    std::vector<ClassifiedRect*> rows = initNavList(grayImage, focusIdx);
    if (rows.empty())
        return false;

    int startIdx = 0;
    int incr = 1;
    if (st::autopilot.isDestDockFocused && focusIdx >= 0) {
        startIdx = focusIdx;
        incr = -1;
    }

    // try current page
    int destBodyNavIdx = -1;
    int destDockNavIdx = -1;
    for (int idx=startIdx; !list[idx].parsed; nextIdx(idx, incr, list.size())) {
        parseNavRow(grayImage, ai::rEnv, *rows[idx], list[idx]);
        list[idx].item = guessNavItem(list[idx]);
        if (destBody && list[idx].item.get() == destBody.get()) {
            destBodyNavIdx = idx;
            parseNavDist(grayImage, ai::rEnv, *rows[idx], list[idx]);
            if (list[idx].dist.valid())
                st::autopilot.distanceToBody = list[idx].dist;
            break;
        }
        if (destDock && list[idx].item.get() == destDock.get()) {
            destDockNavIdx = idx;
            parseNavDist(grayImage, ai::rEnv, *rows[idx], list[idx]);
            if (list[idx].dist.valid())
                st::autopilot.distanceToDock = list[idx].dist;
        }
    }
    if (destBodyNavIdx == focusIdx) {
        st::autopilot.isDestBodyFocused = true;
        return true;
    }
    if (fixupNavList()) {
        for (int idx=0; idx < list.size(); idx++) {
            if (list[idx].item.get() == destBody.get()) {
                destBodyNavIdx = idx;
                parseNavDist(grayImage, ai::rEnv, *rows[idx], list[idx]);
                if (list[idx].dist.valid())
                    st::autopilot.distanceToBody = list[idx].dist;
                break;
            }
        }
        if (destBodyNavIdx == focusIdx) {
            st::autopilot.isDestBodyFocused = true;
            return true;
        }
    }

    if (destDockNavIdx < 0 && destBodyNavIdx >= 0) {
        int bodyIndent = list[destBodyNavIdx].indent;
        for (int idx=destBodyNavIdx+1; idx < rows.size(); idx++) {
            if (!list[idx].parsed)
                break;
            if (list[idx].indent <= bodyIndent)
                break;
            if (list[idx].indent > bodyIndent+1)
                continue; // ??? should not happen
            if (list[idx].item && list[idx].item->isSite() && list[idx].item->parentBodyId > 0) {
                gal::spItem item = gal::getCurrentStarSystem()->getBodyById(list[idx].item->parentBodyId);
                if (item.get() != list[destBodyNavIdx].item.get()) {
                    st::autopilot.badBodyHierarchy = true;
                    break;
                }
            }
        }
    }

    st::autopilot.isDestDockFocused = false;
    st::autopilot.isDestBodyFocused = false;
    for (int retry=0; retry < 10; retry++) {
        startIdx = 0;
        if (destBodyNavIdx >= 0) {
            // found in current page, scroll page down
            int count = destBodyNavIdx - focusIdx;
            if (count > 0) {
                for (int i=0; i < count; i++)
                    kbd::send("UI_Down");
            } else {
                for (int i=0; i < -count; i++)
                    kbd::send("UI_Up");
            }
            startIdx = destBodyNavIdx;
        } else {
            // not found in current page, scroll page down
            int count = int(rows.size()) - focusIdx - 1;
            for (int i = 0; i < count; i++)
                kbd::send("UI_Down");
            int hold = 300 + 8*50;
            kbd::send("UI_Down", hold);
        }
        ai::sleep(300);
        destDockNavIdx = -1;
        destBodyNavIdx = -1;

        rows = initNavList(grayImage, focusIdx);
        if (rows.empty() || focusIdx < 0)
            continue;
        for (int idx = startIdx; !list[idx].parsed; nextIdx(idx, 1, list.size())) {
            parseNavRow(grayImage, ai::rEnv, *rows[idx], list[idx]);
            list[idx].item = guessNavItem(list[idx]);
            if (list[idx].item.get() == destBody.get()) {
                destBodyNavIdx = idx;
                parseNavDist(grayImage, ai::rEnv, *rows[idx], list[idx]);
            }
            if (destBodyNavIdx == focusIdx) {
                st::autopilot.isDestBodyFocused = true;
                return true;
            }
        }
        if (fixupNavList()) {
            for (int idx=0; idx < list.size(); idx++) {
                if (list[idx].item.get() == destBody.get()) {
                    destBodyNavIdx = idx;
                    parseNavDist(grayImage, ai::rEnv, *rows[idx], list[idx]);
                    if (list[idx].dist.valid())
                        st::autopilot.distanceToBody = list[idx].dist;
                    break;
                }
            }
            if (destBodyNavIdx == focusIdx) {
                st::autopilot.isDestBodyFocused = true;
                return true;
            }
        }
    }
    return false;
}

gal::spBody NavList::focusNearestBody(dist_t* dist) {
    if (!focusTopEntry())
        return {};

    for (int retry=0; retry < 3; retry++) {
        int focusIdx;
        cv::Mat grayImage;
        std::vector<ClassifiedRect*> rows = initNavList(grayImage, focusIdx);
        if (rows.empty())
            continue;

        int nearestIdx = -1;
        for (int idx=0; idx < list.size(); idx++) {
            parseNavRow(grayImage, ai::rEnv, *rows[idx], list[idx]);
            list[idx].item = guessNavItem(list[idx]);
            gal::spBody body = std::dynamic_pointer_cast<gal::Body>(list[idx].item);
            if (body || list[idx].icon == nav::STAR.charOCR ||
                list[idx].icon == nav::BODY.charOCR ||
                list[idx].icon == nav::LAND.charOCR)
            {
                if (list[idx].isMarked && !list[idx].isTarget) {
                    nearestIdx = idx;
                    break;
                }
                if (nearestIdx >= 0 && list[idx].indent <= list[nearestIdx].indent)
                    break;
                nearestIdx = idx;
                continue;
            }
            if (list[idx].icon == nav::STAR.charOCR ||
                list[idx].icon == nav::BODY.charOCR ||
                list[idx].icon == nav::LAND.charOCR ||
                list[idx].icon == nav::ERROR.charOCR)
            {
                if (nearestIdx >= 0) {
                    if (list[idx].indent < 2)
                        break;
                    int expectedIndent = list[idx].indent - 1;
                    for (int i=0; i < idx; i++) {
                        if (list[i].indent >= expectedIndent) {
                            nearestIdx = i;
                            break;
                        }
                    }
                    break;
                }
            }
        }
        if (focusIdx >= 0 && nearestIdx == focusIdx) {
            parseNavDist(grayImage, ai::rEnv, *rows[nearestIdx], list[nearestIdx]);
            if (list[nearestIdx].item.get() == st::autopilot.destBody.get()) {
                st::autopilot.isDestBodyFocused = true;
                if (list[nearestIdx].dist.valid())
                    st::autopilot.distanceToBody = list[nearestIdx].dist;
            }
            if (dist)
                *dist = list[nearestIdx].dist;
            return std::dynamic_pointer_cast<gal::Body>(list[nearestIdx].item);
        }
        if (nearestIdx > 0) {
            int count = nearestIdx - focusIdx;
            for (int i = 0; i < count; i++)
                kbd::send("UI_Down");
            ai::sleep(300);
        }
    }
    return {};
}

bool NavList::focusTopEntry() {
    st::autopilot.isDestDockFocused = false;
    st::autopilot.isDestBodyFocused = false;
    for (int retry=0; retry < 3; retry++) {
        int focusIdx;
        cv::Mat grayImage;
        std::vector<ClassifiedRect*> rows = initNavList(grayImage, focusIdx);
        if (rows.empty())
            continue;

        if (focusIdx == 0)
            kbd::send("UI_Down");
        unsigned handle = kbd::post("UI_Up", 10000);
        if (!handle)
            return false;
        ai::sleep(300);
        gal::Item* topItem = nullptr;
        utc_timer timer(10s);
        while (!timer.expired()) {
            ai::sleep(100);
            rows = initNavList(grayImage, focusIdx);
            if (rows.empty())
                continue;
            if (focusIdx != 0)
                continue;
            if (!parseNavRow(grayImage, ai::rEnv, *rows[0], list[0]))
                continue;
            list[0].item = guessNavItem(list[0]);
            if (!list[0].item)
                continue;
            if (topItem != list[0].item.get()) {
                topItem = list[0].item.get();
                continue;
            }
            kbd::clearInput(handle);
            return true;
        }
        kbd::clearInput(handle);
    }
    return false;
}

bool NavList::selectFocused() {
    for (int retry=0; retry < 3; retry++) {
        int focusIdx;
        cv::Mat grayImage;
        std::vector<ClassifiedRect *> rows = initNavList(grayImage, focusIdx);
        if (rows.empty() || focusIdx < 0)
            continue;

        if (!parseNavRow(grayImage, ai::rEnv, *rows[focusIdx], list[focusIdx]))
            continue;
        if (list[focusIdx].isTarget)
            return true;

        kbd::send("UI_Select");
        kbd::send("UI_Select");
        return true;
    }
    return false;
}

dist_t NavList::getFocusedDist(int max_try) {
    for (int retry=0; retry < max_try; retry++) {
        int focusIdx;
        cv::Mat grayImage;
        std::vector<ClassifiedRect *> rows = initNavList(grayImage, focusIdx);
        if (rows.empty() || focusIdx < 0)
            continue;

        if (parseNavDist(grayImage, ai::rEnv, *rows[focusIdx], list[focusIdx])) {
            if (list[focusIdx].dist.valid()) {
                if (st::autopilot.isDestBodyFocused)
                    st::autopilot.distanceToBody = list[focusIdx].dist;
                if (st::autopilot.isDestDockFocused)
                    st::autopilot.distanceToDock = list[focusIdx].dist;
            }
            return list[focusIdx].dist;
        }
    }
    return {};
}

} // namespace nav

namespace ai {

NavListScanTask::NavListScanTask(const TaskTemplate& templ_)
        : Task(templ_)
{
    assert (templ.id == ED_TASK_NAV_SCAN);
    for (auto& p : templ.params) {
        if (p.name == "travel")
            mTravel = std::get<bool>(p.value);
    }
}

bool NavListScanTask::run() {
    kbd::send("SetSpeedZero", 50);
    st::NavPanelFilters filters {};
    filters.star = true;
    filters.planetOrMoon = true;
    filters.landablePlanetOrMoon = true;
    //filters.station = true;
    //filters.fleetCarrier = true;
    //filters.pointOfInterest = true;
    //filters.settlement = true;

    if (!nl.init(filters))
        return false;

    if (!gotoNavPageNavigation())
        return false;
    if (!nl.focusTopEntry())
        return false;

    auto starSystem = gal::getCurrentStarSystem();
    if (!starSystem)
        throw_failed("StarSystem not known");
    std::vector<gal::spBody> scannedBodies;
    std::vector<gal::spSite> scannedSites;
    for (;;) {
        int focusIdx;
        cv::Mat grayImage;
        std::vector<ClassifiedRect *> rows = nl.recognizeWholePage(grayImage, focusIdx);
        for (int idx = 0; idx < nl.list.size(); idx++) {
            auto &nle = nl.list[idx];
            if (nle.item || !(nle.item->isBody() || nle.item->isSite())) {
                if (!nle.focused) {
                    int count = idx - focusIdx;
                    if (count > 0) {
                        for (int i = 0; i < count; i++)
                            kbd::send("UI_Down");
                    } else {
                        for (int i = 0; i < -count; i++)
                            kbd::send("UI_Up");
                    }
                    break;
                }
                if (!nle.isTarget) {
                    kbd::send("UI_Select");
                    kbd::send("UI_Select");
                    sleep(1000);
                    //st::destination.name
                }
            }
        }
    }

    return true;
}

bool NavListScanTask::gotoNavPageNavigation() {
    bool ok = false;
    for (int retry=0; retry < 3; retry++) {
        if (!ai::uiState.match("scr-left-panel:mod-navigation"))
            gotoNavPage("mod-navigation");
        if (ai::uiState.focused_name() != "lst-bodies")
            kbd::send("UI_Right");
        ok = true;
    }
    return ok;
}

} // namespace ai