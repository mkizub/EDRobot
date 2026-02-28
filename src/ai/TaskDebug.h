//
// Created by mkizub on 02.07.2025.
//

#pragma once

#ifndef EDROBOT_TASKDEBUG_H
#define EDROBOT_TASKDEBUG_H

#include "Task.h"
#include "NavList.h"

namespace widget {
class Label;
}

namespace gal {
struct NavType;
}

namespace ai {

class TaskDebugFindAllBase : public Task {
protected:
    TaskDebugFindAllBase(const TaskTemplate& templ_, bool market)
        : Task(templ_)
        , isMarket(market)
    {}
    void checkAndFixOCRText();

    const bool isMarket;
};

class TaskDebugFindAllCommodities final : public TaskDebugFindAllBase {
public:
    explicit TaskDebugFindAllCommodities(const TaskTemplate& templ);
    bool run() final;
private:
    bool checkCommodity(Commodity* commodity, const std::string& marketMode, const std::vector<Commodity*>& table, std::vector<CommodityMatch>* verify);
    void saveOcrMarketRow(const cv::Mat& grayImage, const ClassifiedRect& cr, const Commodity* commodity);
    void saveOcrMarketLbl(const cv::Mat& grayImage, const ClassifiedRect& cr, const Commodity* commodity);

    bool shuffle;
    bool dump_images;
    int start_index;
};

class TaskDebugFindAllNavPoints final : public TaskDebugFindAllBase {
public:
    explicit TaskDebugFindAllNavPoints(const TaskTemplate& templ);
    bool run() final;
private:
    bool checkOcrError(const NavListEntry& nle);
    //bool checkNavPoint(int offset);
    void saveOcrNavigationRow(const cv::Mat& grayImage, const ClassifiedRect& cr, int offset,
                              const NavListEntry& nle);
    void saveOcrNavigationLbl(const cv::Mat& grayImage, const ClassifiedRect& cr, int offset,
                                std::string& lbl_text, const gal::NavType* nt);
    js::value curlGetRequest(const char* url);
    js::value curlPostRequest(const char* url, js::value& data);
    bool getSpanishInfo();
    //bool getSystemStations();
    //bool getSystemBodies();
    const gal::NavType* guessNavType(const std::string& lbl_name, const std::string& lbl_anchor) const;
    int guessBestStation(std::string& text, const gal::NavType* nav_type) const;
    //bool addStationPrefix(std::string& text, const std::string& lbl_anchor) const;

    bool dump_images;
    bool resume;
    bool unfocused;
    int ocr_confidence;
    int txt_confidence;
    int offset_append;

    js::value spanshSystemInfo;
    js::value spanshNearSystems;

    NavList nl;
};

} // ai

#endif //EDROBOT_TASKDEBUG_H
