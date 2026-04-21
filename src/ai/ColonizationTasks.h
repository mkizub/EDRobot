//
// Created by mkizub on 17.02.2026.
//

#pragma once

#ifndef EDROBOT_COLONIZATIONTASKS_H
#define EDROBOT_COLONIZATIONTASKS_H

#include "Types.h"
#include "Task.h"

class RavenColonial;

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
        struct BuyCounts {
            int* count {};  // first depot
            int total {};   // total of all depots
            int bought {};  // already have in ship of carrier
        };
        struct Depot {
            DepotInfo* info;
            spMarket market;
            std::set<Commodity*> assignedCommodities; // assigned to me
            std::set<Commodity*> ignoreCommodities; // assigned to others
            std::set<Commodity*> buyCommodities; // commodities we can try to buy
        };
        std::vector<Depot> allDepots;
        std::vector<Commodity*> specialCommoditiesList;
        std::set<Commodity*> specialCommodities;
        std::map<Commodity*, int> othersShipsCargo;
        std::map<Commodity*, BuyCounts> toDeliver;
        bool needToBuy {false};
        bool firstListed {false};
        bool exceptListed {false};
        bool onlyListed {false};
        std::vector<int> countsBuffer; // size = allDepots.size()*toDeliver.size()
    };
    enum MarketState { MARKET_INVALID, MARKET_UNKNOWN, MARKET_VALID };
    struct MarketInfo {
        MarketState state;
        std::string systemName;
        std::string dockName;
        gal::spEntity dock;
        spMarket dockMarket;
        std::vector<int> canBuy;
        std::vector<int> canBuyListed;
        int sumCanBuy;
        int sumCanBuyListed;
    };

    static gal::spEntity getCurrDock();
    virtual bool ignoreCarrier(gal::spEntity& dock) { return false; }
    gal::spEntity travelTo(const std::string& systemName, const std::string& dockName);
    void travelResume();
    void addDemands(DepotInfo& depot, Demands& demands);
    void fillDemands(Demands& demands);
    Demands calcDemands();

    MarketInfo checkMarketCanBuy(const std::string& systemName, const std::string& dockName,
                                 const Demands& demands);
    MarketInfo chooseBestMarket(const Demands& demands);
    void tradeCommodities(const gal::spEntity& currDock, const Demands& demands,
                          const std::vector<Commodity*>* unnecessaryCargo = nullptr);

    std::vector<DepotInfo> depots;
    Timestamp timestampRavenShipsCargo;
    js::value ravenShipsCargo;
    std::shared_ptr<RavenColonial> rcInstance;
private:
    std::string destSystemName;
    std::string destDockName;
};

class TaskMyCarrierReserve final : public BaseColonizationTask {
public:
    explicit TaskMyCarrierReserve(const TaskTemplate& templ);
    bool run() final;
    virtual bool ignoreCarrier(gal::spEntity& dock) {
        return dock && (dock->marketId == st::cmdr.fleetCarrierId || dock->nameEq(myCarrierName));
    }

private:
    bool deliverToCarrier();

    std::string myCarrierName;
};


class TaskConstruction final : public BaseColonizationTask {
public:
    explicit TaskConstruction(const TaskTemplate& templ);
    bool run() final;

private:
    bool deliverToDepot(BaseColonizationTask::Demands& demands);

    std::vector<Commodity*> unnecessaryCargo;
};



} // ai

#endif //EDROBOT_COLONIZATIONTASKS_H
