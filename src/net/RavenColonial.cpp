//
// Created by mkizub on 12.02.2026.
//

#include "../pch.h"

#include "NetUtils.h"
#include "RavenColonial.h"
#include "../Galaxy.h"

#include <curl/curl.h>

namespace RavenColonial {

//{ "timestamp":"2026-02-11T18:55:36Z", "event":"ColonisationContribution", "MarketID":3955958274, "Contributions":[ { "Name":"$ComputerComponents_name;", "Name_Localised":"Компьютерные компоненты", "Amount":62 }, { "Name":"$FruitAndVegetables_name;", "Name_Localised":"Фрукты и овощи", "Amount":50 }, { "Name":"$InsulatingMembrane_name;", "Name_Localised":"Изолирующая мембрана", "Amount":347 }, { "Name":"$PowerGenerators_name;", "Name_Localised":"Электрогенераторы", "Amount":19 }, { "Name":"$Steel_name;", "Name_Localised":"Сталь", "Amount":13 }, { "Name":"$Water_name;", "Name_Localised":"Вода", "Amount":741 } ] }
void reportContribution(spGameEvent& ge) {
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
        json5pp::value resp = curlSimpleGet("https://ravencolonial100-awcbdvabgze4c5cq.canadacentral-01.azurewebsites.net/api/v2/system/"+std::to_string(starSystem->systemAddress));
        for (auto& site : resp["sites"].asif_array()) {
            if (site["marketId"].is_integer() && site["marketId"].as_int64() == marketId) {
                market->raven.buildId = site["buildId"].asif_string();
                break;
            }
        }
    }
    if (market->raven.buildId.empty() || ge->timestamp <= market->raven.timestamp)
        return;
    std::string cmdr_esc_name = curl_escape(st::cmdr.name.c_str(), st::cmdr.name.size());
    if (!contains(market->raven.linkedCmdrs, st::cmdr.name)) {
        json5pp::value resp = curlSimplePut("https://ravencolonial100-awcbdvabgze4c5cq.canadacentral-01.azurewebsites.net/api/project/"+
                                                    market->raven.buildId+"/link/"+cmdr_esc_name, "");
        market->raven.linkedCmdrs.push_back(st::cmdr.name);
    }

    json5pp::value post_json = json5pp::object({});
    for (auto& jc : je["Contributions"].asif_array()) {
        auto name = jc["Name"].asif_string();
        auto amount = jc["Amount"].as_int32();
        if (name.empty() || name[0] != '$' || !name.ends_with("_name;") || amount <= 0)
            continue;
        name = toLower(name.substr(1, name.size() - 7));
        post_json.as_object().emplace(name, amount);
    }
    json5pp::value resp = curlSimplePost("https://ravencolonial100-awcbdvabgze4c5cq.canadacentral-01.azurewebsites.net/api/project/"+
                                              market->raven.buildId+"/contribute/"+cmdr_esc_name, post_json);
    market->raven.timestamp = ge->timestamp;
    gal::saveMarket(market.get());
}

//{ "timestamp":"2026-02-11T18:55:36Z", "event":"ColonisationConstructionDepot", "MarketID":3955958274, "ConstructionProgress":0.752324, "ConstructionComplete":false, "ConstructionFailed":false, "ResourcesRequired":[ { "Name":"$aluminium_name;", "Name_Localised":"Алюминий", "RequiredAmount":500, "ProvidedAmount":500, "Payment":3239 }, { "Name":"$ceramiccomposites_name;", "Name_Localised":"Керамокомпозиты", "RequiredAmount":521, "ProvidedAmount":521, "Payment":724 }, { "Name":"$cmmcomposite_name;", "Name_Localised":"CMM-композит", "RequiredAmount":4508, "ProvidedAmount":4508, "Payment":6788 }, { "Name":"$computercomponents_name;", "Name_Localised":"Компьютерные компоненты", "RequiredAmount":62, "ProvidedAmount":62, "Payment":1112 }, { "Name":"$copper_name;", "Name_Localised":"Медь", "RequiredAmount":242, "ProvidedAmount":242, "Payment":1050 }, { "Name":"$foodcartridges_name;", "Name_Localised":"Пищевые брикеты", "RequiredAmount":94, "ProvidedAmount":94, "Payment":673 }, { "Name":"$fruitandvegetables_name;", "Name_Localised":"Фрукты и овощи", "RequiredAmount":50, "ProvidedAmount":50, "Payment":865 }, { "Name":"$insulatingmembrane_name;", "Name_Localised":"Изолирующая мембрана", "RequiredAmount":347, "ProvidedAmount":347, "Payment":11788 }, { "Name":"$liquidoxygen_name;", "Name_Localised":"Жидкий кислород", "RequiredAmount":1792, "ProvidedAmount":1792, "Payment":2260 }, { "Name":"$medicaldiagnosticequipment_name;", "Name_Localised":"Диагностическое медоборудование", "RequiredAmount":13, "ProvidedAmount":13, "Payment":3609 }, { "Name":"$nonlethalweapons_name;", "Name_Localised":"Нелетальное оружие", "RequiredAmount":13, "ProvidedAmount":13, "Payment":2503 }, { "Name":"$polymers_name;", "Name_Localised":"Полимеры", "RequiredAmount":521, "ProvidedAmount":521, "Payment":682 }, { "Name":"$powergenerators_name;", "Name_Localised":"Электрогенераторы", "RequiredAmount":19, "ProvidedAmount":19, "Payment":3072 }, { "Name":"$semiconductors_name;", "Name_Localised":"Полупроводники", "RequiredAmount":68, "ProvidedAmount":68, "Payment":1526 }, { "Name":"$steel_name;", "Name_Localised":"Сталь", "RequiredAmount":6660, "ProvidedAmount":6199, "Payment":5057 }, { "Name":"$superconductors_name;", "Name_Localised":"Сверхпроводники", "RequiredAmount":112, "ProvidedAmount":112, "Payment":7657 }, { "Name":"$titanium_name;", "Name_Localised":"Титан", "RequiredAmount":5534, "ProvidedAmount":587, "Payment":5360 }, { "Name":"$water_name;", "Name_Localised":"Вода", "RequiredAmount":741, "ProvidedAmount":741, "Payment":662 }, { "Name":"$waterpurifiers_name;", "Name_Localised":"Водоочистители", "RequiredAmount":38, "ProvidedAmount":38, "Payment":849 } ] }
void reportConstructionDepot(spGameEvent& ge, spMarket market) {
    if (!ge || !market || market->raven.buildId.empty() || market->raven.timestamp > ge->timestamp)
        return;
    int maxNeed = 0;

    for (auto &ml: market->items) {
        maxNeed += ml.second.demand - ml.second.stock;
    }

    json5pp::value post_json = json5pp::object({
        {"buildId", market->raven.buildId},
        {"maxNeed", maxNeed},
        {"commodities",json5pp::object({})}
    });
    json5pp::value& comms = post_json.as_object()["commodities"];
    for (auto &mlp: market->items) {
        Commodity* c = mlp.first;
        int delta = std::max(0, mlp.second.demand - mlp.second.stock);
        comms.as_object().emplace(c->nameId, delta);
    }
    json5pp::value resp = curlSimplePatch("https://ravencolonial100-awcbdvabgze4c5cq.canadacentral-01.azurewebsites.net/api/project/"+
                                                  market->raven.buildId, post_json);
}

}

