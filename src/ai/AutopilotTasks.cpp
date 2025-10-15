//
// Created by mkizub on 28.06.2025.
//

#include "../pch.h"

#include "Task.h"
#include "AIManager.h"
#include "AutopilotTasks.h"
#include "../OCR.h"
#include "../FuzzyMatch.h"
#include "../Galaxy.h"
#include "../ShipStats.h"
#include "../EDWidget.h"

using namespace std::chrono_literals;

namespace ai {

namespace {

int getNavPageIndex(const std::string &page_name) {
    int pageIndex = -1;
    if (page_name == "mod-sysinfo")
        pageIndex = 0;
    else if (page_name == "mod-navigation")
        pageIndex = 1;
    else if (page_name == "mod-transact")
        pageIndex = 2;
    else if (page_name == "mod-contacts")
        pageIndex = 3;
    else if (page_name == "mod-target")
        pageIndex = 4;
    else
        LOG(ERROR) << "Nav page '" << page_name << "' not known";
    return pageIndex;
}

bool gotoNavPage(Step *task, const std::string &page_name) {
    int targetPageIndex = getNavPageIndex(page_name);
    if (targetPageIndex < 0)
        task->task_return(Result::Failure);

    for (int i = 0; i < 6 && !task->mgr.uiState.match("scr-left-panel:" + page_name); i++) {
        task->mgr.detectEDState(DetectLevel::Screen);
        LOG(DEBUG) << "Goto '" << page_name << "'...";

        if (!task->mgr.uiState.match("scr-left-panel:*")) {
            LOG(DEBUG) << "FocusLeftPanel...";
            task->sendKey("FocusLeftPanel", 0, 1500);
            continue;
        }
        if (task->mgr.uiState.match("scr-left-panel:dlg-nav-select") ||
            task->mgr.uiState.match("scr-left-panel:dlg-filters")) {
            task->sendKey("UI_Back", 0, 500);
            continue;
        }
        std::vector<std::string> segments = task->mgr.uiState.splitPath();
        if (segments.size() < 2) {
            LOG(ERROR) << "Expecting 2 segments in " << task->mgr.uiState;
            task->task_return(Result::Failure);
        }
        int currentPageIndex = getNavPageIndex(segments[1]);
        if (currentPageIndex < 0)
            task->task_return(Result::Failure);
        int dist = currentPageIndex - targetPageIndex;
        if (dist >= 0) {
            for (int j = 0; j < dist; j++)
                task->sendKey("CycleNextPanel", 0, 250);
        } else {
            for (int j = 0; j < -dist; j++)
                task->sendKey("CyclePreviousPanel", 0, 250);
        }
    }
    if (!task->mgr.uiState.match("scr-left-panel:" + page_name))
        task->task_return(Result::Trouble);
    return true;
}

bool parseNavName(std::wstring text, NavListEntry &nle) {
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

bool parseNavRow(const cv::Mat &grayImage, const ResolvedEnv& rEnv, const ClassifiedRect &cr, NavListEntry &nle) {
    check_interrupted();
    nle = {};
    cv::Rect rectOut;
    std::string text;
    int ocr_conf = ocr::ocrRowText(grayImage, rEnv, cr, 0, text, &rectOut);
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

bool parseNavDist(const cv::Mat &grayImage, const ResolvedEnv& rEnv, const ClassifiedRect &cr, NavListEntry &nle) {
    check_interrupted();
    std::string dist;
    if (ocr::ocrRowText(grayImage, rEnv, cr, 1, dist) < 60)
        return false;
    std::wstring wdist = toUtf16(dist);
    nle.dist = parseDist(wdist);
    return nle.dist.valid();
}

bool parseFocusedNavRow(const cv::Mat& grayImage, const ResolvedEnv& rEnv, NavListEntry &nle) {
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

} // anonymouse namespace

bool NavList::init(Task* task, const gal::spSite& dock, const gal::spBody& body, st::NavPanelFilters filters) {
    this->task = task;
    this->destDock = dock;
    this->destBody = body;
    this->locationFilters = filters;
    this->focusIdx = -1;
    this->isDestDockFocused = false;
    this->isDestBodyFocused = false;
    this->isNearestBodyFocused = false;
    this->isTopEntryFocused = false;
    this->list.clear();

    for (int retry=0; retry < 3; retry++) {
        if (filters == st::navFilters)
            return true;

        gotoNavPage(task, "mod-navigation");

        int delay = 300;
        task->sendKey("UI_Left", 0, delay);
        for (int i = 0; i < 4; i++)
            task->sendKey("UI_Up", 0, delay);
        task->sendKey("UI_Select", 0, 1000);

        task->mgr.detectEDState(DetectLevel::Buttons);
        if (!task->mgr.uiState.match("scr-left-panel:dlg-filters")) {
            LOG(WARNING) << "TaskDock expecting 'scr-left-panel:dlg-filters' but got " << task->mgr.uiState;
            task->task_return(Result::Trouble);
        }
        // currently ED always opens filters at top position 'stars',
        // so just scroll down and select/deselect what we need
        if (filters.star != st::navFilters.star)
            task->sendKey("UI_Select");
        task->sendKey("UI_Down");

        if (filters.asteroidCluster != st::navFilters.asteroidCluster)
            task->sendKey("UI_Select");
        task->sendKey("UI_Down");

        if (filters.planetOrMoon != st::navFilters.planetOrMoon)
            task->sendKey("UI_Select");
        task->sendKey("UI_Down");

        if (filters.landablePlanetOrMoon != st::navFilters.landablePlanetOrMoon)
            task->sendKey("UI_Select");
        task->sendKey("UI_Down");

        if (filters.settlement != st::navFilters.settlement)
            task->sendKey("UI_Select");
        task->sendKey("UI_Down");

        if (filters.station != st::navFilters.station)
            task->sendKey("UI_Select");
        task->sendKey("UI_Down");

        if (filters.fleetCarrier != st::navFilters.fleetCarrier)
            task->sendKey("UI_Select");
        task->sendKey("UI_Down");

        if (filters.pointOfInterest != st::navFilters.pointOfInterest)
            task->sendKey("UI_Select");
        task->sendKey("UI_Down");

        if (filters.signalSource != st::navFilters.signalSource)
            task->sendKey("UI_Select");
        task->sendKey("UI_Down");

        if (filters.system != st::navFilters.system)
            task->sendKey("UI_Select");
        task->sendKey("UI_Down");

        task->sendKey("UI_Back", 0, 1000); // wait for configuration changes
        task->sendKey("UI_Right");
    }
    return (filters == st::navFilters);
}

std::vector<ClassifiedRect*> NavList::initNavList(cv::Mat& grayImage) {
    std::vector<ClassifiedRect*> rows;
    focusIdx = -1;
    list.clear();
    for (int retry=0; retry < 3; retry++) {
        if (!task->mgr.uiState.match("scr-left-panel:mod-navigation"))
            gotoNavPage(task, "mod-navigation");
        if (task->mgr.uiState.focused_name() != "lst-bodies")
            task->sendKey("UI_Right");
        if (!task->mgr.detectEDState(DetectLevel::ListRows, nullptr, &grayImage))
            continue;
        rows.clear();
        for (auto &cr: task->mgr.rEnv.classified) {
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
        task->sendKey("UI_Right");
    }
    if (focusIdx < 0 || focusIdx >= rows.size())
        return {};
    list[focusIdx].focused = true;
    return rows;
}

std::vector<ClassifiedRect*> NavList::recognizeWholePage(cv::Mat& grayImage) {
    std::vector<ClassifiedRect*> rows = initNavList(grayImage);
    for (int idx=0; idx < rows.size(); idx++) {
        parseNavRow(grayImage, task->mgr.rEnv, *rows[idx], list[idx]);
        list[idx].item = guessNavItem(list[idx]);
    }
    return rows;
}


gal::spItem NavList::guessNavItem(NavListEntry &nle) {
    gal::spStarSystem ss = gal::getCurrentStarSystem();
    gal::spItem bestItem;
    FuzzyMatch fm;
    double bestMatch = 0.5;
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
    if (!destDock)
        return false;
    if (isDestDockFocused && focusIdx >= 0 && focusIdx < list.size())
        return true;
    cv::Mat grayImage;
    std::vector<ClassifiedRect*> rows = initNavList(grayImage);
    if (rows.empty())
        return false;

    int startIdx = 0;
    if (isDestBodyFocused && focusIdx >= 0)
        startIdx = focusIdx;

    // try current page
    int destBodyNavIdx = -1;
    int destDockNavIdx = -1;
    for (int idx=startIdx; !list[idx].parsed; nextIdx(idx, 1, list.size())) {
        parseNavRow(grayImage, task->mgr.rEnv, *rows[idx], list[idx]);
        list[idx].item = guessNavItem(list[idx]);
        if (destBody && list[idx].item.get() == destBody.get()) {
            destBodyNavIdx = idx;
            parseNavDist(grayImage, task->mgr.rEnv, *rows[idx], list[idx]);
        }
        if (list[idx].item.get() == destDock.get()) {
            destDockNavIdx = idx;
            parseNavDist(grayImage, task->mgr.rEnv, *rows[idx], list[idx]);
            break;
        }
    }
    if (destDockNavIdx == focusIdx) {
        isDestDockFocused = true;
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
                    badBodyHierarchy = true;
                    break;
                }
            }
        }
    }

    isDestDockFocused = false;
    isDestBodyFocused = false;
    isNearestBodyFocused = false;
    isTopEntryFocused = false;
    for (int retry=0; retry < 10; retry++) {
        startIdx = 0;
        if (destDockNavIdx >= 0) {
            // found in current page, scroll page down
            int count = destDockNavIdx - focusIdx;
            if (count > 0) {
                for (int i=0; i < count; i++)
                    task->sendKey("UI_Down");
            } else {
                for (int i=0; i < -count; i++)
                    task->sendKey("UI_Up");
            }
            startIdx = destDockNavIdx;
        } else {
            // not found in current page, scroll page down
            int count = int(rows.size()) - focusIdx - 1;
            for (int i = 0; i < count; i++)
                task->sendKey("UI_Down");
            int hold = 300 + 8*50;
            task->sendKey("UI_Down", hold);
        }
        task->sleep(300);
        destDockNavIdx = -1;
        destBodyNavIdx = -1;

        rows = initNavList(grayImage);
        if (rows.empty() || focusIdx < 0)
            continue;
        for (int idx = startIdx; !list[idx].parsed; nextIdx(idx, 1, list.size())) {
            parseNavRow(grayImage, task->mgr.rEnv, *rows[idx], list[idx]);
            list[idx].item = guessNavItem(list[idx]);
            if (list[idx].item.get() == destDock.get()) {
                destDockNavIdx = idx;
                break;
            }
        }
        if (destDockNavIdx == focusIdx) {
            isDestDockFocused = true;
            return true;
        }
    }
    return false;
}

bool NavList::focusDestBody() {
    if (!destBody)
        return false;
    if (isDestBodyFocused && focusIdx >= 0 && focusIdx < list.size())
        return true;
    cv::Mat grayImage;
    std::vector<ClassifiedRect*> rows = initNavList(grayImage);
    if (rows.empty())
        return false;

    int startIdx = 0;
    int incr = 1;
    if (isDestDockFocused && focusIdx >= 0) {
        startIdx = focusIdx;
        incr = -1;
    }

    // try current page
    int destBodyNavIdx = -1;
    int destDockNavIdx = -1;
    for (int idx=startIdx; !list[idx].parsed; nextIdx(idx, incr, list.size())) {
        parseNavRow(grayImage, task->mgr.rEnv, *rows[idx], list[idx]);
        list[idx].item = guessNavItem(list[idx]);
        if (destBody && list[idx].item.get() == destBody.get()) {
            destBodyNavIdx = idx;
            parseNavDist(grayImage, task->mgr.rEnv, *rows[idx], list[idx]);
            break;
        }
        if (destDock && list[idx].item.get() == destDock.get()) {
            destDockNavIdx = idx;
            parseNavDist(grayImage, task->mgr.rEnv, *rows[idx], list[idx]);
        }
    }
    if (destBodyNavIdx == focusIdx) {
        isDestBodyFocused = true;
        return true;
    }
    if (fixupNavList()) {
        for (int idx=0; idx < list.size(); idx++) {
            if (list[idx].item.get() == destBody.get()) {
                destBodyNavIdx = idx;
                parseNavDist(grayImage, task->mgr.rEnv, *rows[idx], list[idx]);
                break;
            }
        }
        if (destBodyNavIdx == focusIdx) {
            isDestBodyFocused = true;
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
                    badBodyHierarchy = true;
                    break;
                }
            }
        }
    }

