//
// Created by mkizub on 23.05.2025.
//

#pragma once

#include <utility>

#include "pch.h"

#ifndef EDROBOT_TASK_H
#define EDROBOT_TASK_H

#include "Template.h"

class Task {
public:
    Task() : master(Master::getInstance()), taskName("EDRobot Task") {}
    virtual ~Task() = default;
    virtual bool run() = 0;
    void stop() { done = true; };

    Master& master;

protected:
    class completed_error : public std::runtime_error {
    public:
        explicit completed_error(const std::string &arg) : runtime_error(arg) {}
        explicit completed_error(const char *arg) : runtime_error(arg) {}
    };

    void preciseSleep(double seconds) const;
    void sleep(int milliseconds) const;
    bool sendKey(const std::string& name, int delay_ms = 35, int pause_ms = 50) const;
    bool sendMouseMove(const cv::Point& point, int pause_ms = 50, bool absolute = true) const;
    bool sendMouseClick(const cv::Point& point, int delay_ms = 35, int pause_ms = 50) const;
    bool decodePosition(const json5pp::value& pos, cv::Point& point, const json5pp::value& args) const;
    void check_completed() const {
        if (done)
            throw completed_error("aborted");
    }
    bool executeAction(const std::string& actionName, const json5pp::value& args);
    bool executeStep(const json5pp::value& step, const json5pp::value& args);
    bool executeWait(const json5pp::value& step, const json5pp::value& args);
    void hardcodedStep(const char* step, DetectLevel level);

    void notifyProgress(const std::string& msg) const;
    void notifyError(const std::string& msg) const;

    std::string taskName;
    json5pp::value taskActions;
    std::string actionName;
    std::string fromState;
    std::string destState;
    std::atomic<bool> done;
};

class TaskCalibrate final : public Task {
public:
    TaskCalibrate();
    bool run() final;
private:
    std::array<std::vector<cv::Vec3b>,4> mButtonLuv;
    std::array<std::vector<cv::Vec3b>,4> mLstRowLuv;
    void recordButtonLuv(const char* button, WState bs);
    void recordLstRowLuv(const char* list, cv::Point mouse, WState bs);
    bool getRowsByState(const ClassifiedRect** rows);
    bool calculateAverage(bool incomplete);
    std::array<cv::Vec3b,4> mButtonLuvAverage;
    std::array<cv::Vec3b,4> mLstRowLuvAverage;
    HistogramTemplate mDetector;
};

class TaskSell final : public Task {
public:
    TaskSell(Commodity* commodity, int total, int items)
        : mCommodity(commodity), mTotal(total), mItems(items)
    {
        taskName = "Selling";
    }
    bool run() final;
private:
    Commodity* mCommodity;
    int mTotal;
    int mItems;
};

class TaskDebugFindAllCommodities final : public Task {
public:
    TaskDebugFindAllCommodities() {
        taskName = "TaskDebugFindAllCommodities";
    }
    bool run() final;
private:
    bool checkCommodity(Commodity* commodity, std::vector<CommodityMatch>* verify);
};

#endif //EDROBOT_TASK_H
