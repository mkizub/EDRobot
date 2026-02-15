//
// Created by mkizub on 11.02.2026.
//

#include "../pch.h"

#include "AIManager.h"
#include "CarrierTasks.h"
#include "AIUtils.h"

#include "../Keyboard.h"

namespace ai {

inline void sendUiBack(int pause=1000) {
    if (pause >= 500 && Cfg.isHeadlookSmoothing())
        pause = 250;
    kbd::send("UI_Back", 0, pause);
}

int getShipPageIndex(const std::string &page_name) {
    int pageIndex = -1;
    if (page_name == "mod-main")
        pageIndex = 0;
    else if (page_name == "mod-modules")
        pageIndex = 1;
    else if (page_name == "mod-weapon")
        pageIndex = 2;
    else if (page_name == "mod-ship-info")
        pageIndex = 3;
    else if (page_name == "mod-eq-cargo")
        pageIndex = 4;
    else if (page_name == "mod-storage")
        pageIndex = 5;
    else if (page_name == "mod-state")
        pageIndex = 6;
    else if (page_name == "mod-playlist")
        pageIndex = 7;
    else
        LOG(ERROR) << "Ship page '" << page_name << "' not known";
    return pageIndex;
}

bool gotoShipPage(const std::string &page_name, bool required) {
    int targetPageIndex = getShipPageIndex(page_name);
    if (targetPageIndex < 0)
        ai::throw_failed("Bad ship panel page: {}", page_name);

    for (int i = 0; i < 6 && !ai::uiState.match("scr-right-panel:" + page_name); i++) {
        ai::detectEDState(DetectLevel::Buttons);
        LOG(DEBUG) << "Goto '" << page_name << "'...";

        if (ai::uiState.guiFocus == GuiFocus::None) {
            LOG(DEBUG) << "FocusRightPanel...";
            kbd::send("FocusRightPanel", 0, 1500);
            continue;
        }
        if (ai::uiState.guiFocus == GuiFocus::Right && !ai::uiState.screen) {
            if ((i & 1) == 0)
                rollBlindCompass();
            else
                sendUiBack();
            continue;
        }
        if (!ai::uiState.match("scr-right-panel:*")) {
            LOG(DEBUG) << "FocusRightPanel...";
            kbd::send("FocusRightPanel", 0, 1500);
            continue;
        }
        if (ai::uiState.match("scr-right-panel:mod-transfer")) {
            sendUiBack();
            continue;
        }
        std::vector<std::string> segments = ai::uiState.splitPath();
        if (segments.size() < 2)
            ai::throw_failed("Expecting 2 segments in {}", ai::uiState.to_string());
        int currentPageIndex = getShipPageIndex(segments[1]);
        if (currentPageIndex < 0)
            ai::throw_failed("Bad ship panel page: {}", segments[1]);
        int dist = targetPageIndex - currentPageIndex;
        if (dist >= 0) {
            for (int j = 0; j < dist; j++)
                kbd::send("CycleNextPanel", 0, 250);
        } else {
            for (int j = 0; j < -dist; j++)
                kbd::send("CyclePreviousPanel", 0, 250);
        }
    }
    if (!ai::uiState.match("scr-right-panel:" + page_name)) {
        if (required)
            ai::throw_trouble("Unexpected scr-right-panel: {}", ai::uiState.to_string());
        return false;
    }
    return true;
}


TaskMyCarrierUnload::TaskMyCarrierUnload(const TaskTemplate &templ)
    : Task(templ)
{
    assert (templ.id == ED_TASK_CARRIER_UNLOAD);
}

bool TaskMyCarrierUnload::run() {
    if (st::shipStats.cargo <= 0 && (!st::currentCargo || st::currentCargo->count <= 0)) {
        status = DONE_NOTHING;
        return true;
    }
    if (!st::cmdr.fleetCarrierId || st::dockedAt.marketId != st::cmdr.fleetCarrierId) {
        throw_failed("Not docked at own carrier");
        return false;
    }
    status = TO_TRANSFER;
    gotoShipPage("mod-eq-cargo", true);
    for (int i=0; i < 3; i++) {
        kbd::send("UI_Up", 0, 250);
        kbd::send("UI_Right", 0, 250);
    }
    kbd::send("UI_Select", 0, 500);

    ai::detectEDState(DetectLevel::Buttons);
    if (!ai::uiState.match("scr-right-panel:mod-transfer"))
        throw_trouble("Cannot enter transfer panel");

    status = UNLOAD;
    kbd::send("UI_Up", 1500, 500);
    kbd::send("UI_Up", 0, 500);
    kbd::send("UI_Left", 0, 250);
    kbd::send("UI_Select", 0, 500);
    kbd::send("UI_Select", 0, 1500);

    waitMarketEvent(4s);
    if (Cfg.marketEvent && Cfg.marketEvent->event == "CargoTransfer") {
        for (auto& item : Cfg.marketEvent->data["Transfers"].as_array()) {
            contributed += item.at("Count",0).as_integer();
        }
    }
    sendUiBack();
    sendUiBack();
    status = DONE;
    return true;
}


std::string TaskMyCarrierUnload::getStatus() {
    switch (status) {
    case READY:
        return {};
    case TO_TRANSFER:
        return _gt("Going to transfer page");
    case UNLOAD:
        return _gt("Unloading");
    case DONE:
        return lc_format("Unloaded {} items", contributed);
    case DONE_NOTHING:
        return _gt("Nothing to unload");
    }
    return {};
}

TaskMyCarrierReserve::TaskMyCarrierReserve(const TaskTemplate &templ)
        : Task(templ) {
    assert (templ.id == ED_TASK_CARRIER_RESERVE);
}

gal::spEntity TaskMyCarrierReserve::getCurrDock() {
    gal::spEntity dock;
    if (st::dockedAt.marketId)
        dock = gal::getCurrentStarSystem()->getDock(st::dockedAt.marketId);
    if (!dock && !st::dockedAt.stationName.empty())
        dock = gal::getCurrentStarSystem()->getDock(st::dockedAt.stationName);
    return dock;
}

std::vector<std::pair<Commodity*, int>> TaskMyCarrierReserve::calcDemands() {
    std::set<Commodity*> commodities;
    Param &p_commodity = templ.get("commodity");
    if (p_commodity.value.is_array()) {
        for (auto &jc: p_commodity.value.as_array()) {
            Commodity* c = Cfg.getCommodityById(jc.asif_string());
            if (c)
                commodities.insert(c);
        }
    }
    bool onlyListed = templ.get("mode").as_string() == "OnlyListed";

    std::map<Commodity*, int> demandsMap;
    Param &p_depots = templ.get("depots");
    if (p_depots.value.is_array()) {
        for (auto &dv: p_depots.value.as_array()) {
            auto& systemName = dv["system"].asif_string();
            auto& dockName = dv["dock"].asif_string();
            auto starSystem = gal::getStarSystem(systemName);
            if (!starSystem)
                throw_failed("Star system '{}' not known", systemName);
            auto depot = starSystem->getDock(dockName);
            if (!depot)
                throw_failed("Construction depot '{}' not known", dockName);
            spMarket depotMarket = gal::getMarket(depot->marketId);
            if (!depotMarket|| depotMarket->items.empty())
                throw_failed("Construction depot '{}' demand is not known", dockName);
            if (!(depot->type == TypeNav::SpaceConstrDepot || depot->type == TypeNav::PlanetaryConstrDepot || depot->type == TypeNav::ColonisationShip || depotMarket->stationType == "ConstrDepot"))
                throw_failed("Site '{}' is not a construction depot", dockName);
            for (auto& item : depotMarket->items) {
                Commodity* c = item.first;
                if (!commodities.empty()) {
                    if (onlyListed) {
                        if (!commodities.contains(c))
                            continue;
                    } else {
                        if (commodities.contains(c))
                            continue;
                    }
                }
                int demand = item.second.demand - item.second.stock;
                demand -= c->ship.count + c->fc.count;
                if (demand > 0)
                    demandsMap[c] += demand;
            }
        }
    }

    std::vector<std::pair<Commodity*, int>> demandsArray;
    for (auto& d : demandsMap) {
        demandsArray.emplace_back(d.first, d.second);
    }
    std::sort(demandsArray.begin(), demandsArray.end(), [](auto& a, auto& b) {
        return a.second < b.second;
    });
    for (auto& d : demandsArray) {
        LOG(INFO) << std::format("Demand for '{}' ({}): {}", d.first->name, d.first->nameId, d.second);
    }
    return demandsArray;
}

TaskMyCarrierReserve::MarketInfo TaskMyCarrierReserve::checkMarket(
        const std::string& systemName, const std::string& dockName,
        const std::vector<std::pair<Commodity*, int>>& demands)
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
        return {systemName, dockName, dock, dockMarket, 0};
    }
    int canBuy = 0;
    for (auto& dp : demands) {
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
    }
    if (canBuy <= 0)
        return {};
    return {systemName, dockName, dock, dockMarket, canBuy};
}

