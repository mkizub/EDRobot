//
// Created by mkizub on 02.07.2025.
//

#pragma once

#ifndef EDROBOT_TASKCALIBRATE_H
#define EDROBOT_TASKCALIBRATE_H

#include "Task.h"

namespace ai {

class TaskCalibrate final : public Task {
public:
    TaskCalibrate(Task *parent, AIManager &mgr, const TaskTemplate &templ);

    bool run() final;

private:
    std::array<std::vector<cv::Vec3b>, 4> mButtonBGR;
    std::array<std::vector<cv::Vec3b>, 4> mLstRowBGR;

    void recordButton(const char *button, WState bs);

    void recordLstRow(const char *list, cv::Point mouse, WState bs);

    void getRowsByState(const ClassifiedRect **rows);

    bool calculateAverage(bool incomplete);

    std::array<cv::Vec3b, 4> mButtonBGRAverage;
    std::array<cv::Vec3b, 4> mLstRowBGRAverage;
    cv::Mat colorImage;
};

} // namespace ai

#endif //EDROBOT_TASKCALIBRATE_H
