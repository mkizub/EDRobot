//
// Created by mkizub on 23.05.2025.
//

#pragma once

#include "pch.h"

#ifndef EDROBOT_TASK_H
#define EDROBOT_TASK_H

#include "Template.h"

class Task {
public:
    Task() : master(Master::getInstance()) {}
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

    void preciseSleep(double seconds);
    void sleep(int milliseconds);
    bool sendKey(const std::string& name, int delay_ms = 35, int pause_ms = 50);
    bool sendMouseMove(int x, int y, int pause_ms = 50);
    bool sendMouseClick(int x, int y, int delay_ms = 35, int pause_ms = 50);
    void check_completed() {
        if (done)
            throw completed_error("aborted");
    }
    bool executeAction(const std::string& actionName, const json5pp::value& args);
    bool executeStep(const json5pp::value& step, const json5pp::value& args);

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
    enum class BS { Normal, Focused, Activated, Disabled };
    int imageCounter = 0;
    std::array<std::vector<cv::Vec3b>,4> mLuvColors;
    std::array<std::vector<cv::Vec3b>,4> mRGBColors;
    cv::Vec3b getLuv(const char* button, BS bs);
    void hardcodedStep(const char* step);
    bool calculateAverage();
    std::array<cv::Vec3b,4> mLuvAverage;
    HistogramTemplate detector;
};

class TaskSell final : public Task {
public:
    TaskSell(int sells, int items)
        : mSells(sells), mItems(items)
    {}
    bool run() final;
private:
    int mSells;
    int mItems;
};

#endif //EDROBOT_TASK_H
