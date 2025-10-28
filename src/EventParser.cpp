//
// Created by mkizub on 25.06.2025.
//

#include "pch.h"
#include "Configuration.h"
#include "Galaxy.h"
#include "ShipStats.h"

namespace st {
Lang lng {Lang::XX};
std::string currentStarSystem;

GuiFocus guiFocus {GuiFocus::None};

const Commander cmdr {};
const GameClient client {};
const ShipInfo shipInfo {};
ShipStats shipStats {};
DockedAt dockedAt {};
Space space {};
NavPanelFilters navFilters {};
Destination destination {};
ShipStatus ship {};
ShipAtBody shipAtBody {};
}

inline void set(std::string& field, const json5pp::value& value) {
    if (!value.is_string()) {
        field.clear();
        return;
    }
    if (field == value.as_string())
        return;
    field = value.as_string();
}

GameEvent::GameEvent(json5pp::value&& j) : data(std::move(j)) {
    auto& jev = data["event"];
    auto& jts = data["timestamp"];
    if (!jev.is_string() || !jts.is_string())
        return;
    if (parseTimestampString(jts.as_string(), timestamp))
        event = data["event"].as_string();
}

spGameEvent Configuration::parseEvent(const std::string& line) {
    spGameEvent gameEvent;
    {
        if (trim(line).empty())
            return {};
        try {
            auto res = json5pp::parse(line);
            gameEvent.reset(new GameEvent(std::move(res)));
            if (gameEvent->event.empty())
                return {};
        } catch (const json5pp::syntax_error& ex) {
            return {};
        }
    }

    auto& event = gameEvent->event;
    LOG(DEBUG) << "Journal event: " << event;

    if (event == "Fileheader")
        parseEvent_Fileheader(gameEvent);
    else if (event == "LoadGame")
        parseEvent_LoadGame(gameEvent);
    else if (event == "Commander")
        parseEvent_Commander(gameEvent);
    else if (event == "CarrierLocation")
        parseEvent_CarrierLocation(gameEvent);
    else if (event == "Location")
        parseEvent_Location(gameEvent);
    else if (event == "Loadout")
        parseEvent_Loadout(gameEvent);
    else if (event == "Cargo")
        Cfg.loadCargo(gameEvent->timestamp);
    else if (event == "Market")
        Cfg.loadMarket(gameEvent->timestamp);
    else if (event == "NavRoute")
        Cfg.loadNavRoute(gameEvent->timestamp);
    else if (event == "NavRouteClear")
        Cfg.currentNavRoute = std::make_shared<NavRoute>();
    else if (event == "ShipyardSwap")
        parseEvent_ShipyardSwap(gameEvent);
    else if (event == "Docked")
        parseEvent_Docked(gameEvent);
    else if (event == "Undocked" || event == "Liftoff")
        parseEvent_Undocked(gameEvent);
    else if (event.starts_with("Docking"))
        parseEvent_Docking(gameEvent);
    else if (event == "StartJump")
        parseEvent_StartJump(gameEvent);
    else if (event == "FSDJump")
        parseEvent_FSDJump(gameEvent);
    else if (event == "SupercruiseDestinationDrop")
        parseEvent_SupercruiseDestinationDrop(gameEvent);
    else if (event == "SupercruiseExit")
        parseEvent_SupercruiseExit(gameEvent);
    else if (event == "FSSSignalDiscovered")
        parseEvent_FSSSignalDiscovered(gameEvent);
    else if (event == "ApproachBody")
        parseEvent_ApproachBody(gameEvent);
    else if (event == "LeaveBody")
        parseEvent_LeaveBody(gameEvent);

    return gameEvent;
}

