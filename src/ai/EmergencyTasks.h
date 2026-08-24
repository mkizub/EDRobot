//
// Created by mkizub on 15.08.2026.
//

#pragma once

#ifndef EDROBOT_EMERGENCYTASKS_H
#define EDROBOT_EMERGENCYTASKS_H

namespace ai {

class BaseEmergencyTask : public Task {
protected:
    explicit BaseEmergencyTask(const TaskTemplate& templ_) : Task(templ_) {}

public:
    virtual bool isEmergencyTask() { return true; }

    utc_timer timer;
};

class TaskDebugEmergency : public Task {
public:
    explicit TaskDebugEmergency(const TaskTemplate& templ);
    bool run() final;

    enum {
        READY, DONE
    } status {READY};
    std::string test;
};

class TaskRelogin final : public BaseEmergencyTask {
public:
    explicit TaskRelogin(const TaskTemplate& templ);
    bool run() final;
    bool isTileDisabled();

    std::string getStatus() override;
    enum {
        READY, BLACK, LOGOUT, LOGIN, DONE
    } status {READY};
};

class TaskRebootRepair final : public BaseEmergencyTask {
public:
    explicit TaskRebootRepair(const TaskTemplate& templ);
    bool run() final;

    std::string getStatus() override;
    enum {
        READY, PREPARE, REPAIR, DONE
    } status {READY};
};

class TaskResurrect final : public BaseEmergencyTask {
public:
    explicit TaskResurrect(const TaskTemplate& templ);
    bool run() final;

    std::string getStatus() override;
    enum {
        READY, REPORT, DEPLOY, DONE
    } status {READY};
};

} // namespace ai

#endif //EDROBOT_EMERGENCYTASKS_H
