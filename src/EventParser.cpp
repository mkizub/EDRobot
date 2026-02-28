//
// Created by mkizub on 25.06.2025.
//

#include "pch.h"
#include "Configuration.h"
#include "Galaxy.h"
#include "ShipStats.h"
#include "ai/AIManager.h"
#include "ui/UIManager.h"
#include "net/RavenColonial.h"

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
    auto jev = data["event"];
    auto jts = data["timestamp"];
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
void parseEvent_CargoTransfer(spGameEvent& ge);
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
        {"CargoTransfer", parseEvent_CargoTransfer},
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
    //LOG(DEBUG) << "Loading Status.json";
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
    //st::ShipStatus old_status = st::ship;
    auto& j = ge->data;
    st::ship.timestamp = ge->timestamp;
    st::ship.flags.all = j["Flags"].as_unsigned_or();
    st::ship.flags2.all = j["Flags2"].as_unsigned_or();
    st::ship.fireGroup = j["FireGroup"].as_unsigned_or();
    auto gf = enum_cast<GuiFocus>(j["GuiFocus"].as_integer_or());
    st::guiFocus = gf.has_value() ? gf.value() : GuiFocus::None;
    if (auto& jp = j["Pips"].as_array_or(); jp.size() == 3) {
        st::ship.pips[0] = jp[0].as_unsigned();
        st::ship.pips[1] = jp[1].as_unsigned();
        st::ship.pips[2] = jp[2].as_unsigned();
    }
    {
        auto& ss = st::shipStats;
        ss.fuelMain = j["Fuel"]["FuelMain"].as_number_or();
        ss.fuelReservoir = j["Fuel"]["FuelReservoir"].as_number_or();
        ss.cargo = j["Cargo"].as_number_or();
        ss.totalMass = ss.unladenMass + ss.fuelMain + ss.fuelReservoir + ss.cargo;
    }
    st::ship.balance = j["Balance"].as_int64_or();
    st::ship.legalState = enum_cast<st::ShipStatus::LegalState>(j["LegalState"].as_string_or())
            .value_or(st::ShipStatus::LegalState::Clean);

    if (j["BodyName"].is_string()) {
        st::shipAtBody.nearBody = true;
        set(st::shipAtBody.bodyName, j["BodyName"]);
        st::shipAtBody.latitude = j["Latitude"].as_number();
        st::shipAtBody.longitude = j["Longitude"].as_number();
        st::shipAtBody.altitude = j["Altitude"].as_number();
        st::shipAtBody.heading = j["Heading"].as_number();
        st::shipAtBody.planetRadius = j["PlanetRadius"].as_number();
        //LOG(INFO) << "Body: " << st::shipAtBody.bodyName << "; alt: " << std::round(st::shipAtBody.altitude/1000) << "km; radius: " << std::round(st::shipAtBody.planetRadius/1000) << "km";
        //if (old_status.flags.cruise && !st::ship.flags.cruise)
        //    LOG(INFO) << "Exit cruise alt: " << std::round(st::shipAtBody.altitude/1000) << "km";
    } else {
        st::shipAtBody.nearBody = false;
    }

    if (auto jd = j["Destination"]; jd.is_object()) {
        // "Destination":{ "System":2381282543995, "Body":0, "Name":"Col 285 Sector XK-O d6-69" }
        // "Destination":{ "System":2868098639337, "Body":1, "Name":"Orbital Construction Site: Piestrak Town" }
        int64 systemAddress = jd["System"].as_int64_or();
        std::string name;
        if (jd["Name_Localised"].is_string())
            name = jd["Name_Localised"].as_string();
        else
            name = jd["Name"].as_string_or();
        int bodyId = jd["Body"].as_integer_or();
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

static void setCommander(const std::string& name, const std::string& fid) {
    if (name != st::cmdr.name || fid != st::cmdr.fid) {
        LOG(INFO) << "CMDR: " << name;
        const_cast<st::Commander&>(st::cmdr) = {};
        const_cast<st::Commander&>(st::cmdr).name = name;
        const_cast<st::Commander&>(st::cmdr).fid = fid;
    }
}

void parseEvent_Commander(spGameEvent& ge) {
    auto& je = ge->data;
    setCommander(je["Name"].as_string(), je["FID"].as_string());
}

void parseEvent_LoadGame(spGameEvent& ge) {
    {
        // for relogin
        Cfg.dockingEvent.reset();
        Cfg.marketEvent.reset();
        st::compass = {};
    }
    auto& je = ge->data;
    if (je["Commander"].is_string())
        setCommander(je["Commander"].as_string(), je["FID"].as_string());

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
    shipInfo.shipId = je["ShipID"].as_integer_or();
    LOG(INFO) << "Ship: " << shipInfo.shipType;
}

void parseEvent_CarrierLocation(spGameEvent& ge) {
    auto& je = ge->data;

    auto& cmdr = const_cast<st::Commander&>(st::cmdr);
    auto type = je["CarrierType"].as_string_or();
    if (gal::FLEET_CARRIER.match_type(type)) {
        cmdr.fleetCarrierId = je["CarrierID"].as_int64_or();
        cmdr.fleetCarrierInSystem = je["StarSystem"].as_string_or();
        cmdr.fleetCarrierAtBodyId = je["BodyID"].as_integer_or(-1);
        LOG(INFO) << "Fleet Carrier: " << cmdr.fleetCarrierId << " in system " << cmdr.fleetCarrierInSystem;
        Cfg.loadCarrierCargo();
    }
    if (gal::SQUADRON_CARRIER.match_type(type)) {
        cmdr.squadronCarrierId = je["CarrierID"].as_int64_or();
        cmdr.squadronCarrierInSystem = je["StarSystem"].as_string_or();
        cmdr.squadronCarrierAtBodyId = je["BodyID"].as_integer_or(-1);
        LOG(INFO) << "Squadron Carrier: " << cmdr.squadronCarrierId << " in system " << cmdr.squadronCarrierInSystem;
    }
}

void parseEvent_Location(spGameEvent& ge) {
    auto& je = ge->data;

    if (auto starSystem = je["StarSystem"]; starSystem.is_string() && starSystem.as_string() != st::currentStarSystem) {
        st::currentStarSystem = starSystem.as_string();
        int64_t address = je["SystemAddress"].as_int64_or();
        gal::spStarSystem ss = gal::getStarSystem(st::currentStarSystem, address);
        if (cv::norm(ss->starPos) == 0 && je["StarPos"].is_array()) {
            auto& jp = je["StarPos"].as_array();
            ss->starPos = {jp[0].as_number(), jp[1].as_number(), jp[2].as_number()};
        }
        gal::setCurrentStarSystem(ss);
    }
    if (ge->event == "Docked" || je["Docked"]) {
        st::dockedAt.marketId = je["MarketID"].as_int64_or();
        set(st::dockedAt.stationName, je["StationName"]);
        set(st::dockedAt.stationType, je["StationType"]);
        gal::getCurrentStarSystem()->addStation(ge);
        auto market = gal::getMarket(st::dockedAt.marketId);
        st::currentMarket.swap(market);
        st::space = {};
    } else {
        if (je["Body"].is_string()) {
            st::space.bodyId = je["BodyID"].as_integer_or();
            set(st::space.bodyName, je["Body"]);
            set(st::space.bodyType, je["BodyType"]);
        } else {
            st::space = {};
        }
        st::dockedAt = {};
    }
}

void parseEvent_Loadout(spGameEvent& ge) {
    auto& je = ge->data;

    auto& shipInfo = const_cast<st::ShipInfo&>(st::shipInfo);
    set(shipInfo.shipType, je["Ship"]);
    set(shipInfo.shipTypeLocalized, je.at("Ship_Localised",shipInfo.shipType));
    set(shipInfo.shipUserName, je["ShipName"]);
    set(shipInfo.shipIdent, je["ShipIdent"]);
    shipInfo.shipId = je["ShipID"].as_integer_or();
    LOG(INFO) << "Ship: " << shipInfo.shipType;

    {
        auto& ss = st::shipStats;
        ss.unladenMass = je["UnladenMass"].as_number_or();
        ss.cargoCapacity = je["CargoCapacity"].as_number_or();
        ss.fuelCapacityMain = je["FuelCapacity"]["Main"].as_number_or();
        ss.fuelCapacityReservoir = je["FuelCapacity"]["Reserve"].as_number_or();
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
    Cfg.loadShipCargo(ge);
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
    set(shipInfo.shipType, je["Ship"]);
    set(shipInfo.shipTypeLocalized, je.at("Ship_Localised",shipInfo.shipType));
    set(shipInfo.shipUserName, je["ShipName"]);
    set(shipInfo.shipIdent, je["ShipIdent"]);
    shipInfo.shipId = je["ShipID"].as_integer_or();
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
    st::space.marketId = je["MarketID"].as_int64_or(st::dockedAt.marketId);
    st::space.stationName = je["StationName"].as_string_or(st::dockedAt.stationName);
    st::space.stationType = je["StationType"].as_string_or(st::dockedAt.stationType);
    st::dockedAt = {};
}

void parseEvent_Docking(spGameEvent& ge) {
    Cfg.dockingEvent = ge;
    auto& je = ge->data;
    st::space.marketId = je["MarketID"].as_int64_or();
    st::space.stationName = je["StationName"].as_string_or();
    st::space.stationType = je["StationType"].as_string_or(st::dockedAt.stationType);
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
    // "Hyperspace" or "Supercruise"
    if (je["JumpType"].as_string_or() == "Hyperspace") {
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
    st::space.bodyId = je["BodyID"].as_integer_or(-1);
    set(st::space.bodyName, je["Body"]);
    set(st::space.bodyType, je["BodyType"]);
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
    st::space.bodyId = je["BodyID"].as_integer_or(-1);
    set(st::space.bodyName, je["Body"]);
    set(st::space.bodyType, je["BodyType"]);
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
    st::space.marketId = je["MarketID"].as_int64_or();
    if (je.at("Type_Localised").is_string())
        st::space.stationName = je["Type_Localised"].as_string();
    else
        st::space.stationName = je["Type"].as_string_or();

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
    st::space.marketId = je["MarketID"].as_int64_or();
    set(st::space.stationName, je["Name"]);
    st::space.stationType.clear();
    st::space.bodyId = je["BodyID"].as_integer_or();
    set(st::space.bodyName, je["BodyName"]);
    set(st::space.bodyType, "Planet");

    if (st::space.stationType.empty() && st::space.stationName.starts_with("Planetary Construction Site"))
        st::space.stationType = "PlanetaryConstructionDepot";
    if (auto ss = gal::getCurrentStarSystem())
        ss->addStation(ge);
}

void parseEvent_SupercruiseExit(spGameEvent& ge) {
    auto& je = ge->data;

    st::dockedAt = {};
    st::space.bodyId = je["BodyID"].as_integer_or();
    set(st::space.bodyName, je["Body"]);
    set(st::space.bodyType, je["BodyType"]);
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

        int64_t address = je["SystemAddress"].as_int64_or();
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

    auto starSystem = je["StarSystem"].as_string_or();
    int64_t address = je["SystemAddress"].as_int64_or();
    int bodyId = je["BodyID"].as_integer_or(-1);
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
        if (je["PlanetClass"].is_string()) {
            body->type = TypeNav::Planet;
            ss->saved = false;
            bool landable = (bool)je["Landable"];
            if (landable != body->special) {
                body->special = landable;
                ss->saved = false;
            }
        }
        else if (gal::BELT.match_name(je["BodyName"].as_string_or())) {
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
    if (auto nm=je["BodyName"]; nm.is_string() && nm.as_string() != body->name) {
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
    if (auto br = je["Radius"]; br.is_number()) {
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

    auto starSystem = je["StarSystem"].as_string_or();
    int64_t address = je["SystemAddress"].as_int64_or();
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
    st::shipAtBody.bodyId = je["BodyID"].as_integer_or();
    set(st::shipAtBody.bodyName, je["Body"]);
}

void parseEvent_LeaveBody(spGameEvent& ge) {
    auto& je = ge->data;

    st::shipAtBody.approachBody = false;
    st::shipAtBody.bodyId = je["BodyID"].as_integer_or();
    set(st::shipAtBody.bodyName, je["Body"]);
}

void parseEvent_ColonisationConstructionDepot(spGameEvent& ge) {
    auto& je = ge->data;

    auto starSystem = gal::getCurrentStarSystem();
    if (!starSystem)
        return;
    int64_t marketId = je["MarketID"].as_int64_or();
    spMarket old_market = gal::getMarket(marketId);
    if (old_market && old_market->timestamp > ge->timestamp)
        return;
    auto dock = starSystem->getDock(marketId);
    spMarket market = std::make_shared<Market>(Market{
            .timestamp = ge->timestamp,
            .marketId = marketId,
            .stationName = dock ? dock->name : "",
            .stationType = "ConstrDepot",
            .starSystem = starSystem->systemName,
    });
    bool complete = je["ConstructionComplete"].as_boolean();
    bool reportToRaven = false;
    if (old_market && !old_market->raven.buildId.empty()) {
        market->raven = old_market->raven;
        if (!scanningOldEvents && market->raven.timestamp.time_since_epoch().count() == 0)
            reportToRaven = true;
        if (complete && market->raven.status != "complete")
            reportToRaven = true;
    }
    for (auto &jr: je["ResourcesRequired"].as_array_or()) {
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
        int provided = jr["ProvidedAmount"].as_int32_or();
        int required = jr["RequiredAmount"].as_int32_or();
        if (old_market) {
            if (!old_market->items.contains(commodity))
                reportToRaven = true;
            else {
                auto& old_ml = old_market->items.at(commodity);
                if (old_ml.stock != provided || old_ml.demand != required)
                    reportToRaven = true;
            }
        }
        MarketLine ml{};
        ml.sellPrice = jr["Payment"].as_int32_or();
        ml.stock = provided;
        ml.demand = required;
        ml.isConsumer = true;
        ml.isProducer = false;
        market->items.emplace(commodity, ml);
    }

    if (reportToRaven) {
        if (market->raven.buildId.empty() || market->raven.timestamp >= ge->timestamp)
            reportToRaven = false;
        else if (market->raven.status == "complete")
            reportToRaven = false;
        else
            market->raven.timestamp = ge->timestamp;
    }
    else if (complete && (!Cfg.isRavenColonialEnabled() || market->raven.buildId.empty()) && market->raven.status != "complete") {
        market->raven.timestamp = ge->timestamp;
        market->raven.status = "complete";
    }
    st::currentMarket.swap(market);
    gal::setMarketData(st::currentMarket);
    if (reportToRaven)
        RavenColonial::reportConstructionDepot(ge, st::currentMarket);
 }

void parseEvent_ColonisationContribution(spGameEvent& ge) {
    Cfg.marketEvent = ge;
    RavenColonial::reportContribution(ge);
}
void parseEvent_MarketBuy(spGameEvent& ge) {
    Cfg.marketEvent = ge;
    auto& je = ge->data;

    Timestamp cargo_timestamp;
    if (st::currentCargo)
        cargo_timestamp = st::currentCargo->timestamp;
    auto market = st::currentMarket;
    if (!market) {
        int64_t marketId = je["MarketID"].as_int64_or();
        market = gal::getMarket(marketId);
        if (!market)
            return;
        st::currentMarket.swap(market);
    }

    bool saveCarrier = false;
    auto* commodity = Cfg.getCommodityById(je["Type"].as_string_or());
    auto count = je["Count"].as_integer_or();
    if (commodity) {
        if (cargo_timestamp < ge->timestamp && commodity->ship.timestamp < ge->timestamp) {
            commodity->ship.count += count;
            commodity->ship.timestamp = ge->timestamp;
        }
        if (market->marketId == st::cmdr.fleetCarrierId && commodity->fc.timestamp < ge->timestamp) {
            commodity->fc.count = std::max(0, commodity->fc.count-count);
            commodity->fc.timestamp = ge->timestamp;
            saveCarrier = true;
        }
        if (market->timestamp < ge->timestamp) {
            auto it = market->items.find(commodity);
            if (it != market->items.end()) {
                MarketLine &ml = it->second;
                ml.stock = std::max(0, ml.stock - count);
            }
        }
    }
    UIManager::updateCargoDialog();
    if (saveCarrier) {
        std::map<Commodity *, int> fcPatch{{commodity, -count}};
        Cfg.saveCarrierCargo(ge->timestamp, fcPatch);
    }
}
void parseEvent_MarketSell(spGameEvent& ge) {
    Cfg.marketEvent = ge;
    auto& je = ge->data;

    Timestamp cargo_timestamp;
    if (st::currentCargo)
        cargo_timestamp = st::currentCargo->timestamp;
    auto market = st::currentMarket;
    if (!market) {
        int64_t marketId = je["MarketID"].as_int64_or();
        market = gal::getMarket(marketId);
        if (!market)
            return;
        st::currentMarket.swap(market);
    }

    bool saveCarrier = false;
    auto* commodity = Cfg.getCommodityById(je["Type"].as_string_or());
    auto count = je["Count"].as_integer_or();
    if (commodity) {
        if (cargo_timestamp < ge->timestamp && commodity->ship.timestamp < ge->timestamp) {
            commodity->ship.count = std::max(0, commodity->ship.count-count);
            commodity->ship.timestamp = ge->timestamp;
        }
        if (market->marketId == st::cmdr.fleetCarrierId && commodity->fc.timestamp < ge->timestamp) {
            commodity->fc.count += count;
            commodity->fc.timestamp = ge->timestamp;
            saveCarrier = true;
        }
        if (market->timestamp < ge->timestamp) {
            auto it = market->items.find(commodity);
            if (it != market->items.end()) {
                MarketLine &ml = it->second;
                if (market->stationType != "FleetCarrier" && !ml.isConsumer)
                    ml.stock += count;
                ml.demand = std::max(0, ml.demand - count);
            }
        }
    }
    UIManager::updateCargoDialog();
    if (saveCarrier) {
        std::map<Commodity*,int> fcPatch {{commodity, count}};
        Cfg.saveCarrierCargo(ge->timestamp, fcPatch);
    }
}

void parseEvent_CargoTransfer(spGameEvent& ge) {
    Cfg.marketEvent = ge;
    auto& je = ge->data;

    if (!je["Transfers"].is_array())
        return;

    Timestamp cargo_timestamp;
    if (st::currentCargo)
        cargo_timestamp = st::currentCargo->timestamp;
    bool atMyCarrier = st::dockedAt.marketId == st::cmdr.fleetCarrierId;
    std::map<Commodity*,int> fcPatch;

    for (auto& jt : je["Transfers"].as_array()) {
        auto *commodity = Cfg.getCommodityById(jt["Type"].as_string_or());
        auto direction = jt["Direction"].as_string_or();
        auto count = jt["Count"].as_integer_or();
        if (!commodity || count <= 0 || direction.empty()) {
            LOG(ERROR) << "CargoTransfer, commodity not found: " << jt;
            continue;
        }
        if (direction == "toship") {
            if (cargo_timestamp < ge->timestamp && commodity->ship.timestamp < ge->timestamp) {
                commodity->ship.count += count;
                commodity->ship.timestamp = ge->timestamp;
            }
            if (atMyCarrier && (commodity->fc.timestamp < ge->timestamp || fcPatch.contains(commodity))) {
                if (fcPatch.contains(commodity))
                    fcPatch[commodity] -= count;
                else
                    fcPatch[commodity] = -count;
                commodity->fc.count = std::max(0, commodity->fc.count - count);
                commodity->fc.timestamp = ge->timestamp;
            }
        }
        else if (direction == "tocarrier") {
            atMyCarrier = true;
            if (commodity->fc.timestamp < ge->timestamp || fcPatch.contains(commodity)) {
                if (fcPatch.contains(commodity))
                    fcPatch[commodity] += count;
                else
                    fcPatch[commodity] = count;
                commodity->fc.count += count;
                commodity->fc.timestamp = ge->timestamp;
            }
            if (cargo_timestamp < ge->timestamp && commodity->ship.timestamp < ge->timestamp) {
                commodity->ship.count = std::max(0, commodity->ship.count - count);
                commodity->ship.timestamp = ge->timestamp;
            }
        }
        else if (direction == "tosrv") {
            if (cargo_timestamp < ge->timestamp && commodity->ship.timestamp < ge->timestamp) {
                commodity->ship.count = std::max(0, commodity->ship.count - count);
                commodity->ship.timestamp = ge->timestamp;
            }
        }
    }
    if (!fcPatch.empty())
        Cfg.saveCarrierCargo(ge->timestamp, fcPatch);
    UIManager::updateCargoDialog();
}
