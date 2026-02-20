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

// https://ravencolonial100-awcbdvabgze4c5cq.canadacentral-01.azurewebsites.net/api/fc/{marketId}
// {"marketId":3708647424,"name":"VFT-85B","displayName":"Daimonio tou Sokrati","owner":"mkzu","cargo":{"agronomictreatment":32,"bertrandite":234,"cobalt":403,"drones":11,"titanium":587,"tritium":1337}}
json5pp::value carrierGetCargo(int64_t marketId) {
    return curlSimpleGet(RCAPI_FC+std::to_string(marketId));
}

void carrierPostCargo(int64_t marketId, json5pp::value& j) {
    if (!Cfg.isRavenColonialEnabled())
        return;
    LOG(INFO) << "RavenColonial FC cargo post: " << j;
    curlSimplePost(RCAPI_FC+std::to_string(marketId)+"/cargo", j);
}

void carrierPatchCargo(int64_t marketId, json5pp::value& j) {
    if (!Cfg.isRavenColonialEnabled())
        return;
    std::jthread jt([=](std::stop_token stop_token){
        LOG(INFO) << "RavenColonial FC cargo patch: " << j;
        curlSimplePatch(RCAPI_FC+std::to_string(marketId)+"/cargo", j);
    });
    jt.detach(); // After this call, 'jt' no longer owns any thread
}

//{ "timestamp":"2026-02-11T18:55:36Z", "event":"ColonisationContribution", "MarketID":3955958274, "Contributions":[ { "Name":"$ComputerComponents_name;", "Name_Localised":"Компьютерные компоненты", "Amount":62 }, { "Name":"$FruitAndVegetables_name;", "Name_Localised":"Фрукты и овощи", "Amount":50 }, { "Name":"$InsulatingMembrane_name;", "Name_Localised":"Изолирующая мембрана", "Amount":347 }, { "Name":"$PowerGenerators_name;", "Name_Localised":"Электрогенераторы", "Amount":19 }, { "Name":"$Steel_name;", "Name_Localised":"Сталь", "Amount":13 }, { "Name":"$Water_name;", "Name_Localised":"Вода", "Amount":741 } ] }
void reportContribution(spGameEvent& ge) {
    if (!Cfg.isRavenColonialEnabled())
        return;
    auto& je = ge->data;

    int64_t marketId = je["MarketID"].as_int64();
    spMarket market = gal::getMarket(marketId);
    if (!market)
        return;
    auto starSystem = gal::getCurrentStarSystem();
    if (!starSystem)
        return;
    if (market->raven.buildId.empty()) {
        // https://ravencolonial100-awcbdvabgze4c5cq.canadacentral-01.azurewebsites.net/api/v2/system/44770052491
        json5pp::value resp = curlSimpleGet(RCAPI+"v2/system/"+std::to_string(starSystem->systemAddress));
        std::vector<std::string> active_builds;
        for (auto& site : resp["sites"].asif_array()) {
            if (site["marketId"].is_integer() && site["marketId"].as_int64() == marketId) {
                market->raven.buildId = site["buildId"].asif_string();
                market->raven.status = site["status"].asif_string();
                break;
            }
        }
        if (market->raven.buildId.empty()) {
            for (auto& site : resp["sites"].asif_array()) {
                if (site["buildId"].empty() || site["status"].asif_string() != "build")
                    continue;
                if (site["marketId"].empty() && st::dockedAt.stationName.ends_with(site["name"].asif_string())) {
                    market->raven.buildId = site["buildId"].asif_string();
                    market->raven.status = site["status"].asif_string();
                }
                if (site["status"].asif_string() == "build") {
                    active_builds.push_back(site["buildId"].asif_string());
                }
            }
        }
        if (market->raven.buildId.empty() && active_builds.size() == 1) {
            market->raven.buildId = active_builds.front();
            market->raven.status = "build";
        }
    }
    if (market->raven.buildId.empty())
        return;
    std::string cmdr_esc_name = curl_escape(st::cmdr.name.c_str(), st::cmdr.name.size());
    if (!market->raven.commanders.contains(st::cmdr.name)) {
        json5pp::value resp = curlSimplePut(RCAPI_PRJ+market->raven.buildId+"/link/"+cmdr_esc_name, "");
        market->raven.commanders[st::cmdr.name] = {};
    }

    auto& rci = market->raven.commanders[st::cmdr.name];
    if (rci.timestamp >= ge->timestamp)
        return;
    int contributed = 0;
    json5pp::value post_json = json5pp::object({});
    for (auto& jc : je["Contributions"].asif_array()) {
        auto name = jc["Name"].asif_string();
        auto amount = jc["Amount"].as_int32();
        if (name.empty() || name[0] != '$' || !name.ends_with("_name;") || amount <= 0)
            continue;
        name = toLower(name.substr(1, name.size() - 7));
        post_json.as_object().emplace(name, amount);
        contributed += amount;
    }
    json5pp::value resp = curlSimplePost(RCAPI_PRJ+market->raven.buildId+"/contribute/"+cmdr_esc_name, post_json);
    rci.timestamp = ge->timestamp;
    rci.deliveries += 1;
    rci.contributed += contributed;
    gal::saveMarket(market.get());
}

