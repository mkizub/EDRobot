//
// Created by mkizub on 02.07.2025.
//

#pragma once

#ifndef EDROBOT_TRADETASKS_H
#define EDROBOT_TRADETASKS_H

#include "Task.h"

namespace ai {

class BaseMarketTask : public Task {
protected:
    explicit BaseMarketTask(const TaskTemplate& templ_) : Task(templ_) {}

public:
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
    explicit TaskSell(const TaskTemplate& templ);
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
    explicit TaskSellAll(const TaskTemplate& templ);
    bool run() final;
    std::string getTitle() override;
private:
    int mChunk;
    json5pp::value mExcept;
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
    explicit TaskBuy(const TaskTemplate& templ);
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

class TaskBuyAll final : public BaseMarketTask {
public:
    explicit TaskBuyAll(const TaskTemplate& templ);
    bool run() final;
    std::string getTitle() override;
private:
    bool addSubTask(const json5pp::value& commodity);
    struct SubTask {
        Commodity* commodity {};
        std::shared_ptr<TaskBuy> task;
        bool complete {};
        bool failed {};
    };
    std::vector<SubTask> buy_queue;
};

class TaskBuyConstr final : public BaseMarketTask {
public:
    explicit TaskBuyConstr(const TaskTemplate& templ);
    bool run() final;

    std::string destSystemName;
    std::string destConstrName;
    bool bulkFirst {};
    bool onlyListed {};
    std::vector<Commodity*> commodities;

    struct SubTask {
        Commodity* commodity {};
        std::shared_ptr<TaskBuy> task;
        int total_demand;
        bool complete {};
        bool failed {};
    };
    std::vector<SubTask> buy_queue;
};

class TaskConstrUnload final : public BaseMarketTask {
public:
    explicit TaskConstrUnload(const TaskTemplate& templ);
    bool run() final;

    int contributed {};

    std::string getStatus() override;
    enum {
        READY, TO_MARKET, UNLOAD, DONE, DONE_NOTHING
    } status {READY};
};

class TaskTradeAt : public Task {
public:
    explicit TaskTradeAt(const TaskTemplate& templ);
    ~TaskTradeAt() override = default;
    bool run() override;
};

class TradeLoopTask : public Task {
public:
    explicit TradeLoopTask(const TaskTemplate& templ);
    ~TradeLoopTask() override = default;
    bool run() override;

    struct MarketInfo {
        std::string system;
        std::string dock;
        bool sell_all;
        bool buy_all;
        std::vector<Commodity*> sell_list;
        std::vector<Commodity*> sell_except;
        std::vector<Commodity*> buy_list;
        std::vector<json5pp::value> sell_tasks;
        std::vector<json5pp::value> buy_tasks;
    };
    std::vector<MarketInfo> markets;
};

} // ai

#endif //EDROBOT_TRADETASKS_H
