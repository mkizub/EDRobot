//
// Created by mkizub on 17.02.2026.
//

#include "../pch.h"

#include "AIManager.h"
#include "ColonizationTasks.h"
#include "AIUtils.h"

#include "../Keyboard.h"
#include "../Galaxy.h"
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
    auto fullName = dv["dock"].as_string_or();
    std::string shortName;
    for (auto& pr : constructionPrefixes) {
        if (fullName.starts_with(pr)) {
            shortName = trim(fullName.substr(pr.size()));
            break;
        }
    }
    auto starSystem = gal::getStarSystem(systemName);
    if (!starSystem)
        throw_failed("Star system '{}' not known", systemName);
    auto depot = starSystem->getDock(fullName);
    if (!depot)
        throw_failed("Construction depot '{}' not known", fullName);
    spMarket depotMarket = gal::getMarket(depot->marketId);
    if (!depotMarket|| depotMarket->items.empty())
        throw_failed("Construction depot '{}' demand is not known", fullName);
    if (!(depot->type == TypeNav::SpaceConstrDepot || depot->type == TypeNav::PlanetaryConstrDepot || depot->type == TypeNav::ColonisationShip || depotMarket->stationType == "ConstrDepot"))
        throw_failed("Site '{}' is not a construction depot", fullName);
    auto& depotInfo = depots.emplace_back(systemName, fullName, shortName);
    depotInfo.marketId = depot->marketId;
    if (depotMarket) {
        depotInfo.ravenBuildId = depotMarket->raven.buildId;
        depotInfo.ravenProjectTimestamp = depotMarket->raven.timestamp;
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

gal::spEntity BaseColonizationTask::travelTo(std::string systemName, std::string dockName) {
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

bool BaseColonizationTask::cargoMissmatch() {
    if (!Cfg.isRavenColonialEnabled() || depots.empty())
        return false;
    Timestamp tm_now = Timestamp::clock::now();
    if (ravenShipsCargo.empty() || (timestampRavenShipsCargo + 30s) < tm_now) {
        spMarket depot;
        for (auto& di : depots) {
            if (di.marketId)
                depot = gal::getMarket(di.marketId);
            if (depot)
                break;
        }
        if (depot) {
            ravenShipsCargo = RavenColonial::queryShipsCargo(depot);
            timestampRavenShipsCargo = tm_now;
        }
    }
    if (ravenShipsCargo.is_array()) {
        // [{"cmdr":"mkz","name":"MK-28P","type":"panthermkii","time":"2026-03-04T03:58:30.8340778+00:00","maxCargo":1236,"cargo":{}},{"cmdr":"mkzu","name":"MK-13P","type":"panthermkii","time":"2026-03-04T04:12:40.7833535+00:00","maxCargo":1216,"cargo":{}}]
        auto currentCargo = CM.getShipCargo();
        for (auto& record : ravenShipsCargo.as_array()) {
            if (st::cmdr.name != record["cmdr"].as_string_or())
                continue;
            for (auto [cid,count] : record["cargo"].key_value()) {
                Commodity* c = Cfg.getCommodityById(cid);
                if (c && c->ship.count != count.as_int_or())
                    return true;
            }
            for (Commodity* c : currentCargo->cargo) {
                if (c->ship.count != record["cargo"][c->nameId].as_int_or())
                    return true;
            }
        }
    }
    return false;
}


void BaseColonizationTask::addDemands(DepotInfo& dv, Demands& demands) {
    spMarket depotMarket = gal::getMarket(dv.marketId);
    if (Cfg.isRavenColonialEnabled()) {
        Timestamp tm_now = Timestamp::clock::now();
        if (!dv.ravenBuildId.empty() && (!depotMarket || (depotMarket->timestamp+30s) < tm_now) && (dv.ravenProjectTimestamp+30s) < tm_now) {
            depotMarket = RavenColonial::updateConstructionDepot(depotMarket);
            dv.ravenProjectTimestamp = depotMarket->raven.timestamp;
        }
        if (ravenShipsCargo.empty() || (timestampRavenShipsCargo + 30s) < tm_now) {
            ravenShipsCargo = RavenColonial::queryShipsCargo(depotMarket);
            timestampRavenShipsCargo = tm_now;
        }
        if (ravenShipsCargo.is_array()) {
            // [{"cmdr":"mkz","name":"MK-28P","type":"panthermkii","time":"2026-03-04T03:58:30.8340778+00:00","maxCargo":1236,"cargo":{}},{"cmdr":"mkzu","name":"MK-13P","type":"panthermkii","time":"2026-03-04T04:12:40.7833535+00:00","maxCargo":1216,"cargo":{}}]
            for (auto& record : ravenShipsCargo.as_array()) {
                if (st::cmdr.name == record["cmdr"].as_string_or() || record["cargo"].empty())
                    continue;
                for (auto [cid,count] : record["cargo"].key_value()) {
                    Commodity* c = Cfg.getCommodityById(cid);
                    if (c)
                        demands.othersShipsCargo[c] += count.as_int_or();
                }
            }
        }
    }
    for (auto& item : depotMarket->items) {
        Commodity* c = item.first;
        if (!demands.specialCommodityList.empty()) {
            if (demands.onlyListed) {
                if (!demands.specialCommodityList.contains(c))
                    continue;
            }
            else if (demands.exceptListed) {
                if (demands.specialCommodityList.contains(c))
                    continue;
            }
        }
        int demand = item.second.demand - item.second.stock;
        if (demands.othersShipsCargo.contains(c))
            demand -= demands.othersShipsCargo[c];
        if (demand > 0) {
            demands.toDeliver[c] += demand;
            if (!demands.specialCommodityList.empty() && demands.specialCommodityList.contains(c))
                demands.toDeliverListed[c] += demand;
        }
    }
}
BaseColonizationTask::Demands BaseColonizationTask::calcDemands() {
    Demands demands;
    {
        Param &p_commodity = templ.get("commodity");
        if (p_commodity.value.is_array()) {
            for (auto &jc: p_commodity.value.as_array()) {
                Commodity *c = Cfg.getCommodityById(jc.as_string_or());
                if (c)
                    demands.specialCommodityList.insert(c);
            }
        }
    }
    if (!demands.specialCommodityList.empty()) {
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

    bool considerCarrier = (templ.id == ED_TASK_CONSTR_RESERVE);
    for (auto& d : demands.toDeliver) {
        Commodity* c = d.first;
        int demand = d.second;
        if (c->ship.count > 0)
            demand -= c->ship.count;
        if (considerCarrier && c->fc.count > 0)
            demand -= c->fc.count;
        if (demand > 0)
            demands.toBuy[c] = demand;
    }
    for (auto& d : demands.toDeliverListed) {
        Commodity* c = d.first;
        int demand = d.second;
        if (c->ship.count > 0)
            demand -= c->ship.count;
        if (considerCarrier && c->fc.count > 0)
            demand -= c->fc.count;
        if (demand > 0)
            demands.toBuyListed[c] = demand;
    }
    for (auto& d : demands.toDeliver) {
        Commodity* c = d.first;
        int toDeliver = d.second;
        int toBuy = 0;
        if (demands.toBuy.contains(c))
            toBuy = demands.toBuy[c];
        LOG(INFO) << std::format("Demand for '{}' ({}): {} to deliver, {} to buy", c->name, c->nameId, toDeliver, toBuy);
    }
    return demands;
}

BaseColonizationTask::MarketInfo BaseColonizationTask::checkMarketCanBuy(
        const std::string& systemName, const std::string& dockName, const Demands& demands)
{
    auto starSystem = gal::getStarSystem(systemName);
    if (!starSystem)
        throw_failed("Star system '{}' not known", systemName);
    auto dock = starSystem->getDock(dockName);
    if (!dock)
        throw_failed("Market '{}' not known", dockName);
    switch (dock->type) {
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
    case TypeNav::PlanetaryStation:
    case TypeNav::PlanetaryPort:
    case TypeNav::Settlement:
        break;
    default:
        notify_error("Site '{}' is not a market", dockName);
        return {};
    }
    spMarket dockMarket = gal::getMarket(dock->marketId);
    if (!dockMarket || dockMarket->items.empty()) {
        return {systemName, dockName, dock, dockMarket, 0, 0};
    }
    int canBuy = 0;
    int canBuyListed = 0;
    for (auto& dp : demands.toBuy) {
        Commodity* c = dp.first;
        int demand = dp.second;
        if (!dockMarket->items.contains(c))
            continue;
        auto& ml = dockMarket->items[c];
        if (ml.isConsumer)
            continue;
        if (ml.stock <= 0)
            continue;
        canBuy += std::min(demand, ml.stock);
        if (!demands.specialCommodityList.empty() && (demands.firstListed || demands.onlyListed)) {
            if (demands.specialCommodityList.contains(c))
                canBuyListed += std::min(demand, ml.stock);
        }
    }
    int freeCargoSpace = st::shipStats.cargoCapacity - st::shipStats.cargo;
    canBuy = std::clamp(canBuy, 0, freeCargoSpace);
    canBuyListed = std::clamp(canBuyListed, 0, freeCargoSpace);
    if (canBuy <= 0)
        return {};
    return {systemName, dockName, dock, dockMarket, canBuy, canBuyListed};
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
        int toBuy;
        int listedOrder;
    };
    std::vector<BuyInfo> list;
    for (auto &d: demands.toBuy) {
        BuyInfo bi {d.first, d.second, 1000};
        if ((demands.firstListed || demands.onlyListed) && !demands.specialCommodityList.empty()) {
            auto it = std::find(demands.specialCommodityList.begin(), demands.specialCommodityList.end(), bi.commodity);
            if (it != demands.specialCommodityList.end())
                bi.listedOrder = std::distance(demands.specialCommodityList.begin(), it);
        }
        list.push_back(bi);
    }
    std::sort(list.begin(),list.end(),[](const auto& a, const auto& b){
        if (a.listedOrder != b.listedOrder)
            return a.listedOrder < b.listedOrder;
        return a.toBuy < b.toBuy;
    });
    for (auto &d: list) {
        int freeCargoSpace = st::shipStats.cargoCapacity - st::shipStats.cargo;
        if (freeCargoSpace <= 0)
            break;
        Commodity *c = d.commodity;
        auto &ml = market->items[c];
        if (!ml.isProducer || ml.stock < 0)
            continue;
        TaskTemplate impl = getTemplate(ED_TASK_MARKET_BUY);
        impl.set("commodity", c->nameId);
        impl.set("amount", d.toBuy);
        run_sub_step(impl.factory(impl));
    }
}

TaskMyCarrierReserve::TaskMyCarrierReserve(const TaskTemplate &templ_)
        : BaseColonizationTask(templ_)
{
    assert (templ.id == ED_TASK_CONSTR_RESERVE);
}

BaseColonizationTask::MarketInfo TaskMyCarrierReserve::chooseBestMarket(const Demands& demands) {
    gal::spEntity currDock = getCurrDock();
    if (currDock && currDock->marketId != st::cmdr.fleetCarrierId && !currDock->nameEq(myCarrierName)) {
        MarketInfo mi = checkMarketCanBuy(gal::getCurrentStarSystem()->systemName, currDock->name, demands);
        if (mi.dock)
            return mi;
    }

    std::vector<MarketInfo> markets;
    Param &p = templ.get("markets");
    if (p.value.is_array()) {
        for (auto &dv: p.value.as_array()) {
            auto systemName = dv["system"].as_string_or();
            auto dockName = dv["dock"].as_string_or();
            MarketInfo mi = checkMarketCanBuy(systemName, dockName, demands);
            if (mi.dock)
                markets.push_back(mi);
        }
    }
    if (markets.empty())
        return {};

    int freeCargoSpace = st::shipStats.cargoCapacity - st::shipStats.cargo;

    // process first-listed commodities
    if (!demands.specialCommodityList.empty() && demands.firstListed) {
        // the first market that can fill full cargo capacity of listed commodities
        for (auto &m: markets) {
            if (m.canBuyListed >= freeCargoSpace)
                return m;
        }
        // then the market that provides more listed goods
        MarketInfo* bestMarket {};
        for (auto& m : markets) {
            if (m.canBuyListed <= 0)
                continue;
            if (!bestMarket || bestMarket->canBuyListed < m.canBuyListed)
                bestMarket = &m;
        }
        if (bestMarket)
            return *bestMarket;
    }

    for (auto& m : markets) {
        if (m.canBuy >= freeCargoSpace)
            return m;
    }
    MarketInfo* bestMarket {};
    for (auto& m : markets) {
        if (m.canBuy <= 0)
            continue;
        if (!bestMarket || bestMarket->canBuy < m.canBuy)
            bestMarket = &m;
    }
    if (bestMarket)
        return *bestMarket;

    return {};
}

bool TaskMyCarrierReserve::deliverToCarrier() {
    RavenColonial::reportShipCargo();
    if (st::shipStats.cargo <= 0)
        return false;

    Param &p = templ.get("carrier");
    travelTo(p.value["system"].as_string_or(), p.value["dock"].as_string_or());

    TaskTemplate unloadImpl = getTemplate(ED_TASK_CARRIER_UNLOAD);
    run_sub_step(unloadImpl.factory(unloadImpl));
    RavenColonial::reportShipCargo();
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
        if (!currDock || demands.toBuy.empty() || (st::shipStats.cargo >= st::shipStats.cargoCapacity)) {
            deliverToCarrier();
        }
    }

    for (;;) {
        if (demands.toDeliver.empty())
            return true;

        MarketInfo mi = chooseBestMarket(demands);
        if (!mi.dock) {
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
        const Param &p_depot = templ.get("depot");
        addDepotInfo(p_depot.value);
        RavenColonial::reportShipCargo();
    }
    else if (cargoMissmatch()) {
        RavenColonial::reportShipCargo();
    }

    travelResume();

    auto demands = calcDemands();
    gal::spEntity currDock = getCurrDock();
    if (st::shipStats.cargo > 0) {
        if (!currDock || demands.toBuy.empty() || (st::shipStats.cargo >= st::shipStats.cargoCapacity)) {
            deliverToDepot();
        }
    }

    for (;;) {
        if (demands.toDeliver.empty())
            return true;

        MarketInfo mi = chooseBestMarket(demands);
        if (!mi.dock) {
            if (!demands.toDeliver.empty() && st::shipStats.cargo > 0) {
                deliverToDepot();
                continue;
            }
            return true;
        }

        currDock = travelTo(mi.systemName, mi.dockName);
        if (currDock && !isConstrDepot(currDock->type)) {
            demands = calcDemands();
            tradeCommodities(currDock, demands, &unnecessaryCargo);
            if (Cfg.isRavenColonialEnabled()) {
                RavenColonial::reportShipCargo();
                sleep(3000);
            }
        }
        demands = calcDemands();

        if (!demands.toDeliver.empty() && st::shipStats.cargo > 0) {
            deliverToDepot();
            demands = calcDemands();
        }
    }
}

BaseColonizationTask::MarketInfo TaskConstruction::chooseBestMarket(const Demands& demands) {
    gal::spEntity currDock = getCurrDock();
    if (currDock && !isConstrDepot(currDock->type)) {
        MarketInfo mi = checkMarketCanBuy(gal::getCurrentStarSystem()->systemName, currDock->name, demands);
        if (mi.dock)
            return mi;
    }

    std::vector<MarketInfo> markets;
    Param &p = templ.get("markets");
    if (p.value.is_array()) {
        for (auto &dv: p.value.as_array()) {
            auto systemName = dv["system"].as_string_or();
            auto dockName = dv["dock"].as_string_or();
            MarketInfo mi = checkMarketCanBuy(systemName, dockName, demands);
            if (mi.dock)
                markets.push_back(mi);
        }
    }
    if (markets.empty())
        return {};

    // first, unload from my carrier
    for (auto& m : markets) {
        if (m.dock->marketId == st::cmdr.fleetCarrierId)
            return m;
    }

    int freeCargoSpace = st::shipStats.cargoCapacity - st::shipStats.cargo;

    // process first-listed commodities
    if (!demands.specialCommodityList.empty() && demands.firstListed) {
        // the first market that can fill full cargo capacity of listed commodities
        for (auto &m: markets) {
            if (m.canBuyListed >= freeCargoSpace)
                return m;
        }
        // then the market that provides more listed goods
        MarketInfo* bestMarket {};
        for (auto& m : markets) {
            if (m.canBuyListed <= 0)
                continue;
            if (!bestMarket || bestMarket->canBuyListed < m.canBuyListed)
                bestMarket = &m;
        }
        if (bestMarket)
            return *bestMarket;
    }

    // then first market that can fill full cargo capacity
    for (auto& m : markets) {
        if (m.canBuy >= freeCargoSpace)
            return m;
    }

    // then the market that provides more goods
    MarketInfo* bestMarket {};
    for (auto& m : markets) {
        if (m.canBuy <= 0)
            continue;
        if (!bestMarket || bestMarket->canBuy < m.canBuy)
            bestMarket = &m;
    }
    if (bestMarket)
        return *bestMarket;

    return {};
}

bool TaskConstruction::deliverToDepot() {
    RavenColonial::reportShipCargo();
    if (st::shipStats.cargo <= 0)
        return false;

    Param &p = templ.get("depot");
    travelTo(p.value["system"].as_string_or(), p.value["dock"].as_string_or());

    TaskTemplate unloadImpl = getTemplate(ED_TASK_CONSTR_UNLOAD);
    run_sub_step(unloadImpl.factory(unloadImpl));

    for (int i=0; i < 5; i++) {
        sleep(1000);
        if (CM.getShipCargo()->count)
            continue;
    }
    unnecessaryCargo = CM.getShipCargo()->cargo;
    RavenColonial::reportShipCargo();

    return true;
}

} // ai