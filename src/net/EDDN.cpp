//
// Created by mkizub on 20.03.2026.
//

#include "../pch.h"

#include "EDDN.h"
#include "HttpInterceptor.h"
#include "../Galaxy.h"

#include <cpr/cpr.h>

namespace EDDN {

const std::string API = "https://eddn.edcd.io:4430/upload/";

void cleanupLocalised(js::value& j) {
    if (!j.is_object())
        return;
    auto& map = j.as_object();
    auto it = map.begin();
    while (it != map.end()) {
        auto key = it->first.operator std::string_view();
        if (key.ends_with("_Localised")) {
            it = map.erase(it);
            continue;
        }
        auto& val = it->second;
        if (val.is_object())
            cleanupLocalised(val);
        else if (val.is_array()) {
            for (auto& el : val.as_array()) {
                if (el.is_object())
                    cleanupLocalised(el);
            }
        }
        ++it;
    }
}

void cleanupLocation(js::value& j) {
    j.erase("Wanted");
    if (j["Factions"].is_array()) {
        for (auto &f: j["Factions"].as_array()) {
            f.erase("HappiestSystem");
            f.erase("HomeSystem");
            f.erase("MyReputation");
            f.erase("SquadronFaction");
        }
    }
}

// {
//  "$schemaRef": "https://eddn.edcd.io/schemas/shipyard/2",
//  "header": {
//    "uploaderID": "Bill",
//    "gameversion": "4.0.0.1451",
//    "gamebuild": "r286916/r0 ",
//    "softwareName": "My excellent app",
//    "softwareVersion": "0.0.1"
//  },
//  "message": {
//    "systemName": "Munfayl",
//    "stationName": "Samson",
//    "marketId": 128023552,
//    "horizons": true,
//    "odyssey": true,
//    "timestamp": "2022-09-27T06:39:43Z",
//  }
//}
std::string packMessage(const char* schema, js::value& orig_msg) {
    js::value j = js::object({
        {"$schemaRef", schema},
        {"header", js::object({
            {"uploaderID", st::cmdr.name},
            {"gameversion", st::client.gameversion},
            {"gamebuild", st::client.build},
            {"softwareName", "EDRobot"},
            {"softwareVersion", EDROBOT_VERSION},
        })},
        {"message", orig_msg},
        });

    auto& msg = j["message"].deref();
    if (st::client.isHorizons.has_value())
        msg["horizons"] = st::client.isHorizons.value();
    if (st::client.isOdyssey.has_value())
        msg["odyssey"] = st::client.isOdyssey.value();

    std::ostringstream os;
    os << std::fixed << std::setprecision(5) << js::rule::ecma404() << js::rule::no_object_nulls() << j;
    return os.str();
}

void event_Location(spGameEvent &ge) {
    if (!Cfg.isEddnSystemsEnabled() || ge->event != "Location")
        return;
    js::value j = ge->data;
    if (j["StarSystem"].empty() || j["SystemAddress"].empty() || j["StarPos"].empty())
        return;

    cleanupLocalised(j);
    cleanupLocation(j);
    j.erase("Latitude");
    j.erase("Longitude");

    auto json_data = packMessage("https://eddn.edcd.io/schemas/journal/1", j);
    LOG(INFO) << "EDDN post Location";
    cpr::PostAsync(cpr::Url{API}, cpr::Body{json_data});
}

void event_FSDJump(spGameEvent &ge) {
    if (!Cfg.isEddnSystemsEnabled() || ge->event != "FSDJump")
        return;
    js::value j = ge->data;
    if (j["StarSystem"].empty() || j["SystemAddress"].empty() || j["StarPos"].empty())
        return;

    cleanupLocalised(j);
    cleanupLocation(j);
    j.erase("BoostUsed");
    j.erase("FuelLevel");
    j.erase("FuelUsed");
    j.erase("JumpDist");

    auto json_data = packMessage("https://eddn.edcd.io/schemas/journal/1", j);
    LOG(INFO) << "EDDN post FSDJump";
    cpr::PostAsync(cpr::Url{API}, cpr::Body{json_data});
}

void event_CarrierJump(spGameEvent &ge) {
    if (!Cfg.isEddnSystemsEnabled() || ge->event != "CarrierJump")
        return;
    js::value j = ge->data;
    if (j["StarSystem"].empty() || j["SystemAddress"].empty() || j["StarPos"].empty())
        return;

    cleanupLocalised(j);
    cleanupLocation(j);

    auto json_data = packMessage("https://eddn.edcd.io/schemas/journal/1", j);
    LOG(INFO) << "EDDN post CarrierJump";
    cpr::PostAsync(cpr::Url{API}, cpr::Body{json_data});
}

void event_Docked(spGameEvent &ge) {
    if (!Cfg.isEddnSystemsEnabled() || ge->event != "Docked")
        return;
    js::value j = ge->data;
    if (!j["SystemAddress"].empty() && j["SystemAddress"].as_int() != st::eddnStarSystem.addr)
        return;
    if (!j["StarSystem"].empty() && j["StarSystem"].as_string() != st::eddnStarSystem.name)
        return;

    cleanupLocalised(j);
    j.erase("Wanted");
    j.erase("ActiveFine");
    j.erase("CockpitBreach");
    auto& ss = st::eddnStarSystem;
    if (j["StarSystem"].empty())
        j["StarSystem"] = ss.name;
    if (j["SystemAddress"].empty())
        j["SystemAddress"] = ss.addr;
    if (j["StarPos"].empty())
        j["StarPos"] = js::array({ss.pos.x,ss.pos.y,ss.pos.z});

    auto json_data = packMessage("https://eddn.edcd.io/schemas/journal/1", j);
    LOG(INFO) << "EDDN post Docked";
    cpr::PostAsync(cpr::Url{API}, cpr::Body{json_data});
}

void event_NavRoute(spGameEvent& ge) {
    if (!Cfg.isEddnSystemsEnabled() || ge->event != "NavRoute" || ge->data["Route"].as_array_or().empty())
        return;
    js::value j = ge->data;
    cleanupLocalised(j);
    auto json_data = packMessage("https://eddn.edcd.io/schemas/navroute/1", j);
    LOG(INFO) << "EDDN post NavRoute";
    cpr::PostAsync(cpr::Url{API}, cpr::Body{json_data});
}

void event_FSSSignalDiscovered(const std::vector<spGameEvent> &events) {
    if (!Cfg.isEddnSystemsEnabled() || events.empty())
        return;
    gal::spStarSystem ss = gal::getCurrentStarSystem();
    for (auto& ge : events) {
        if (ge->event != "FSSSignalDiscovered" || ge->expired || ge->data["SystemAddress"].as_int_or() != ss->systemAddress)
            return;
    }

    js::value j = js::object({
        {"timestamp", events[0]->data["timestamp"].deref()},
        {"event", "FSSSignalDiscovered"},
        {"SystemAddress", ss->systemAddress},
        {"StarSystem", ss->systemName},
        {"StarPos", js::array({ss->starPos.x,ss->starPos.y,ss->starPos.z})},
        {"signals", js::array({})}
    });
    auto& arr = j["signals"].as_array();
    for (auto& ge : events) {
        if (ge->data["USSType"].as_string_or() == "$USS_Type_MissionTarget;")
            continue;
        auto& el = arr.emplace_back(ge->data);
        el.erase("event");
        el.erase("SystemAddress");
        el.erase("TimeRemaining");
        cleanupLocalised(el);
    }
    if (arr.empty())
        return;

    auto json_data = packMessage("https://eddn.edcd.io/schemas/fsssignaldiscovered/1", j);
    LOG(INFO) << "EDDN post FSSSignalDiscovered";
    cpr::PostAsync(cpr::Url{API}, cpr::Body{json_data});
}

void scanEvent(spGameEvent& ge, const char* sysNameFld, const char* schema) {
    if (!Cfg.isEddnSystemsEnabled())
        return;
    js::value j = ge->data;
    if (ge->event == "FSSDiscoveryScan")
        j.erase("Progress");

    auto eddnSS = st::eddnStarSystem;
    if (j["SystemAddress"].as_int_or() != eddnSS.addr)
        return;
    if (!j[sysNameFld].empty() && j[sysNameFld].as_string_or() != eddnSS.name)
        return;

    cleanupLocalised(j);
    if (j[sysNameFld].empty() || !j[sysNameFld].is_string())
        j[sysNameFld] = eddnSS.name;
    if (j["StarPos"].empty() || !j["StarPos"].is_array())
        j["StarPos"] = js::array({eddnSS.pos.x, eddnSS.pos.y, eddnSS.pos.z});

    auto json_data = packMessage(schema, j);
    LOG(INFO) << "EDDN post " << ge->event;
    cpr::PostAsync(cpr::Url{API}, cpr::Body{json_data});
}

void event_FSSDiscoveryScan(spGameEvent& ge) {
    scanEvent(ge, "SystemName", "https://eddn.edcd.io/schemas/fssdiscoveryscan/1");
}

void event_FSSAllBodiesFound(spGameEvent& ge) {
    scanEvent(ge, "SystemName", "https://eddn.edcd.io/schemas/fssallbodiesfound/1");
}

void event_NavBeaconScan(spGameEvent& ge) {
    scanEvent(ge, "StarSystem", "https://eddn.edcd.io/schemas/navbeaconscan/1");
}

void event_SAASignalsFound(spGameEvent& ge) {
    scanEvent(ge, "StarSystem", "https://eddn.edcd.io/schemas/journal/1");
}

void event_FSSBodySignals(spGameEvent &ge) {
    scanEvent(ge, "StarSystem", "https://eddn.edcd.io/schemas/fssbodysignals/1");
}

void event_Scan(spGameEvent &ge) {
    scanEvent(ge, "StarSystem", "https://eddn.edcd.io/schemas/journal/1");
}

void event_ScanBaryCentre(spGameEvent &ge) {
    scanEvent(ge, "StarSystem", "https://eddn.edcd.io/schemas/scanbarycentre/1");
}


void event_Market(spGameEvent &ge) {
    if (!Cfg.isEddnMarketsEnabled() || ge->event != "Market")
        return;

    auto& je = ge->data;

    js::value j = js::object({
        {"timestamp", je["timestamp"].as_string()},
        {"marketId", je["MarketID"].as_int()},
        {"systemName", je["StarSystem"].as_string()},
        {"stationName", je["StationName"].as_string()},
        {"commodities", js::array({})},
    });
    if (!je["CarrierDockingAccess"].empty() && je["CarrierDockingAccess"].is_string())
        j["carrierDockingAccess"] = je["CarrierDockingAccess"].as_string();

    auto& arr = j["commodities"].as_array();
    for (auto& c : je["Items"].as_array_or()) {
        std::string name = c["Name"].as_string();
        name = name.substr(1, name.size() - 7); // "$ ... _name;"
        if (name == "drones")
            continue;
        arr.emplace_back(js::object({
            {"name", name},
            {"buyPrice", c["BuyPrice"].as_int_or()},
            {"sellPrice", c["SellPrice"].as_int_or()},
            {"meanPrice", c["MeanPrice"].as_int_or()},
            {"stockBracket", c["StockBracket"].as_int_or()},
            {"demandBracket", c["DemandBracket"].as_int_or()},
            {"stock", c["Stock"].as_int_or()},
            {"demand", c["Demand"].as_int_or()},
        }));
    }

    auto json_data = packMessage("https://eddn.edcd.io/schemas/commodity/3", j);
    LOG(INFO) << "EDDN post Market";
    cpr::PostAsync(cpr::Url{API}, cpr::Body{json_data});
}

} // namespace EDDN
