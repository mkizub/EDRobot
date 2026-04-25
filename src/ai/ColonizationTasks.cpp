//
// Created by mkizub on 17.02.2026.
//

#include "../pch.h"

#include "AIManager.h"
#include "ColonizationTasks.h"
#include "AIUtils.h"

#include "../Keyboard.h"
#include "../net/RavenColonial.h"

namespace ai {

std::string BaseColonizationTask::constructionPrefixes[] {
        "Orbital Construction Site:",
        "Planetary Construction Site:",
        "System Colonisation Ship",
        "Колонизационный корабль",
};


void BaseColonizationTask::addDepotInfo(const js::value& dv) {
    auto systemName = dv["system"].as_string_or();
    auto starSystem = gal::getStarSystem(systemName);
    if (!starSystem)
        throw_failed("Star system '{}' not known", systemName);
    auto fullName = dv["dock"].as_string_or();
    //if (fullName == "*") {
    //    RavenColonial::carrierGetCargo()
    //}
    std::string shortName;
    for (auto& pr : constructionPrefixes) {
        if (fullName.starts_with(pr)) {
            shortName = trim(fullName.substr(pr.size()));
            break;
        }
    }
    if (starSystem->getDock(shortName))
        return; // construction complete
    bool depotImported = false;
    auto depot = starSystem->getDock(fullName);
    if (!depot) {
        depot = RavenColonial::importConstructionProject(systemName, fullName, shortName);
        if (!depot)
            throw_failed("Construction depot '{}' not known", fullName);
        depotImported = true;
    }
    spMarket depotMarket = gal::getMarket(depot->marketId);
    if (!depotMarket || depotMarket->items.empty()) {
        RavenColonial::importConstructionProject(systemName, fullName, shortName);
        depotMarket = gal::getMarket(depot->marketId);
        if (!depotMarket|| depotMarket->items.empty())
            throw_failed("Construction depot '{}' demand is not known", fullName);
        depotImported = true;
    }
    if (!(depot->type == TypeNav::SpaceConstrDepot || depot->type == TypeNav::PlanetaryConstrDepot || depot->type == TypeNav::ColonisationShip || depotMarket->stationType == "ConstrDepot"))
        throw_failed("Site '{}' is not a construction depot", fullName);
    if (!depotImported) {
        RavenColonial::importConstructionProject(systemName, fullName, shortName);
        depotMarket = gal::getMarket(depot->marketId);
        depotImported = true;
    }
    auto& depotInfo = depots.emplace_back(systemName, fullName, shortName);
    depotInfo.marketId = depot->marketId;
    if (depotMarket && depotMarket->raven) {
        depotInfo.ravenBuildId = depotMarket->raven->buildId;
        depotInfo.ravenProjectTimestamp = depotMarket->raven->timestamp;
    }
}

gal::spEntity BaseColonizationTask::getCurrDock() {
    gal::spEntity dock;
    if (st::dockedAt.marketId)
        dock = gal::getCurrentStarSystem()->getDock(st::dockedAt.marketId);
    if (!dock && !st::dockedAt.stationName.empty())
        dock = gal::getCurrentStarSystem()->getDock(st::dockedAt.stationName);
    return dock;
}

gal::spEntity BaseColonizationTask::travelTo(const std::string& systemName, const std::string& dockName) {
    auto currDock = getCurrDock();
    if (currDock && gal::getCurrentStarSystem()->systemName == systemName && currDock->nameEq(dockName)) {
        destSystemName.clear();
        destDockName.clear();
        return currDock;
    }
    destSystemName = systemName;
    destDockName = dockName;
    TaskTemplate impl = getTemplate(ED_TASK_TRAVEL);
    impl.nm.clear();
    impl.set("dock", js::object({{"system", destSystemName},
                                 {"dock",   destDockName}}));
    bool ok = run_sub_step(impl.factory(impl));
    destSystemName.clear();
    destDockName.clear();
    currDock = getCurrDock();
    if (!ok || !currDock || !currDock->nameEq(dockName) || gal::getCurrentStarSystem()->systemName != systemName)
        throw_trouble("Trouble traveling to {}", destDockName);
    return currDock;
}

void BaseColonizationTask::travelResume() {
    if (st::dockedAt.marketId || destSystemName.empty() || destDockName.empty()) {
        destSystemName.clear();
        destDockName.clear();
        return;
    }
    travelTo(destSystemName, destDockName);
}


void BaseColonizationTask::addDemands(DepotInfo& dv, Demands& demands) {
    spMarket depotMarket = gal::getMarket(dv.marketId);
    if (Cfg.isRavenColonialEnabled()) {
        Timestamp tm_now = Timestamp::clock::now();
        if (!dv.ravenBuildId.empty() && (!depotMarket || (depotMarket->timestamp+30s) < tm_now) && (dv.ravenProjectTimestamp+30s) < tm_now) {
            depotMarket = RavenColonial::updateConstructionDepot(depotMarket);
            if (!depotMarket || !depotMarket->raven)
                return;
            dv.ravenProjectTimestamp = depotMarket->raven->timestamp;
        }
        if (ravenShipsCargo.empty() || (timestampRavenShipsCargo + 30s) < tm_now) {
            if (rcInstance) {
                ravenShipsCargo = rcInstance->queryShipsCargo(depotMarket);
                timestampRavenShipsCargo = tm_now;
            }
        }
        if (ravenShipsCargo.is_array()) {
            // [{"cmdr":"mkz","name":"MK-28P","type":"panthermkii","time":"2026-03-04T03:58:30.8340778+00:00","maxCargo":1236,"cargo":{}},{"cmdr":"mkzu","name":"MK-13P","type":"panthermkii","time":"2026-03-04T04:12:40.7833535+00:00","maxCargo":1216,"cargo":{}}]
            for (auto& record : ravenShipsCargo.as_array()) {
                if (st::cmdr.name == record["cmdr"].as_string_or() || record["cargo"].empty())
                    continue;
                for (auto [cid,count] : record["cargo"].key_value()) {
                    Commodity* c = Cfg.getCommodityById(cid);
                    if (c)
                        demands.othersShipsCargo[c] += (int)count.as_int_or();
                }
            }
        }
    }
    if (!depotMarket->raven || depotMarket->raven->status == "complete")
        return;
    Demands::Depot& ddp = demands.allDepots.emplace_back(&dv, depotMarket);
    for (auto& [cmdr, info]: depotMarket->raven->commanders) {
        for (auto c : info.assigned) {
            const auto& ml = ddp.market->items[c];
            if (ml.demand <= ml.stock)
                continue;
            if (cmdr == st::cmdr.name)
                ddp.assignedCommodities.insert(info.assigned.begin(), info.assigned.end());
            else
                ddp.ignoreCommodities.insert(info.assigned.begin(), info.assigned.end());
        }
    }
    for (auto &item: depotMarket->items) {
        Commodity *c = item.first;
        if (demands.onlyListed) {
            if (!demands.specialCommodities.contains(c) && !ddp.assignedCommodities.contains(c))
                continue;
        } else if (demands.exceptListed) {
            if (demands.specialCommodities.contains(c) && !ddp.assignedCommodities.contains(c))
                continue;
        } else {
            if (!ddp.assignedCommodities.contains(c) && ddp.ignoreCommodities.contains(c))
                continue; // assigned to someone else
        }
        ddp.buyCommodities.insert(c);
        demands.toDeliver.try_emplace(c);
    }
}

void BaseColonizationTask::fillDemands(Demands& demands) {
    // sort depots, move to front depots with assigned commodities, then with listed commodities
    std::stable_sort(demands.allDepots.begin(), demands.allDepots.end(), [demands](const auto& dp1, const auto& dp2)->bool {
        if (!dp1.assignedCommodities.empty() && dp2.assignedCommodities.empty()) {
            for (auto c : dp1.assignedCommodities)
                if (dp1.buyCommodities.contains(c))
                    return true;
        }
        if (dp1.assignedCommodities.empty() && !dp2.assignedCommodities.empty()) {
            for (auto c : dp2.assignedCommodities)
                if (dp1.buyCommodities.contains(c))
                    return true;
        }
        if (demands.onlyListed || demands.firstListed) {
            bool x1 = false;
            bool x2 = false;
            for (auto c : demands.specialCommoditiesList) {
                if (dp1.buyCommodities.contains(c))
                    x1 = true;
                if (dp2.buyCommodities.contains(c))
                    x2 = true;
            }
            if (x1 && !x2)
                return true;
            if (x2 && !x1)
                return false;
        }
        return false;
    });
    demands.countsBuffer.resize(demands.allDepots.size()*demands.toDeliver.size());
    {
        int idx = 0;
        for (auto& [c,cnt] : demands.toDeliver) {
            cnt.bought = 0;
            cnt.total = 0;
            cnt.count = demands.countsBuffer.data() + idx * demands.allDepots.size();
            idx += 1;
        }
    }
    for (int depotIdx=0; depotIdx < demands.allDepots.size(); ++depotIdx) {
        auto& ddp = demands.allDepots[depotIdx];
        for (auto &item: ddp.market->items) {
            Commodity *c = item.first;
            if (!ddp.buyCommodities.contains(c))
                continue;
            int demand = item.second.demand - item.second.stock;
            if (demands.othersShipsCargo.contains(c))
                demand -= demands.othersShipsCargo[c];
            if (demand > 0) {
                Demands::BuyCounts &bc = demands.toDeliver[c];
                bc.total += demand;
                bc.count[depotIdx] += demand;
                ddp.needToDeliver += demand;
            }
        }
    }
    bool considerCarrier = (templ.id == ED_TASK_CONSTR_RESERVE);
    for (auto& d : demands.toDeliver) {
        Commodity* c = d.first;
        auto& demand = d.second;
        if (c->ship.count > 0)
            demand.bought += c->ship.count;
        if (considerCarrier && c->fc.count > 0)
            demand.bought += c->fc.count;
        if (demand.total > demand.bought)
            demands.needToBuy = true;
    }
}

BaseColonizationTask::Demands BaseColonizationTask::calcDemands() {
    Demands demands;
    {
        Param &p_commodity = templ.get("commodity");
        if (p_commodity.value.is_array()) {
            for (auto &jc: p_commodity.value.as_array()) {
                Commodity *c = Cfg.getCommodityById(jc.as_string_or());
                if (c && !contains(demands.specialCommoditiesList, c))
                    demands.specialCommoditiesList.push_back(c);
            }
        }
    }
    if (!demands.specialCommoditiesList.empty()) {
        demands.specialCommodities.insert(demands.specialCommoditiesList.begin(), demands.specialCommoditiesList.end());
        auto mode = templ.get("mode").as_string();
        if (mode == "FirstListed")
            demands.firstListed = true;
        else if (mode == "ExceptListed")
            demands.exceptListed = true;
        else if (mode == "OnlyListed")
            demands.onlyListed = true;
    }

    for (auto& di : depots) {
        addDemands(di, demands);
    }
    fillDemands(demands);

    for (int i=0; i < demands.allDepots.size(); i++) {
        auto& ddp = demands.allDepots[i];
        LOG_INFO("Demand depot {}: {}", i, ddp.info->fullName);
    }
    for (auto& d : demands.toDeliver) {
        Commodity* c = d.first;
        Demands::BuyCounts toDeliver = d.second;
        std::string counts;
        for (int i=0; i < demands.allDepots.size(); i++) {
            auto& ddp = demands.allDepots[i];
            counts += std::format(" {:5}", toDeliver.count[i]);
            if (ddp.assignedCommodities.contains(c) || (!demands.exceptListed && demands.specialCommodities.contains(c)))
                counts += "*";
            else if (ddp.ignoreCommodities.contains(c) || (demands.exceptListed && demands.specialCommodities.contains(c)))
                counts += "x";
            else
                counts += " ";
        }
        LOG_INFO("Demand for {:>36}: {} to deliver, {:5} to buy ({} - {})", c->name,
                 counts, std::max(0, toDeliver.total-toDeliver.bought), toDeliver.total, toDeliver.bought);
    }
    return demands;
}

BaseColonizationTask::MarketInfo BaseColonizationTask::checkMarketCanBuy(
        const std::string& systemName, const std::string& dockName, const Demands& demands)
{
    BaseColonizationTask::MarketInfo mi {MARKET_INVALID, systemName, dockName};
    auto starSystem = gal::getStarSystem(systemName);
    if (!starSystem) {
        notify_warn("Star system '{}' not known", systemName);
        return mi;
    }
    mi.dock = starSystem->getDock(dockName);
    if (!mi.dock) {
        notify_warn("Market '{}' not known", dockName);
        mi.state = MARKET_UNKNOWN;
        return mi;
    }
    switch (mi.dock->type) {
    case TypeNav::SpaceStation:
    case TypeNav::Orbis:
    case TypeNav::Ocellus:
    case TypeNav::Dodec:
    case TypeNav::Coriolis:
    case TypeNav::AsteroidBase:
    case TypeNav::SpaceOutpost:
    case TypeNav::StationMegaShip:
    case TypeNav::FleetCarrier:
    case TypeNav::SquadronCarrier:
    case TypeNav::StrongholdCarrier:
    case TypeNav::PlanetaryPort:
    case TypeNav::Settlement:
        break;
    case TypeNav::Other:
    case TypeNav::Error:
    case TypeNav::NotExplored:
        notify_warn("Site '{}' type is not known", dockName);
        mi.state = MARKET_UNKNOWN;
        return mi;
    default:
        notify_warn("Site '{}' is not a market", dockName);
        return {MARKET_INVALID};
    }
    mi.dockMarket = gal::getMarket(mi.dock->marketId);
    if (!mi.dockMarket || mi.dockMarket->items.empty()) {
        mi.state = MARKET_UNKNOWN;
        return mi;
    }
    std::map<Commodity*,int> need;
    for (auto &[c, counts] : demands.toDeliver) {
        need[c] = counts.total - counts.bought;
    }
    mi.canBuy.resize(demands.allDepots.size(), 0);
    mi.canBuyListed.resize(demands.allDepots.size(), 0);
    for (int depotIdx=0; depotIdx < demands.allDepots.size(); ++depotIdx) {
        auto& ddp = demands.allDepots[depotIdx];
        for (auto &[c, counts] : demands.toDeliver) {
            if (!mi.dockMarket->items.contains(c))
                continue;
            auto &ml = mi.dockMarket->items[c];
            if (ml.isConsumer)
                continue;
            if (ml.stock <= 0)
                continue;
            int toBuy = std::min(need[c], ml.stock);
            if (toBuy <= 0)
                continue;
            bool listed = false;
            if (ddp.assignedCommodities.contains(c))
                listed = true;
            else if (ddp.ignoreCommodities.contains(c))
                continue;
            if ((demands.firstListed || demands.onlyListed) && demands.specialCommodities.contains(c))
                listed = true;
            need[c] -= toBuy;
            mi.canBuy[depotIdx] = toBuy;
            if (listed)
                mi.canBuyListed[depotIdx] = toBuy;
        }
    }
    int freeCargoSpace = st::shipStats.cargoCapacity - st::shipStats.cargo;
    for (int depotIdx=0; depotIdx < demands.allDepots.size(); ++depotIdx) {
        if (mi.canBuy[depotIdx] > freeCargoSpace)
            mi.canBuy[depotIdx] = freeCargoSpace;
        if (mi.canBuyListed[depotIdx] > freeCargoSpace)
            mi.canBuyListed[depotIdx] = freeCargoSpace;
        mi.sumCanBuy += mi.canBuy[depotIdx];
        mi.sumCanBuyListed += mi.canBuyListed[depotIdx];
    }
    if (mi.sumCanBuy > freeCargoSpace)
        mi.sumCanBuy = freeCargoSpace;
    if (mi.sumCanBuyListed > freeCargoSpace)
        mi.sumCanBuyListed = freeCargoSpace;
    if (mi.sumCanBuy > 0)
        mi.state = MARKET_VALID;
    return mi;
}

int canBuyAtMarket(spMarket& market, Commodity* c) {
    auto it = market->items.find(c);
    if (it == market->items.end())
        return 0;
    auto &ml = it->second;
    if (!ml.isProducer || ml.stock < 0)
        return 0;
    return ml.stock;
}

void BaseColonizationTask::tradeCommodities(const gal::spEntity& currDock, const Demands& demands,
                                            const std::vector<Commodity*>* unnecessaryCargo)
{
    if (unnecessaryCargo && !unnecessaryCargo->empty()) {
        gotoMarketScreen(false);
        for (auto* c: *unnecessaryCargo) {
            if (demands.toDeliver.contains(c))
                continue;
            TaskTemplate impl = getTemplate(ED_TASK_MARKET_SELL);
            impl.set("commodity", c->nameId);
            run_sub_step(impl.factory(impl));
        }
    }
    gotoMarketScreen(true);
    spMarket market = gal::getMarket(currDock->marketId);
    struct BuyInfo {
        Commodity* commodity;
        int buy;
        int order;
        int dp_buy;
    };
    // first buy assigned or listed commodities
    // then buy others
    std::vector<BuyInfo> list;
    for (auto& [commodity, buy]: demands.toDeliver) {
        if (buy.total <= buy.bought)
            continue;
        BuyInfo bi {commodity};
        list.push_back(bi);
    }
    int freeCargoSpace = st::shipStats.cargoCapacity - st::shipStats.cargo;
    for (int depotIdx=0; depotIdx < demands.allDepots.size(); ++depotIdx) {
        auto& ddp = demands.allDepots[depotIdx];
        for (auto& bi : list) {
            if (ddp.assignedCommodities.contains(bi.commodity))
                bi.order = 0;
            else if (!demands.exceptListed && demands.specialCommodities.contains(bi.commodity)) {
                auto it = std::find(demands.specialCommoditiesList.begin(), demands.specialCommoditiesList.end(), bi.commodity);
                bi.order = 1 + (int)std::distance(demands.specialCommoditiesList.begin(), it);
            }
            else if (!ddp.ignoreCommodities.contains(bi.commodity))
                bi.order = 1000;
            else
                bi.order = 2000;
            bi.dp_buy = demands.toDeliver.at(bi.commodity).count[depotIdx];
            if (bi.dp_buy <= 0)
                bi.dp_buy = INT_MAX;
        }
        std::stable_sort(list.begin(),list.end(),[demands,depotIdx](const auto& a, const auto& b){
            if (a.order != b.order)
                return a.order < b.order;
            return a.dp_buy < b.dp_buy;
        });
        for (auto &bi: list) {
            if (freeCargoSpace <= 0)
                break;
            int canBuy = canBuyAtMarket(market, bi.commodity);
            int buyMore = std::min({canBuy, freeCargoSpace, demands.toDeliver.at(bi.commodity).count[depotIdx]});
            if (buyMore > 0) {
                bi.buy += buyMore;
                freeCargoSpace -= buyMore;
            }
        }
        int nextDemand = 1000000;
        if (depotIdx+1 < demands.allDepots.size())
            nextDemand = demands.allDepots[depotIdx+1].needToDeliver;
        if (freeCargoSpace > nextDemand && freeCargoSpace <= 0.25 * st::shipStats.cargoCapacity)
            break;
    }
    std::erase_if(list, [](const auto& bi) {
        return bi.buy <= 0;
    });
    std::stable_sort(list.begin(),list.end(),[](const auto& a, const auto& b){
        return a.buy < b.buy;
    });
    for (auto &d: list) {
        freeCargoSpace = st::shipStats.cargoCapacity - st::shipStats.cargo;
        if (freeCargoSpace <= 0)
            break;
        Commodity *c = d.commodity;
        auto &ml = market->items[c];
        if (!ml.isProducer || ml.stock < 0)
            continue;
        TaskTemplate impl = getTemplate(ED_TASK_MARKET_BUY);
        impl.set("commodity", c->nameId);
        impl.set("amount", d.buy);
        run_sub_step(impl.factory(impl));
    }
}

BaseColonizationTask::MarketInfo BaseColonizationTask::chooseBestMarket(const Demands& demands) {
    gal::spEntity currDock = getCurrDock();
    if (currDock && !isConstrDepot(currDock->type) && !ignoreCarrier(currDock)) {
        MarketInfo mi = checkMarketCanBuy(gal::getCurrentStarSystem()->systemName, currDock->name, demands);
        if (mi.state != MARKET_INVALID && mi.sumCanBuy > 0)
            return mi;
    }

    std::vector<MarketInfo> markets;
    Param &p = templ.get("markets");
    if (p.value.is_array()) {
        for (auto &dv: p.value.as_array()) {
            auto systemName = dv["system"].as_string_or();
            auto dockName = dv["dock"].as_string_or();
            MarketInfo mi = checkMarketCanBuy(systemName, dockName, demands);
            if (mi.state != MARKET_INVALID)
                markets.push_back(mi);
        }
    }
    if (markets.empty())
        return {MARKET_INVALID};

    // first, unload from carriers
    for (auto& mi : markets) {
        if (mi.dock && mi.dock->type == TypeNav::FleetCarrier && !ignoreCarrier(mi.dock))
            return mi;
    }

    MarketInfo* bestMarket {};
    // process assigned/listed commodities
    if (!demands.specialCommodities.empty() && demands.firstListed) {
        // then the market that provides more listed goods
        for (int depotIdx=0; depotIdx < demands.allDepots.size(); ++depotIdx) {
            for (auto& mi : markets) {
                if (mi.canBuyListed[depotIdx] <= 0)
                    continue;
                if (!bestMarket || bestMarket->canBuyListed[depotIdx] < mi.canBuyListed[depotIdx])
                    bestMarket = &mi;
            }
            if (bestMarket)
                return *bestMarket;
        }
    }
    // process all commodities
    for (int depotIdx=0; depotIdx < demands.allDepots.size(); ++depotIdx) {
        for (auto& mi : markets) {
            if (mi.state == MARKET_UNKNOWN)
                return mi; // investigate this dock
            if (mi.canBuy[depotIdx] <= 0)
                continue;
            if (!bestMarket || bestMarket->canBuy[depotIdx] < mi.canBuy[depotIdx])
                bestMarket = &mi;
        }
        if (bestMarket)
            return *bestMarket;
    }

    return {MARKET_INVALID};
}

TaskMyCarrierReserve::TaskMyCarrierReserve(const TaskTemplate &templ_)
        : BaseColonizationTask(templ_)
{
    assert (templ.id == ED_TASK_CONSTR_RESERVE);
}

bool TaskMyCarrierReserve::deliverToCarrier() {
    if (st::shipStats.cargo <= 0)
        return false;

    Param &p = templ.get("carrier");
    travelTo(p.value["system"].as_string_or(), p.value["dock"].as_string_or());

    TaskTemplate unloadImpl = getTemplate(ED_TASK_CARRIER_UNLOAD);
    run_sub_step(unloadImpl.factory(unloadImpl));
    return true;
}

bool TaskMyCarrierReserve::run() {
    if (!st::cmdr.fleetCarrierId)
        throw_failed("Pilot has no fleet carrier");
    if (myCarrierName.empty())
        myCarrierName = templ.get("carrier").value["dock"].as_string_or();
    if (depots.empty()) {
        const Param &p_depots = templ.get("depots");
        for (auto &dv: p_depots.value.as_array_or())
            addDepotInfo(dv);
    }

    travelResume();

    auto demands = calcDemands();
    gal::spEntity currDock = getCurrDock();
    if (st::shipStats.cargo > 0) {
        if (!currDock || !demands.needToBuy || (st::shipStats.cargo >= st::shipStats.cargoCapacity)) {
            deliverToCarrier();
        }
    }

    for (;;) {
        if (demands.toDeliver.empty())
            return true;

        MarketInfo mi = chooseBestMarket(demands);
        if (mi.state == MARKET_INVALID) {
            if (!demands.toDeliver.empty() && st::shipStats.cargo > 0) {
                deliverToCarrier();
                continue;
            }
            return true;
        }

        currDock = travelTo(mi.systemName, mi.dockName);
        if (currDock && currDock->marketId != st::cmdr.fleetCarrierId && !currDock->nameEq(myCarrierName)) {
            tradeCommodities(currDock, demands);
        }
        demands = calcDemands();

        if (!demands.toDeliver.empty() && st::shipStats.cargo > 0) {
            deliverToCarrier();
            demands = calcDemands();
        }
    }
}

TaskConstruction::TaskConstruction(const TaskTemplate &templ)
        : BaseColonizationTask(templ) {
    assert (templ.id == ED_TASK_CONSTR_BUILD);
}

bool TaskConstruction::run() {
    if (depots.empty()) {
        const Param &p_depots = templ.get("depots");
        for (auto &dv: p_depots.value.as_array_or())
            addDepotInfo(dv);
    }
    if (Cfg.isRavenColonialEnabled()) {
        for (auto &di: depots)
            RavenColonial::linkCmdr(di.marketId);
    }
    if (!rcInstance)
        rcInstance = RavenColonial::newInstance();

    travelResume();

    auto demands = calcDemands();
    gal::spEntity currDock = getCurrDock();
    if (st::shipStats.cargo > 0) {
        if (currDock && isConstrDepot(currDock->type))
            deliverToDepot(demands);
        else if (!currDock || !demands.needToBuy || (st::shipStats.cargo >= st::shipStats.cargoCapacity))
            deliverToDepot(demands);
    }

    while (!demands.toDeliver.empty()) {
        MarketInfo mi = chooseBestMarket(demands);
        if (mi.state == MARKET_INVALID) {
            if (!demands.toDeliver.empty() && st::shipStats.cargo > 0) {
                deliverToDepot(demands);
                continue;
            }
            break;
        }

        currDock = travelTo(mi.systemName, mi.dockName);
        if (currDock && !isConstrDepot(currDock->type)) {
            demands = calcDemands();
            tradeCommodities(currDock, demands, &unnecessaryCargo);
        }
        demands = calcDemands();

        if (!demands.toDeliver.empty() && st::shipStats.cargo > 0) {
            deliverToDepot(demands);
            demands = calcDemands();
        }
    }
    return true;
}

bool TaskConstruction::deliverToDepot(BaseColonizationTask::Demands& demands) {
    if (st::shipStats.cargo <= 0)
        return false;

    for (auto& dp : demands.allDepots) {
        auto market = dp.market;
        if (!market)
            market = gal::getMarket(dp.info->marketId);
        if (!market || !market->raven || !(market->raven->status.empty() || market->raven->status == "build"))
            continue;
        bool hasCommodityToDeliver = false;
        for (auto& p : market->items) {
            auto commodity = p.first;
            if (commodity->ship.count <= 0)
                continue;
            auto& ml = p.second;
            if (ml.demand <= ml.stock)
                continue;
            hasCommodityToDeliver = true;
            break;
        }
        if (!hasCommodityToDeliver)
            continue;

        travelTo(dp.info->systemName, dp.info->fullName);

        TaskTemplate unloadImpl = getTemplate(ED_TASK_CONSTR_UNLOAD);
        unloadImpl.set("continue", true);
        run_sub_step(unloadImpl.factory(unloadImpl));

        for (int i=0; i < 5; i++) {
            sleep(1000);
            if (CM.getShipCargo()->count)
                continue;
        }
    }

    unnecessaryCargo = CM.getShipCargo()->cargo;

    return true;
}

} // ai