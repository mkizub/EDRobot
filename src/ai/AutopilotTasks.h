//
// Created by mkizub on 28.06.2025.
//

#ifndef EDROBOT_AUTOPILOTTASKS_H
#define EDROBOT_AUTOPILOTTASKS_H


#include "Types.h"
#include "Task.h"

namespace ai {


class TaskDepart : public Task {
public:
    TaskDepart(Task* parent, AIManager& mgr, const TaskTemplate& templ);
    Result run() final;

};

} // namespace ai

#endif //EDROBOT_AUTOPILOTTASKS_H
