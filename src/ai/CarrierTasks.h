//
// Created by mkizub on 11.02.2026.
//

#pragma once

#ifndef EDROBOT_CARRIERTASKS_H
#define EDROBOT_CARRIERTASKS_H

#include "Types.h"
#include "Task.h"

namespace ai {

class TaskMyCarrierUnload final : public Task {
public:
    explicit TaskMyCarrierUnload(const TaskTemplate& templ);
    bool run() final;

    int contributed {};

    std::string getStatus() override;
    enum {
        READY, TO_TRANSFER, UNLOAD, DONE, DONE_NOTHING
    } status {READY};
};

class TaskMyCarrierReserve final : public Task {
public:
    explicit TaskMyCarrierReserve(const TaskTemplate& templ);
    bool run() final;

private:
    struct MarketInfo {
        std::string systemName;
        std::string dockName;
        gal::spEntity dock;
        spMarket dockMarket;
        int canBuy;
    };
    gal::spEntity getCurrDock();
    std::vector<std::pair<Commodity*, int>> calcDemands();
    MarketInfo checkMarket(const std::string& systemName, const std::string& dockName,
                           const std::vector<std::pair<Commodity*, int>>& demands);
    MarketInfo chooseBestMarket(const std::vector<std::pair<Commodity*, int>>& demands);
    bool deliverToCarrier();

    std::string destSystemName;
    std::string destDockName;
    std::string myCarrierName;
};


}

#endif //EDROBOT_CARRIERTASKS_H
