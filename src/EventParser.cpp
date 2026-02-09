//
// Created by mkizub on 25.06.2025.
//

#include "pch.h"
#include "Configuration.h"
#include "Galaxy.h"
#include "ShipStats.h"
#include "ai/AIManager.h"

namespace st {
Lang lng {Lang::XX};
std::string currentStarSystem;

GuiFocus guiFocus {GuiFocus::None};
bool isDead {};

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

spMarket currentMarket;
spShipCargo currentCargo;
spShipCargo carrierCargo;
spNavRoute currentNavRoute;
CompassInfo compass;
Autopilot autopilot;

void Autopilot::setDestBody(gal::spEntity body) {
    destBody = body;
    isDestDockFocused = false;
    isDestBodyFocused = false;
    if (destBody && destBody->nameEq(st::destination.name)) {
        isDestBodyTargeted = true;
        isDestDockTargeted = false;
    } else {
        isDestBodyTargeted = false;
    }
}
void Autopilot::setDestDock(gal::spEntity dock) {
    destDock = dock;
    isDestDockFocused = false;
    isDestBodyFocused = false;
    if (destDock && destDock->nameEq(st::destination.name)) {
        isDestBodyTargeted = false;
        isDestDockTargeted = true;
    } else {
        isDestDockTargeted = false;
    }
}
std::ostream& operator<<(std::ostream& os, const st::NavPanelFilters& f) {
    os << "{";
    if (f.star) os << "Stars,";
    if (f.asteroidCluster) os << "AsteroidClusters,";
    if (f.planetOrMoon) os << "Planets,";
    if (f.landablePlanetOrMoon) os << "LandablePlanets,";
    if (f.settlement) os << "Settlements,";
    if (f.station) os << "SpaceStations,";
    if (f.fleetCarrier) os << "Carriers,";
    if (f.pointOfInterest) os << "PointsOfInterest,";
    if (f.signalSource) os << "SignalSources,";
    if (f.system) os << "StarSystems,";
    os << "}";
    return os;
}
std::ostream& operator<<(std::ostream& os, const st::ShipStatus& st) {
    os << "{";
    os << "gui-focus:" << enum_name<GuiFocus>(::st::guiFocus)<<",";
    if (st.flags.docked) os << "docked,";
    if (st.flags.landed) os << "landed,";
    if (st.flags.landing_gear_down) os << "landing-gear,";
    if (st.flags.shields_up) os << "shields,";
    if (st.flags.cruise) os << "cruise,";
    if (st.flags.fa_off) os << "fa-off,";
    if (st.flags.weapon_on) os << "weapon,";
    if (st.flags.in_wing) os << "wing,";
    if (st.flags.lights_on) os << "lights,";
    if (st.flags.cargo_scoop_on) os << "cargo-scoop,";
    if (st.flags.silent_run) os << "silent,";
    if (st.flags.fuel_scooping) os << "fuel-scooping,";
    if (st.flags.fsd_masslocked) os << "fsd-masslocked,";
    if (st.flags.fsd_charging) os << "fsd-charging,";
    if (st.flags2.fsd_hyperdrive_charging) os << "fsd-hyperdrive-charging,";
    if (st.flags.fsd_cooldown) os << "fsd-сooldown,";
    if (st.flags.fsd_jump) os << "fsd-jump,";
    if (st.flags.fuel_low) os << "fuel-low,";
    if (st.flags.overheating) os << "overheating,";
    if (st.flags.in_danger) os << "in-danger,";
    if (st.flags.in_interdiction) os << "in-interdiction,";
    if (st.flags.hud_in_analysis) os << "hud-in-analysis,";
    if (st.flags.night_vision) os << "night-vision,";
    os << "pips:[" << int(st.pips[0]) << "," << int(st.pips[1]) << "," << int(st.pips[2]) << "]";
    os << "}";
    return os;
}
}

static int64_t fssSignalSystemAddress;
static std::vector<spGameEvent> allFSSSignalEvents;
static bool scanningOldEvents = true;

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

