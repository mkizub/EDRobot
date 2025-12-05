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
#include "../detect/NavPanel.h"

namespace {

bool parseNavName(std::wstring text, ai::NavListEntry &nle) {
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
            nle.icon = gal::ERROR_MARK;
        text = trim(text.substr(1));
        if (text.empty())
            return false;
        ch = text.front();
    }
    ch = text.back();
    if (ch == gal::LOCATION_MARK) {
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
    if (ch == gal::SHIELD1_MARK || ch == gal::SHIELD2_MARK || ch == gal::SHIELD3_MARK) {
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

}

namespace ai {

bool NavList::parseNavRow(const cv::Mat &grayImage, const ResolvedEnv& rEnv, const ClassifiedRect &cr, int idx) {
    ai::check_interrupted();
    if (idx < 0 || idx >= list.size())
        return false;
    NavListEntry &nle = list[idx];
    assert (nle.index == idx);
    nle = {};
    nle.index = idx;
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

bool NavList::parseNavDist(const cv::Mat &grayImage, const ResolvedEnv& rEnv, const ClassifiedRect &cr, int idx) {
    ai::check_interrupted();
    if (idx < 0 || idx >= list.size())
        return false;
    NavListEntry &nle = list[idx];
    assert (nle.index == idx);
    std::string dist;
    int conf = ocr::ocrRowText(ocr::DISTANCE, grayImage, rEnv, cr, 1, dist);
    if (conf < 60)
        return false;
    std::wstring wdist = toUtf16(dist);
    nle.dist = parseDist(wdist, conf);
    return nle.dist.valid();
}

bool NavList::init(st::NavPanelFilters filters) {
    this->list.clear();

    if (filters == st::navFilters)
        return true;

    st::autopilot.isDestBodyFocused = false;
    st::autopilot.isDestDockFocused = false;
    for (int retry=0; retry < 3; retry++) {
        if (filters == st::navFilters)
            return true;

        if (!ai::gotoNavPage("mod-nav-list", false))
            continue;

        int delay = 300;
        kbd::send("UI_Left", 0, delay);
        for (int i = 0; i < 4; i++)
            kbd::send("UI_Up", 0, delay);
        kbd::send("UI_Select", 0, 1000);

        detect::NavPanelDetectLock lock("flt-line");
        ai::detectEDState(DetectLevel::Buttons);
        if (!ai::uiState.match("scr-left-panel:dlg-filters")) {
            ai::throw_trouble("Expecting 'scr-left-panel:dlg-filters' but got {}", ai::uiState.to_string());
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
        if (!ai::uiState.match("scr-left-panel:mod-nav-list"))
            if (!ai::gotoNavPage("mod-nav-list", false))
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
    for (int idx=0; idx < list.size(); idx++)
        list[idx].index = idx;
    list[focusIdx].focused = true;
    return rows;
}

std::vector<ClassifiedRect*> NavList::recognizeWholePage(cv::Mat& grayImage, int& focusIdx) {
    std::vector<ClassifiedRect*> rows = initNavList(grayImage, focusIdx);
    for (int idx=0; idx < rows.size(); idx++) {
        parseNavRow(grayImage, ai::rEnv, *rows[idx], idx);
        guessNavItem(idx);
    }
    fixupNavList();
    return rows;
}

gal::spEntity NavList::guessNavItem(int idx) {
    if (idx < 0 || idx >= list.size())
        return {};
    NavListEntry &nle = list[idx];
    gal::spStarSystem ss = gal::getCurrentStarSystem();
    gal::spEntity bestItem;
    FuzzyMatch fm;
    double bestMatch = 0.5;
    {
        double match = fm.ratio(fm.toOCR(L"Не исследовано"), nle.name);
        if (nle.icon != gal::SIGNAL.charOCR)
            match -= 10;
        if (match >= 60) {
            bestMatch = match;
            bestItem = std::make_shared<gal::Entity>();
            bestItem->name = "Не исследовано";
        }
    }
    {
        gal::spEntity bestSite;
        for (auto& s : ss->stations) {
            bool duplicated = false;
            for (auto& ne : list)
                if (ne.confirmed >= 0 && ne.item.get() == s.get())
                    duplicated = true;
            if (duplicated)
                continue;
            bool typeMatch = false;
            if ((int(s->type) & int(TypeNav::SpaceStation)) == int(TypeNav::SpaceStation)) {
                if (s->type == TypeNav::Orbis || s->type == TypeNav::Ocellus)
                    typeMatch = (nle.icon == gal::ORBIS.charOCR);
                else if (s->type == TypeNav::Coriolis)
                    typeMatch = (nle.icon == gal::CORIOLIS.charOCR);
                else if (s->type == TypeNav::AsteroidBase)
                    typeMatch = (nle.icon == gal::MINER_BASE.charOCR);
                else if (s->type == TypeNav::SpaceOutpost)
                    typeMatch = (nle.icon == gal::SPACE_OUTPOST.charOCR);
            }
            else if (s->type == TypeNav::SpaceInstallation) {
                typeMatch = (nle.icon == gal::SPACE_INSTALLATION.charOCR);
            }
            else if (s->type == TypeNav::FleetCarrier) {
                typeMatch = (nle.icon == gal::FLEET_CARRIER.charOCR);
            }
            else if (s->type == TypeNav::SquadronCarrier) {
                typeMatch = (nle.icon == gal::SQUADRON_CARRIER.charOCR);
            }
            else if (s->type == TypeNav::StationMegaShip) {
                typeMatch = (nle.icon == gal::STATION_MEGASHIP.charOCR);
            }
            else if (s->type == TypeNav::Megaship) {
                typeMatch = (nle.icon == gal::MEGASHIP.charOCR);
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
            bestItem = bestSite;
    }
    {
        gal::spEntity bestBody;
        for (auto& b : ss->bodies) {
            bool duplicated = false;
            for (auto& ne : list)
                if (ne.confirmed > 0 && ne.item.get() == b.get())
                    duplicated = true;
            if (duplicated)
                continue;
            bool typeMatch = false;
            if (b->type == TypeNav::Star) {
                typeMatch = (nle.icon == gal::STAR.charOCR);
            }
            else if (b->type == TypeNav::Planet) {
                if (b->special)
                    typeMatch = (nle.icon == gal::LAND.charOCR);
                else
                    typeMatch = (nle.icon == gal::BODY.charOCR);
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
            bestItem = bestBody;
    }
    list[idx].item = bestItem;
    return bestItem;
}

bool NavList::fixupNavList() {
    bool hasFixes = false;
    for (int idx=0; idx < list.size(); idx++) {
        if (!list[idx].parsed || list[idx].ocr_conf < 80 || !list[idx].item)
            continue;
        if (isSite(list[idx].item->type) && list[idx].item->parentBodyId > 0) {
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
        parseNavRow(grayImage, ai::rEnv, *rows[idx], idx);
        guessNavItem(idx);
        if (destBody && list[idx].item.get() == destBody.get()) {
            destBodyNavIdx = idx;
            parseNavDist(grayImage, ai::rEnv, *rows[idx], idx);
            if (list[idx].dist)
                st::autopilot.distanceToBody = list[idx].dist;
        }
        if (list[idx].item.get() == destDock.get()) {
            destDockNavIdx = idx;
            parseNavDist(grayImage, ai::rEnv, *rows[idx], idx);
            if (list[idx].dist)
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
            if (list[idx].item && isSite(list[idx].item->type) && list[idx].item->parentBodyId > 0) {
                gal::spEntity item = gal::getCurrentStarSystem()->getBodyById(list[idx].item->parentBodyId);
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
            parseNavRow(grayImage, ai::rEnv, *rows[idx], idx);
            guessNavItem(idx);
            if (list[idx].item.get() == destDock.get()) {
                destDockNavIdx = idx;
                parseNavDist(grayImage, ai::rEnv, *rows[idx], idx);
                if (list[idx].dist)
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
        parseNavRow(grayImage, ai::rEnv, *rows[idx], idx);
        guessNavItem(idx);
        if (destBody && list[idx].item.get() == destBody.get()) {
            destBodyNavIdx = idx;
            parseNavDist(grayImage, ai::rEnv, *rows[idx], idx);
            if (list[idx].dist)
                st::autopilot.distanceToBody = list[idx].dist;
            break;
        }
        if (destDock && list[idx].item.get() == destDock.get()) {
            destDockNavIdx = idx;
            parseNavDist(grayImage, ai::rEnv, *rows[idx], idx);
            if (list[idx].dist)
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
                parseNavDist(grayImage, ai::rEnv, *rows[idx], idx);
                if (list[idx].dist)
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
            if (list[idx].item && isSite(list[idx].item->type) && list[idx].item->parentBodyId > 0) {
                gal::spEntity item = gal::getCurrentStarSystem()->getBodyById(list[idx].item->parentBodyId);
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
            parseNavRow(grayImage, ai::rEnv, *rows[idx], idx);
            guessNavItem(idx);
            if (list[idx].item.get() == destBody.get()) {
                destBodyNavIdx = idx;
                parseNavDist(grayImage, ai::rEnv, *rows[idx], idx);
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
                    parseNavDist(grayImage, ai::rEnv, *rows[idx], idx);
                    if (list[idx].dist)
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

bool NavList::focusDestination(int& focusIdx) {
    if (st::destination.name.empty())
        return false;
    cv::Mat grayImage;
    std::vector<ClassifiedRect*> rows = initNavList(grayImage, focusIdx);
    if (rows.empty())
        return false;

    int startIdx = 0;
    int destSignalNavIdx = -1;
    for (int idx=startIdx; !list[idx].parsed; nextIdx(idx, 1, list.size())) {
        parseNavRow(grayImage, ai::rEnv, *rows[idx], idx);
        guessNavItem(idx);
        if (list[idx].isTarget) {
            destSignalNavIdx = idx;
            parseNavDist(grayImage, ai::rEnv, *rows[idx], idx);
            return true;
        }
    }
    if (destSignalNavIdx == focusIdx)
        return true;

    st::autopilot.isDestDockFocused = false;
    st::autopilot.isDestBodyFocused = false;
    for (int retry=0; retry < 10; retry++) {
        startIdx = 0;
        if (destSignalNavIdx >= 0) {
            // found in current page, scroll page down
            int count = destSignalNavIdx - focusIdx;
            if (count > 0) {
                for (int i=0; i < count; i++)
                    kbd::send("UI_Down");
            } else {
                for (int i=0; i < -count; i++)
                    kbd::send("UI_Up");
            }
            startIdx = destSignalNavIdx;
        } else {
            // not found in current page, scroll page down
            int count = int(rows.size()) - focusIdx - 1;
            for (int i = 0; i < count; i++)
                kbd::send("UI_Down");
            int hold = 300 + 8*50;
            kbd::send("UI_Down", hold);
        }
        ai::sleep(300);
        destSignalNavIdx = -1;

        rows = initNavList(grayImage, focusIdx);
        if (rows.empty() || focusIdx < 0)
            continue;
        for (int idx = startIdx; !list[idx].parsed; nextIdx(idx, 1, list.size())) {
            parseNavRow(grayImage, ai::rEnv, *rows[idx], idx);
            guessNavItem(idx);
            if (list[idx].isTarget) {
                destSignalNavIdx = idx;
                parseNavDist(grayImage, ai::rEnv, *rows[idx], idx);
                return true;
            }
        }
        if (destSignalNavIdx == focusIdx)
            return true;
    }
    return false;
}

gal::spEntity NavList::focusNearestBody(dist_t* dist) {
    if (!focusTopEntry())
        return {};

    for (int retry=0; retry < 3; retry++) {
        int focusIdx;
        cv::Mat grayImage;
        std::vector<ClassifiedRect*> rows = initNavList(grayImage, focusIdx);
        if (rows.empty())
            continue;

        int nearestIdx = -1;
        for (int idx=focusIdx; idx < list.size(); idx++) {
            parseNavRow(grayImage, ai::rEnv, *rows[idx], idx);
            guessNavItem(idx);
            gal::spEntity item = list[idx].item;
            if ((item && isBody(item->type)) ||
                list[idx].icon == gal::STAR.charOCR ||
                list[idx].icon == gal::BODY.charOCR ||
                list[idx].icon == gal::LAND.charOCR)
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
            if ((item && !isBody(item->type)) || !(
                    list[idx].icon == gal::STAR.charOCR ||
                    list[idx].icon == gal::BODY.charOCR ||
                    list[idx].icon == gal::LAND.charOCR ||
                    list[idx].icon == gal::ERROR_MARK))
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
        if (nearestIdx == focusIdx) {
            parseNavDist(grayImage, ai::rEnv, *rows[nearestIdx], nearestIdx);
            if (list[nearestIdx].item.get() == st::autopilot.destBody.get()) {
                st::autopilot.isDestBodyFocused = true;
                if (list[nearestIdx].dist)
                    st::autopilot.distanceToBody = list[nearestIdx].dist;
            }
            if (dist)
                *dist = list[nearestIdx].dist;
            return list[nearestIdx].item;
        }
        if (nearestIdx >= 0) {
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
        gal::Entity* topItem = nullptr;
        utc_timer timer(10s);
        while (!timer.expired()) {
            ai::sleep(100);
            rows = initNavList(grayImage, focusIdx);
            if (rows.empty())
                continue;
            if (focusIdx != 0)
                continue;
            if (!parseNavRow(grayImage, ai::rEnv, *rows[0], 0))
                continue;
            guessNavItem(0);
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

        if (!parseNavRow(grayImage, ai::rEnv, *rows[focusIdx], focusIdx))
            continue;
        if (list[focusIdx].isTarget)
            return true;

        kbd::send("UI_Select");
        kbd::send("UI_Select");
        return true;
    }
    return false;
}

bool NavList::discoverSelected() {
    if (st::destination.systemAddress != gal::getCurrentStarSystem()->systemAddress)
        return false;
    for (int retry=0; retry < 3; retry++) {
        int focusIdx = -1;
        if (!focusDestination(focusIdx))
            continue;
        NavListEntry nle = list[focusIdx];

        kbd::send("UI_Select");

        std::string nav_icon;
        for (int retr=0; nav_icon.empty() && retr < 3; retr++){
            detect::NavPanelDetectLock lock("nav-line");
            ai::sleep(1000);
            cv::Mat grayImage;
            ai::detectEDState(DetectLevel::Buttons, nullptr, &grayImage);
            for (auto &cr: ai::rEnv.classified) {
                if (cr.cdt == ClsDetType::LineDetected && cr.text.starts_with("nav-line:")) {
                    nav_icon = cr.text.substr(7);
                    LOG(INFO) << "NavType icon: '" << nav_icon << "'";
                    break;
                }
            }
        }

        kbd::send("UI_Back", 0, 500);

        gal::getCurrentStarSystem()->addNavListEntry(nle.icon, nav_icon, st::destination.name, st::destination.bodyId);

        return true;
    }
    return false;
}

dist_t NavList::getFocusedDist(int max_try) {
    for (int retry=0; retry < max_try; retry++) {
        int focusIdx;
        cv::Mat grayImage;
        std::vector<ClassifiedRect*> rows = initNavList(grayImage, focusIdx);
        if (rows.empty())
            continue;
        if (parseNavDist(grayImage, ai::rEnv, *rows[focusIdx], focusIdx)) {
            if (list[focusIdx].dist) {
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


NavListScanTask::NavListScanTask(const TaskTemplate& templ_)
        : Task(templ_)
{
    assert (templ.id == ED_TASK_NAV_SCAN);
    for (auto& p : templ.params) {
        if (p.id == "travel")
            mTravel = p.as_boolean();
    }
}

bool NavListScanTask::run() {
    ai::setSpeed(0, true);
    st::NavPanelFilters savedFilters = st::navFilters;

    st::NavPanelFilters filters {};
    filters.star = true;
    filters.planetOrMoon = true;
    filters.landablePlanetOrMoon = true;
    filters.station = true;
    filters.fleetCarrier = true;
    filters.pointOfInterest = true;
    filters.settlement = true;

    if (!nl.init(filters))
        return false;

    if (!gotoNavPageNavigation())
        return false;
    if (!nl.focusTopEntry())
        return false;

    auto starSystem = gal::getCurrentStarSystem();
    if (!starSystem)
        throw_failed("Current star system not known");
    std::vector<gal::spEntity> scannedBodies;
    std::vector<gal::spEntity> scannedSites;
    gal::spEntity parentBody;
    for (;;) {
        int focusIdx;
        cv::Mat grayImage;
        std::vector<ClassifiedRect *> rows = nl.recognizeWholePage(grayImage, focusIdx);
        if (rows.empty())
            throw_failed("Cannot recognize nav list");

        gal::NavType* navType = nullptr;
        kbd::send("UI_Select", 0, 1500);
        ai::detectEDState(DetectLevel::Buttons, nullptr, &grayImage);
        for (auto& cr : ai::rEnv.classified) {
            if (cr.cdt == ClsDetType::LineDetected && cr.text.starts_with("nvline:")) {
                std::string lbl_anchor = cr.text.substr(7);
                LOG(INFO) << "NavType icon: '" << lbl_anchor << "'";
                for (auto nt : gal::ALL_NAV_TYPES) {
                    if (contains(nt->navIcons, lbl_anchor)) {
                        navType = nt;
                        LOG(INFO) << "NavType from icon: poi=" << enum_name<TypeNav>(navType->type);
                        break;
                    }
                }
                if (!navType)
                    throw_failed("Cannot recognize nav type from icon");
                break;
            }
        }
        kbd::send("UI_Select", 0, 500);
        //if (navType && navType)
        st::destination.name;
        st::destination.bodyId;

        kbd::send("UI_Back", 50, 1000);

        nl.selectFocused();

        for (int idx = 0; idx < nl.list.size(); idx++) {
            auto &nle = nl.list[idx];

            if (!nle.item || !(isBody(nle.item->type) || isSite(nle.item->type))) {
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
        if (!ai::uiState.match("scr-left-panel:mod-nav-list"))
            gotoNavPage("mod-nav-list");
        if (ai::uiState.focused_name() != "lst-bodies")
            kbd::send("UI_Right");
        ok = true;
    }
    return ok;
}

} // namespace ai