    isDestDockFocused = false;
    isDestBodyFocused = false;
    isNearestBodyFocused = false;
    isTopEntryFocused = false;
    for (int retry=0; retry < 10; retry++) {
        startIdx = 0;
        if (destBodyNavIdx >= 0) {
            // found in current page, scroll page down
            int count = destBodyNavIdx - focusIdx;
            if (count > 0) {
                for (int i=0; i < count; i++)
                    task->sendKey("UI_Down");
            } else {
                for (int i=0; i < -count; i++)
                    task->sendKey("UI_Up");
            }
            startIdx = destBodyNavIdx;
        } else {
            // not found in current page, scroll page down
            int count = int(rows.size()) - focusIdx - 1;
            for (int i = 0; i < count; i++)
                task->sendKey("UI_Down");
            int hold = 300 + 8*50;
            task->sendKey("UI_Down", hold);
        }
        task->sleep(300);
        destDockNavIdx = -1;
        destBodyNavIdx = -1;

        rows = initNavList(grayImage);
        if (rows.empty() || focusIdx < 0)
            continue;
        for (int idx = startIdx; !list[idx].parsed; nextIdx(idx, 1, list.size())) {
            parseNavRow(grayImage, task->mgr.rEnv, *rows[idx], list[idx]);
            list[idx].item = guessNavItem(list[idx]);
            if (list[idx].item.get() == destBody.get()) {
                destBodyNavIdx = idx;
                parseNavDist(grayImage, task->mgr.rEnv, *rows[idx], list[idx]);
            }
            if (destBodyNavIdx == focusIdx) {
                isDestBodyFocused = true;
                return true;
            }
        }
        if (fixupNavList()) {
            for (int idx=0; idx < list.size(); idx++) {
                if (list[idx].item.get() == destBody.get()) {
                    destBodyNavIdx = idx;
                    parseNavDist(grayImage, task->mgr.rEnv, *rows[idx], list[idx]);
                    break;
                }
            }
            if (destBodyNavIdx == focusIdx) {
                isDestBodyFocused = true;
                return true;
            }
        }
    }
    return false;
}

bool NavList::focusNearestBody() {
    if (isNearestBodyFocused && focusIdx == 0 && focusIdx < list.size())
        return true;

    if (!focusTopEntry())
        return false;

    for (int retry=0; retry < 3; retry++) {
        cv::Mat grayImage;
        std::vector<ClassifiedRect*> rows = initNavList(grayImage);
        if (rows.empty())
            continue;

        int nearestIdx = -1;
        for (int idx=0; idx < list.size(); idx++) {
            parseNavRow(grayImage, task->mgr.rEnv, *rows[idx], list[idx]);
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
            if (nearestIdx >= 0)
                break;
        }
        if (focusIdx >= 0 && nearestIdx == focusIdx) {
            if (parseNavDist(grayImage, task->mgr.rEnv, *rows[nearestIdx], list[nearestIdx])) {
                isNearestBodyFocused = true;
                return true;
            }
        }
        if (nearestIdx > 0) {
            int count = nearestIdx - focusIdx;
            for (int i = 0; i < count; i++)
                task->sendKey("UI_Down");
            task->sleep(300);
        }
    }
    return false;
}

bool NavList::focusTopEntry() {
    if (isTopEntryFocused && focusIdx == 0 && focusIdx < list.size())
        return true;

    isDestDockFocused = false;
    isDestBodyFocused = false;
    isNearestBodyFocused = false;
    isTopEntryFocused = false;
    for (int retry=0; retry < 3; retry++) {
        cv::Mat grayImage;
        std::vector<ClassifiedRect*> rows = initNavList(grayImage);
        if (rows.empty())
            continue;

        if (focusIdx == 0)
            task->sendKey("UI_Down");
        unsigned handle = task->holdKeyDown("UI_Up", 10000);
        if (!handle)
            return false;
        task->sleep(300);
        gal::Item* topItem = nullptr;
        utc_timer timer(10s);
        while (!timer.expired()) {
            task->sleep(100);
            rows = initNavList(grayImage);
            if (rows.empty())
                continue;
            if (focusIdx != 0)
                continue;
            if (!parseNavRow(grayImage, task->mgr.rEnv, *rows[0], list[0]))
                continue;
            list[0].item = guessNavItem(list[0]);
            if (!list[0].item)
                continue;
            if (topItem != list[0].item.get()) {
                topItem = list[0].item.get();
                continue;
            }
            task->releaseKey(handle);
            isTopEntryFocused = true;
            return true;
        }
        task->releaseKey(handle);
    }
    return false;
}

bool NavList::selectFocused() {
    for (int retry=0; retry < 3; retry++) {
        cv::Mat grayImage;
        std::vector<ClassifiedRect *> rows = initNavList(grayImage);
        if (rows.empty())
            continue;

        if (!parseNavRow(grayImage, task->mgr.rEnv, *rows[focusIdx], list[focusIdx]))
            continue;
        if (list[focusIdx].isTarget)
            return true;

        task->sendKey("UI_Select");
        task->sendKey("UI_Select");
        return true;
    }
    return false;
}

dist_t NavList::getFocusedDist() {
    for (int retry=0; retry < 3; retry++) {
        cv::Mat grayImage;
        std::vector<ClassifiedRect *> rows = initNavList(grayImage);
        if (rows.empty())
            continue;

        if (parseNavDist(grayImage, task->mgr.rEnv, *rows[focusIdx], list[focusIdx]))
            return list[focusIdx].dist;
    }
    return {};
}

BaseAutopilotTask::BaseAutopilotTask(Task* parent, AIManager& mgr, const TaskTemplate& templ)
    : Task(parent, mgr, templ)
{}

