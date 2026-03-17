//
// Created by mkizub on 04.03.2026.
//

#include "pch.h"

#include "CargoManager.h"

#include "Galaxy.h"
#include "ui/UIManager.h"
#include "net/RavenColonial.h"

CargoManager& CM = CargoManager::getInstance();

CargoManager& CargoManager::getInstance() {
    static CargoManager cm;
    return cm;
}

CargoManager::CargoManager() {
    shipCargo = std::shared_ptr<ShipCargo>(new ShipCargo({
        .cargo = {},
        .timestamp = {},
        .count = 0,
        .vessel = "Ship",
        }));
    carrierCargo = std::shared_ptr<ShipCargo>(new ShipCargo({
        .cargo = {},
        .timestamp = {},
        .count = 0,
        .vessel = "FleetCarrier",
        }));
//    srvCargo = std::shared_ptr<ShipCargo>(new ShipCargo({
//        .cargo = {},
//        .timestamp = {},
//        .count = 0,
//        .vessel = "SRV",
//        }));
}

bool CargoManager::loadShipCargo(spGameEvent ge) {
    if (timestampShip > ge->timestamp)
        return false;

    Timestamp file_timestamp = ge->timestamp;
    if ((ge->data["Count"].is_int() && ge->data["Count"].as_int() != 0) || ge->data["Inventory"].empty()) {
        js::value jv;
        try {
            std::ifstream cargoFile(Cfg.mEDLogsPath + L"/Cargo.json", std::ifstream::in);
            if (cargoFile.fail()) {
                LOG(ERROR) << "Cannot read file: " << (Cfg.mEDLogsPath + L"/Cargo.json");
                return false;
            }
            jv = js::parse5(cargoFile);
            cargoFile.close();
        } catch (...) {
            LOG(ERROR) << "Failed to read/parse Cargo.json";
            return false;
        }
        if (jv.empty())
            return false;
        Timestamp timestamp;
        if (!parseTimestamp(jv, timestamp) || timestamp < ge->timestamp || timestamp < timestampShip)
            return false;
        file_timestamp = timestamp;
        const_cast<js::value&>(ge->data) = jv;
    }

    if (ge->data["Vessel"].as_string_or() != "Ship")
        return false;

    {
        std::scoped_lock<std::mutex> lock(cargoMutex);
        std::set<Commodity*> eventCargo;
        auto items = ge->data["Inventory"].as_array_or();
        for (auto &item: items) {
            auto name = item["Name"].as_string();
            if (name.empty()) {
                LOG(ERROR) << "Bad cargo item name: " << name;
                continue;
            }
            Commodity *c = Cfg.getCommodityById(name);
            if (!c) {
                LOG(ERROR) << "Unknown cargo item name: " << name << ", adding to dummy category";
                std::array<std::string, 2> translation;
                if (st::lng == Lang::EN)
                    translation = {item.at("Name_Localised").as_string(), ""};
                if (st::lng == Lang::RU)
                    translation = {"", item.at("Name_Localised").as_string()};
                c = &Cfg.getOrAddCommodity({
                    .intId = 0,
                    .nameId = name,
                    .category = Cfg.getCommodityCategoryById(0),
                    .translation = translation,
                    .rare = false
                });
            }
            c->ship.count = item.at("Count").as_int_or();
            c->ship.stolen = item.at("Stolen").as_int_or();
            LOG(DEBUG) << std::format("Ship cargo: '{}' count {} stolen {}", name, c->ship.count, c->ship.stolen);
            eventCargo.insert(c);
        }
        for (auto &c: Cfg.allKnownCommodities) {
            if (!eventCargo.contains(&c)) {
                c.ship.count = 0;
                c.ship.stolen = 0;
            }
        }
        shipCargo = std::shared_ptr<ShipCargo>(new ShipCargo({
            .cargo = std::vector(eventCargo.begin(), eventCargo.end()),
            .timestamp = file_timestamp,
            .count = (int)ge->data["Count"].as_int_or(),
            .vessel = ge->data["Vessel"].as_string_or(),
            }));
        timestampShip = file_timestamp;
    }

    return true;
}