void parseEvent_Fileheader(spGameEvent& ge);
void parseEvent_Commander(spGameEvent& ge);
void parseEvent_LoadGame(spGameEvent& ge);
void parseEvent_CarrierLocation(spGameEvent& ge);
void parseEvent_Location(spGameEvent& ge);
void parseEvent_Loadout(spGameEvent& ge);
void parseEvent_Died(spGameEvent& ge);
void parseEvent_Resurrect(spGameEvent& ge);
void parseEvent_Cargo(spGameEvent& ge);
void parseEvent_Market(spGameEvent& ge);
void parseEvent_NavRoute(spGameEvent& ge);
void parseEvent_NavRouteClear(spGameEvent& ge);
void parseEvent_ColonisationConstructionDepot(spGameEvent& ge);
void parseEvent_ColonisationContribution(spGameEvent& ge);
void parseEvent_MarketBuy(spGameEvent& ge);
void parseEvent_MarketSell(spGameEvent& ge);
void parseEvent_ShipyardSwap(spGameEvent& ge);
void parseEvent_Docked(spGameEvent& ge);
void parseEvent_Undocked(spGameEvent& ge);
void parseEvent_Docking(spGameEvent& ge);
void parseEvent_StartJump(spGameEvent& ge);
void parseEvent_FSDJump(spGameEvent& ge);
void parseEvent_CarrierJump(spGameEvent& ge);
void parseEvent_SupercruiseDestinationDrop(spGameEvent& ge);
void parseEvent_ApproachSettlement(spGameEvent& ge);
void parseEvent_SupercruiseExit(spGameEvent& ge);
void parseEvent_FSSSignalDiscovered(spGameEvent& ge);
void parseEvent_Scan(spGameEvent& ge);
void parseEvent_ScanBaryCentre(spGameEvent& ge);
void parseEvent_ApproachBody(spGameEvent& ge);
void parseEvent_LeaveBody(spGameEvent& ge);

std::unordered_map<std::string,void(*)(spGameEvent& ge)> eventMap {
        {"Fileheader", parseEvent_Fileheader},
        {"Commander", parseEvent_Commander},
        {"LoadGame", parseEvent_LoadGame},
        {"CarrierLocation", parseEvent_CarrierLocation},
        {"Location", parseEvent_Location},
        {"Loadout", parseEvent_Loadout},
        {"Died", parseEvent_Died},
        {"Resurrect", parseEvent_Resurrect},
        {"Cargo", parseEvent_Cargo},
        {"Market", parseEvent_Market},
        {"NavRoute", parseEvent_NavRoute},
        {"NavRouteClear", parseEvent_NavRouteClear},
        {"ColonisationConstructionDepot", parseEvent_ColonisationConstructionDepot},
        {"ColonisationContribution", parseEvent_ColonisationContribution},
        {"MarketBuy", parseEvent_MarketBuy},
        {"MarketSell", parseEvent_MarketSell},
        {"ShipyardSwap", parseEvent_ShipyardSwap},
        {"Docked", parseEvent_Docked},
        {"Undocked", parseEvent_Undocked},
        {"Liftoff", parseEvent_Undocked},
        {"DockingCancelled", parseEvent_Docking},
        {"DockingDenied", parseEvent_Docking},
        {"DockingGranted", parseEvent_Docking},
        {"DockingRequested", parseEvent_Docking},
        {"DockingTimeout", parseEvent_Docking},
        {"StartJump", parseEvent_StartJump},
        {"FSDJump", parseEvent_FSDJump},
        {"CarrierJump", parseEvent_CarrierJump},
        {"SupercruiseDestinationDrop", parseEvent_SupercruiseDestinationDrop},
        {"ApproachSettlement", parseEvent_ApproachSettlement},
        {"SupercruiseExit", parseEvent_SupercruiseExit},
        {"FSSSignalDiscovered", parseEvent_FSSSignalDiscovered},
        {"Scan", parseEvent_Scan},
        {"ScanBaryCentre", parseEvent_ScanBaryCentre},
        {"ApproachBody", parseEvent_ApproachBody},
        {"LeaveBody", parseEvent_LeaveBody},
};

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

    if (fssSignalSystemAddress) {
        if (event != "FSSSignalDiscovered") {
            spGameEvent empty;
            parseEvent_FSSSignalDiscovered(empty);
        }
    }
    auto it = eventMap.find(event);
    if (it != eventMap.end())
        it->second(gameEvent);

    return gameEvent;
}

void Configuration::preloadOldEventsComplete() {
    scanningOldEvents = false;
}

