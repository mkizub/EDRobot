//
// Created by mkizub on 02.07.2025.
//

#pragma once

#ifndef EDROBOT_TASKSELL_H
#define EDROBOT_TASKSELL_H

#include "Task.h"

namespace ai {

class TaskSell;

class BaseMarketTask : public Task {
public:
    BaseMarketTask(Task* parent, AIManager& mgr, const TaskTemplate& templ_)
        : Task(parent, mgr, templ_)
    {}

    bool clickButton(const char* btn);
    void gotoMarketScreen(bool buy);
    bool waitUiState(const std::string& state, std::chrono::seconds duration);
    bool enterTradeDialog(Commodity* commodity, std::string state);
    bool commitTradeDialog(Commodity* commodity, std::string state);
};

class TaskSellAll final : public BaseMarketTask {
public:
    TaskSellAll(Task* parent, AIManager& mgr, const TaskTemplate& templ);
    Result run() final;
private:
    void plan();

    int mChunk;
    std::deque<spTask> sell_queue;
    std::deque<spTask> sell_archive;
};

class TaskSell final : public BaseMarketTask {
public:
    TaskSell(Task* parent, AIManager& mgr, const TaskTemplate& templ);
    Result run() final;
    bool processTradeDialog();

    Commodity* mCommodity;
    const int mTotal;
    const int mChunk;
    int mSold;
    int mLeft;

    std::string getStatus() override;
    enum {
        READY, TO_MARKET, TO_COMMODITY, TRADING
    } status {READY};
};

class TaskBuy final : public BaseMarketTask {
public:
    TaskBuy(Task* parent, AIManager& mgr, const TaskTemplate& templ);
    Result run() final;
    bool processTradeDialog();

    Commodity* mCommodity;
    const int mTotal;
    int mBought;
    int mLeft;

    std::string getStatus() override;
    enum {
        READY, TO_MARKET, TO_COMMODITY, TRADING
    } status {READY};
};


} // ai

#endif //EDROBOT_TASKSELL_H