bool Configuration::loadGameStatus() {
    static std::ifstream ifs;
    LOG(INFO) << "Loading Status.json";
    if (!ifs.is_open()) {
        std::wstring filename = mEDLogsPath + L"\\Status.json";
        ifs.open(filename);
        if (!ifs.is_open())
            return false;
    }

    std::optional<json5pp::value> read_result;
    spGameEvent ge;
    for (int cnt=0; ; cnt++) {
        if (!ifs) {
            ifs.clear();
            ifs.seekg(0, std::ios::beg);
        }
        std::stringstream buffer;
        buffer << ifs.rdbuf();
        ifs.clear();
        ifs.seekg(0, std::ios::beg);
        ge = parseEvent(buffer.str());
        if (!ge) {
            if (cnt < 3) {
                Sleep(50);
                ifs.clear();
                ifs.seekg(0, std::ios::beg);
                continue;
            }
            LOG(ERROR) << "Error loading Status.json";
            return false;
        }
        break;
    }
    if (ge->event != "Status")
        return false;
    auto& j = ge->data;
    st::ship.timestamp = ge->timestamp;
    st::ship.flags.all = j.at("Flags",0).as_unsigned();
    st::ship.flags2.all = j.at("Flags2",0).as_unsigned();
    st::ship.fireGroup = j.at("FireGroup",0).as_unsigned();
    auto gf = enum_cast<GuiFocus>(j.at("GuiFocus",0).as_number());
    st::guiFocus = gf.has_value() ? gf.value() : GuiFocus::None;
    if (auto& jp = j["Pips"]; jp.is_array()) {
        st::ship.pips[0] = jp[0].as_unsigned();
        st::ship.pips[1] = jp[1].as_unsigned();
        st::ship.pips[2] = jp[2].as_unsigned();
    }
    {
        auto& ss = st::shipStats;
        if (auto &jf = j["Fuel"]; jf.is_object()) {
            ss.fuelMain = jf.at("FuelMain", 0.0).as_number();
            ss.fuelReservoir = jf.at("FuelReservoir", 0.0).as_number();
        }
        ss.cargo = j.at("Cargo", 0.0).as_number();
        ss.totalMass = ss.unladenMass + ss.fuelMain + ss.fuelReservoir + ss.cargo;
    }
    st::ship.balance = j.at("Balance",0).as_int64();
    if (auto& jl = j["LegalState"]; jl.is_string()) {
        auto ls = enum_cast<st::ShipStatus::LegalState>(jl.as_string());
        st::ship.legalState = ls.has_value() ? ls.value() : st::ShipStatus::LegalState::Clean;
    }

    if (j["BodyName"].is_string()) {
        st::shipAtBody.nearBody = true;
        set(st::shipAtBody.bodyName, j["BodyName"]);
        st::shipAtBody.latitude = j["Latitude"].as_number();
        st::shipAtBody.longitude = j["Longitude"].as_number();
        st::shipAtBody.altitude = j["Altitude"].as_number();
        st::shipAtBody.heading = j["Heading"].as_number();
        st::shipAtBody.planetRadius = j["PlanetRadius"].as_number();
    } else {
        st::shipAtBody.nearBody = false;
    }

    if (auto& jd = j["Destination"]; jd.is_object()) {
        // "Destination":{ "System":2381282543995, "Body":0, "Name":"Col 285 Sector XK-O d6-69" }
        // "Destination":{ "System":2868098639337, "Body":1, "Name":"Orbital Construction Site: Piestrak Town" }
        int64 system = jd.at("System",0).as_int64();
        std::string name = jd.at("Name","").as_string();
        int bodyId = jd.at("Body",0).as_integer();
        if (system != st::destination.system || name != st::destination.name || bodyId != st::destination.bodyId) {
            st::destination.system = jd.at("System", 0).as_int64();
            st::destination.name = jd.at("Name", "").as_string();
            st::destination.bodyId = jd.at("Body", 0).as_integer();
            auto &ss = gal::getCurrentStarSystem();
            if (ss)
                ss->addDestination();
        }
    } else {
        st::destination = {};
    }

    //LOG(INFO) << "Ship status: " << st::ship;
    return true;
}

void Configuration::parseEvent_Fileheader(spGameEvent& ge) {
    auto& je = ge->data;
    auto& client = const_cast<st::GameClient&>(st::client);
    client.isOdyssey = je["Odyssey"].as_boolean();
    client.language = je["language"].as_string();
    client.gameversion = je["gameversion"].as_string();
    client.build = je["build"].as_string();

    if (client.language == "Russian/RU" || client.language.ends_with("/RU"))
        st::lng = Lang::RU;
    else if (client.language == "English/EN" || client.language.ends_with("/EN"))
        st::lng = Lang::EN;
    else if (client.language == "English/UK" || client.language.ends_with("/UK"))
        st::lng = Lang::EN;
    else {
        LOG(ERROR) << "Unsupported game language: " << client.language;
        st::lng = Lang::XX;
    }
}

void Configuration::parseEvent_Commander(spGameEvent& ge) {
    auto& je = ge->data;
    const_cast<st::Commander&>(st::cmdr).name = je["Name"].as_string();
    const_cast<st::Commander&>(st::cmdr).fid = je["FID"].as_string();
}

