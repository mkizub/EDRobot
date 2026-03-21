//
// Created by mkizub on 20.03.2026.
//

#include "../pch.h"

#include "EDDN.h"
#include "../Galaxy.h"

#include <curl/curl.h>
#include <cpr/cpr.h>

cpr::ConnectionPool eddnPool;

const std::string API = "https://eddn.edcd.io:4430/upload/";

class EDDNInterceptor : public cpr::Interceptor {
    static std::atomic<int> reqCounter;
    const int reqId;
public:
    EDDNInterceptor() : reqId(++reqCounter) {}
    cpr::Response intercept(cpr::Session& session) override {
        // Log the request URL
        LOG(INFO) << "HTTP["<<reqId<<"] request url: " << session.GetFullRequestUrl();
        auto& content = session.GetContent();
        if (std::holds_alternative<cpr::Body>(content))
            LOG(INFO) << "HTTP["<<reqId<<"] request body: " << std::get<cpr::Body>(content).str();
        else if (std::holds_alternative<cpr::Body>(content))
            LOG(INFO) << "HTTP["<<reqId<<"] request body: " << std::get<cpr::BodyView>(content).str();

        static std::string ua;
        if (ua.empty())
            ua = std::format("EDRobot {} {}", EDROBOT_VERSION, curl_version());
        session.SetUserAgent(cpr::UserAgent(ua));

        session.UpdateHeader({{"Content-Type", "application/json; charset: utf-8"}}); // "Accept: application/json" ?

        session.SetTimeout(10s);
        session.SetConnectionPool(eddnPool);
        if (Cfg.getCurlInsecure()) {
            auto curl = session.GetCurlHolder()->handle;
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYSTATUS, 0L);
            curl_easy_setopt(curl, CURLOPT_DOH_SSL_VERIFYPEER, 0L);
            curl_easy_setopt(curl, CURLOPT_DOH_SSL_VERIFYHOST, 0L);
            curl_easy_setopt(curl, CURLOPT_DOH_SSL_VERIFYSTATUS, 0L);
            curl_easy_setopt(curl, CURLOPT_PROXY_SSL_VERIFYPEER, 0L);
            curl_easy_setopt(curl, CURLOPT_PROXY_SSL_VERIFYHOST, 0L);
        }
        if (auto& proxy = Cfg.getCurlProxyURL(); !proxy.empty()) {
            auto curl = session.GetCurlHolder()->handle;
            curl_easy_setopt(curl, CURLOPT_PROXY, proxy.c_str());
        }

        // Proceed the request and save the response
        cpr::Response response = proceed(session);

        if (response.status_code == 0) {
            LOG(ERROR) << "HTTP["<<reqId<<"] request error: " << response.error.message;
        } else if (response.status_code >= 400) {
            LOG(ERROR) << "HTTP["<<reqId<<"] error code [" << response.status_code << "] in request to " << response.url;
        } else {
            LOG(INFO) << "HTTP["<<reqId<<"] response [" << response.status_code << "] took " << response.elapsed;
            LOG(INFO) << "HTTP["<<reqId<<"] response body:" << response.text;
        }

        // Return the stored response
        return response;
    }
};

std::atomic<int> EDDNInterceptor::reqCounter;

namespace cpr::priv {

template <>
inline void set_option_internal<false, Url>(Session& session, Url&& url) {
    session.SetUrl(std::forward<Url>(url));
    session.AddInterceptor(std::shared_ptr<cpr::Interceptor>(new EDDNInterceptor()));
}

} //namespace cpr::priv

std::shared_ptr<EDDN> EDDN::getInstance() {
    static std::shared_ptr<EDDN> instance = std::shared_ptr<EDDN>(new EDDN);
    return instance;
}


EDDN::EDDN() {
}

EDDN::~EDDN() {
}

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

void EDDN::event_Location(spGameEvent &ge) {
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

void EDDN::event_FSDJump(spGameEvent &ge) {
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

void EDDN::event_CarrierJump(spGameEvent &ge) {
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

void EDDN::event_Docked(spGameEvent &ge) {
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

void EDDN::event_NavRoute(spGameEvent& ge) {
    if (!Cfg.isEddnSystemsEnabled() || ge->event != "NavRoute" || ge->data["Route"].as_array_or().empty())
        return;
    js::value j = ge->data;
    cleanupLocalised(j);
    auto json_data = packMessage("https://eddn.edcd.io/schemas/navroute/1", j);
    LOG(INFO) << "EDDN post NavRoute";
    cpr::PostAsync(cpr::Url{API}, cpr::Body{json_data});
}

void EDDN::event_FSSSignalDiscovered(const std::vector<spGameEvent> &events) {
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

void EDDN::scanEvent(spGameEvent& ge, const char* event, const char* schema) {
    if (!Cfg.isEddnSystemsEnabled() || ge->event != event)
        return;
    js::value j = ge->data;
    if (j["SystemAddress"].empty() || j["SystemAddress"].as_int() != st::eddnStarSystem.addr)
        return;
    if (!j["StarSystem"].empty() && j["StarSystem"].as_string() != st::eddnStarSystem.name)
        return;

    cleanupLocalised(j);

    auto json_data = packMessage(schema, j);
    LOG(INFO) << "EDDN post " << event;
    cpr::PostAsync(cpr::Url{API}, cpr::Body{json_data});
}

void EDDN::event_NavBeaconScan(spGameEvent& ge) {
    scanEvent(ge, "NavBeaconScan", "https://eddn.edcd.io/schemas/navbeaconscan/1");
}

void EDDN::event_FSSDiscoveryScan(spGameEvent& ge) {
    scanEvent(ge, "FSSDiscoveryScan", "https://eddn.edcd.io/schemas/fssdiscoveryscan/1");
}

void EDDN::event_FSSAllBodiesFound(spGameEvent& ge) {
    scanEvent(ge, "FSSAllBodiesFound", "https://eddn.edcd.io/schemas/fssallbodiesfound/1");
}

void EDDN::event_FSSBodySignals(spGameEvent &ge) {
    scanEvent(ge, "FSSBodySignals", "https://eddn.edcd.io/schemas/fssbodysignals/1");
}

void EDDN::event_Scan(spGameEvent &ge) {
    scanEvent(ge, "Scan", "https://eddn.edcd.io/schemas/journal/1");
}

void EDDN::event_ScanBaryCentre(spGameEvent &ge) {
    scanEvent(ge, "ScanBaryCentre", "https://eddn.edcd.io/schemas/scanbarycentre/1");
}

void EDDN::event_Market(spGameEvent &ge) {
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
        auto& el = arr.emplace_back(js::object({
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