void Configuration::readJournalChanges(std::ifstream& journalStream, std::string& journalLine) {
    if (!journalStream.is_open())
        return;
    for (;;) {
        journalStream.clear();
        char buffer[1024];
        journalStream.getline(buffer, sizeof(buffer));
        int count = journalStream.gcount();
        if (count == 0) {
            if (journalStream.eof())
                return;
            if (journalStream.fail()) {
                LOG(ERROR) << "Journal read error: " << strerror(errno);
                return;
            }
        } else {
            int len = strlen(buffer);
            journalLine.append(buffer, len);
            if (len == count)
                continue; // no '\n' was extracted from stream
            auto ge = parseEvent(journalLine);
            if (ge && ge->event == "Shutdown")
                journalStream.close();
            journalLine.clear();
        }
    }
}

bool Configuration::loadGameStatus() {
    static std::ifstream ifs;
    LOG(DEBUG) << "Loading Status.json";
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
        //LOG(INFO) << "Body: " << st::shipAtBody.bodyName << "; alt: " << std::round(st::shipAtBody.altitude/1000) << "km; radius: " << std::round(st::shipAtBody.planetRadius/1000) << "km";
    } else {
        st::shipAtBody.nearBody = false;
    }

    if (auto& jd = j["Destination"]; jd.is_object()) {
        // "Destination":{ "System":2381282543995, "Body":0, "Name":"Col 285 Sector XK-O d6-69" }
        // "Destination":{ "System":2868098639337, "Body":1, "Name":"Orbital Construction Site: Piestrak Town" }
        int64 systemAddress = jd.at("System",0).as_int64();
        std::string name;
        if (jd.at("Name_Localised").is_string())
            name = jd["Name_Localised"].as_string();
        else
            name = jd.at("Name","").as_string();
        int bodyId = jd.at("Body",0).as_integer();
        if (systemAddress != st::destination.systemAddress || name != st::destination.name || bodyId != st::destination.bodyId) {
            st::destination.systemAddress = systemAddress;
            st::destination.name = name;
            st::destination.bodyId = bodyId;
            auto &ss = gal::getCurrentStarSystem();
            if (ss)
                ss->addDestination();
        }
        if (st::autopilot.destBody && st::autopilot.destBody->nameEq(st::destination.name)) {
            st::autopilot.isDestBodyTargeted = true;
            st::autopilot.isDestDockTargeted = false;
        }
        else if (st::autopilot.destDock && st::autopilot.destDock->nameEq(st::destination.name)) {
            st::autopilot.isDestBodyTargeted = false;
            st::autopilot.isDestDockTargeted = true;
        }
        else {
            st::autopilot.isDestBodyTargeted = false;
            st::autopilot.isDestDockTargeted = false;
        }
    } else {
        st::destination = {};
        st::autopilot.isDestBodyTargeted = false;
        st::autopilot.isDestDockTargeted = false;
    }

    //LOG(INFO) << "Ship status: " << st::ship;
    return true;
}

void parseEvent_Fileheader(spGameEvent& ge) {
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
    LOG(INFO) << "Game language: " << client.language << ": " << enum_name<Lang>(st::lng);
}

void parseEvent_Commander(spGameEvent& ge) {
    auto& je = ge->data;
    const_cast<st::Commander&>(st::cmdr).name = je["Name"].as_string();
    const_cast<st::Commander&>(st::cmdr).fid = je["FID"].as_string();
    LOG(INFO) << "CMDR: " << st::cmdr.name;
}

void parseEvent_LoadGame(spGameEvent& ge) {
    auto& je = ge->data;
    if (je["Commander"].is_string()) {
        const_cast<st::Commander&>(st::cmdr).name = je["Commander"].as_string();
        const_cast<st::Commander&>(st::cmdr).fid = je["FID"].as_string();
        LOG(INFO) << "CMDR: " << st::cmdr.name;
    }

    auto& client = const_cast<st::GameClient&>(st::client);
    client.isOdyssey = bool(je["Odyssey"]);
    client.isHorizons = bool(je["Horizons"]);
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
    LOG(INFO) << "Game language: " << client.language << ": " << enum_name<Lang>(st::lng);

    auto& shipInfo = const_cast<st::ShipInfo&>(st::shipInfo);
    set(shipInfo.shipType, je.at("Ship",""));
    set(shipInfo.shipTypeLocalized, je.at("Ship_Localised",""));
    set(shipInfo.shipUserName, je.at("ShipName",""));
    set(shipInfo.shipIdent, je.at("ShipIdent",""));
    shipInfo.shipId = je.at("ShipID",0).as_integer();
    LOG(INFO) << "Ship: " << shipInfo.shipType;
}