TaskMyCarrierReserve::MarketInfo TaskMyCarrierReserve::chooseBestMarket(const std::vector<std::pair<Commodity*, int>>& demands) {
    gal::spEntity currDock = getCurrDock();
    if (currDock && currDock->marketId != st::cmdr.fleetCarrierId && !currDock->nameEq(myCarrierName)) {
        MarketInfo mi = checkMarket(gal::getCurrentStarSystem()->systemName, currDock->name, demands);
        if (mi.dock)
            return mi;
    }

    std::vector<MarketInfo> markets;
    Param &p = templ.get("markets");
    if (p.value.is_array()) {
        for (auto &dv: p.value.as_array()) {
            auto& systemName = dv["system"].asif_string();
            auto& dockName = dv["dock"].asif_string();
            MarketInfo mi = checkMarket(systemName, dockName, demands);
            if (mi.dock)
                markets.push_back(mi);
        }
    }
    if (markets.empty())
        return {};

    int freeCargoSpace = st::shipStats.cargoCapacity - st::shipStats.cargo;
    for (auto& m : markets) {
        if (m.canBuy >= freeCargoSpace)
            return m;
    }
    MarketInfo* bestMarket {};
    for (auto& m : markets) {
        if (!bestMarket || bestMarket->canBuy > m.canBuy)
            bestMarket = &m;
    }
    if (bestMarket)
        return *bestMarket;

    return {};
}

