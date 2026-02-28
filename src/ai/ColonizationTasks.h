//
// Created by mkizub on 17.02.2026.
//

#pragma once

#ifndef EDROBOT_COLONIZATIONTASKS_H
#define EDROBOT_COLONIZATIONTASKS_H

#include "Types.h"
#include "Task.h"

namespace ai {

class BaseColonizationTask : public Task {
protected:
    static std::string constructionPrefixes[];

    explicit BaseColonizationTask(const TaskTemplate& templ_) : Task(templ_) {}
    void addDepotInfo(const js::value& dv);

    struct DepotInfo {
        std::string systemName;
        std::string fullName;
        std::string shortName;
        int64_t marketId {};
        Timestamp ravenProjectTimestamp {};
        std::string ravenBuildId;
    };
    struct Demands {
        std::set<Commodity*> specialCommodityList;
        std::map<Commodity*, int> toDeliver;
        std::map<Commodity*, int> toDeliverListed;
        std::map<Commodity*, int> toBuy;
        std::map<Commodity*, int> toBuyListed;
        bool firstListed {false};
        bool exceptListed {false};
        bool onlyListed {false};
    };
    struct MarketInfo {
        std::string systemName;
        std::string dockName;
        gal::spEntity dock;
        spMarket dockMarket;
        int canBuy {};
        int canBuyListed {};
    };

    gal::spEntity getCurrDock();
    gal::spEntity travelTo(std::string systemName, std::string dockName);
    void travelResume();
    void addDemands(DepotInfo& depot, Demands& demands);
    Demands calcDemands();

    MarketInfo checkMarketCanBuy(const std::string& systemName, const std::string& dockName,
                                 const Demands& demands);
    void tradeCommodities(const gal::spEntity& currDock, const Demands& demands,
                          const std::vector<Commodity*>* unnecessaryCargo = nullptr);

    std::vector<DepotInfo> depots;
private:
    std::string destSystemName;
    std::string destDockName;
};

class TaskMyCarrierReserve final : public BaseColonizationTask {
public:
    explicit TaskMyCarrierReserve(const TaskTemplate& templ);
    bool run() final;

private:
    MarketInfo chooseBestMarket(const Demands& demands);
    bool deliverToCarrier();

    std::string myCarrierName;
};


class TaskConstruction final : public BaseColonizationTask {
public:
    explicit TaskConstruction(const TaskTemplate& templ);
    bool run() final;

private:
    MarketInfo chooseBestMarket(const Demands& demands);
    bool deliverToDepot();

    std::vector<Commodity*> unnecessaryCargo;
};



} // ai

#endif //EDROBOT_COLONIZATIONTASKS_H
