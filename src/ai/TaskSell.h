//
// Created by mkizub on 02.07.2025.
//

#pragma once

#ifndef EDROBOT_TASKSELL_H
#define EDROBOT_TASKSELL_H

#include "Task.h"

namespace ai {

class TaskSell;

class TaskSellAll final : public Task {
public:
    TaskSellAll(Task* parent, AIManager& mgr, const TaskTemplate& templ);
    Result run() final;
private:
    void plan();

    int mChunk;
    std::vector<spTask> sell_archive;
};

class TaskSell final : public Task {
public:
    TaskSell(Task* parent, AIManager& mgr, const TaskTemplate& templ);
    Result run() final;

    Commodity* mCommodity;
    int mTotal;
    int mItems;

    int mSold;
};

} // ai

#endif //EDROBOT_TASKSELL_H
