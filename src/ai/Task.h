//
// Created by mkizub on 23.05.2025.
//

#pragma once

#ifndef EDROBOT_TASK_H
#define EDROBOT_TASK_H

#include "Types.h"
#include "TaskTemplate.h"

namespace ai {

class Step : std::enable_shared_from_this<Step> {
protected:
    Step();
public:
    virtual ~Step() = default;

    virtual bool run() = 0;
    virtual bool run_sub_step(spStep step);
    bool run_sub_step(Step* step) { return run_sub_step(spStep(step)); }

    virtual std::string getTitle() = 0;

    void addMessage(MessageSeverity severity, const std::string_view msg);
    std::vector<std::string> getMessages();
    virtual std::string getStatus();

    Step * const parent;

    struct Message {
        std::chrono::time_point<std::chrono::steady_clock> timestamp;
        MessageSeverity severity;
        std::string message;
        [[nodiscard]] bool expired() const;
    };
    std::deque<Message> messages;
    std::mutex messagesMutex;
    spStep currSubStep;
    spStep prevSubStep;
};

class Task : public Step {
protected:
    explicit Task(const TaskTemplate& templ);
public:
    ~Task() override = default;
    std::string getTitle() override;

    TaskTemplate templ;

    bool decodePosition(const json5pp::value& pos, cv::Point& point, const json5pp::value& args) const;
    bool executeStep(const json5pp::value& step, const json5pp::value& args);
    bool executeWait(const json5pp::value& step, const json5pp::value& args);
    void hardcodedStep(const std::string& step, DetectLevel level, cv::Mat* colorImage = nullptr, cv::Mat* grayImage = nullptr);

    int missCount {};
    bool failed {};
};

class TaskRepeat : public Task {
public:
    explicit TaskRepeat(const TaskTemplate& templ);
    ~TaskRepeat() override = default;
    bool run() override;

    std::string getTitle() override;

    int mTotal {};
    int mDuration {};
    std::vector<TaskTemplate> steps;

    bool mStarted {};
    int mCompleted {};
    utc_timer timer;
    int mStepIdx {};
};

} //namespace ai

#endif //EDROBOT_TASK_H