BaseAutopilotStep::BaseAutopilotStep(Step* parent)
    : Step(parent, parent->mgr)
{
    task = dynamic_cast<BaseAutopilotTask*>(getTask());
    if (!task) {
        LOG(ERROR) << "BaseAutopilotStep needs BaseAutopilotTask";
        throw std::runtime_error("BaseAutopilotStep needs BaseAutopilotTask");
    }
}


void BaseAutopilotTask::relogin() {
    // something is really wrong, logout and login again
    notifyProgress("Something is wrong with departure, trying to re-login");
    sendKey("Pause", 0, 1000);
    sendKey("UI_Up", 0, 100); // go to Exit button
    sendKey("UI_Select", 0, 1000); // logout
    sendKey("UI_Select", 0, 8000); // logout to main menu
    notifyProgress("Login to Solo...");
    sendKey("UI_Select", 0, 3000); // login, select mode screen
    sendKey("UI_Right", 0, 100);
    sendKey("UI_Right", 0, 500);  // choose Solo
    sendKey("UI_Select", 0, 12000); // login
    notifyError("Finished re-login", Result::Trouble);
}

bool BaseAutopilotTask::getFocusedNavRow(NavListEntry& nle) {
    if (!mgr.uiState.match("scr-left-panel:mod-navigation"))
        gotoNavPage(this, "mod-navigation");

    cv::Mat grayImage;
    double dist_km = 10;
    for (int cnt=0; cnt < 3; cnt++) {
        mgr.detectEDState(DetectLevel::ListRows, nullptr, &grayImage);
        if (mgr.uiState.focused_name() != "lst-bodies") {
            sendKey("UI_Right", 0, 500);
            continue;
        }
        if (!parseFocusedNavRow(grayImage, mgr.rEnv, nle)) {
            LOG(INFO) << "Failed to parse nav row";
            continue;
        }
        return true;
    }
    return false;
}

bool BaseAutopilotTask::setSpeed(int percents) {
    percents = std::clamp(percents, -100, +100);
    switch (percents / 25) {
    case 4:
        sendKey("SetSpeed100", 50);
        speed_set_to = 100;
        break;
    case 3:
        sendKey("SetSpeed75", 50);
        speed_set_to = 75;
        break;
    case 2:
        sendKey("SetSpeed50", 50);
        speed_set_to = 50;
        break;
    case 1:
        sendKey("SetSpeed25", 50);
        speed_set_to = 25;
        break;
    case 0:
        sendKey("SetSpeedZero", 50);
        speed_set_to = 0;
        break;
    case -1:
        sendKey("SetSpeedMinus25", 50);
        speed_set_to = -25;
        break;
    case -2:
        sendKey("SetSpeedMinus50", 50);
        speed_set_to = -50;
        break;
    case -3:
        sendKey("SetSpeedMinus75", 50);
        speed_set_to = -75;
        break;
    case -4:
        sendKey("SetSpeedMinus100", 50);
        speed_set_to = -100;
        break;
    }
    return true;
}

// angle1 = duration1/1000*speed+3
// 1000*(angle1-3)/speed = duration1
// duration1 = 1000*(angle1-3)/speed
//
// angle2 = (duration2-150)/1000*(2*speed+3)
// (1000*angle2)/(2*speed+3) = (duration2-150)
// duration2 = 150 + (1000*angle2)/(2*speed+3)


static int getDuration(double angle, double speed) {
    angle = std::abs(angle);
    if (angle < 0.7)
        return 100;
    int duration1 = 1000*(angle-3)/speed;
    int duration2 = 150 + 1000*angle/(2*speed+3);
    int duration = std::max(duration1, duration2);
    return duration;
}

void BaseAutopilotTask::orientRollStep(double requiredRoll, int max_time_ms) {
    float delta = mgr.compassInfo.targetRoll - requiredRoll;
    if (delta > 180) delta = 360-delta;
    if (delta < -180) delta = 360+delta;
    ShipStats shipStats(Cfg.getShipType());
    double speed = shipStats.getRoll(speed_set_to);
    const KeyBindings& bind = Cfg.getGameKeyBindings("RollAxisRaw");
    if ((bind.mode == KeyBindings::Axis || bind.mode == KeyBindings::AxisInv) && bind.primary.device == GameKey::vJoy) {
        double value = delta / speed;
        int duration, pause;
        if (std::abs(value) >= 1) {
            duration = std::min(max_time_ms, int(1000 * std::abs(value)));
            pause = 10*speed;
            value = value > 0 ? +1.0 : -1.0;
        } else {
            duration = 1000;
            pause = 1000 / (25+std::abs(value));
        }
        notifyProgress(std::format("Orientation: fix roll {} (joystick) hold {}ms", delta, duration));
        sendAxis(bind, value);
        sleep(duration);
        sendAxis(bind, 0);
        sleep(pause);
    } else {
        int duration = std::min(max_time_ms, getDuration(delta, speed));
        int pause = duration < 1000 ? duration : 1000;
        notifyProgress(std::format("Orientation: fix roll {} (button) hold {}ms", delta, duration));
        if (delta > 0)
            sendKey("RollRightButton", duration, pause);
        else
            sendKey("RollLeftButton", duration, pause);
    }
}

void BaseAutopilotTask::orientPitchStep(double requiredPitch, int max_time_ms) {
    float delta = mgr.compassInfo.targetPitch - requiredPitch;
    if (delta > 180) delta = 360-delta;
    if (delta < -180) delta = 360+delta;
    ShipStats shipStats(Cfg.getShipType());
    double speed = shipStats.getPitch(speed_set_to);
    const KeyBindings& bind = Cfg.getGameKeyBindings("PitchAxisRaw");
    if ((bind.mode == KeyBindings::Axis || bind.mode == KeyBindings::AxisInv) && bind.primary.device == GameKey::vJoy) {
        double value = -delta / speed;
        int duration, pause;
        if (std::abs(value) >= 1) {
            duration = std::min(max_time_ms, int(1000 * std::abs(value)));
            pause = 10*speed;
            value = value > 0 ? +1.0 : -1.0;
        } else {
            duration = 1000;
            pause = 1000 / (25+std::abs(value));
        }
        notifyProgress(std::format("Orientation: fix pitch {} (joystick) hold {}ms", delta, duration));
        sendAxis(bind, value);
        sleep(duration);
        sendAxis(bind, 0);
        sleep(pause);
    } else {
        int duration = std::min(max_time_ms, getDuration(delta, speed));
        int pause = duration < 1000 ? duration : 1000;
        notifyProgress(std::format("Orientation: fix pitch {} (button) hold {}ms", delta, duration));
        if (delta > 0)
            sendKey("PitchUpButton", duration, pause);
        else
            sendKey("PitchDownButton", duration, pause);
    }
}

void BaseAutopilotTask::orientYawStep(double requiredYaw, int max_time_ms) {
    float delta = mgr.compassInfo.targetYaw - requiredYaw;
    if (delta > 180) delta = 360-delta;
    if (delta < -180) delta = 360+delta;
    ShipStats shipStats(Cfg.getShipType());
    double speed = shipStats.getYaw(speed_set_to);
    const KeyBindings& bind = Cfg.getGameKeyBindings("YawAxisRaw");
    if ((bind.mode == KeyBindings::Axis || bind.mode == KeyBindings::AxisInv) && bind.primary.device == GameKey::vJoy) {
        double value = delta / speed;
        int duration, pause;
        if (std::abs(value) >= 1) {
            duration = std::min(max_time_ms, int(1000 * std::abs(value)));
            pause = 10*speed;
            value = value > 0 ? +1.0 : -1.0;
        } else {
            duration = 1000;
            pause = 1000 / (25+std::abs(value));
        }
        notifyProgress(std::format("Orientation: fix yaw {} (joystick)", delta, duration));
        sendAxis(bind, value);
        sleep(duration);
        sendAxis(bind, 0);
        sleep(pause);
    } else {
        int duration = std::min(max_time_ms, getDuration(delta, speed));
        int pause = duration < 1000 ? duration : 1000;
        notifyProgress(std::format("Orientation: fix yaw {} (button) hold {}ms", delta, duration));
        if (delta > 0)
            sendKey("YawRightButton", duration, pause);
        else
            sendKey("YawLeftButton", duration, pause);
    }
}

bool BaseAutopilotTask::orientTowardTargetStep(double precision, int max_time_ms) {
    if (!mgr.compassInfo.has_nav_target)
        precision = std::max(3.0, precision);
    bool front = mgr.compassInfo.hemisphere > 0;
    int hemiYaw = mgr.compassInfo.targetYaw;
    if (!front) {
        if (hemiYaw > 0)
            hemiYaw = 180 - hemiYaw;
        else
            hemiYaw = -180 - hemiYaw;
    }
    if (std::abs(hemiYaw) > 20) {
        float roll = mgr.compassInfo.targetRoll;
        orientRollStep(0, max_time_ms);
        return false;
    }

    ShipStats shipStats(Cfg.getShipType());
    double pitchSpd = shipStats.getPitch(speed_set_to);
    double yawSpd = shipStats.getYaw(speed_set_to);
    float pitch = mgr.compassInfo.targetPitch;
    int p_duration = std::min(5000, getDuration(pitch, pitchSpd));
    float yaw = mgr.compassInfo.targetYaw;
    int y_duration = std::min(5000, getDuration(yaw, yawSpd));

    if (std::abs(pitch) > precision && (p_duration >= y_duration || std::abs(yaw) < precision)) {
        orientPitchStep(0, max_time_ms);
        return false;
    }

    if (std::abs(yaw) > precision) {
        orientYawStep(0, max_time_ms);
        return false;
    }
    return true;
}