bool CargoManager::loadCarrierCargo() {
    if (!st::cmdr.fleetCarrierId || Cfg.allKnownCommodities.empty())
        return false;
    std::string fname = std::format("cache/carriers/{}.json", st::cmdr.fleetCarrierId);
    js::value j_cargo;
    try {
        std::ifstream cargoFile(fname, std::ifstream::in);
        if (cargoFile.fail()) {
            LOG(ERROR) << "Cannot read file: " << fname;
            return false;
        }
        j_cargo = js::parse5(cargoFile);
        cargoFile.close();
    } catch (...) {
        LOG(ERROR) << "Failed to read/parse file: " << fname;
        return false;
    }
    if (!j_cargo)
        return false;
    Timestamp timestamp;
    if (!parseTimestamp(j_cargo, timestamp))
        return false;
    //if (timestampFC >= timestamp)
    //    return false;

    {
        std::scoped_lock<std::mutex> lock(cargoMutex);
        std::set<Commodity*> savedCargo;
        auto items = j_cargo.at("Cargo").as_array();
        for (auto &item: items) {
            auto name = item["Name"].as_string();
            if (name.empty()) {
                LOG(ERROR) << "Bad cargo item name: " << name;
                continue;
            }
            Commodity *c = Cfg.getCommodityById(name);
            if (!c)
                continue;
            c->fc.count = item.at("Count").as_int_or();
            LOG(DEBUG) << std::format("Carrier cargo: '{}' count {}", name, c->fc.count);
            savedCargo.insert(c);
        }
        int countTotal = 0;
        for (auto &c: Cfg.allKnownCommodities) {
            if (!savedCargo.contains(&c)) {
                c.fc.count = 0;
            } else {
                countTotal += c.fc.count;
            }
        }
        carrierCargo = std::shared_ptr<ShipCargo>(new ShipCargo({
            .cargo = std::vector(savedCargo.begin(), savedCargo.end()),
            .timestamp = timestamp,
            .count = countTotal,
            .vessel = "FleetCarrier",
            }));
        timestampFC = timestamp;
    }

    return true;
}

bool CargoManager::saveCarrierCargo(Timestamp timestamp, const std::map<Commodity*,int>& patch) {
    if (!st::cmdr.fleetCarrierId)
        return false;

    js::value j_cargo;
    {
        std::scoped_lock<std::mutex> lock(cargoMutex);
        std::set<Commodity*> savedCargo;
        j_cargo = js::object({
            {"timestamp", formatTimestampString(timestamp)},
            {"Vessel",    "FleetCarrier"},
            {"Cargo",     js::array({})},
            });
        auto &jarr = j_cargo["Cargo"].as_array();
        for (auto &c: Cfg.allKnownCommodities) {
            if (c.fc.count <= 0)
                continue;
            savedCargo.insert(&c);
            js::value &jv = jarr.emplace_back(js::value{{"Id",    c.intId},
                                                        {"Name",  c.nameId},
                                                        {"Count", c.fc.count}
                                                        });
            if (!c.translation[int(st::lng)].empty())
                jv["Name_Localised"] = c.name;
            jv.add_flags(js::force::no_indent);
        }
        int countTotal = 0;
        for (auto* c: savedCargo) {
            countTotal += c->fc.count;
        }
        carrierCargo = std::shared_ptr<ShipCargo>(new ShipCargo({
            .cargo = std::vector(savedCargo.begin(), savedCargo.end()),
            .timestamp = timestamp,
            .count = countTotal,
            .vessel = "FleetCarrier",
            }));
        timestampFC = timestamp;
    }

    std::filesystem::path fp("cache/carriers/"+std::to_string(st::cmdr.fleetCarrierId)+".json");
    std::ofstream ofs(fp);
    ofs << js::rule::ecma404() << js::rule::space_indent<1>() << j_cargo;
    ofs.close();

    if (!patch.empty()) {
        js::value j = js::object({});
        for (auto& p : patch)
            j[p.first->nameId] = p.second;
        RavenColonial::carrierPatchCargo(st::cmdr.fleetCarrierId, j);
    }
    return true;
}

