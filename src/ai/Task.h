//
// Created by mkizub on 23.05.2025.
//

#pragma once

#ifndef EDROBOT_TASK_H
#define EDROBOT_TASK_H

#include "Types.h"

namespace ai {

class Task {
public:
    Task(Task* parent, AIManager& mgr, const TaskTemplate& templ);
    virtual ~Task() = default;
    virtual Result run() = 0;
    virtual Result run_sub_task(upTask& task);

    Task const * parent;
    AIManager& mgr;
    TaskTemplate templ;

    void sleep(int milliseconds) const;
    bool sendKey(const std::string& name, int delay_ms = 35, int pause_ms = 50) const;
    bool sendMouseMove(const cv::Point& point, int pause_ms = 50, bool absolute = true) const;
    bool sendMouseClick(const cv::Point& point, int delay_ms = 35, int pause_ms = 50) const;
    bool decodePosition(const json5pp::value& pos, cv::Point& point, const json5pp::value& args) const;
    void check_interrupted() const;
    bool executeAction(const std::string& actionName, const json5pp::value& args);
    bool executeStep(const json5pp::value& step, const json5pp::value& args);
    bool executeWait(const json5pp::value& step, const json5pp::value& args);
    void hardcodedStep(const std::string& step, DetectLevel level);

    void notifyProgress(const char* msg) const;
    void notifyProgress(const std::string& msg) const;
    [[noreturn]] void notifyError(const char* msg, Result result) const;
    [[noreturn]] void notifyError(const std::string& msg, Result result) const;

    [[noreturn]] void task_return(Result result) const;
    [[noreturn]] void task_return(Result result, const char* msg) const;

    std::string taskName;
    json5pp::value taskActions;
    std::string fromState;
    std::string destState;

    std::deque<upTask> sub_tasks;

    short missCount { 0 };
    short maxMisses { 0 };
    Result result { Result::Created };
};

class TaskCalibrate final : public Task {
public:
    TaskCalibrate(Task* parent, AIManager& mgr, const TaskTemplate& templ);
    Result run() final;
private:
    std::array<std::vector<cv::Vec3b>,4> mButtonBGR;
    std::array<std::vector<cv::Vec3b>,4> mLstRowBGR;
    void recordButton(const char* button, WState bs);
    void recordLstRow(const char* list, cv::Point mouse, WState bs);
    void getRowsByState(const ClassifiedRect** rows);
    bool calculateAverage(bool incomplete);
    std::array<cv::Vec3b,4> mButtonBGRAverage;
    std::array<cv::Vec3b,4> mLstRowBGRAverage;
    HistogramTemplate mDetector;
};

class TaskSell;

class TaskSellAll final : public Task {
public:
    TaskSellAll(Task* parent, AIManager& mgr, const TaskTemplate& templ);
    Result run() final;
private:
    void plan();

    int mChunk;
    std::vector<upTask> sell_archive;
};

class TaskSell final : public Task {
public:
    TaskSell(Task* parent, AIManager& mgr, const TaskTemplate& templ);
    Result run() final;

    Commodity* mCommodity;
    int mTotal;
    int mItems;

    int mSold;
};

class TaskDebugFindAllCommodities final : public Task {
public:
    TaskDebugFindAllCommodities(Task* parent, AIManager& mgr, const TaskTemplate& templ);
    Result run() final;
private:
    bool checkCommodity(Commodity* commodity, const std::string& marketMode, const std::vector<Commodity*>& table, std::vector<CommodityMatch>* verify);
    void saveOcrTrainingData(cv::Rect rect, const Commodity* commodity, bool invert);

    const bool shuffle;
    const int dump_index;
    const int start_index;
};

} //namespace ai

#endif //EDROBOT_TASK_H