bool BaseAutopilotTask::orientTowardTarget(double precision) {
    if (mgr.uiState.guiFocus != GuiFocus::None) {
        notifyProgress("Orientation: goto compass");
        sendKey("UI_Back", 0, 1500);
    }
    for (int fails=0; fails < 10; fails++) {
        if (fails > 2) {
            setSpeed(0);
            continue;
        }
        mgr.detectEDState(DetectLevel::Screen);
        if (mgr.uiState.guiFocus != GuiFocus::None) {
            notifyProgress(std::format("Unexpected ui mode {}", mgr.uiState.to_string()));
            LOG(WARNING) << "Unexpected ui mode " << mgr.uiState;
            sendKey("UI_Back", 0, 1500);
            continue;
        }
        if (!mgr.compassInfo.hemisphere) {
            notifyProgress(std::format("Compass not detected, fails {}", fails));
            LOG(WARNING) << "Compass not detected";
            sendKey("RollRightButton", 800, 500);
            continue;
        }
        fails = 0;
        if (orientTowardTargetStep(precision))
            return true;
    }
    LOG(ERROR) << "Compass not detected";
    return false;
}

bool BaseAutopilotTask::orientAwayFromTargetStep(double precision) {
    precision = std::max(5.0, precision);
    bool front = mgr.compassInfo.hemisphere > 0;
    int hemiYaw = mgr.compassInfo.targetYaw;
    if (!front) {
        if (hemiYaw > 0)
            hemiYaw = 180 - hemiYaw;
        else
            hemiYaw = -180 - hemiYaw;
    }
    if (std::abs(hemiYaw) > 20) {
        float roll = mgr.compassInfo.targetRoll;
        orientRollStep(0);
        return false;
    }

    ShipStats shipStats(Cfg.getShipType());
    double pitchSpd = shipStats.getPitch(speed_set_to);
    double yawSpd = shipStats.getYaw(speed_set_to);
    float pitch = mgr.compassInfo.targetPitch;
    int p_duration = std::min(5000, getDuration(pitch, pitchSpd));
    float yaw = mgr.compassInfo.targetYaw;
    int y_duration = std::min(5000, getDuration(yaw, yawSpd));

    if (180-std::abs(pitch) > precision) {
        orientPitchStep(180);
        return false;
    }

    if (180-std::abs(yaw) > precision) {
        orientYawStep(180);
        return false;
    }
    return true;
}

bool BaseAutopilotTask::orientAwayFromTarget(double precision) {
    if (mgr.uiState.guiFocus != GuiFocus::None) {
        notifyProgress("Orientation: goto compass");
        sendKey("UI_Back", 0, 1500);
    }
    int speedDropped = 0;
    for (int fails=0; fails < 10; fails++) {
        if (fails > 2) {
            setSpeed(0);
            continue;
        }
        mgr.detectEDState(DetectLevel::Screen);
        if (mgr.uiState.guiFocus != GuiFocus::None) {
            notifyProgress(std::format("Unexpected ui mode {}", mgr.uiState.to_string()));
            LOG(WARNING) << "Unexpected ui mode " << mgr.uiState;
            sendKey("UI_Back", 0, 1500);
            continue;
        }
        if (!mgr.compassInfo.hemisphere) {
            notifyProgress(std::format("Compass not detected, fails {}", fails));
            LOG(WARNING) << "Compass not detected";
            sendKey("RollRightButton", 800, 500);
            continue;
        }
        fails = 0;
        if (orientAwayFromTargetStep(precision))
            return true;
    }
    LOG(ERROR) << "Compass not detected";
    return false;
}

TaskDebugAutopilot::TaskDebugAutopilot(ai::Task *parent, ai::AIManager &mgr, const ai::TaskTemplate &templ)
        : BaseAutopilotTask(parent, mgr, templ)
{
    assert (templ.name == ED_TASK_DEBUG_AUTOPILOT);
    for (auto& p : templ.params) {
        if (p.name == "test")
            test = std::get<std::string>(p.value);
        if (p.name == "target")
            target = std::get<std::string>(p.value);
        if (p.name == "precision")
            orient_precision = std::get<double>(p.value);
    }
}


Result TaskDebugAutopilot::run() {
    auto starSystem = gal::getCurrentStarSystem();
    if (target.empty())
        target = Cfg.getCurrentStatus()->destinationName;
    destDock = starSystem->getDock(target);
    if (destDock) {
        auto body = starSystem->getBodyById(destDock->parentBodyId);
        destBody = std::dynamic_pointer_cast<gal::Body>(body);
    } else {
        destBody = starSystem->getBody(target);
    }

    st::NavPanelFilters filters = {};
    filters.star = true;
    filters.planetOrMoon = true;
    filters.landablePlanetOrMoon = true;
    filters.station = true;
    if (destDock) {
        if (destDock->typeNav == gal::TypeNav::SpacePort)
            filters.station = true;
        if (destDock->typeNav == gal::TypeNav::Carrier)
            filters.fleetCarrier = true;
        if (destDock->typeNav == gal::TypeNav::MegashipDock) {
            if (destDock->typeSite == gal::TypeSite::TrailblazerDream)
                filters.pointOfInterest = true;
            else
                filters.station = true;
        }
    }

    nl.init(this, destDock, destBody, filters);

    setSpeed(0);

    if (test == "OrientTowards") {
        setSpeed(50);
        orientTowardTarget(orient_precision);
        setSpeed(0);
    }
    else if (test == "OrientAway") {
        setSpeed(50);
        orientAwayFromTarget(orient_precision);
    }
    else if (test == "Departure") {
        run_sub_step(spStep(new DepartureStep(this)));
    }
    else if (test == "Dock") {
        run_sub_step(spStep(new DockStep(this)));
    }
    else if (test == "EnterCruise") {
        run_sub_step(spStep(new EnterCruiseStep(this)));
    }
    else if (test == "FocusDestDock") {
        nl.focusDestDock();
    }
    else if (test == "FocusDestBody") {
        nl.focusDestBody();
    }
    else if (test == "FocusNearestBody") {
        nl.focusNearestBody();
    }
    else if (test == "FocusTopEntry") {
        nl.focusTopEntry();
    }
    else if (test == "NavDockSelect") {
        run_sub_step(spStep(new NavDockSelect(this, destDock)));
    }
    else if (test == "NavBodySelect") {
        run_sub_step(spStep(new NavBodySelect(this, destBody)));
    }
    else if (test == "DockAndBodyDist") {
        run_sub_step(spStep(new DockAndBodyDist(this)));
        LOG(INFO) << "Dist to dock: " << distanceToDock;
        LOG(INFO) << "Dist to body: " << distanceToBody;
    }
    else if (test == "CruiseToDist") {
        double dist_km = 15000;
        if (destBody && destBody->radius > 0)
            dist_km = destBody->radius * 50;
        run_sub_step(spStep(new CruiseToDistStep(this, dist_km)));
    }
    else if (test == "DiveUnderPlanet") {
        run_sub_step(spStep(new DiveUnderPlanetStep(this)));
    }
    else if (test == "ExitCruiseToStation") {
        run_sub_step(spStep(new ExitCruiseToStationStep(this)));
    }
    return Result::Success;
}


DepartureStep::DepartureStep(ai::Step *parent)
    : BaseAutopilotStep(parent)
{
}

