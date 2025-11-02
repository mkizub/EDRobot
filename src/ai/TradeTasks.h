//
// Created by mkizub on 02.07.2025.
//

#pragma once

#ifndef EDROBOT_TRADETASKS_H
#define EDROBOT_TRADETASKS_H

#include "Task.h"

namespace ai {

class BaseMarketTask : public Task {
public:
    BaseMarketTask(Step* parent, AIManager& mgr, const TaskTemplate& templ_)
        : Task(parent, mgr, templ_)
    {}

    bool clickButton(const char* btn);
    bool moveToWidget(const char* widget);
    void gotoMarketScreen(bool buy);
    bool waitUiState(const std::string& state, std::chrono::seconds duration);
    bool enterTradeDialog(Commodity* commodity, std::string state);
    bool commitTradeDialog(Commodity* commodity, std::string state);
};

class TaskSell final : public BaseMarketTask {
public:
    TaskSell(Step* parent, AIManager& mgr, const TaskTemplate& templ);
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

class TaskSellAll final : public BaseMarketTask {
public:
    TaskSellAll(Step* parent, AIManager& mgr, const TaskTemplate& templ);
    bool run() final;
private:
    int mChunk;
    struct SubTask {
        Commodity* commodity {};
        std::shared_ptr<TaskSell> task;
        bool complete {};
        bool failed {};
    };
    std::vector<SubTask> sell_queue;
};

class TaskBuy final : public BaseMarketTask {
public:
    TaskBuy(Step* parent, AIManager& mgr, const TaskTemplate& templ);
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

class TaskBuyConstr final : public BaseMarketTask {
public:
    TaskBuyConstr(Step* parent, AIManager& mgr, const TaskTemplate& templ);
    bool run() final;

    std::string destSystemName;
    std::string destConstrName;

    struct SubTask {
        Commodity* commodity {};
        std::shared_ptr<TaskBuy> task;
        bool complete {};
        bool failed {};
    };
    std::vector<SubTask> buy_queue;
};

class TaskConstrUnload final : public BaseMarketTask {
public:
    TaskConstrUnload(Step* parent, AIManager& mgr, const TaskTemplate& templ);
    bool run() final;

    std::string getStatus() override;
    enum {
        READY, TO_MARKET, UNLOAD
    } status {READY};
};


} // ai

#endif //EDROBOT_TRADETASKS_H