void parseEvent_CarrierLocation(spGameEvent& ge) {
    auto& je = ge->data;

    auto& cmdr = const_cast<st::Commander&>(st::cmdr);
    auto type = je["CarrierType"].asif_string();
    if (gal::FLEET_CARRIER.match_type(type)) {
        cmdr.fleetCarrierId = je.at("CarrierID",0).as_int64();
        cmdr.fleetCarrierInSystem = je.at("StarSystem","").as_string();
        cmdr.fleetCarrierAtBodyId = je.at("BodyID",-1).as_integer();
        LOG(INFO) << "Fleet Carrier: " << cmdr.fleetCarrierId << " in system " << cmdr.fleetCarrierInSystem;
        Cfg.loadCarrierCargo();
    }
    if (gal::SQUADRON_CARRIER.match_type(type)) {
        cmdr.squadronCarrierId = je.at("CarrierID",0).as_int64();
        cmdr.squadronCarrierInSystem = je.at("StarSystem","").as_string();
        cmdr.squadronCarrierAtBodyId = je.at("BodyID",-1).as_integer();
        LOG(INFO) << "Squadron Carrier: " << cmdr.squadronCarrierId << " in system " << cmdr.squadronCarrierInSystem;
    }
}

void parseEvent_Location(spGameEvent& ge) {
    auto& je = ge->data;

    auto& starSystem = je["StarSystem"];
    if (starSystem.is_string() && starSystem.as_string() != st::currentStarSystem) {
        st::currentStarSystem = starSystem.as_string();
        int64_t address = je.at("SystemAddress",0).as_int64();
        gal::spStarSystem ss = gal::getStarSystem(st::currentStarSystem, address);
        if (cv::norm(ss->starPos) == 0 && je["StarPos"].is_array()) {
            auto& jp = je["StarPos"].as_array();
            ss->starPos = {jp[0].as_number(), jp[1].as_number(), jp[2].as_number()};
        }
        gal::setCurrentStarSystem(ss);
    }
    if (ge->event == "Docked" || je["Docked"]) {
        st::dockedAt.marketId = je.at("MarketID",0).as_int64();
        set(st::dockedAt.stationName, je.at("StationName",""));
        set(st::dockedAt.stationType, je.at("StationType",""));
        gal::getCurrentStarSystem()->addStation(ge);
        st::space = {};
    } else {
        if (je["Body"].is_string()) {
            st::space.bodyId = je.at("BodyID",0).as_integer();
            set(st::space.bodyName, je.at("Body",""));
            set(st::space.bodyType, je.at("BodyType",""));
        } else {
            st::space = {};
        }
        st::dockedAt = {};
    }
}

