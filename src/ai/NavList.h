//
// Created by mkizub on 18.10.2025.
//

#pragma once

#ifndef EDROBOT_NAVLIST_H
#define EDROBOT_NAVLIST_H

#include "Types.h"
#include "Task.h"
#include "../Galaxy.h"

namespace ai {

struct NavListEntry {
    wchar_t icon {L'\0'};  // ✦ / ☄ / ✇ / etc.
    bool isTarget {false}; // < Name >
    bool isMarked {false}; // Name∇
    uint8_t indent {0};
    uint8_t portSize {0}; // Name ++
    wchar_t portDanger {L'\0'}; // ◇ / ⬖ / ◆
    std::wstring name;
    dist_t dist;
    const gal::NavType* navType {nullptr};
    gal::spEntity item {};
    int ocr_conf {};
    bool focused {};
    bool parsed {};
    int8_t confirmed {};
    int8_t index {};
};

class NavList {
public:

    NavList() = default;
    bool init(st::NavPanelFilters filters);
    std::vector<ClassifiedRect*> initNavList(cv::Mat& grayImage, int& focusIdx);
    std::vector<ClassifiedRect*> recognizeWholePage(cv::Mat& grayImage, int& focusIdx);
    gal::spEntity guessNavItem(int idx);
    bool fixupNavList();

    bool focusDestDock();
    bool focusDestBody();
    bool focusDestination(int& focusIdx);
    gal::spEntity focusNearestBody(dist_t* dist=nullptr);
    bool focusTopEntry();

    bool selectFocused();
    bool discoverSelected();

    dist_t getFocusedDist(int max_try);

    bool parseNavRow(const cv::Mat &grayImage, const ResolvedEnv& rEnv, const ClassifiedRect &cr, int idx);
    bool parseNavDist(const cv::Mat &grayImage, const ResolvedEnv& rEnv, const ClassifiedRect &cr, int idx);

    std::vector<NavListEntry> list;

};


class NavListScanTask : public Task {
public:
    NavListScanTask(const TaskTemplate& templ);
    bool run() final;
    bool gotoNavPageNavigation();

    bool mTravel {false};
    NavList nl;
};

} // namespace ai

#endif //EDROBOT_NAVLIST_H