bool CargoManager::processMarketBuy(spGameEvent ge) {
    auto& je = ge->data;

    auto market = st::currentMarket;
    if (!market || market->marketId != je["MarketID"].as_int_or()) {
        int64_t marketId = je["MarketID"].as_int_or();
        market = gal::getMarket(marketId);
    }

    bool saveCarrier = false;
    auto* commodity = Cfg.getCommodityById(je["Type"].as_string_or());
    int count = je["Count"].as_int_or();
    if (commodity) {
        std::scoped_lock<std::mutex> lock(cargoMutex);
        if (timestampShip < ge->timestamp) {
            commodity->ship.count += count;
            LOG(DEBUG) << std::format("Market buy '{}': {} ship cargo {}", commodity->nameId, count, commodity->ship.count);
        }
        if (market && market->marketId == st::cmdr.fleetCarrierId && timestampFC < ge->timestamp) {
            commodity->fc.count = std::max(0, commodity->fc.count - count);
            LOG(DEBUG) << std::format("Carrier sell '{}': {} carrier cargo {}", commodity->nameId, count, commodity->fc.count);
            saveCarrier = true;
        }
        if (market && market->timestamp < ge->timestamp) {
            auto it = market->items.find(commodity);
            if (it != market->items.end()) {
                MarketLine &ml = it->second;
                ml.stock = std::max(0, ml.stock - count);
                LOG(DEBUG) << std::format("Market '{}': stock {} demand {}", commodity->nameId, ml.stock, ml.demand);
            }
        }
    }
    if (saveCarrier) {
        std::map<Commodity *, int> fcPatch{{commodity, -count}};
        saveCarrierCargo(ge->timestamp, fcPatch);
    }
    return true;
}

bool CargoManager::processMarketSell(spGameEvent ge) {
    auto& je = ge->data;

    auto market = st::currentMarket;
    if (!market || market->marketId != je["MarketID"].as_int_or()) {
        int64_t marketId = je["MarketID"].as_int_or();
        market = gal::getMarket(marketId);
    }

    bool saveCarrier = false;
    auto* commodity = Cfg.getCommodityById(je["Type"].as_string_or());
    int count = je["Count"].as_int_or();
    if (commodity) {
        std::scoped_lock<std::mutex> lock(cargoMutex);
        if (timestampShip < ge->timestamp) {
            commodity->ship.count = std::max(0, commodity->ship.count-count);
            LOG(DEBUG) << std::format("Market sell '{}': {} ship cargo {}", commodity->nameId, count, commodity->ship.count);
        }
        if (market && market->marketId == st::cmdr.fleetCarrierId && timestampFC < ge->timestamp) {
            commodity->fc.count += count;
            LOG(DEBUG) << std::format("Carrier buy '{}': {} carrier cargo {}", commodity->nameId, count, commodity->fc.count);
            saveCarrier = true;
        }
        if (market->timestamp < ge->timestamp) {
            auto it = market->items.find(commodity);
            if (it != market->items.end()) {
                MarketLine &ml = it->second;
                if (market->stationType != "FleetCarrier" && !ml.isConsumer)
                    ml.stock += count;
                ml.demand = std::max(0, ml.demand - count);
                LOG(DEBUG) << std::format("Market '{}': stock {} demand {}", commodity->nameId, ml.stock, ml.demand);
            }
        }
    }
    if (saveCarrier) {
        std::map<Commodity*,int> fcPatch {{commodity, count}};
        saveCarrierCargo(ge->timestamp, fcPatch);
    }
    return true;
}

bool CargoManager::processColonisationContribution(spGameEvent ge) {
    auto& je = ge->data;
    if (timestampShip >= ge->timestamp)
        return false;
    //{ "timestamp":"2026-02-11T18:55:36Z", "event":"ColonisationContribution", "MarketID":3955958274, "Contributions":[ { "Name":"$ComputerComponents_name;", "Name_Localised":"Компьютерные компоненты", "Amount":62 }, { "Name":"$FruitAndVegetables_name;", "Name_Localised":"Фрукты и овощи", "Amount":50 }, { "Name":"$InsulatingMembrane_name;", "Name_Localised":"Изолирующая мембрана", "Amount":347 }, { "Name":"$PowerGenerators_name;", "Name_Localised":"Электрогенераторы", "Amount":19 }, { "Name":"$Steel_name;", "Name_Localised":"Сталь", "Amount":13 }, { "Name":"$Water_name;", "Name_Localised":"Вода", "Amount":741 } ] }
    auto market = st::currentMarket;
    if (!market || market->marketId != je["MarketID"].as_int_or()) {
        int64_t marketId = je["MarketID"].as_int_or();
        market = gal::getMarket(marketId);
    }
    if (!market || market->timestamp > ge->timestamp || timestampShip >= ge->timestamp)
        return false;

    std::scoped_lock<std::mutex> lock(cargoMutex);
    for (auto& jc : je["Contributions"].as_array_or()) {
        auto name = jc["Name"].as_string_or();
        int amount = jc["Amount"].as_int_or();
        if (name.empty() || name[0] != '$' || !name.ends_with("_name;") || amount <= 0)
            continue;
        name = toLower(name.substr(1, name.size() - 7));
        Commodity* c = Cfg.getCommodityById(name);
        if (!c)
            continue;
        if (market && market->timestamp < ge->timestamp) {
            if (market->items.contains(c)) {
                MarketLine& ml = market->items.at(c);
                ml.stock += amount;
                LOG(DEBUG) << std::format("Colonization contribution '{}': market stock {} demand {}", c->nameId, ml.stock, ml.demand);
            }
        }
        if (timestampShip < ge->timestamp) {
            c->ship.count = std::max(0, c->ship.count - amount);
            LOG(DEBUG) << std::format("Colonization contribution '{}': ship {}", c->nameId, c->ship.count);
        }
    }
    return true;
}

