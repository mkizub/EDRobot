//
// Created by mkizub on 02.07.2025.
//

#pragma once

#ifndef EDROBOT_TASKDEBUG_H
#define EDROBOT_TASKDEBUG_H

#include "Task.h"

namespace ai {

class TaskDebugFindAllCommodities final : public Task {
public:
    TaskDebugFindAllCommodities(Task* parent, AIManager& mgr, const TaskTemplate& templ);
    Result run() final;
private:
    bool checkCommodity(Commodity* commodity, const std::string& marketMode, const std::vector<Commodity*>& table, std::vector<CommodityMatch>* verify);
    void saveOcrTrainingData(const cv::Mat& grayImage, cv::Rect rect, const Commodity* commodity, bool invert);

    const bool shuffle;
    const int dump_index;
    const int start_index;
};

} // ai

#endif //EDROBOT_TASKDEBUG_H