void parseEvent_Loadout(spGameEvent& ge) {
    auto& je = ge->data;

    auto& shipInfo = const_cast<st::ShipInfo&>(st::shipInfo);
    set(shipInfo.shipType, je.at("Ship",""));
    set(shipInfo.shipTypeLocalized, je.at("Ship_Localised",shipInfo.shipType));
    set(shipInfo.shipUserName, je.at("ShipName",""));
    set(shipInfo.shipIdent, je.at("ShipIdent",""));
    shipInfo.shipId = je.at("ShipID",0).as_integer();
    LOG(INFO) << "Ship: " << shipInfo.shipType;

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

void parseEvent_Died(spGameEvent& ge) {
    st::isDead = true;
    if (!scanningOldEvents)
        ai::interrupt(ai::InterruptReason::DEATH);
}

void parseEvent_Resurrect(spGameEvent& ge) {
    st::isDead = false;
}

void parseEvent_Cargo(spGameEvent& ge) {
    Cfg.loadShipCargo(ge->timestamp);
}
void parseEvent_Market(spGameEvent& ge) {
    Cfg.loadMarket(ge->timestamp);
}
void parseEvent_NavRoute(spGameEvent& ge) {
    Cfg.loadNavRoute(ge->timestamp);
}
void parseEvent_NavRouteClear(spGameEvent& ge) {
    st::currentNavRoute = std::make_shared<NavRoute>();
}

void parseEvent_ShipyardSwap(spGameEvent& ge) {
    auto& je = ge->data;

    auto& shipInfo = const_cast<st::ShipInfo&>(st::shipInfo);
    shipInfo = {};
    set(shipInfo.shipType, je.at("Ship",""));
    set(shipInfo.shipTypeLocalized, je.at("Ship_Localised",shipInfo.shipType));
    set(shipInfo.shipUserName, je.at("ShipName",""));
    set(shipInfo.shipIdent, je.at("ShipIdent",""));
    shipInfo.shipId = je.at("ShipID",0).as_integer();
    LOG(INFO) << "Ship: " << shipInfo.shipType;
}

void parseEvent_Docked(spGameEvent& ge) {
    Cfg.dockingEvent = ge;
    parseEvent_Location(ge);
}

void parseEvent_Undocked(spGameEvent& ge) {
    Cfg.dockingEvent.reset();
    st::currentMarket.reset();
    auto& je = ge->data;
    st::space.marketId = je.at("MarketID",st::dockedAt.marketId).as_int64();
    st::space.stationName = je.at("StationName",st::dockedAt.stationName).as_string();
    st::space.stationType = je.at("StationType", st::dockedAt.stationType).as_string();
    st::dockedAt = {};
}

void parseEvent_Docking(spGameEvent& ge) {
    Cfg.dockingEvent = ge;
    auto& je = ge->data;
    st::space.marketId = je.at("MarketID",0).as_int64();
    st::space.stationName = je["StationName"].as_string();
    st::space.stationType = je.at("StationType", st::dockedAt.stationType).as_string();
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

void parseEvent_StartJump(spGameEvent& ge) {
    auto& je = ge->data;

    st::shipAtBody.approachBody = false;
    st::shipAtBody.nearBody = false;
    st::dockedAt = {};
    st::space = {};
    auto& jjt = je["JumpType"]; // "Hyperspace" or "Supercruise"
    if (jjt.is_string() && jjt.as_string() == "Hyperspace") {
        st::currentStarSystem = je["StarSystem"].as_string();
        int64_t address = je["SystemAddress"].as_int64();
        gal::spStarSystem ss = gal::getStarSystem(st::currentStarSystem, address);
        gal::setCurrentStarSystem(ss);
    }
    ai::resetCompassDetects();
}

void parseEvent_FSDJump(spGameEvent& ge) {
    auto& je = ge->data;

    st::shipAtBody.approachBody = false;
    st::shipAtBody.nearBody = false;
    st::dockedAt = {};
    st::space = {};
    st::currentStarSystem = je["StarSystem"].as_string();
    int64_t address = je["SystemAddress"].as_int64();
    gal::spStarSystem ss = gal::getStarSystem(st::currentStarSystem, address);
    if (cv::norm(ss->starPos) == 0 && je["StarPos"].is_array()) {
        auto& jp = je["StarPos"].as_array();
        ss->starPos = {jp[0].as_number(), jp[1].as_number(), jp[2].as_number()};
    }
    gal::setCurrentStarSystem(ss);
    st::space.bodyId = je.at("BodyID",-1).as_integer();
    set(st::space.bodyName, je.at("Body",""));
    set(st::space.bodyType, je.at("BodyType",""));
    ai::resetCompassDetects();
}

void parseEvent_CarrierJump(spGameEvent& ge) {
    auto& je = ge->data;

    gal::spEntity carrier;
    if (!st::dockedAt.stationName.empty()) {
        auto ss = gal::getCurrentStarSystem();
        if (ss) {
            carrier = ss->getDock(st::dockedAt.marketId);
            if (!carrier)
                carrier = ss->getDock(st::dockedAt.stationName);
        }
        if (carrier && (carrier->type == TypeNav::FleetCarrier || carrier->type == TypeNav::SquadronCarrier)) {
            carrier->parentBodyId = -1;
            std::erase(ss->stations, carrier);
            ss->saved = false;
            gal::saveStarSystem(ss.get());
        } else {
            carrier.reset();
        }
    }
    st::shipAtBody.approachBody = false;
    st::shipAtBody.nearBody = false;
    st::currentStarSystem = je["StarSystem"].as_string();
    int64_t address = je["SystemAddress"].as_int64();
    gal::spStarSystem ss = gal::getStarSystem(st::currentStarSystem, address);
    if (cv::norm(ss->starPos) == 0 && je["StarPos"].is_array()) {
        auto& jp = je["StarPos"].as_array();
        ss->starPos = {jp[0].as_number(), jp[1].as_number(), jp[2].as_number()};
    }
    gal::setCurrentStarSystem(ss);
    st::space.bodyId = je.at("BodyID",-1).as_integer();
    set(st::space.bodyName, je.at("Body",""));
    set(st::space.bodyType, je.at("BodyType",""));
    if (carrier) {
        std::erase_if(ss->stations, [carrier](auto& st)->bool {
            if (st->marketId == carrier->marketId)
                return true;
            return st->type == TypeNav::FleetCarrier && (st->nameEq(carrier->code) || st->nameEq(carrier->name));
        });
        gal::spEntity old_carrier;
        ss->stations.push_back(carrier);
        carrier->parentBodyId = st::space.bodyId;
        ss->saved = false;
        gal::saveStarSystem(ss.get());
    }
}

void parseEvent_SupercruiseDestinationDrop(spGameEvent& ge) {
    auto& je = ge->data;

    st::dockedAt = {};
    st::space.marketId = je.at("MarketID",0).as_int64();
    if (je.at("Type_Localised").is_string())
        st::space.stationName = je["Type_Localised"].as_string();
    else
        st::space.stationName = je.at("Type","").as_string();

    st::space.stationType.clear();
    if (st::space.stationType.empty() && st::space.stationName.starts_with("Orbital Construction Site"))
        st::space.stationType = "SpaceConstructionDepot";
    if (auto ss = gal::getCurrentStarSystem())
        ss->addStation(ge);
    ai::resetCompassDetects();
}

void parseEvent_ApproachSettlement(spGameEvent& ge) {
    // "Name":"Planetary Construction Site: Long Defence Enterprise", "MarketID":4304174851,
    // "SystemAddress":2381282543995, "BodyID":3, "BodyName":"Col 285 Sector XK-O d6-69 A 1", "Latitude":48.513012, "Longitude":119.000992 }
    auto& je = ge->data;

    st::dockedAt = {};
    st::space.marketId = je.at("MarketID",0).as_int64();
    set(st::space.stationName, je.at("Name",""));
    st::space.stationType.clear();
    st::space.bodyId = je.at("BodyID",0).as_integer();
    set(st::space.bodyName, je.at("BodyName",""));
    set(st::space.bodyType, "Planet");

    if (st::space.stationType.empty() && st::space.stationName.starts_with("Planetary Construction Site"))
        st::space.stationType = "PlanetaryConstructionDepot";
    if (auto ss = gal::getCurrentStarSystem())
        ss->addStation(ge);
}

void parseEvent_SupercruiseExit(spGameEvent& ge) {
    auto& je = ge->data;

    st::dockedAt = {};
    st::space.bodyId = je.at("BodyID",0).as_integer();
    set(st::space.bodyName, je.at("Body",""));
    set(st::space.bodyType, je.at("BodyType",""));
    ai::resetCompassDetects();
}

void parseEvent_FSSSignalDiscovered(spGameEvent& ge) {
    if (!ge || ge->event != "FSSSignalDiscovered") {
        // stop FSSSignalDiscovered chain
        gal::spStarSystem ss = gal::getCurrentStarSystem();
        if (ss && fssSignalSystemAddress && ss->systemAddress == fssSignalSystemAddress && !allFSSSignalEvents.empty()) {
            ss->addFSSSignalDiscovered(allFSSSignalEvents);
        }
        allFSSSignalEvents.clear();
        fssSignalSystemAddress = 0;
    } else {
        auto& je = ge->data;

        int64_t address = je.at("SystemAddress",0).as_int64();
        gal::spStarSystem ss = gal::getCurrentStarSystem();
        if (!ss || !address || ss->systemAddress != address) {
            allFSSSignalEvents.clear();
            fssSignalSystemAddress = 0;
            return;
        }
        if (!fssSignalSystemAddress) {
            allFSSSignalEvents.clear();
            fssSignalSystemAddress = address;
        }
        allFSSSignalEvents.push_back(ge);
    }
}

void parseEvent_Scan(spGameEvent& ge) {
    auto& je = ge->data;

    auto starSystem = je["StarSystem"].asif_string();
    int64_t address = je.at("SystemAddress",0).as_int64();
    int bodyId = je["BodyID"].as_integer();
    gal::spStarSystem ss = gal::getStarSystem(starSystem, address);
    auto body = ss->getBodyById(bodyId);
    if (!body) {
        body = std::make_shared<gal::Entity>();
        body->bodyId = bodyId;
        ss->bodies.push_back(body);
        ss->saved = false;
    }
    if (je["StarType"].is_string()) {
        if (body->type != TypeNav::Star) {
            body->type = TypeNav::Star;
            ss->saved = false;
        }
        std::string code = je["StarType"].as_string();
        if (je["Subclass"].is_integer())
            code += std::to_string(je["Subclass"].as_integer());
        if (body->code != code) {
            body->code = code;
            ss->saved = false;
        }
    } else {
        if (auto &cl = je["PlanetClass"]; cl.is_string()) {
            body->type = TypeNav::Planet;
            ss->saved = false;
            bool landable = (bool)je["Landable"];
            if (landable != body->special) {
                body->special = landable;
                ss->saved = false;
            }
        }
        else if (auto &nm = je["BodyName"]; nm.is_string() && gal::BELT.match_name(nm.as_string())) {
            if (body->type != TypeNav::AsteroidCluster) {
                body->type = TypeNav::AsteroidCluster;
                ss->saved = false;
            }
        } else if (!isBody(body->type)) {
            body->type = TypeNav::Body;
            ss->saved = false;
        }
        if (je["Parents"].is_array()) {
            auto b = body;
            for (auto jp : je["Parents"].as_array()) {
                TypeNav p_type = TypeNav::Error;
                int p_id = -1;
                if (jp["Null"].is_integer()) {
                    p_type = TypeNav::Barycenter;
                    p_id = jp["Null"].as_integer();
                }
                else if (jp["Star"].is_integer()) {
                    p_type = TypeNav::Star;
                    p_id = jp["Star"].as_integer();
                }
                else if (jp["Planet"].is_integer()) {
                    p_type = TypeNav::Planet;
                    p_id = jp["Planet"].as_integer();
                }
                else if (jp["Ring"].is_integer()) {
                    p_type = TypeNav::Ring;
                    p_id = jp["Ring"].as_integer();
                }
                else if (jp["AsteroidCluster"].is_integer()) {
                    p_type = TypeNav::AsteroidCluster;
                    p_id = jp["AsteroidCluster"].as_integer();
                }
                if (p_type == TypeNav::Error || p_id < 0)
                    break;
                if (b->parentBodyId != p_id) {
                    b->parentBodyId = p_id;
                    ss->saved = false;
                }
                auto p = ss->getBodyById(p_id);
                if (!p) {
                    p = std::make_shared<gal::Entity>();
                    p->type = p_type;
                    p->bodyId = p_id;
                    ss->bodies.push_back(p);
                    ss->saved = false;
                }
                b = p;
            }
        }
    }
    if (auto& nm=je["BodyName"]; nm.is_string() && nm.as_string() != body->name) {
        body->name = nm.as_string();
        ss->saved = false;
    }
    if (auto bd=je["DistanceFromArrivalLS"]; bd.is_number()) {
        double dist_ls = bd.as_number();
        if (std::round(body->main_star_distance.get_ls()) != std::round(dist_ls)) {
            body->main_star_distance = dist_t(dist_t::LS, dist_ls);
            ss->saved = false;
        }
    }
    if (auto& br = je["Radius"]; br.is_number()) {
        double r = std::round(br.as_number()) / 1000.0; // meters->kilometers
        if (body->radius != r) {
            body->radius = r;
            ss->saved = false;
        }
    }
    if (!ss->saved)
        gal::saveStarSystem(ss.get());
}

void parseEvent_ScanBaryCentre(spGameEvent& ge) {
    auto& je = ge->data;

    auto starSystem = je["StarSystem"].asif_string();
    int64_t address = je.at("SystemAddress",0).as_int64();
    int bodyId = je["BodyID"].as_integer();
    gal::spStarSystem ss = gal::getStarSystem(starSystem, address);
    auto body = ss->getBodyById(bodyId);
    if (!body) {
        body = std::make_shared<gal::Entity>();
        body->type = TypeNav::Barycenter;
        body->bodyId = bodyId;
        ss->bodies.push_back(body);
        ss->saved = false;
    }
    else if (body->type != TypeNav::Barycenter) {
        body->type = TypeNav::Barycenter;
        ss->saved = false;
    }
    if (!ss->saved)
        gal::saveStarSystem(ss.get());
}

void parseEvent_ApproachBody(spGameEvent& ge) {
    auto& je = ge->data;

    st::shipAtBody.approachBody = true;
    st::shipAtBody.bodyId = je.at("BodyID",0).as_integer();
    set(st::shipAtBody.bodyName, je.at("Body",""));
}

void parseEvent_LeaveBody(spGameEvent& ge) {
    auto& je = ge->data;

    st::shipAtBody.approachBody = false;
    st::shipAtBody.bodyId = je.at("BodyID",0).as_integer();
    set(st::shipAtBody.bodyName, je.at("Body",""));
}

void parseEvent_ColonisationConstructionDepot(spGameEvent& ge) {
    auto& je = ge->data;

    auto starSystem = gal::getCurrentStarSystem();
    if (!starSystem)
        return;
    int64_t marketId = je.at("MarketID").as_int64();
    spMarket market = gal::getMarket(marketId);
    if (market && market->timestamp > ge->timestamp)
        return;
    auto dock = starSystem->getDock(marketId);
    market = std::make_shared<Market>(Market{
            .timestamp = ge->timestamp,
            .marketId = marketId,
            .stationName = dock ? dock->name : "",
            .stationType = "ConstrDepot",
            .starSystem = starSystem->systemName,
    });
    bool complete = je["ConstructionComplete"].as_boolean();
    float progress = je["ConstructionProgress"].as_number();
    if (!complete) {
        auto &resources = je.at("ResourcesRequired").as_array();
        for (auto &jr: resources) {
            if (!jr["Name"].is_string())
                continue;
            std::string name = jr["Name"].as_string();
            // "$aluminium_name;"
            if (name.empty() || name[0] != '$' || !name.ends_with("_name;"))
                continue;
            name = name.substr(1, name.size() - 7);
            Commodity *commodity = Cfg.getCommodityById(name);
            if (!commodity)
                continue;
            MarketLine ml{};
            ml.sellPrice = jr.at("Payment", 0).as_int32();
            ml.stock = jr.at("ProvidedAmount", 0).as_int32();
            ml.demand = jr.at("RequiredAmount", 0).as_int32();
            ml.isConsumer = true;
            ml.isProducer = false;
            market->items.emplace(commodity, ml);
        }
    }

    st::currentMarket.swap(market);
    gal::setMarketData(st::currentMarket);
}

void parseEvent_ColonisationContribution(spGameEvent& ge) {
    Cfg.marketEvent = ge;
}
void parseEvent_MarketBuy(spGameEvent& ge) {
    Cfg.marketEvent = ge;
    auto& je = ge->data;

    Timestamp cargo_timestamp;
    if (st::currentCargo)
        cargo_timestamp = st::currentCargo->timestamp;
    auto* market = st::currentMarket.get();
    if (!market || ge->timestamp <= market->timestamp)
        return;

    auto* commodity = Cfg.getCommodityById(je["Type"].asif_string());
    auto count = je.at("Count",0).as_integer();
    if (commodity) {
        if (cargo_timestamp < ge->timestamp && commodity->ship.timestamp < ge->timestamp) {
            commodity->ship.count += count;
            commodity->ship.timestamp = ge->timestamp;
        }
        auto it = market->items.find(commodity);
        if (it != market->items.end()) {
            MarketLine &ml = it->second;
            ml.stock = std::max(0, ml.stock - count);
        }
    }
}
void parseEvent_MarketSell(spGameEvent& ge) {
    Cfg.marketEvent = ge;
    auto& je = ge->data;

    Timestamp cargo_timestamp;
    if (st::currentCargo)
        cargo_timestamp = st::currentCargo->timestamp;
    auto* market = st::currentMarket.get();
    if (!market || ge->timestamp <= market->timestamp)
        return;

    auto* commodity = Cfg.getCommodityById(je["Type"].asif_string());
    auto count = je.at("Count",0).as_integer();
    if (commodity) {
        if (cargo_timestamp < ge->timestamp && commodity->ship.timestamp < ge->timestamp) {
            commodity->ship.count -= count;
            commodity->ship.timestamp = ge->timestamp;
        }
        auto it = market->items.find(commodity);
        if (it != market->items.end()) {
            MarketLine &ml = it->second;
            if (market->stationType != "FleetCarrier" && !ml.isConsumer)
                ml.stock += count;
            ml.demand = std::max(0, ml.demand - count);
        }
    }
}
