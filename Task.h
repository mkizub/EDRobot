//
// Created by mkizub on 23.05.2025.
//

#ifndef EDROBOT_TASK_H
#define EDROBOT_TASK_H

#include "Master.h"

class Task {
public:
    explicit Task(Master& master) : master(master) {}
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

    void sleep(int milliseconds);
    bool sendKey(const std::string& name, int delay_ms = 35, int pause_ms = 50);
    void check_completed() {
        if (done)
            throw completed_error("aborted");
    }
    bool executeAction(const std::string& actionName, const json5pp::value& args, std::string& expect_goto);
    bool executeStep(const json5pp::value& step, const json5pp::value& args);

    std::atomic<bool> done;
};

class TaskSell final : public Task {
public:
    TaskSell(Master& master, int sells, int items)
        : Task(master), mSells(sells), mItems(items)
    {}
    bool run() final;
private:
    int mSells;
    int mItems;
};

#endif //EDROBOT_TASK_H