bool DepartureStep::step() {
    bool fromSpaceConstruction = false; // need UpThrustButton
    if (st::dockedAt.stationType == "SpaceConstructionDepot") {
        fromSpaceConstruction = true;
    }

    auto& ss = mgr.cfg.getCurrentStatus();

    if (ss->flags.docked) {
        if (mgr.cfg.getGuiFocus() != GuiFocus::None) {
            LOG(INFO) << "Going to dock...";
            status = GOING_TO_DOCK;
            for (int i = 0; i < 10 && mgr.cfg.getGuiFocus() != GuiFocus::None; i++) {
                sendKey("UI_Back", 0, 1000);
                mgr.detectEDState(DetectLevel::Screen);
            }
        }
        mgr.detectEDState(DetectLevel::Screen);
        if (mgr.cfg.getGuiFocus() != GuiFocus::None)
            return false;

        LOG(INFO) << "Refuel...";
        status = REFUEL;
        for (int i = 0; i < 4; i++)
            sendKey("UI_Up");
        sendKey("UI_Select", 0, 500); // refuel
        sendKey("UI_Right");
        sendKey("UI_Select", 0, 500); // repair
        sendKey("UI_Right");
        sendKey("UI_Select", 0, 500); // rearm
        sendKey("UI_Down");
        sendKey("UI_Down");
        sendKey("UI_Select");

        LOG(INFO) << "Takeoff...";
        // 20 seconds to leave landing pad
        timer = utc_timer(20s);
        status = TAKEOFF;
        while (ss->flags.docked && !timer.expired()) {
            sleep(1000);
            mgr.detectEDState(DetectLevel::Screen);
            if (mgr.uiState.autopilot)
                break;
        }
        if (ss->flags.docked && !mgr.uiState.autopilot)
            return false;
    }
    if (!mgr.uiState.autopilot) {
        LOG(INFO) << "Departure autopilot waiting...";
        // 15 seconds wait autopilot
        timer = utc_timer(15s);
        status = WAIT_AUTOPILOT;
        // wait at least 15 seconds for autopilot to departure
        while (!mgr.uiState.autopilot && !timer.expired()) {
            sleep(250);
            mgr.detectEDState(DetectLevel::Screen);
        }
    }
    // 4 minutes for departure
    timer = utc_timer(4min);
    status = AUTOPILOT;
    notAutoPilotCounter = 0;
    int logCounter = 0;
    for (;;) {
        if (timer.expired()) {
            LOG(ERROR) << "Autopilot time expired";
            status = RELOGIN;
            task->relogin();
            return false;
        }
        sleep(250);
        mgr.detectEDState(DetectLevel::Screen);
        if (mgr.uiState.autopilot) {
            LOG(INFO) << "Still in auto-pilot...";
            notAutoPilotCounter = 0;
            continue;
        }
        if (++notAutoPilotCounter > 4) {
            LOG(INFO) << "Departure complete (autopilot off)";
            break;
        } else {
            LOG(INFO) << "Auto-pilot off counter: " << notAutoPilotCounter;
        }
    }

    timer = utc_timer(1min);
    status = MASSLOCKED;
    if (fromSpaceConstruction) {
        task->setSpeed(0);
        sendKey("UpThrustButton", 15000);
    }
    task->setSpeed(100);
    while (mgr.cfg.getCurrentStatus()->flags.fsd_masslocked) {
        LOG(INFO) << "Mass-locked, flying away";
        sleep(1000);
    }
    LOG(INFO) << "Ready to jump, flying away";
    timer = utc_timer(15s);
    status = FLYAWAY;
    sleep(10000);
    task->setSpeed(50);
    return true;
}

std::string DepartureStep::getStatus() {
    switch (status) {
    case READY:
        return "Ready";
    case GOING_TO_DOCK:
        return "Going to dock";
    case REFUEL:
        return "Refuel";
    case TAKEOFF:
        return std::format("Takeoff: {}s", timer.left());
    case WAIT_AUTOPILOT:
        return std::format("Wait for autopilot: {}s", timer.left());
    case AUTOPILOT:
        if (notAutoPilotCounter > 0)
            return std::format("Autopilot exiting: {}", notAutoPilotCounter);
        else
            return std::format("Autopilot: {}", timer.left());
    case MASSLOCKED:
        return std::format("Mass-locked: {}", timer.passed());
    case FLYAWAY:
        return std::format("Fly away: {}", timer.passed());
    case RELOGIN:
        return "Re-login";
    default:
        return "----";
    }
}


bool EnterCruiseStep::step() {
    if (Cfg.getCurrentStatus()->flags.cruise)
        return true;

    status = LOCK_BODY;
    if (task->nl.focusNearestBody()) {
        auto nle = task->nl.getFocusedEntry();
        gal::spBody body = std::dynamic_pointer_cast<gal::Body>(nle->item);
        if (body && body->radius > 0 && nle->dist.valid()) {
            if (nle->dist.get(dist_t::KM) / body->radius < 4) {
                task->nl.selectFocused();
                status = ORIENT;
                sendKey("UI_Back", 0, 500);
                task->orientAwayFromTarget(10);
            }
        }
    }
    sendKey("UI_Back");

    const auto& ss = mgr.cfg.getCurrentStatus();
    task->setSpeed(100);
    sleep(500);
    while (ss->flags.fsd_masslocked) {
        timer = utc_timer(60s);
        status = MASSLOCKED;
        //notifyProgress("Mass-locked, flying away");
        sleep(1000);
    }

    if (ss->flags.cargo_scoop_on || ss->flags.weapon_on) {
        status = PREPARE;
        if (ss->flags.cargo_scoop_on)
            sendKey("ToggleCargoScoop");
        if (ss->flags.weapon_on)
            sendKey("DeployHardpointToggle");
        sleep(1000);
    }

    while (ss->flags.fsd_cooldown) {
        timer = utc_timer(20s);
        status = FSD_COOLDOWN;
        sleep(1000);
    }

    timer = utc_timer(20s);
    status = ENTER_CRUISE;
    //notifyProgress("Entering supercruise");
    sendKey("Supercruise", 100, 1000);
    if (!(ss->flags.fsd_charging || ss->flags.fsd_jump)) {
        parent->notifyProgress("Entering supercruise failed");
        return false;
    }

    while (!ss->flags.cruise && (ss->flags.fsd_charging || ss->flags.fsd_jump) && !timer.expired()) {
        mgr.detectEDState(DetectLevel::Screen);
        if (mgr.compassInfo.hemisphere > 0) {
            if (task->orientTowardTargetStep(3))
                sleep(500);
        }
    }

    if (!ss->flags.cruise) {
        parent->notifyProgress("Entering supercruise failed");
        return false;
    }

    if (task->nl.focusDestDock() || task->nl.focusDestBody())
        task->nl.selectFocused();

    return true;
}

std::string EnterCruiseStep::getStatus() {
    switch (status) {
    case READY:
        return "Ready";
    case LOCK_BODY:
        return "Locking body";
    case LOCK_TARGET:
        return "Locking target";
    case ORIENT:
        return "Orienting";
    case MASSLOCKED:
        return std::format("Mass-locked: {}", timer.passed());
    case PREPARE:
        return std::format("Preparing");
    case FSD_COOLDOWN:
        return std::format("FSD Cooldown: {}", timer.passed());
    case ENTER_CRUISE:
        return std::format("Entering cruise: {}", timer.left());
    default:
        return "----";
    }
}

bool DockStep::step() {
    auto& ss = mgr.cfg.getCurrentStatus();
    if (ss->flags.cruise) {
        LOG(ERROR) << "Docking not possible in super-cruise mode: " << *ss;
        task_return(Result::Trouble);
    }
    if (ss->flags.docked) {
        LOG(INFO) << "Docking - already docked:" << *ss;
        return true;
    }
    mgr.detectEDState(DetectLevel::Screen);
    if (mgr.uiState.autopilot) {
        LOG(ERROR) << "Docking request while autopilot is active";
        return false;
    }

    task->setSpeed(0);

    status = PREPARE;
    // leave all UI panels
    if (mgr.cfg.getGuiFocus() != GuiFocus::None) {
        for (int cnt = 0; cnt < 3; cnt++) {
            if (mgr.cfg.getGuiFocus() == GuiFocus::None)
                break;
            sendKey("UI_Back", 0, 1500);
            mgr.detectEDState(DetectLevel::Screen);
        }
        if (mgr.cfg.getGuiFocus() != GuiFocus::None)
            task_return(Result::Trouble);
    }

    if (!task->nl.focusDestDock())
        task_return(Result::Trouble);
    if (!task->nl.selectFocused())
        task_return(Result::Trouble);
    if (!task->orientTowardTarget(5))
        task_return(Result::Trouble);

    // clear expired docking event
    auto de = mgr.cfg.dockingEvent;
    if (de) {
        if ((de->timestamp - std::chrono::utc_clock::now()) > 15min) {
            mgr.cfg.dockingEvent.reset();
            de.reset();
        }
    }
    // try to dock, retry if something goes wrong
    for (int cnt=0; cnt < 10; cnt++) {
        de = mgr.cfg.dockingEvent;
        // end loop if we granted to tock
        if (de && (de->event == "DockingGranted" || de->event == "Docked"))
            break;
        // if we are close enough (or don't know the distance) - request docking permit
        if (!task->distanceToDock.valid() || task->distanceToDock.get(dist_t::KM) >= 7.5) {
            flyTowardsTarget();
            continue;
        }
        de = requestDockingPermit();
        LOG(INFO) << "Docking status: " << (de ? de->event : "null");
        if (de && (de->event == "DockingGranted" || de->event == "Docked"))
            break;
        if (!de || de->event == "DockingRequested") {
            // need to wait a bit
            sleep(2000);
            continue;
        }
        if (de->event == "DockingCancelled") {
            // oops, we canceled docking, try again
            sleep(2000);
            continue;
        }
        if (de->event == "DockingTimeout") {
            // have not completed docking in time, try docking again
            continue;
        }
        // NoSpace, TooLarge, Hostile, Offences, Distance, ActiveFighter, NoReason, etc.
        if (de->event == "DockingDenied") {
            auto reason = de->data["Reason"].as_string();
            if (reason == "NoSpace") {
                LOG(ERROR) << "DockingDenied reason: NoSpace, waiting...";
                sleep(5000);
                cnt = 0;
                continue;
            }
            if (reason == "Distance") {
                LOG(ERROR) << "DockingDenied reason: Distance, flying towards station...";
                // need to get close
                flyTowardsTarget();
                continue;
            }
        }
        // all others are fatal
        LOG(ERROR) << "Unknown docking event: " << de->data;
        task_return(Result::Failure);
    }
    if (ss->flags.docked || (de && de->event == "Docked")) {
        LOG(ERROR) << "Docking - already docked:" << *ss;
        return true;
    }
    if (!de || de->event != "DockingGranted") {
        LOG(ERROR) << "Docking not granted";
        return false;
    }

    status = AUTOPILOT;
    task->setSpeed(0); // set speed to 0 to start autopilot
    sendKey("UI_Back", 0, 1500);

    // 8 minutes for docking
    timer = utc_timer(8min);
    // wait at least 5 seconds for autopilot to start docking
    for (int i=0; i < 40; i++) {
        sleep(250);
        mgr.detectEDState(DetectLevel::Screen);
        if (mgr.uiState.autopilot) {
            LOG(INFO) << "Docking autopilot started";
            break;
        }
        LOG(INFO) << "Docking autopilot waiting...";
    }
    int notAutoPilotCounter = 0;
    for (;;) {
        if (timer.expired()) {
            LOG(ERROR) << "Autopilot time expired";
            task->relogin();
        }
        sleep(250);
        mgr.detectEDState(DetectLevel::Screen);
        if (ss->flags.docked || (mgr.cfg.dockingEvent && mgr.cfg.dockingEvent->event == "Docked")) {
            LOG(INFO) << "Docking complete, status docked: " << ss->flags.docked
                      << ", docking event: " << (mgr.cfg.dockingEvent ? mgr.cfg.dockingEvent->event : "null");
            break;
        }
        if (!mgr.cfg.dockingEvent || mgr.cfg.dockingEvent->event != "DockingGranted") {
            LOG(ERROR) << "Docking permission revoked, docking event: " << (mgr.cfg.dockingEvent ? mgr.cfg.dockingEvent->event : "null");
            return false;
        }
    }

    return true;
}

