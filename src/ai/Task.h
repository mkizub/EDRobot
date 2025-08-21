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
    Result safe_run();
    virtual Result run() = 0;
    virtual Result run_sub_task(spTask& task);

    Task const * parent;
    AIManager& mgr;
    TaskTemplate templ;

    void sleep(int milliseconds, bool precise=false) const;
    bool sendKey(const std::string& name, int delay_ms = 35, int pause_ms = 50, bool precise=false) const;
    bool hasAxis(const std::string& name) const;
    bool sendAxis(const std::string& name, double value) const;
    bool sendAxis(const KeyBindings& bindings, double value) const;
    bool sendMouseMove(const cv::Point& point, int pause_ms = 50, bool absolute = true) const;
    bool sendMouseClick(const cv::Point& point, int delay_ms = 35, int pause_ms = 50) const;
    bool decodePosition(const json5pp::value& pos, cv::Point& point, const json5pp::value& args) const;
    void check_interrupted() const;
    bool executeAction(const std::string& actionName, const json5pp::value& args = json5pp::value());
    bool executeStep(const json5pp::value& step, const json5pp::value& args);
    bool executeWait(const json5pp::value& step, const json5pp::value& args);
    void hardcodedStep(const std::string& step, DetectLevel level, cv::Mat* colorImage = nullptr, cv::Mat* grayImage = nullptr);

    void addMessage(const char* msg);
    void addMessage(const std::string& msg);
    std::vector<std::string> getMessages();
    void notifyProgress(const char* msg);
    void notifyProgress(const std::string& msg);
    [[noreturn]] void notifyError(const char* msg, Result result);
    [[noreturn]] void notifyError(const std::string& msg, Result result);

    [[noreturn]] void task_return(Result result);
    [[noreturn]] void task_return(Result result, const char* msg);

    std::string taskName;
    json5pp::value taskActions;
    std::string fromState;
    std::string destState;

    std::deque<spTask> sub_tasks;

    short missCount { 0 };
    short maxMisses { 0 };
    Result result { Result::Created };

    struct Message {
        std::chrono::time_point<std::chrono::steady_clock> timestamp;
        std::string message;
    };
    std::deque<Message> messages;
    spTask currentSubTask;
    std::mutex messagesMutex;
};

} //namespace ai

#endif //EDROBOT_TASK_H