bool TaskMyCarrierReserve::deliverToCarrier() {
    if (st::shipStats.cargo <= 0)
        return false;
    Param &p_carrier = templ.get("carrier");
    TaskTemplate travelImpl = getTemplate(ED_TASK_TRAVEL);
    travelImpl.set("dock", p_carrier.value);
    run_sub_step(travelImpl.factory(travelImpl));
    auto currDock = getCurrDock();
    if (!currDock || currDock->marketId != st::cmdr.fleetCarrierId || !currDock->nameEq(myCarrierName))
        throw_trouble("Trouble traveling to carrier {}", myCarrierName);

    TaskTemplate unloadImpl = getTemplate(ED_TASK_CARRIER_UNLOAD);
    run_sub_step(unloadImpl.factory(unloadImpl));

    return true;
}

bool TaskMyCarrierReserve::run() {
    if (!st::cmdr.fleetCarrierId)
        throw_failed("Pilot has no fleet carrier");
    if (myCarrierName.empty())
        myCarrierName = templ.get("carrier").value["dock"].asif_string();

    auto demands = calcDemands();
    gal::spEntity currDock = getCurrDock();
    if (destSystemName.empty() || destDockName.empty() || st::shipStats.cargo > 0) {
        deliverToCarrier();
        currDock = getCurrDock();
    }

    for (;;) {
        if (demands.empty())
            return true;

        if (destSystemName.empty() || destDockName.empty()) {
            MarketInfo mi = chooseBestMarket(demands);
            if (!mi.dock)
                return true;
            if (mi.dock != currDock) {
                destSystemName = mi.systemName;
                destDockName = mi.dockName;
            }
        }

        if (!destSystemName.empty() && !destDockName.empty()) {
            TaskTemplate impl = getTemplate(ED_TASK_TRAVEL);
            impl.set("dock", json5pp::object({{"system", destSystemName},
                                              {"dock",   destDockName}}));
            run_sub_step(impl.factory(impl));
            currDock = getCurrDock();
            destSystemName.clear();
            if (!currDock || !currDock->nameEq(destDockName))
                throw_trouble("Trouble traveling to market {}", destDockName);
            destDockName.clear();
        }

        currDock = getCurrDock();
        if (currDock && currDock->marketId != st::cmdr.fleetCarrierId && !currDock->nameEq(myCarrierName)) {
            gotoMarketScreen(true);
            spMarket market = gal::getMarket(currDock->marketId);
            for (auto &d: demands) {
                int freeCargoSpace = st::shipStats.cargoCapacity - st::shipStats.cargo;
                if (freeCargoSpace <= 0)
                    break;
                Commodity *c = d.first;
                auto &ml = market->items[c];
                if (!ml.isProducer || ml.stock < 0)
                    continue;
                TaskTemplate impl = getTemplate(ED_TASK_MARKET_BUY);
                impl.set("commodity", c->nameId);
                impl.set("amount", d.second);
                run_sub_step(impl.factory(impl));
            }
        }
        demands = calcDemands();

        if (st::shipStats.cargo > 0) {
            deliverToCarrier();
            currDock = getCurrDock();
        }
        demands = calcDemands();
    }

    return false;
}

} // namespace ai
