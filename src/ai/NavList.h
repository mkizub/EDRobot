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
    int8_t index {};
    uint8_t indent {0};
    uint8_t portSize {0}; // Name ++
    uint8_t portDanger {L'\0'}; // ◇ / ⬖ / ◆
    std::wstring name;
    dist_t dist;
    gal::spEntity item {};
    int8_t ocr_conf {};
    int8_t txt_conf {};
    int8_t conf {};
    int8_t confirmed {};
    bool focused {};
    bool parsed {};
};

class NavList {
public:

    NavList() = default;
    bool init(st::NavPanelFilters filters);
    void setNearestSystems(const std::string& nearSystem, std::vector<std::string> systems);
    std::vector<ClassifiedRect*> initNavList(cv::Mat& grayImage, int& focusIdx);
    std::vector<ClassifiedRect*> recognizeWholePage(cv::Mat& grayImage, int& focusIdx);
    gal::spEntity guessNavItem(int idx);
    bool fixupNavList();

    bool focusDestDock(int* conf=nullptr);
    bool focusDestBody(int* conf=nullptr);
    bool focusDestination(int& focusIdx);
    gal::spEntity focusNearestBody(dist_t* dist=nullptr);
    bool focusDockBody(int* conf=nullptr);
    bool focusTopEntry();

    bool selectFocused(gal::Entity* dest);
    bool discoverSelected();

    dist_t getFocusedDist(int max_try);

    bool parseNavRow(const cv::Mat &grayImage, const ResolvedEnv& rEnv, const ClassifiedRect &cr, int idx);
    bool parseNavDist(const cv::Mat &grayImage, const ResolvedEnv& rEnv, const ClassifiedRect &cr, int idx);

    std::vector<NavListEntry> list;
    std::string nearSystem;
    std::vector<std::wstring> ocrSystemNames;
};


class NavListScanTask : public Task {
public:
    NavListScanTask(const TaskTemplate& templ);
    bool run() final;
    bool gotoNavPageNavigation();

    bool mTravel {false};
    NavList nl;
};

class NavListScanSystemsTask : public Task {
public:
    NavListScanSystemsTask(const TaskTemplate& templ);
    bool run() final;

    NavList nl;
    enum {
        READY, DONE
    } status {READY};
    std::vector<gal::spStarSystem> foundSystems;
};

} // namespace ai

#endif //EDROBOT_NAVLIST_H
