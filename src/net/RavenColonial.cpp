//
// Created by mkizub on 12.02.2026.
//

#include "../pch.h"

#include "NetUtils.h"
#include "RavenColonial.h"
#include "../Galaxy.h"

#include <curl/curl.h>

namespace RavenColonial {

const std::string RCAPI = "https://ravencolonial100-awcbdvabgze4c5cq.canadacentral-01.azurewebsites.net/api/";
const std::string RCAPI_FC = RCAPI+"fc/";
const std::string RCAPI_PRJ = RCAPI+"project/";
const std::string RCAPI_CMDR = RCAPI+"cmdr/";

static Timestamp timestampCargo;
static js::value reportedCargo;
static std::mutex mutexCargo;


gal::spEntity importConstructionProject(const std::string& systemName, const std::string& fullName, const std::string& shortName) {
    // https://ravencolonial100-awcbdvabgze4c5cq.canadacentral-01.azurewebsites.net/api/v2/system/44770052491
    auto starSystem = gal::getStarSystem(systemName);
    if (!starSystem)
        return {};
    auto depot = starSystem->getDock(fullName);

    auto cr = curlSimpleGet(RCAPI + "v2/system/" + std::to_string(starSystem->systemAddress));
    if (!cr.ok)
        return {};
    std::string buildId;
    for (auto& site : cr.body["sites"].as_array_or()) {
        if (depot && depot->marketId) {
            if (depot && depot->marketId && site["marketId"].is_int() && site["marketId"].as_int() == depot->marketId) {
                buildId = site["buildId"].as_string_or();
                break;
            }
        } else {
            if (site["status"].as_string_or() != "build")
                continue;
            if (site["name"].as_string_or() == shortName) {
                buildId = site["buildId"].as_string_or();
                break;
            }
        }
    }
    if (buildId.empty()) {
        // check (the only) primary port
        if (!depot || !depot->marketId) {
            std::vector<std::string> active_builds;
            for (auto &site: cr.body["sites"].as_array_or()) {
                if (site["buildId"].empty() || site["status"].as_string_or() != "build")
                    continue;
                active_builds.push_back(site["buildId"].as_string_or());
            }
            if (active_builds.size() == 1) {
                buildId = active_builds.front();
            }
        }
    }
    if (buildId.empty())
        return {};
    cr = curlSimpleGet(RCAPI_PRJ + buildId);
    if (!cr.ok)
        return {};
    Timestamp timestamp;
    parseTimestamp(cr.body, timestamp);
    if (!depot) {
        depot = std::make_shared<gal::Entity>();
        depot->name = fullName;
        depot->marketId = cr.body["marketId"].as_int_or();
        depot->parentBodyId = cr.body["bodyNum"].as_int_or(-1);
        if (cr.body["isPrimaryPort"].as_bool_or())
            depot->type = TypeNav::ColonisationShip;
        else if (fullName.starts_with("Orbital Construction Site:"))
            depot->type = TypeNav::SpaceConstrDepot;
        else if (fullName.starts_with("Planetary Construction Site:"))
            depot->type = TypeNav::PlanetaryConstrDepot;
        starSystem->addStation(depot);
        starSystem->saved = false;
        starSystem->save();
    } else {
        if (!depot->marketId) {
            depot->marketId = cr.body["marketId"].as_int_or();
            starSystem->saved = false;
        }
        if (depot->parentBodyId < 0) {
            depot->parentBodyId = cr.body["bodyNum"].as_int_or(-1);
            starSystem->saved = false;
        }
        if (!starSystem->saved)
            starSystem->save();
    }

    spMarket market = gal::getMarket(depot->marketId);
    if (!market)
        market = std::make_shared<Market>(timestamp, depot->marketId, fullName, "ConstrDepot", systemName);
    else
        market = std::make_shared<Market>(*market);
    if (!market->raven)
        market->raven = std::make_shared<RavenProj>();
    market->raven->timestamp = timestamp;
    market->raven->buildId = buildId;
    for (auto [key,count] : cr.body["commodities"].key_value()) {
        Commodity* c = Cfg.getCommodityById(key);
        if (!c)
            continue;
        if (!market->items.contains(c)) {
            MarketLine ml {};
            ml.demand = count.as_int_or();
            market->items.emplace(c, ml);
        } else {
            auto &ml = market->items[c];
            if (ml.demand) {
                ml.stock = std::max(0, ml.demand - (int) count.as_int_or());
            } else {
                ml.demand = ml.stock + (int) count.as_int_or();
            }
        }
    }
    gal::setMarketData(market);

    return depot;
}


// https://ravencolonial100-awcbdvabgze4c5cq.canadacentral-01.azurewebsites.net/api/fc/{marketId}
// {"marketId":3708647424,"name":"VFT-85B","displayName":"Daimonio tou Sokrati","owner":"mkzu","cargo":{"agronomictreatment":32,"bertrandite":234,"cobalt":403,"drones":11,"titanium":587,"tritium":1337}}
js::value carrierGetCargo(int64_t marketId) {
    auto cr = curlSimpleGet(RCAPI_FC+std::to_string(marketId));
    if (!cr.ok)
        return {};
    return cr.body;
}

void carrierPostCargo(int64_t marketId, js::value& j) {
    if (!Cfg.isRavenColonialEnabled())
        return;
    LOG(INFO) << "RavenColonial FC cargo post: " << j;
    curlSimplePost(RCAPI_FC+std::to_string(marketId)+"/cargo", j);
}

void carrierPatchCargo(int64_t marketId, js::value& j) {
    if (!Cfg.isRavenColonialEnabled())
        return;
    std::jthread jt([=](std::stop_token){
        LOG(INFO) << "RavenColonial FC cargo patch: " << j;
        curlSimplePatch(RCAPI_FC+std::to_string(marketId)+"/cargo", j);
    });
    jt.detach(); // After this call, 'jt' no longer owns any thread
}

js::value buildCargoReeport() {
    std::scoped_lock<std::mutex> lock(mutexCargo);
    Timestamp tm_now = Timestamp::clock::now();
    js::value j = js::object({
        {"cmdr", st::cmdr.name},
        {"name", st::shipInfo.shipIdent},
        {"type", toLower(st::shipInfo.shipType)},
        {"maxCargo", st::shipStats.cargoCapacity},
        {"cargo", js::object({})},
        });
    for (auto* c : CM.getShipCargo()->cargo)
        j["cargo"][c->nameId] = c->ship.count;
    if ((timestampCargo+40s) > tm_now && !reportedCargo.empty()) {
        if (j == reportedCargo)
            return {};
    }
    timestampCargo = tm_now;
    reportedCargo = j;
    return j;
}
void reportShipCargo() {
    if (!Cfg.isRavenColonialEnabled() || st::cmdr.ravenKey.empty() || buildCargoReeport().empty())
        return;

    std::jthread jt([](std::stop_token) mutable {
        js::value j = buildCargoReeport();
        if (j.empty())
            return;
        LOG(INFO) << "RavenColonial current ship cargo: " << j;
        std::vector<std::string> headers = {"rcc-key: " + st::cmdr.ravenKey};
        auto cr = curlSimplePostWithHeaders(RCAPI_CMDR + "currentShip", j, headers);
        if (cr.ok)
            return;
        std::this_thread::sleep_for(15s);
        reportedCargo = nullptr;
        j = buildCargoReeport();
        if (j.empty())
            return;
        cr = curlSimplePostWithHeaders(RCAPI_CMDR + "currentShip", j, headers);
        if (!cr.ok) {
            timestampCargo = {};
            reportedCargo = nullptr;
        }
    });
}

js::value queryShipsCargo(spMarket market) {
    if (!Cfg.isRavenColonialEnabled())
        return {};
    if (!market || market->ravenBuildId().empty() || market->raven->status == "complete")
        return {};
    std::vector<std::string> headers;
    if (!st::cmdr.ravenKey.empty())
        headers.emplace_back("rcc-key: "+st::cmdr.ravenKey);
    auto cr = curlSimpleGetWithHeaders(RCAPI_PRJ + market->raven->buildId + "/ships", headers);
    return cr.body;
}

//{ "timestamp":"2026-02-11T18:55:36Z", "event":"ColonisationContribution", "MarketID":3955958274, "Contributions":[ { "Name":"$ComputerComponents_name;", "Name_Localised":"Компьютерные компоненты", "Amount":62 }, { "Name":"$FruitAndVegetables_name;", "Name_Localised":"Фрукты и овощи", "Amount":50 }, { "Name":"$InsulatingMembrane_name;", "Name_Localised":"Изолирующая мембрана", "Amount":347 }, { "Name":"$PowerGenerators_name;", "Name_Localised":"Электрогенераторы", "Amount":19 }, { "Name":"$Steel_name;", "Name_Localised":"Сталь", "Amount":13 }, { "Name":"$Water_name;", "Name_Localised":"Вода", "Amount":741 } ] }
void reportContribution(spGameEvent& ge) {
    if (!Cfg.isRavenColonialEnabled())
        return;
    auto& je = ge->data;

    int64_t marketId = je["MarketID"].as_int();
    spMarket market = gal::getMarket(marketId);
    if (!market)
        return;
    auto starSystem = gal::getCurrentStarSystem();
    if (!starSystem)
        return;
    if (!market->raven)
        market->raven = std::make_shared<RavenProj>();
    auto& raven = market->raven;
    if (raven->buildId.empty()) {
        // https://ravencolonial100-awcbdvabgze4c5cq.canadacentral-01.azurewebsites.net/api/v2/system/44770052491
        auto cr = curlSimpleGet(RCAPI + "v2/system/" + std::to_string(starSystem->systemAddress));
        std::vector<std::string> active_builds;
        for (auto& site : cr.body["sites"].as_array_or()) {
            if (site["marketId"].is_int() && site["marketId"].as_int() == marketId) {
                raven->buildId = site["buildId"].as_string_or();
                raven->status = site["status"].as_string_or();
                break;
            }
        }
        if (raven->buildId.empty()) {
            for (auto& site : cr.body["sites"].as_array_or()) {
                if (site["buildId"].empty() || site["status"].as_string_or() != "build")
                    continue;
                if (site["marketId"].empty() && st::dockedAt.stationName.ends_with(site["name"].as_string_or())) {
                    raven->buildId = site["buildId"].as_string_or();
                    raven->status = site["status"].as_string_or();
                }
                if (site["status"].as_string_or() == "build") {
                    active_builds.push_back(site["buildId"].as_string_or());
                }
            }
        }
        if (raven->buildId.empty() && active_builds.size() == 1) {
            raven->buildId = active_builds.front();
            raven->status = "build";
        }
    }
    if (raven->buildId.empty())
        return;
    std::string cmdr_esc_name = curl_escape(st::cmdr.name.c_str(), st::cmdr.name.size());
    if (!raven->commanders.contains(st::cmdr.name)) {
        auto cr = curlSimplePut(RCAPI_PRJ + raven->buildId + "/link/" + cmdr_esc_name, "");
        if (!cr.ok)
            return;
        raven->commanders[st::cmdr.name] = {};
    }

    auto& rci = raven->commanders[st::cmdr.name];
    if (rci.timestamp >= ge->timestamp)
        return;
    int contributed = 0;
    js::value post_json = js::object({});
    for (auto& jc : je["Contributions"].as_array_or()) {
        auto name = jc["Name"].as_string_or();
        int amount = jc["Amount"].as_int_or();
        if (name.empty() || name[0] != '$' || !name.ends_with("_name;") || amount <= 0)
            continue;
        name = toLower(name.substr(1, name.size() - 7));
        post_json[name] = amount;
        contributed += amount;
    }
    curlSimplePost(RCAPI_PRJ + raven->buildId + "/contribute/" + cmdr_esc_name, post_json);
    rci.timestamp = ge->timestamp;
    rci.deliveries += 1;
    rci.contributed += contributed;
    gal::saveMarket(market.get());
}

//{ "timestamp":"2026-02-11T18:55:36Z", "event":"ColonisationConstructionDepot", "MarketID":3955958274, "ConstructionProgress":0.752324, "ConstructionComplete":false, "ConstructionFailed":false, "ResourcesRequired":[ { "Name":"$aluminium_name;", "Name_Localised":"Алюминий", "RequiredAmount":500, "ProvidedAmount":500, "Payment":3239 }, { "Name":"$ceramiccomposites_name;", "Name_Localised":"Керамокомпозиты", "RequiredAmount":521, "ProvidedAmount":521, "Payment":724 }, { "Name":"$cmmcomposite_name;", "Name_Localised":"CMM-композит", "RequiredAmount":4508, "ProvidedAmount":4508, "Payment":6788 }, { "Name":"$computercomponents_name;", "Name_Localised":"Компьютерные компоненты", "RequiredAmount":62, "ProvidedAmount":62, "Payment":1112 }, { "Name":"$copper_name;", "Name_Localised":"Медь", "RequiredAmount":242, "ProvidedAmount":242, "Payment":1050 }, { "Name":"$foodcartridges_name;", "Name_Localised":"Пищевые брикеты", "RequiredAmount":94, "ProvidedAmount":94, "Payment":673 }, { "Name":"$fruitandvegetables_name;", "Name_Localised":"Фрукты и овощи", "RequiredAmount":50, "ProvidedAmount":50, "Payment":865 }, { "Name":"$insulatingmembrane_name;", "Name_Localised":"Изолирующая мембрана", "RequiredAmount":347, "ProvidedAmount":347, "Payment":11788 }, { "Name":"$liquidoxygen_name;", "Name_Localised":"Жидкий кислород", "RequiredAmount":1792, "ProvidedAmount":1792, "Payment":2260 }, { "Name":"$medicaldiagnosticequipment_name;", "Name_Localised":"Диагностическое медоборудование", "RequiredAmount":13, "ProvidedAmount":13, "Payment":3609 }, { "Name":"$nonlethalweapons_name;", "Name_Localised":"Нелетальное оружие", "RequiredAmount":13, "ProvidedAmount":13, "Payment":2503 }, { "Name":"$polymers_name;", "Name_Localised":"Полимеры", "RequiredAmount":521, "ProvidedAmount":521, "Payment":682 }, { "Name":"$powergenerators_name;", "Name_Localised":"Электрогенераторы", "RequiredAmount":19, "ProvidedAmount":19, "Payment":3072 }, { "Name":"$semiconductors_name;", "Name_Localised":"Полупроводники", "RequiredAmount":68, "ProvidedAmount":68, "Payment":1526 }, { "Name":"$steel_name;", "Name_Localised":"Сталь", "RequiredAmount":6660, "ProvidedAmount":6199, "Payment":5057 }, { "Name":"$superconductors_name;", "Name_Localised":"Сверхпроводники", "RequiredAmount":112, "ProvidedAmount":112, "Payment":7657 }, { "Name":"$titanium_name;", "Name_Localised":"Титан", "RequiredAmount":5534, "ProvidedAmount":587, "Payment":5360 }, { "Name":"$water_name;", "Name_Localised":"Вода", "RequiredAmount":741, "ProvidedAmount":741, "Payment":662 }, { "Name":"$waterpurifiers_name;", "Name_Localised":"Водоочистители", "RequiredAmount":38, "ProvidedAmount":38, "Payment":849 } ] }
void reportConstructionDepot(spGameEvent& ge, spMarket market) {
    if (!Cfg.isRavenColonialEnabled())
        return;
    if (!ge || !market || market->ravenBuildId().empty() || market->raven->status == "complete" || market->raven->timestamp > ge->timestamp)
        return;
    int sumNeed = 0;
    int maxNeed = 0;

    for (auto &ml: market->items) {
        sumNeed += ml.second.demand - ml.second.stock;
        maxNeed += ml.second.demand;
    }

    js::value post_json = js::object({
        {"buildId",     market->raven->buildId},
        {"sumNeed",     sumNeed},
        {"maxNeed",     maxNeed},
        {"commodities", js::object({})}
    });
    js::value& comms = post_json["commodities"];
    for (auto &mlp: market->items) {
        Commodity* c = mlp.first;
        int delta = std::max(0, mlp.second.demand - mlp.second.stock);
        comms[c->nameId] = delta;
    }
    curlSimplePatch(RCAPI_PRJ + market->raven->buildId, post_json);
    //{"timestamp":"2026-03-06T22:06:49.7348045+00:00","eTag":"W/\"datetime'2026-02-25T20%3A02%3A19.2367861Z'\"","buildId":"48145df8-dcd0-45f5-bdd3-7c1e5239592c","sumNeed":156112,"maxNeed":269924,"complete":false,"commodities":{"aluminium":40263,"autofabricators":0,"ceramiccomposites":0,"cmmcomposite":40983,"computercomponents":0,"copper":0,"foodcartridges":0,"fruitandvegetables":0,"insulatingmembrane":0,"liquidoxygen":0,"medicaldiagnosticequipment":0,"nonlethalweapons":0,"polymers":0,"powergenerators":0,"semiconductors":0,"steel":74062,"superconductors":0,"titanium":0,"water":804,"waterpurifiers":0},"ready":[],"linkedFC":[],"buildType":"dodec","buildName":"Primary port","marketId":3962779906,"systemAddress":1457705423626,"systemName":"Col 359 Sector YE-W c3-5","starPos":[-311.656,282.625,272.906],"bodyNum":2,"bodyName":null,"factionName":null,"architectName":null,"discordLink":null,"timeDue":null,"timeCompleted":null,"timestarted":null,"isPrimaryPort":true,"commanders":{"mkz":[],"mkzu":[]},"notes":"","bodyType":null,"bodyFeatures":null,"systemFeatures":null,"reserveLevel":null}

    if (ge->data["ConstructionComplete"].as_bool_or(false)) {
        post_json = js::object({{"buildId", market->raven->buildId}});
        curlSimplePost(RCAPI_PRJ+market->raven->buildId + "/complete", post_json);
        market->raven->status = "complete";
        gal::saveMarket(market.get());
    }
}

spMarket updateConstructionDepot(spMarket market) {
    if (!market || market->ravenBuildId().empty() || market->raven->status == "complete")
        return market;
    auto& raven = market->raven;
    auto cr = curlSimpleGet(RCAPI_PRJ + raven->buildId + "/last");
    // "2026-02-20T07:52:28.3894916+00:00"
    Timestamp timestamp;
    if (!cr.ok || !parseTimestamp(cr.body, timestamp))
        return market;
    if ((market->timestamp+5s) >= timestamp || (raven->timestamp+5s) >= timestamp)
        return market;
    cr = curlSimpleGet(RCAPI_PRJ+raven->buildId);
    if (auto jid = cr.body["marketId"]; cr.ok && jid.is_int() && jid.as_int() == market->marketId) {
        market = std::make_shared<Market>(*market.get());
        raven->timestamp = timestamp;
        if (auto jst = cr.body["complete"]; jst.is_bool() && jst.as_bool()) {
            for (auto& it : market->items) {
                auto& ml = it.second;
                ml.stock = ml.demand;
            }
            raven->status = "complete";
        } else {
            for (auto& it : market->items) {
                Commodity* c = it.first;
                auto& ml = it.second;
                int demandOld = ml.demand - ml.stock;
                int demandNew = cr.body["commodities"][c->nameId].as_int_or();
                if (demandNew != demandOld) {
                    LOG(INFO) << std::format("Demand update from RavenColonial: '{}' {} => {}",
                                             c->name, demandOld, demandNew);
                    ml.stock = std::clamp(ml.demand-demandNew, 0, ml.demand);
                }
            }
        }
        gal::setMarketData(market);
    }
    return market;
}


}