spGameEvent DockStep::requestDockingPermit() {
    status = REQUEST;
    for (int retry=0; retry < 3; retry++) {
        task->setSpeed(0);
        gotoNavPage(this, "mod-contacts");

        if (mgr.uiState.focused_name() != "btn-landing") {
            bool have_btn_landing = false;
            for (auto& cr : mgr.rEnv.classified) {
                if (cr.cdt == ClsDetType::Widget && cr.text == "btn-landing") {
                    have_btn_landing = true;
                    break;
                }
            }
            if (!have_btn_landing) {
                sendKey("UI_Down");
                sendKey("UI_Up", 1500);
            }
            sendKey("UI_Right");
        }

        LOG(INFO) << "TaskDock requesting landing permission";
        mgr.cfg.dockingEvent.reset();
        // poll for docking event
        timer = utc_timer(5s);
        sendKey("UI_Right");
        sendKey("UI_Select");
        sendKey("UI_Select");
        while (!timer.expired()) {
            auto de = mgr.cfg.dockingEvent;
            if (!de) {
                sleep(250);
                continue;
            }
            if (de->event == "DockingRequested") {
                sleep(250);
                continue;
            }
            return de;
        }
    }
    return {};
}

bool DockStep::flyTowardsTarget() {
    status = APPROACH;
    task->setSpeed(0);
    int compassTry = 5;
    for (int fails=0; fails < 10; fails++) {
        if (compassTry <= 0) {
            compassTry += 1;
            dist_t focused_dist = task->nl.getFocusedDist();
            if (!focused_dist.valid()) {
                task->setSpeed(0);
                sendKey("UI_Back", 100, 1000);
                sendKey("RollRightButton", 800);
                continue;
            }
            task->distanceToDock = focused_dist.convertTo(dist_t::KM);
        } else {
            mgr.detectEDState(DetectLevel::ListRows);
            if (mgr.uiState.guiFocus != GuiFocus::None) {
                sendKey("UI_Back", 100, 1000);
                continue;
            }
            if (!mgr.compassInfo.nav_target_dist.valid()) {
                LOG(DEBUG) << "Failed to get distance from compass: " << mgr.compassInfo.nav_target_dist;
                task->setSpeed(0);
                if (!mgr.compassInfo.hemisphere) {
                    sendKey("RollRightButton", 800);
                } else {
                    task->orientTowardTargetStep(7, 1000);
                }
                compassTry -= 1;
                if (compassTry <= 0)
                    compassTry = -2;
                continue;
            }
            compassTry = 5;
            task->distanceToDock = mgr.compassInfo.nav_target_dist.convertTo(dist_t::KM);
        }

        if (task->distanceToDock.get(dist_t::KM) < 7.5) {
            task->setSpeed(0);
            return true;
        }
        else if (task->distanceToDock.get(dist_t::KM) < 8.5)
            task->setSpeed(25);
        else if (task->distanceToDock.get(dist_t::KM) > 10.5)
            task->setSpeed(100);
        else
            task->setSpeed(50);

        task->orientTowardTargetStep(7, 1000);

        fails = 0;
    }
    task->setSpeed(0);
    return false;
}

std::string DockStep::getStatus() {
    switch (status) {
    default:
        return "----";
    case PREPARE:
        return "Prepare docking";
    case APPROACH:
        return std::format("Approach\n  dist {}", task->distanceToDock.to_string());
    case REQUEST:
        return "Requesting permit";
    case AUTOPILOT:
        return "Autopilot";
    }
}

bool NavDockSelect::step() {
    task->setSpeed(0);
    if (!dock)
        return false;

    for (int retry=0; retry < 3; retry++) {
        if (!task->nl.focusDestDock())
            continue;
        if (!task->nl.selectFocused())
            continue;
        return true;
    }
    return false;
}

bool NavBodySelect::step() {
    task->setSpeed(0);
    if (!body)
        return false;

    for (int retry=0; retry < 3; retry++) {
        if (!task->nl.focusDestBody())
            continue;
        if (!task->nl.selectFocused())
            continue;
        return true;
    }
    return false;
}

bool DockAndBodyDist::step() {
    task->setSpeed(0);
    if (!task->destBody || !task->destDock)
        return false;
    gal::spStarSystem ss = gal::getCurrentStarSystem();
    if (!ss) {
        LOG(ERROR) << "Current system not known";
        return false;
    }
    task->distanceToDock = {};
    task->distanceToBody = {};

    // get distance to station
    for (int i=0; i < 5 && !task->distanceToDock.valid(); i++) {
        status = DIST_DOCK;
        if (!task->nl.focusDestDock())
            continue;
        if (task->nl.getFocusedEntry()->dist.valid()) {
            task->distanceToDock = task->nl.getFocusedEntry()->dist;
            break;
        }
        if (!task->nl.selectFocused())
            continue;
        if (mgr.uiState.guiFocus != GuiFocus::None)
            sendKey("UI_Back", 0, 1500);
        mgr.detectEDState(DetectLevel::Screen);
        if (!mgr.compassInfo.has_nav_target || mgr.compassInfo.nav_target_dist.valid())
            task->orientTowardTarget(5);
        if (mgr.compassInfo.has_nav_target && mgr.compassInfo.nav_target_dist.valid()) {
            task->distanceToDock = mgr.compassInfo.nav_target_dist;
            break;
        }
    }
    if (!task->distanceToDock.valid())
        return false;

    // get distance to body
    for (int i=0; i < 5 && !task->distanceToBody.valid(); i++) {
        status = DIST_BODY;
        if (!task->nl.focusDestBody())
            continue;
        if (task->nl.getFocusedEntry()->dist.valid()) {
            task->distanceToBody = task->nl.getFocusedEntry()->dist;
            break;
        }
        if (!task->nl.selectFocused())
            continue;
        if (mgr.uiState.guiFocus != GuiFocus::None)
            sendKey("UI_Back", 0, 1500);
        mgr.detectEDState(DetectLevel::Screen);
        if (!mgr.compassInfo.has_nav_target || mgr.compassInfo.nav_target_dist.valid())
            task->orientTowardTarget(5);
        if (mgr.compassInfo.has_nav_target && mgr.compassInfo.nav_target_dist.valid()) {
            task->distanceToBody = mgr.compassInfo.nav_target_dist;
            break;
        }
    }
    if (!task->distanceToBody.valid())
        return false;

    return true;
}

std::string DockAndBodyDist::getStatus() {
    const char* st = "----";
    switch (status) {
    case READY: st = "----"; break;
    case DIST_DOCK: st = "Distance to dock"; break;
    case DIST_BODY: st = "Distance to body"; break;
    }
    return std::format("{}\ndock: {}\nbody: {}", st, task->distanceToDock.to_string(), task->distanceToBody.to_string());
}