void Configuration::parseEvent_LoadGame(spGameEvent& ge) {
    auto& je = ge->data;
    if (je["Commander"].is_string()) {
        const_cast<st::Commander&>(st::cmdr).name = je["Commander"].as_string();
        const_cast<st::Commander&>(st::cmdr).fid = je["FID"].as_string();
    }

    auto& client = const_cast<st::GameClient&>(st::client);
    client.isOdyssey = je["Odyssey"];
    client.isHorizons = je["Horizons"];
    client.gameversion = je["gameversion"].as_string();
    client.build = je["build"].as_string();
    client.language = je["language"].as_string();
    if (client.language == "Russian/RU" || client.language.ends_with("/RU"))
        st::lng = Lang::RU;
    else if (client.language == "English/EN" || client.language.ends_with("/EN"))
        st::lng = Lang::EN;
    else if (client.language == "English/UK" || client.language.ends_with("/UK"))
        st::lng = Lang::EN;
    else {
        LOG(ERROR) << "Unsupported game language: " << client.language;
        st::lng = Lang::XX;
    }

    auto& shipInfo = const_cast<st::ShipInfo&>(st::shipInfo);
    set(shipInfo.shipType, je.at("Ship",""));
    set(shipInfo.shipTypeLocalized, je.at("Ship_Localised",""));
    set(shipInfo.shipUserName, je.at("ShipName",""));
    set(shipInfo.shipIdent, je.at("ShipIdent",""));
    shipInfo.shipId = je.at("ShipID",0).as_integer();
}

void Configuration::parseEvent_CarrierLocation(spGameEvent& ge) {
}

void Configuration::parseEvent_Location(spGameEvent& ge) {
    auto& je = ge->data;

    auto& starSystem = je["StarSystem"];
    if (starSystem.is_string() && starSystem.as_string() != st::currentStarSystem) {
        st::currentStarSystem = starSystem.as_string();
        int64_t address = je.at("SystemAddress",0).as_int64();
        gal::spStarSystem ss = gal::getStarSystem(st::currentStarSystem, address);
        if (cv::norm(ss->pos) == 0 && je["StarPos"].is_array()) {
            auto& jp = je["StarPos"].as_array();
            ss->pos = {jp[0].as_number(), jp[1].as_number(), jp[2].as_number()};
        }
        gal::setCurrentStarSystem(ss);
    }
    if (ge->event == "Docked" || je["Docked"]) {
        st::space = {};
        st::dockedAt.marketId = je.at("MarketID",0).as_int64();
        set(st::dockedAt.stationName, je.at("StationName",""));
        set(st::dockedAt.stationType, je.at("StationType",""));
    }
    else {
        st::dockedAt = {};
        if (je["Body"].is_string()) {
            st::space.bodyId = je.at("BodyID",0).as_integer();
            set(st::space.bodyName, je.at("Body",""));
            set(st::space.bodyType, je.at("BodyType",""));
        } else {
            st::space = {};
        }
    }
}

void Configuration::parseEvent_Loadout(spGameEvent& ge) {
    auto& je = ge->data;

    auto& shipInfo = const_cast<st::ShipInfo&>(st::shipInfo);
    set(shipInfo.shipType, je.at("Ship",""));
    set(shipInfo.shipTypeLocalized, je.at("Ship_Localised",shipInfo.shipType));
    set(shipInfo.shipUserName, je.at("ShipName",""));
    set(shipInfo.shipIdent, je.at("ShipIdent",""));
    shipInfo.shipId = je.at("ShipID",0).as_integer();

    {
        auto& ss = st::shipStats;
        ss.unladenMass = je.at("UnladenMass", 0.0).as_number();
        ss.cargoCapacity = je.at("CargoCapacity", 0.0).as_number();
        if (auto &jf = je["FuelCapacity"]; jf.is_object()) {
            ss.fuelCapacityMain = jf.at("Main", 0.0).as_number();
            ss.fuelCapacityReservoir = jf.at("Reserve", 0.0).as_number();
        }
        ss.totalMass = ss.unladenMass + ss.fuelMain + ss.fuelReservoir + ss.cargo;
    }

    auto ss = eddb::initShipStats(shipInfo.shipType);
    if (ss && je["Modules"].is_array()) {
        auto& modules = je["Modules"].as_array();
        for (auto& m : modules) {
            ss->setSlotModule(m);
        }
        ss->updateStats();
    }
    eddb::setShipStats(ss);
}

void Configuration::parseEvent_ShipyardSwap(spGameEvent& ge) {
    auto& je = ge->data;

    auto& shipInfo = const_cast<st::ShipInfo&>(st::shipInfo);
    shipInfo = {};
    set(shipInfo.shipType, je.at("Ship",""));
    set(shipInfo.shipTypeLocalized, je.at("Ship_Localised",shipInfo.shipType));
    set(shipInfo.shipUserName, je.at("ShipName",""));
    set(shipInfo.shipIdent, je.at("ShipIdent",""));
    shipInfo.shipId = je.at("ShipID",0).as_integer();
}

void Configuration::parseEvent_Docked(spGameEvent& ge) {
    dockingEvent = ge;
    parseEvent_Location(ge);
}

