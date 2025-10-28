//
// Created by mkizub on 02.07.2025.
//

#pragma once

#ifndef EDROBOT_TRADETASKS_H
#define EDROBOT_TRADETASKS_H

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
    bool run() final;
private:
    int mChunk;
    struct SubTask {
        Commodity* commodity {};
        spTask task;
        bool complete {};
        bool failed {};
    };
    std::vector<SubTask> sell_queue;
};

class TaskSell final : public BaseMarketTask {
public:
    TaskSell(Task* parent, AIManager& mgr, const TaskTemplate& templ);
    bool run() final;
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
    bool run() final;
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

class TaskConstr final : public BaseMarketTask {
public:
    TaskConstr(Task* parent, AIManager& mgr, const TaskTemplate& templ);
    bool run() final;

    std::string getStatus() override;
    enum {
        READY, TO_MARKET, UNLOAD
    } status {READY};
};


} // ai

#endif //EDROBOT_TRADETASKS_H
