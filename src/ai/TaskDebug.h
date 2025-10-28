//
// Created by mkizub on 02.07.2025.
//

#pragma once

#ifndef EDROBOT_TASKDEBUG_H
#define EDROBOT_TASKDEBUG_H

#include "Task.h"

namespace widget {
class Label;
}

namespace ai {

class TaskDebugFindAllBase : public Task {
protected:
    TaskDebugFindAllBase(Task* parent, AIManager& mgr, const TaskTemplate& templ, bool market)
        : Task(parent, mgr, templ)
        , isMarket(market)
    {}
    void checkAndFixOCRText();

    const bool isMarket;
};

class TaskDebugFindAllCommodities final : public TaskDebugFindAllBase {
public:
    TaskDebugFindAllCommodities(Task* parent, AIManager& mgr, const TaskTemplate& templ);
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
    TaskDebugFindAllNavPoints(Task* parent, AIManager& mgr, const TaskTemplate& templ);
    bool run() final;
private:
    bool checkOcrError(const ClassifiedRect& cr);
    bool checkNavPoint(int offset);
    void saveOcrNavigationRow(const cv::Mat& grayImage, const ClassifiedRect& cr, int offset,
                                const std::string& lbl_text, const nav::NavType* nt);
    void saveOcrNavigationLbl(const cv::Mat& grayImage, const ClassifiedRect& cr, int offset,
                                std::string& lbl_text, const nav::NavType* nt);
    json5pp::value curlGetRequest(const char* url);
    json5pp::value curlPostRequest(const char* url, json5pp::value& data);
    bool getSpanishInfo();
    //bool getSystemStations();
    //bool getSystemBodies();
    const nav::NavType* guessNavType(const std::string& lbl_name, const std::string& lbl_anchor) const;
    int guessBestStation(std::string& text, const nav::NavType* nav_type) const;
    //bool addStationPrefix(std::string& text, const std::string& lbl_anchor) const;

    bool dump_images;
    bool resume;
    bool unfocused;
    int ocr_confidence;
    int txt_confidence;
    int offset_append;

    json5pp::value spanishSystemInfo;
    json5pp::value spanishNearSystems;
    //json5pp::array systemStations;
    //json5pp::array systemBodies;

    struct StationRowInfo {
        wchar_t type;  // ✦ / ☄ / ✇ / etc.
        bool isTarget; // < Name >
        bool isLocation; // Name∇
        uint8_t size; // Name ++
        wchar_t danger; // ◇ / ⬖ / ◆
        std::wstring name;
    };
    bool parseRowInfo(std::wstring text, StationRowInfo& rowInfo);
};

} // ai

#endif //EDROBOT_TASKDEBUG_H