bool CruiseToDistStep::step() {
    // select destination dock or body
    if (!task->destDock || !task->destBody)
        return false;
    if (!(task->nl.isDestDockFocused || task->nl.isDestBodyFocused)) {
        if (task->destDock) {
            task->nl.focusDestDock();
        }
        else if (task->destBody) {
            task->nl.focusDestBody();
        }
    }
    if (!(task->nl.isDestDockFocused || task->nl.isDestBodyFocused))
        return false;
    // wait until we get to required distance
    int compassTry = 5;
    for (;;) {
        if (!Cfg.getCurrentStatus()->flags.cruise) {
            LOG(ERROR) << "Unexpected cruise exit";
            task_return(Result::Trouble);
        }
        if (compassTry <= 0) {
            compassTry += 1;
            dist_t focused_dist = task->nl.getFocusedDist();
            if (!focused_dist.valid()) {
                status = BAD_ROW;
                if (!currentDist_km.valid())
                    task->setSpeed(0);
                else if (task->speed_set_to > 25 && currentDist_km.get(dist_t::LS) < 10)
                    task->setSpeed(25);
                continue;
            }
            currentDist_km = focused_dist.convertTo(dist_t::KM);
            sendKey("UI_Back", 100, 1000);
            mgr.detectEDState(DetectLevel::Screen);
        } else {
            mgr.detectEDState(DetectLevel::Screen);
            if (mgr.uiState.guiFocus != GuiFocus::None) {
                sendKey("UI_Back", 100, 1000);
                continue;
            }
            if (!mgr.compassInfo.nav_target_dist.valid()) {
                status = BAD_COMPASS;
                if (!currentDist_km.valid())
                    task->setSpeed(0);
                else if (task->speed_set_to > 25 && currentDist_km.get(dist_t::LS) < 10)
                    task->setSpeed(25);
                if (mgr.compassInfo.has_nav_target) {
                    sendKey("RollRightButton", 800, 1000);
                } else {
                    if (mgr.compassInfo.hemisphere < 0 || std::abs(mgr.compassInfo.targetYaw) > 7 || std::abs(mgr.compassInfo.targetPitch) > 7) {
                        task->setSpeed(0);
                        task->orientTowardTargetStep(7);
                    }
                }
                compassTry -= 1;
                if (compassTry <= 0)
                    compassTry = -2;
                continue;
            }
            currentDist_km = mgr.compassInfo.nav_target_dist.convertTo(dist_t::KM);
        }
        if (currentDist_km.dist <= requiredDist_km.dist) {
            status = DIST_STOP;
            task->setSpeed(0);
            return true;
        }
        else if (currentDist_km.dist <= requiredDist_km.dist * 1.5) {
            status = DIST_NEAR;
            task->setSpeed(25);
        } else {
            status = DIST_FAR;
            task->setSpeed(75);
        }

        compassTry = 5;

        if (!mgr.compassInfo.hemisphere) {
            sendKey("RollRightButton", 800, 1000);
            continue;
        } else {
            if (mgr.compassInfo.hemisphere < 0 || std::abs(mgr.compassInfo.targetYaw) > 2 || std::abs(mgr.compassInfo.targetPitch) > 2) {
                task->orientTowardTargetStep(0.5);
                continue;
            }
        }

        task->orientTowardTargetStep(0.5);
    }

    return true;
}

std::string CruiseToDistStep::getStatus() {
    dist_t curr_mm = currentDist_km.convertTo(dist_t::MM);
    dist_t curr_ls = currentDist_km.convertTo(dist_t::LS);
    dist_t curr = (curr_ls.dist >= 0.1) ? curr_ls : curr_mm;

    dist_t reqr_mm = requiredDist_km.convertTo(dist_t::MM);
    dist_t reqr_ls = requiredDist_km.convertTo(dist_t::LS);
    dist_t reqr = (reqr_ls.dist >= 0.1) ? reqr_ls : curr_mm;

    const char* st = "----";
    switch (status) {
    case READY: st="----"; break;
    case BAD_ROW: st="Bad row"; break;
    case BAD_COMPASS: st="Bad compass"; break;
    case DIST_FAR: st="Dist far"; break;
    case DIST_NEAR: st="Dist near"; break;
    case DIST_STOP: st="Reached"; break;
    }
    return std::format("{}: {} / {}", st, curr.to_string(), reqr.to_string());
}

bool DiveUnderPlanetStep::step() {
    bool ok = true;

    if (ok && !run_sub_step(spStep(new NavDockSelect(this, task->destDock))))
        ok = false;

    if (ok && !task->orientTowardTarget(3))
        ok = false;

    if (ok && !run_sub_step(spStep(new NavBodySelect(this, task->destBody))))
        ok = false;

    // compass dot 50 degree above center
    if (ok && !orient(50))
        ok = false;

    // fly till compass dot 90 degree above center
    if (ok && !fly(90))
        ok = false;

    if (!run_sub_step(spStep(new NavDockSelect(this, task->destDock))))
        ok = false;

    return ok;
}

bool DiveUnderPlanetStep::orient(int pitchGoal) {
    task->setSpeed(0);
    if (mgr.uiState.guiFocus != GuiFocus::None) {
        notifyProgress("Orientation: goto compass");
        task->sendKey("UI_Back", 0, 1500);
    }
    const int rollPrecision = 5;
    const int pitchPrecision = 5;

    int speedDropped = 0;
    for (int fails=0; fails < 10; fails++) {
        mgr.detectEDState(DetectLevel::Screen);
        if (mgr.uiState.guiFocus != GuiFocus::None) {
            notifyProgress(std::format("Unexpected ui mode {}", mgr.uiState.to_string()));
            LOG(WARNING) << "Unexpected ui mode " << mgr.uiState;
            sendKey("UI_Back", 0, 1500);
            continue;
        }
        if (!mgr.compassInfo.hemisphere) {
            notifyProgress(std::format("Compass not detected, fails {}", fails));
            LOG(WARNING) << "Compass not detected";
            continue;
        }
        fails = 0;

        if (std::abs(mgr.compassInfo.targetRoll) < rollPrecision)
            break;
        task->orientRollStep(0);
    }
    if (std::abs(mgr.compassInfo.targetRoll) > rollPrecision)
        return false;

    for (int fails=0; fails < 10; fails++) {
        mgr.detectEDState(DetectLevel::Screen);
        if (mgr.uiState.guiFocus != GuiFocus::None) {
            notifyProgress(std::format("Unexpected ui mode {}", mgr.uiState.to_string()));
            LOG(WARNING) << "Unexpected ui mode " << mgr.uiState;
            sendKey("UI_Back", 0, 1500);
            continue;
        }
        if (!mgr.compassInfo.hemisphere) {
            notifyProgress(std::format("Compass not detected, fails {}", fails));
            LOG(WARNING) << "Compass not detected";
            continue;
        }
        fails = 0;

        if (std::abs(mgr.compassInfo.targetPitch - pitchGoal) < pitchPrecision)
            break;
        task->orientPitchStep(pitchGoal);
    }
    if (std::abs(mgr.compassInfo.targetPitch - pitchGoal) > rollPrecision)
        return false;

    return true;
}

bool DiveUnderPlanetStep::fly(int pitchGoal) {
    if (mgr.uiState.guiFocus != GuiFocus::None) {
        notifyProgress("Orientation: goto compass");
        task->sendKey("UI_Back", 0, 1500);
    }
    task->setSpeed(75);
    for (;;) {
        mgr.detectEDState(DetectLevel::Screen);
        if (mgr.uiState.guiFocus != GuiFocus::None) {
            sendKey("UI_Back", 100, 1000);
            continue;
        }
        if (mgr.compassInfo.hemisphere && mgr.compassInfo.targetPitch >= pitchGoal) {
            task->setSpeed(0);
            return true;
        }
        task->sleep(1000);
    }
}

