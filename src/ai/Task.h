//
// Created by mkizub on 23.05.2025.
//

#pragma once

#ifndef EDROBOT_TASK_H
#define EDROBOT_TASK_H

#include "Types.h"

namespace ai {

void check_interrupted();
void ai_sleep(int milliseconds, bool precise=false);
bool gotoNavPage(Step *task, const std::string &page_name);
void rollBlindCompass();

class Step : std::enable_shared_from_this<Step> {
public:
    Step(Step* parent, AIManager& mgr);
    virtual ~Step() = default;

    Task* getTask();

    virtual bool step() = 0;
    virtual bool run_sub_step(spStep step);

    virtual const char* getName() = 0;

    void sleep(int milliseconds, bool precise=false) const { ai_sleep(milliseconds, precise); }

    void notifyProgress(const char* msg);
    void notifyProgress(const std::string& msg);
    [[noreturn]] void notifyError(const char* msg, Result result);
    [[noreturn]] void notifyError(const std::string& msg, Result result);

    [[noreturn]] void task_return(Result result);
    [[noreturn]] void task_return(Result result, const char* msg);
    void addMessage(const char* msg);
    void addMessage(const std::string& msg);
    std::vector<std::string> getMessages();
    virtual std::string getStatus();

    Step * const parent;
    AIManager& mgr;

    struct Message {
        std::chrono::time_point<std::chrono::steady_clock> timestamp;
        std::string message;
    };
    std::deque<Message> messages;
    spStep currentSubStep;
    std::mutex messagesMutex;
};

class Task : public Step {
public:
    Task(Task* parent, AIManager& mgr, const TaskTemplate& templ);
    virtual ~Task() = default;
    virtual bool step();
    Result safe_run();
    virtual Result run() = 0;
    virtual const char* getName();

    TaskTemplate templ;

    bool decodePosition(const json5pp::value& pos, cv::Point& point, const json5pp::value& args) const;
    bool executeStep(const json5pp::value& step, const json5pp::value& args);
    bool executeWait(const json5pp::value& step, const json5pp::value& args);
    void hardcodedStep(const std::string& step, DetectLevel level, cv::Mat* colorImage = nullptr, cv::Mat* grayImage = nullptr);

    std::string taskName;

    short missCount { 0 };
    short maxMisses { 0 };
    Result result { Result::Created };

};

} //namespace ai

#endif //EDROBOT_TASK_H
