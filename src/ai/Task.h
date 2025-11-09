//
// Created by mkizub on 23.05.2025.
//

#pragma once

#ifndef EDROBOT_TASK_H
#define EDROBOT_TASK_H

#include "Types.h"

namespace ai {

class Step : std::enable_shared_from_this<Step> {
public:
    Step();
    virtual ~Step() = default;

    virtual bool run() = 0;
    virtual bool run_sub_step(spStep step);
    bool run_sub_step(Step* step) { return run_sub_step(spStep(step)); }

    virtual std::string getTitle() = 0;

    void addMessage(MessageSeverity severity, const char* msg);
    void addMessage(MessageSeverity severity, const std::string& msg);
    std::vector<std::string> getMessages();
    virtual std::string getStatus();

    Step * const parent;

    struct Message {
        std::chrono::time_point<std::chrono::steady_clock> timestamp;
        MessageSeverity severity;
        std::string message;
        bool expired();
    };
    std::deque<Message> messages;
    std::mutex messagesMutex;
    spStep currSubStep;
    spStep prevSubStep;
};

class Task : public Step {
public:
    Task(const TaskTemplate& templ);
    virtual ~Task() = default;
    virtual std::string getTitle();

    TaskTemplate templ;

    bool decodePosition(const json5pp::value& pos, cv::Point& point, const json5pp::value& args) const;
    bool executeStep(const json5pp::value& step, const json5pp::value& args);
    bool executeWait(const json5pp::value& step, const json5pp::value& args);
    void hardcodedStep(const std::string& step, DetectLevel level, cv::Mat* colorImage = nullptr, cv::Mat* grayImage = nullptr);

    short missCount { 0 };
};

class TaskRepeat : public Task {
public:
    TaskRepeat(const TaskTemplate& templ);
    virtual ~TaskRepeat() = default;
    bool run() override;

    std::string getTitle() override;

    int mTotal {};
    int mDuration;

    bool mStarted {};
    int mCompleted {};
    utc_timer timer;
    int mStepIdx {};
};

} //namespace ai

#endif //EDROBOT_TASK_H