void Configuration::parseEvent_Undocked(spGameEvent& ge) {
    dockingEvent.reset();
    auto& je = ge->data;
    st::space.marketId = je.at("MarketID",0).as_int64();
    st::space.stationName = je["StationName"];
    st::space.stationType = je.at("StationType", st::dockedAt.stationType);
}

void Configuration::parseEvent_Docking(spGameEvent& ge) {
    dockingEvent = ge;
    auto& je = ge->data;
    st::space.marketId = je.at("MarketID",0).as_int64();
    st::space.stationName = je["StationName"];
    st::space.stationType = je.at("StationType", st::dockedAt.stationType);
//    if (event == "DockingDenied") {
//        // NoSpace, TooLarge, Hostile, Offences, Distance, ActiveFighter, NoReason, etc.
//        if (je.contains("Reason"))
//            dockingStatus = "DockingDenied:" + je["Reason"].as_string();
//        else
//            dockingStatus = "DockingDenied:NoReason";
//    } else {
//        dockingStatus = event;
//    }
}

void Configuration::parseEvent_StartJump(spGameEvent& ge) {
    auto& je = ge->data;

    st::dockedAt = {};
    st::space = {};
    auto& jjt = je["JumpType"]; // "Hyperspace" or "Supercruise"
    if (jjt.is_string() && jjt.as_string() == "Hyperspace") {
        st::currentStarSystem = je["StarSystem"].as_string();
        int64_t address = je["SystemAddress"].as_int64();
        gal::spStarSystem ss = gal::getStarSystem(st::currentStarSystem, address);
        gal::setCurrentStarSystem(ss);
    }
}

void Configuration::parseEvent_FSDJump(spGameEvent& ge) {
    auto& je = ge->data;

    st::dockedAt = {};
    st::space = {};
    st::currentStarSystem = je["StarSystem"].as_string();
    int64_t address = je["SystemAddress"].as_int64();
    gal::spStarSystem ss = gal::getStarSystem(st::currentStarSystem, address);
    if (cv::norm(ss->pos) == 0 && je["StarPos"].is_array()) {
        auto& jp = je["StarPos"].as_array();
        ss->pos = {jp[0].as_number(), jp[1].as_number(), jp[2].as_number()};
    }
    gal::setCurrentStarSystem(ss);
    st::space.bodyId = je.at("BodyID",0).as_integer();
    set(st::space.bodyName, je.at("Body",""));
    set(st::space.bodyType, je.at("BodyType",""));
}

void Configuration::parseEvent_SupercruiseDestinationDrop(spGameEvent& ge) {
    auto& je = ge->data;

    st::dockedAt = {};
    st::space.marketId = je.at("MarketID",0).as_int64();
    st::space.stationName = je.at("Type").as_string();
    st::space.stationType.clear();
    gal::spStarSystem ss = gal::getCurrentStarSystem();
    if (!ss)
        return;
    gal::spSite dock = ss->getDock(st::space.marketId);
    if (dock) {
        switch (dock->typeSite) {
        case gal::TypeSite::FleetCarrier: st::space.stationType = "FleetCarrier"; break;
        case gal::TypeSite::SpaceConstr: st::space.stationType = "SpaceConstructionDepot"; break;
        }
    }
    if (st::space.stationType.empty() && st::space.stationName.starts_with("Orbital Construction Site"))
        st::space.stationType = "SpaceConstructionDepot";
}

void Configuration::parseEvent_SupercruiseExit(spGameEvent& ge) {
    auto& je = ge->data;

    st::dockedAt = {};
    st::space.bodyId = je.at("BodyID",0).as_integer();
    set(st::space.bodyName, je.at("Body",""));
    set(st::space.bodyType, je.at("BodyType",""));
}

void Configuration::parseEvent_FSSSignalDiscovered(spGameEvent& ge) {
    auto& je = ge->data;

    int64_t address = je["SystemAddress"].as_int64();
    gal::spStarSystem ss = gal::getCurrentStarSystem();
    if (!ss || ss->address != address)
        return;
    ss->addFSSSignalDiscovered(ge);
}

void Configuration::parseEvent_ApproachBody(spGameEvent& ge) {
    auto& je = ge->data;

    st::shipAtBody.approachBody = true;
    st::shipAtBody.bodyId = je.at("BodyID",0).as_integer();
    set(st::shipAtBody.bodyName, je.at("Body",""));
}

void Configuration::parseEvent_LeaveBody(spGameEvent& ge) {
    auto& je = ge->data;

    st::shipAtBody.approachBody = false;
    st::shipAtBody.bodyId = je.at("BodyID",0).as_integer();
    set(st::shipAtBody.bodyName, je.at("Body",""));
}