bool CargoManager::processCargoTransfer(spGameEvent ge) {
    Cfg.marketEvent = ge;
    auto& je = ge->data;

    if (!je["Transfers"].is_array())
        return false;

    bool atMyCarrier = st::dockedAt.marketId == st::cmdr.fleetCarrierId;
    std::map<Commodity*,int> fcPatch;

    {
        std::scoped_lock<std::mutex> lock(cargoMutex);
        for (auto &jt: je["Transfers"].as_array()) {
            auto *commodity = Cfg.getCommodityById(jt["Type"].as_string_or());
            auto direction = jt["Direction"].as_string_or();
            int count = jt["Count"].as_int_or();
            if (!commodity || count <= 0 || direction.empty()) {
                LOG(ERROR) << "CargoTransfer, commodity not found: " << jt;
                continue;
            }
            if (direction == "toship") {
                if (timestampShip < ge->timestamp) {
                    commodity->ship.count += count;
                    LOG(DEBUG) << std::format("Cargo transfer '{}': {} to ship {}", commodity->nameId, count, commodity->ship.count);
                }
                if (atMyCarrier && (timestampFC < ge->timestamp || fcPatch.contains(commodity))) {
                    if (fcPatch.contains(commodity))
                        fcPatch[commodity] -= count;
                    else
                        fcPatch[commodity] = -count;
                    commodity->fc.count = std::max(0, commodity->fc.count - count);
                    LOG(DEBUG) << std::format("Cargo transfer '{}': {} from carrier {}", commodity->nameId, count, commodity->fc.count);
                }
            } else if (direction == "tocarrier") {
                atMyCarrier = true;
                if (timestampFC < ge->timestamp || fcPatch.contains(commodity)) {
                    if (fcPatch.contains(commodity))
                        fcPatch[commodity] += count;
                    else
                        fcPatch[commodity] = count;
                    commodity->fc.count += count;
                    LOG(DEBUG) << std::format("Cargo transfer '{}': {} to carrier {}", commodity->nameId, count, commodity->fc.count);
                }
                if (timestampShip < ge->timestamp) {
                    if (count <= commodity->ship.count) {
                        commodity->ship.count = commodity->ship.count - count;
                    } else {
                        int stolen = count - commodity->ship.count;
                        commodity->ship.count = std::max(0, commodity->ship.count - count);
                        commodity->ship.stolen = std::max(0, commodity->ship.stolen - stolen);
                    }
                    LOG(DEBUG) << std::format("Cargo transfer '{}': {} from ship {}", commodity->nameId, count, commodity->ship.count);
                }
            } else if (direction == "tosrv") {
                if (timestampShip < ge->timestamp) {
                    if (count <= commodity->ship.count) {
                        commodity->ship.count = commodity->ship.count - count;
                    } else {
                        int stolen = count - commodity->ship.count;
                        commodity->ship.count = std::max(0, commodity->ship.count - count);
                        commodity->ship.stolen = std::max(0, commodity->ship.stolen - stolen);
                    }
                    LOG(DEBUG) << std::format("Cargo transfer '{}': to SRV {} from ship {}", commodity->nameId, count, commodity->ship.count);
                }
            }
        }
    }
    if (!fcPatch.empty())
        saveCarrierCargo(ge->timestamp, fcPatch);
    return true;
}
