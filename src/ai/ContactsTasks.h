//
// Created by mkizub on 27.12.2025.
//

#pragma once

#ifndef EDROBOT_CONTACTS_TASKS_H
#define EDROBOT_CONTACTS_TASKS_H

namespace ai {

class TaskResurrect final : public Task {
public:
    explicit TaskResurrect(const TaskTemplate& templ);
    bool run() final;

    std::string getStatus() override;
    enum {
        READY, REPORT, DEPLOY, DONE
    } status {READY};
};

class TaskAcquirePPC final : public Task {
public:
    explicit TaskAcquirePPC(const TaskTemplate& templ);
    bool run() final;
    Commodity* mCommodity;
};

class TaskDeliverPPC final : public Task {
public:
    explicit TaskDeliverPPC(const TaskTemplate& templ);
    bool run() final;
    Commodity* mCommodity;
};

} // namespace ai

#endif //EDROBOT_CONTACTS_TASKS_H