bool ExitCruiseToStationStep::step() {
    if (!Cfg.getCurrentStatus()->flags.cruise) {
        LOG(ERROR) << "Unexpected cruise exit";
        task_return(Result::Trouble);
    }
    status = ORIENT;
    double dist_km = 15000;
    if (!task->nl.focusDestDock())
        return false;
    if (!task->nl.selectFocused())
        return false;
    if (!task->orientTowardTarget(5))
        return false;
    status = APPROACH;
    // wait until we get to 1mm
    for (;;) {
        if (!Cfg.getCurrentStatus()->flags.cruise) {
            LOG(ERROR) << "Unexpected cruise exit";
            task_return(Result::Trouble);
        }
        if (use_nav_panel) {
            if (dist_fails > 3) {
                use_nav_panel = false;
                sendKey("UI_Back", 0, 1000);
                continue;
            }
            dist_t focused_dist = task->nl.getFocusedDist();
            if (focused_dist.valid())
                task->distanceToDock = focused_dist.convertTo(dist_t::KM);
        } else {
            mgr.detectEDState(DetectLevel::Screen);
            if (mgr.uiState.guiFocus != GuiFocus::None) {
                sendKey("UI_Back", 0, 1000);
                continue;
            }
            if (mgr.compassInfo.nav_target_dist.valid())
                task->distanceToDock = mgr.compassInfo.nav_target_dist.convertTo(dist_t::KM);
        }

        if (!task->distanceToDock.valid()) {
            dist_fails += 1;
            if (dist_km < 5000 && (task->speed_set_to > 0 || dist_fails >= 5))
                task->setSpeed(0);
            if (!use_nav_panel) {
                if (dist_fails >= 15) {
                    use_nav_panel = true;
                    dist_fails = 0;
                }
                else if ((dist_fails % 5) == 4)
                    sendKey("RollRightButton", 800);
                continue;
            } else {
                use_nav_panel = false;
                dist_fails = 0;
                sendKey("UI_Back", 0, 1000);
            }
            continue;
        }
        dist_km = task->distanceToDock.get(dist_t::KM);
        if (dist_km < 1000)
            exit_confirm += 1;
        else
            exit_confirm = 0;
        if (exit_confirm >= 2)
            break;
        if (dist_km < 3000)
            task->setSpeed(25);
        else if (dist_km < 5000)
            task->setSpeed(50);
        else
            task->setSpeed(75);
        if (!exit_confirm && !use_nav_panel)
            task->orientTowardTargetStep(1, 1000);
    }

    // wait until we exit super-cruise
    timer = utc_timer(10s);
    status = EXITING;
    task->setSpeed(25);
    const auto& cs = mgr.cfg.getCurrentStatus();
    while (cs->flags.cruise && !timer.expired()) {
        notifyProgress("Waiting cruise exit...");
        sendKey("HyperSuperCombination", 100, 1000);
        sleep(1000);
        task->orientTowardTargetStep(10, 1000);
    }

    notifyProgress("Arrived, speed zero");
    task->setSpeed(0);
    sleep(500);

    for (dist_fails=0; dist_fails < 15; dist_fails++) {
        mgr.detectEDState(DetectLevel::Screen);
        if (mgr.uiState.guiFocus != GuiFocus::None) {
            sendKey("UI_Back", 0, 1000);
            mgr.detectEDState(DetectLevel::Screen);
        }
        if (mgr.compassInfo.nav_target_dist.valid()) {
            if (mgr.compassInfo.nav_target_dist.get(dist_t::KM) > 25) {
                LOG(ERROR) << "Unexpected distance after cruise exit: " << mgr.compassInfo.nav_target_dist;
                task_return(Result::Trouble);
            }
            return true;
        }
        if ((dist_fails % 3) == 2)
            sendKey("RollRightButton", 800);
    }

    LOG(WARNING) << "Cannot confirm distance after cruise exit";
    return true;
}

std::string ExitCruiseToStationStep::getStatus() {
    if (status == ORIENT) {
        return "Orienting towards target";
    }
    if (status == APPROACH) {
        if (dist_fails)
            return std::format("Approaching:\ndist {} (fails {})\nspeeed {}%", task->distanceToDock.to_string(), dist_fails, task->speed_set_to);
        else if (exit_confirm)
            return std::format("Approaching:\ndist {} (confirm {})\nspeeed {}%", task->distanceToDock.to_string(), exit_confirm, task->speed_set_to);
        else
            return std::format("Approaching:\ndist {}\nspeeed {}%", task->distanceToDock.to_string(), task->speed_set_to);
    }
    if (status == EXITING) {
        return std::format("Exiting cruise {}", timer.left());
    }
    if (status == CONFIRM) {
        return std::format("Checking distance\nfails {}", dist_fails);
    }
    return "----";
}


TaskTravel::TaskTravel(Task *parent, AIManager &mgr, const TaskTemplate &templ)
    : BaseAutopilotTask(parent, mgr, templ)
{
    for (auto& p : templ.params) {
        if (p.name == "system")
            destSystem = std::get<std::string>(p.value);
        else if (p.name == "dock")
            destDock = std::get<std::string>(p.value);
    }
}

void TaskTravel::plan() {
//    TaskTemplate taskDepart = mgr.getTaskTemplate(ED_TASK_DEPART);
//    sub_tasks.push_back(std::make_unique<TaskDepart>(this, mgr, taskDepart));
//
//    TaskTemplate taskJumpToSystem = mgr.getTaskTemplate(ED_TASK_JUMP_TO_SYSTEM);
//    taskJumpToSystem.set("system", destSystem);
//    sub_tasks.push_back(std::make_unique<TaskJumpToSystem>(this, mgr, taskJumpToSystem));
//
//    TaskTemplate taskCruiseToDock = mgr.getTaskTemplate(ED_TASK_CRUISE_TO_STATION);
//    taskCruiseToDock.set("dock", destDock);
//    sub_tasks.push_back(std::make_unique<TaskCruiseToDock>(this, mgr, taskCruiseToDock));
//
//    TaskTemplate taskDock = mgr.getTaskTemplate(ED_TASK_DOCK);
//    sub_tasks.push_back(std::make_unique<TaskDock>(this, mgr, taskDock));
}

Result TaskTravel::run() {
    return Result::Failure;
//    switch (result) {
//    case Result::Created:
//    case Result::Started:
//    case Result::Trouble:
//        plan();
//        break;
//    case Result::Failure:
//    case Result::Partly:
//    case Result::Success:
//        LOG(ERROR) << "Bad state on task run(): " << enum_name<Result>(result);
//        return result;
//    }
//
//    while (!sub_tasks.empty()) {
//        spTask& pTask = sub_tasks.front();
//        Result res = run_sub_task(pTask);
//        switch (res) {
//        case Result::Created:
//        case Result::Started:
//            LOG(ERROR) << "Bad state after task run(): " << enum_name<Result>(res);
//            plan();
//            continue;
//        case Result::Trouble:
//            if (pTask->missCount < pTask->maxMisses) {
//                plan();
//                pTask->result = Result::Started;
//                continue;
//            }
//            pTask->result = Result::Failure;
//            // fall through
//        case Result::Failure:
//        case Result::Partly:
//        case Result::Success:
//            sub_tasks.pop_front();
//            break;
//        }
//    }
//    notifyProgress(_("End of travel"));
//    result = Result::Success;
//    return result;
}

Autopilot::Autopilot(Task *parent, AIManager &mgr, const TaskTemplate &templ)
        : BaseAutopilotTask(parent, mgr, templ)
{
}

Result Autopilot::run() {
    auto starSystem = gal::getCurrentStarSystem();
    std::string dest = Cfg.getCurrentStatus()->destinationName;
    if (dest.empty())
        return Result::Failure;
    destDock = starSystem->getDock(dest);
    if (!destDock)
        return Result::Failure;
    int bodyId = Cfg.getCurrentStatus()->destinationBody;
    if (destDock->typeNav == gal::TypeNav::SpacePort) {
        if (!destDock->bodyId.has_value()) {
            destDock->bodyId = bodyId;
            starSystem->saved = false;
            gal::saveStarSystem(starSystem);
        }
        bodyId = destDock->parentBodyId;
    }
    {
        auto body = starSystem->getBodyById(bodyId);
        if (!body)
            return Result::Failure;
        destBody = std::dynamic_pointer_cast<gal::Body>(body);
    }
    if (!destBody)
        return Result::Failure;

    st::NavPanelFilters filters = {};
    filters.star = true;
    filters.planetOrMoon = true;
    filters.landablePlanetOrMoon = true;
    filters.station = true;
    if (destDock) {
        if (destDock->typeNav == gal::TypeNav::SpacePort)
            filters.station = true;
        if (destDock->typeNav == gal::TypeNav::Carrier)
            filters.fleetCarrier = true;
        if (destDock->typeNav == gal::TypeNav::MegashipDock) {
            if (destDock->typeSite == gal::TypeSite::TrailblazerDream)
                filters.pointOfInterest = true;
            else
                filters.station = true;
        }
    }

    nl.init(this, destDock, destBody, filters);

    if (Cfg.getCurrentStatus()->flags.docked) {
        if (!st::dockedAt.stationName.empty() && (destDock->name == st::dockedAt.stationName || destDock->nloc == st::dockedAt.stationName))
            return Result::Success;
        if (!run_sub_step(spStep(new DepartureStep(this))))
            return Result::Failure;
    }

    bool at_dock = false;
    if (!Cfg.getCurrentStatus()->flags.cruise) {
        if (!st::space.body.empty() && (destDock->name == st::space.body || destDock->nloc == st::space.body))
            at_dock = true;
    }
    if (!at_dock && !Cfg.getCurrentStatus()->flags.cruise) {
        if (!run_sub_step(spStep(new EnterCruiseStep(this))))
            return Result::Failure;
    }

    if (!at_dock) {
        // a few degrees visible angle to stop and avoid planet
        double distToCorrect = destBody->radius * 70;
        if (distToCorrect < 15000)
            distToCorrect = 15000;
        if (!run_sub_step(spStep(new CruiseToDistStep(this, distToCorrect))))
            return Result::Trouble;

        for (int i = 0; i < 5; i++) {
            setSpeed(0);
            check_interrupted();
            if (!run_sub_step(spStep(new DockAndBodyDist(this))))
                return Result::Trouble;
            if (distanceToDock.get(dist_t::KM) >= distanceToBody.get(dist_t::KM)) {
                if (!run_sub_step(spStep(new DiveUnderPlanetStep(this))))
                    return Result::Trouble;
            }
            nl.focusDestDock();
            break;
        }

        if (!run_sub_step(spStep(new ExitCruiseToStationStep(this))))
            return Result::Trouble;
    }

    if (!run_sub_step(spStep(new DockStep(this))))
        return Result::Trouble;

    return Result::Success;
}

} // ai