//{ "timestamp":"2026-02-11T18:55:36Z", "event":"ColonisationConstructionDepot", "MarketID":3955958274, "ConstructionProgress":0.752324, "ConstructionComplete":false, "ConstructionFailed":false, "ResourcesRequired":[ { "Name":"$aluminium_name;", "Name_Localised":"Алюминий", "RequiredAmount":500, "ProvidedAmount":500, "Payment":3239 }, { "Name":"$ceramiccomposites_name;", "Name_Localised":"Керамокомпозиты", "RequiredAmount":521, "ProvidedAmount":521, "Payment":724 }, { "Name":"$cmmcomposite_name;", "Name_Localised":"CMM-композит", "RequiredAmount":4508, "ProvidedAmount":4508, "Payment":6788 }, { "Name":"$computercomponents_name;", "Name_Localised":"Компьютерные компоненты", "RequiredAmount":62, "ProvidedAmount":62, "Payment":1112 }, { "Name":"$copper_name;", "Name_Localised":"Медь", "RequiredAmount":242, "ProvidedAmount":242, "Payment":1050 }, { "Name":"$foodcartridges_name;", "Name_Localised":"Пищевые брикеты", "RequiredAmount":94, "ProvidedAmount":94, "Payment":673 }, { "Name":"$fruitandvegetables_name;", "Name_Localised":"Фрукты и овощи", "RequiredAmount":50, "ProvidedAmount":50, "Payment":865 }, { "Name":"$insulatingmembrane_name;", "Name_Localised":"Изолирующая мембрана", "RequiredAmount":347, "ProvidedAmount":347, "Payment":11788 }, { "Name":"$liquidoxygen_name;", "Name_Localised":"Жидкий кислород", "RequiredAmount":1792, "ProvidedAmount":1792, "Payment":2260 }, { "Name":"$medicaldiagnosticequipment_name;", "Name_Localised":"Диагностическое медоборудование", "RequiredAmount":13, "ProvidedAmount":13, "Payment":3609 }, { "Name":"$nonlethalweapons_name;", "Name_Localised":"Нелетальное оружие", "RequiredAmount":13, "ProvidedAmount":13, "Payment":2503 }, { "Name":"$polymers_name;", "Name_Localised":"Полимеры", "RequiredAmount":521, "ProvidedAmount":521, "Payment":682 }, { "Name":"$powergenerators_name;", "Name_Localised":"Электрогенераторы", "RequiredAmount":19, "ProvidedAmount":19, "Payment":3072 }, { "Name":"$semiconductors_name;", "Name_Localised":"Полупроводники", "RequiredAmount":68, "ProvidedAmount":68, "Payment":1526 }, { "Name":"$steel_name;", "Name_Localised":"Сталь", "RequiredAmount":6660, "ProvidedAmount":6199, "Payment":5057 }, { "Name":"$superconductors_name;", "Name_Localised":"Сверхпроводники", "RequiredAmount":112, "ProvidedAmount":112, "Payment":7657 }, { "Name":"$titanium_name;", "Name_Localised":"Титан", "RequiredAmount":5534, "ProvidedAmount":587, "Payment":5360 }, { "Name":"$water_name;", "Name_Localised":"Вода", "RequiredAmount":741, "ProvidedAmount":741, "Payment":662 }, { "Name":"$waterpurifiers_name;", "Name_Localised":"Водоочистители", "RequiredAmount":38, "ProvidedAmount":38, "Payment":849 } ] }
void reportConstructionDepot(spGameEvent& ge, spMarket market) {
    if (!Cfg.isRavenColonialEnabled())
        return;
    if (!ge || !market || market->raven.buildId.empty() || market->raven.status == "complete" || market->raven.timestamp > ge->timestamp)
        return;
    int sumNeed = 0;
    int maxNeed = 0;

    for (auto &ml: market->items) {
        sumNeed += ml.second.demand - ml.second.stock;
        maxNeed += ml.second.demand;
    }

    json5pp::value post_json = json5pp::object({
        {"buildId", market->raven.buildId},
        {"sumNeed", sumNeed},
        {"maxNeed", maxNeed},
        {"commodities",json5pp::object({})}
    });
    json5pp::value& comms = post_json.as_object()["commodities"];
    for (auto &mlp: market->items) {
        Commodity* c = mlp.first;
        int delta = std::max(0, mlp.second.demand - mlp.second.stock);
        comms.as_object().emplace(c->nameId, delta);
    }
    json5pp::value resp = curlSimplePatch(RCAPI_PRJ+market->raven.buildId, post_json);

    if (ge->data["ConstructionComplete"].is_boolean() && ge->data["ConstructionComplete"].as_boolean()) {
        post_json = json5pp::object({{"buildId", market->raven.buildId}});
        curlSimplePost(RCAPI_PRJ+market->raven.buildId + "/complete", post_json);
        market->raven.status = "complete";
        gal::saveMarket(market.get());
    }
}

spMarket updateConstructionDepot(spMarket market) {
    if (!market || market->raven.buildId.empty() || market->raven.status == "complete")
        return market;
    json5pp::value resp = curlSimpleGet(RCAPI_PRJ+market->raven.buildId + "/last");
    // "2026-02-20T07:52:28.3894916+00:00"
    Timestamp timestamp;
    if (!parseTimestamp(resp, timestamp))
        return market;
    if ((market->raven.timestamp+5s) >= timestamp)
        return market;
    resp = curlSimpleGet(RCAPI_PRJ+market->raven.buildId);
    if (auto jid = resp["marketId"]; jid.is_integer() && jid.as_int64() == market->marketId) {
        market = std::make_shared<Market>(*market.get());
        market->raven.timestamp = timestamp;
        if (auto jst = resp["complete"]; jst.is_boolean() && jst.as_boolean()) {
            for (auto& it : market->items) {
                auto& ml = it.second;
                ml.stock = ml.demand;
            }
            market->raven.status = "complete";
        } else {
            for (auto& it : market->items) {
                Commodity* c = it.first;
                auto& ml = it.second;
                int demandOld = ml.demand - ml.stock;
                int demandNew;
                if (auto& jleft = resp["commodities"][c->nameId]; jleft.is_integer())
                    demandNew = jleft.as_integer();
                else
                    demandNew = 0;
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

