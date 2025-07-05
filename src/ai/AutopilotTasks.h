//
// Created by mkizub on 28.06.2025.
//

#ifndef EDROBOT_AUTOPILOTTASKS_H
#define EDROBOT_AUTOPILOTTASKS_H


#include "Types.h"
#include "Task.h"

namespace ai {

class BaseAutopilotTask : public Task {
protected:
    BaseAutopilotTask(Task* parent, AIManager& mgr, const TaskTemplate& templ);
    void relogin();
};

class TaskDepart : public BaseAutopilotTask {
public:
    TaskDepart(Task* parent, AIManager& mgr, const TaskTemplate& templ);
    Result run() final;

};

class TaskDock : public BaseAutopilotTask {
public:
    TaskDock(Task* parent, AIManager& mgr, const TaskTemplate& templ);
    Result run() final;

    spGameEvent requestDockingPermit();
    bool selectDockingFilters();
    bool lockDockingStation();
    bool flyTowardsTarget();

};

} // namespace ai

#endif //EDROBOT_AUTOPILOTTASKS_H
