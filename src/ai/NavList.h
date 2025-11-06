//
// Created by mkizub on 18.10.2025.
//

#pragma once

#ifndef EDROBOT_NAVLIST_H
#define EDROBOT_NAVLIST_H

#include "Types.h"
#include "Task.h"

namespace gal {
class Item;
class Body;
class Site;
typedef std::shared_ptr<Item> spItem;
typedef std::shared_ptr<Body> spBody;
typedef std::shared_ptr<Site> spSite;
}

namespace nav {

struct NavListEntry {
    wchar_t icon {L'\0'};  // ✦ / ☄ / ✇ / etc.
    bool isTarget {false}; // < Name >
    bool isMarked {false}; // Name∇
    uint8_t indent {0};
    uint8_t portSize {0}; // Name ++
    wchar_t portDanger {L'\0'}; // ◇ / ⬖ / ◆
    std::wstring name;
    dist_t dist;
    const nav::NavType* navType {nullptr};
    gal::spItem item {};
    int ocr_conf {};
    bool focused {};
    bool parsed {};
    int8_t confirmed {};
};

class NavList {
public:

    NavList() = default;
    bool init(st::NavPanelFilters filters);
    std::vector<ClassifiedRect*> initNavList(cv::Mat& grayImage, int& focusIdx);
    std::vector<ClassifiedRect*> recognizeWholePage(cv::Mat& grayImage, int& focusIdx);
    bool recognizeFocusedNavRow(nav::NavListEntry& nle);
    gal::spItem guessNavItem(NavListEntry &nle);
    bool fixupNavList();

    bool focusDestDock();
    bool focusDestBody();
    gal::spBody focusNearestBody(dist_t* dist=nullptr);
    bool focusTopEntry();

    bool selectFocused();

    dist_t getFocusedDist(int max_try);

    std::vector<NavListEntry> list;

};

} // namespace nav

namespace ai {

class NavListScanTask : public Task {
public:
    NavListScanTask(const TaskTemplate& templ);
    bool run() final;
    bool gotoNavPageNavigation();

    bool mTravel {false};
    nav::NavList nl;
};

} // namespace ai

#endif //EDROBOT_NAVLIST_H
