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
    BaseMarketTask(const TaskTemplate& templ_) : Task(templ_) {}

    bool clickButton(const char* btn);
    bool moveToWidget(const char* widget);
    void gotoMarketScreen(bool buy);
    bool waitUiState(const std::string& state, std::chrono::seconds duration);
    bool waitMarketEvent(std::chrono::seconds duration);
    bool enterTradeDialog(Commodity* commodity, std::string state, bool force);
    bool commitTradeDialog(Commodity* commodity, std::string state);

    int lastCommitCount {};
};

class TaskSell final : public BaseMarketTask {
public:
    TaskSell(const TaskTemplate& templ);
    bool run() final;
    bool processTradeDialog(bool force);

    Commodity* mCommodity;
    const int mTotal;
    const int mChunk;
    int mSold;
    int mLeft;

    std::string getTitle() override;
    std::string getStatus() override;
    enum {
        READY, TO_MARKET, TO_COMMODITY, TRADING, DONE
    } status {READY};
};

class TaskSellAll final : public BaseMarketTask {
public:
    TaskSellAll(const TaskTemplate& templ);
    bool run() final;
    std::string getTitle() override;
private:
    int mChunk;
    int mSold;
    std::string mExcept;
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
    TaskBuy(const TaskTemplate& templ);
    bool run() final;
    bool processTradeDialog(bool force);

    Commodity* mCommodity;
    const int mTotal;
    int mBought;
    int mLeft;

    std::string getTitle() override;
    std::string getStatus() override;
    enum {
        READY, TO_MARKET, TO_COMMODITY, TRADING, DONE
    } status {READY};
};

class TaskBuyConstr final : public BaseMarketTask {
public:
    TaskBuyConstr(const TaskTemplate& templ);
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
    TaskConstrUnload(const TaskTemplate& templ);
    bool run() final;

    int contributed {};

    std::string getStatus() override;
    enum {
        READY, TO_MARKET, UNLOAD, DONE, DONE_NOTHING
    } status {READY};
};


} // ai

#endif //EDROBOT_TRADETASKS_H
