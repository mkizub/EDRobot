//
// Created by mkizub on 25.06.2025.
//

#include "pch.h"
#include "Configuration.h"
#include "CargoManager.h"
#include "Galaxy.h"
#include "ShipStats.h"
#include "ai/AIManager.h"
#include "net/RavenColonial.h"
#include "net/EDDN.h"
#include "ui/UIManager.h"

#include <openssl/md5.h>
#include <openssl/evp.h>

namespace st {
Lang lng {Lang::XX};

GuiFocus guiFocus {GuiFocus::None};
bool isDead {};
bool isNeedRebootRepair {};

EddnStarSystem eddnStarSystem;

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

spNavRoute currentNavRoute;
CompassInfo compass;
Autopilot autopilot;

void Autopilot::setDestBody(gal::spEntity body) {
    if (destBody == body)
        return;
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
    if (destDock == dock)
        return;
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
    if (st.flags2.fsd_hyperdrive_charging) os << "fsd_hyperdrive_charging,";
    if (st.flags2.supercruise_overcharge) os << "supercruise_overcharge,";
    if (st.flags2.supercruise_assist) os << "supercruise_assist,";
    if (st.oxygen < 1) os << std::format("oxygen {:.3f},", st.oxygen);
    if (st.health < 1) os << std::format("health {:.3f},", st.health);
    os << "pips:[" << int(st.pips[0]) << "," << int(st.pips[1]) << "," << int(st.pips[2]) << "]";
    os << "}";
    return os;
}
}

static int64_t fssSignalSystemAddress;
static std::vector<spGameEvent> allFSSSignalEvents;

inline void set(std::string& field, const js::value& value) {
    if (!value.is_string()) {
        field.clear();
        return;
    }
    if (field == value.as_string())
        return;
    field = value.as_string();
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
void parseEvent_NavBeaconScan(spGameEvent& ge);
void parseEvent_SAASignalsFound(spGameEvent& ge);
void parseEvent_FSSDiscoveryScan(spGameEvent& ge);
void parseEvent_FSSAllBodiesFound(spGameEvent& ge);
void parseEvent_FSSBodySignals(spGameEvent& ge);
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
        {"NavBeaconScan", parseEvent_NavBeaconScan},
        {"SAASignalsFound", parseEvent_SAASignalsFound},
        {"FSSDiscoveryScan", parseEvent_FSSDiscoveryScan},
        {"FSSAllBodiesFound", parseEvent_FSSAllBodiesFound},
        {"FSSBodySignals", parseEvent_FSSBodySignals},
        {"Scan", parseEvent_Scan},
        {"ScanBaryCentre", parseEvent_ScanBaryCentre},
        {"ApproachBody", parseEvent_ApproachBody},
        {"LeaveBody", parseEvent_LeaveBody},
};

spGameEvent Configuration::parseEvent(Timestamp& latest_log_timestamp, const std::string& line) {
    spGameEvent gameEvent;
    {
        if (trim(line).empty())
            return {};
        try {
            auto res = js::parse(line);
            auto& jev = res["event"].deref();
            auto& jts = res["timestamp"].deref();
            Timestamp ts;
            if (!jev.is_string() || jev.empty() || !jts.is_string() || !parseTimestampString(jts.as_string(), ts))
                return {};
            bool expired = false;
            if (ts < latest_log_timestamp)
                expired = true;
            else
                latest_log_timestamp = ts;
            gameEvent.reset(new GameEvent{std::move(res), ts, jev.as_string(), expired});
        } catch (const js::syntax_error& ex) {
            return {};
        }
    }

    auto& event = gameEvent->event;
    LOG_DEBUG("Journal event: {}", event);

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

void Configuration::readJournalChanges(std::ifstream& journalStream, Timestamp& latest_log_timestamp, std::string& journalLine) {
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
            auto ge = parseEvent(latest_log_timestamp, journalLine);
            if (ge && ge->event == "Shutdown")
                journalStream.close();
            journalLine.clear();
        }
    }
}

bool Configuration::loadGameStatus() {
    static std::ifstream ifs;
    //LOG_DEBUG("Loading Status.json");
    if (!ifs.is_open()) {
        std::wstring filename = mEDLogsPath + L"\\Status.json";
        ifs.open(filename);
        if (!ifs.is_open())
            return false;
    }

    std::optional<js::value> read_result;
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
        Timestamp latest_log_timestamp {};
        ge = parseEvent(latest_log_timestamp, buffer.str());
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
    st::ship.flags.all = j["Flags"].as_int_or();
    st::ship.flags2.all = j["Flags2"].as_int_or();
    st::ship.fireGroup = j["FireGroup"].as_int_or();
    auto gf = enum_cast<GuiFocus>(j["GuiFocus"].as_int_or());
    st::guiFocus = gf.has_value() ? gf.value() : GuiFocus::None;
    if (auto& jp = j["Pips"].as_array_or(); jp.size() == 3) {
        st::ship.pips[0] = jp[0].as_unsigned();
        st::ship.pips[1] = jp[1].as_unsigned();
        st::ship.pips[2] = jp[2].as_unsigned();
    }
    {
        auto& ss = st::shipStats;
        ss.fuelMain = j["Fuel"]["FuelMain"].as_real_or();
        ss.fuelReservoir = j["Fuel"]["FuelReservoir"].as_real_or();
        ss.cargo = j["Cargo"].as_real_or();
        ss.totalMass = ss.unladenMass + ss.fuelMain + ss.fuelReservoir + ss.cargo;
    }
    st::ship.balance = j["Balance"].as_int_or();
    st::ship.oxygen = j["Oxygen"].as_real_or(1);
    st::ship.health = j["Health"].as_real_or(1);
    st::ship.legalState = enum_cast<st::ShipStatus::LegalState>(j["LegalState"].as_string_or())
            .value_or(st::ShipStatus::LegalState::Clean);

    if (j["BodyName"].is_string()) {
        st::shipAtBody.nearBody = true;
        set(st::shipAtBody.bodyName, j["BodyName"]);
        st::shipAtBody.latitude = j["Latitude"].as_real_or();
        st::shipAtBody.longitude = j["Longitude"].as_real_or();
        st::shipAtBody.altitude = j["Altitude"].as_real_or();
        st::shipAtBody.heading = j["Heading"].as_real_or();
        st::shipAtBody.planetRadius = j["PlanetRadius"].as_real_or();
        st::shipAtBody.gravity = j["Gravity"].as_real_or();
        //LOG(INFO) << "Body: " << st::shipAtBody.bodyName << "; alt: " << std::round(st::shipAtBody.altitude/1000) << "km; radius: " << std::round(st::shipAtBody.planetRadius/1000) << "km";
        //if (old_status.flags.cruise && !st::ship.flags.cruise)
        //    LOG(INFO) << "Exit cruise alt: " << std::round(st::shipAtBody.altitude/1000) << "km";
    } else {
        st::shipAtBody.nearBody = false;
    }

    if (auto jd = j["Destination"]; jd.is_object()) {
        // "Destination":{ "System":2381282543995, "Body":0, "Name":"Col 285 Sector XK-O d6-69" }
        // "Destination":{ "System":2868098639337, "Body":1, "Name":"Orbital Construction Site: Piestrak Town" }
        int64 systemAddress = jd["System"].as_int_or();
        std::string name;
        if (jd["Name_Localised"].is_string())
            name = jd["Name_Localised"].as_string();
        else
            name = jd["Name"].as_string_or();
        int bodyId = jd["Body"].as_int_or();
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
    if (je["Odyssey"].is_bool())
        client.isOdyssey = je["Odyssey"].as_bool();
    if (je["Horizons"].is_bool())
        client.isHorizons = je["Horizons"].as_bool();
    client.language = je["language"].as_string();
    client.gameversion = je["gameversion"].as_string();
    client.build = je["build"].as_string();

    Lang lng = Lang::XX;
    if (client.language == "Russian/RU" || client.language.ends_with("/RU"))
        lng = Lang::RU;
    else if (client.language == "English/EN" || client.language.ends_with("/EN"))
        lng = Lang::EN;
    else if (client.language == "English/UK" || client.language.ends_with("/UK"))
        lng = Lang::EN;
    else {
        LOG(ERROR) << "Unsupported game language: " << client.language;
        lng = Lang::XX;
    }
    LOG(INFO) << "Game language: " << client.language << ": " << enum_name<Lang>(lng);
    Cfg.updateLanguage(lng);
}

static void setCommander(const std::string& name, const std::string& fid) {
    if (name != st::cmdr.name || fid != st::cmdr.fid) {
        LOG(INFO) << "CMDR: " << name;
        auto& cmdr = const_cast<st::Commander&>(st::cmdr);
        cmdr = {};
        cmdr.name = name;
        cmdr.fid = fid;
        cmdr.ravenKey = Cfg.getRavenColonialKey(name);
        {
            std::string key = name + ":" + fid;
            unsigned char digestMD5[MD5_DIGEST_LENGTH];
            MD5((unsigned char*)key.data(), key.size(), digestMD5);
            char output[((MD5_DIGEST_LENGTH + 2) / 3 * 4) + 1];
            EVP_EncodeBlock((unsigned char*)output, digestMD5, MD5_DIGEST_LENGTH);
            cmdr.uploaderId = output;
            std::erase(cmdr.uploaderId, '=');
        }
    }
}

static void setEddnStarSystem(spGameEvent& ge) {
    auto& je = ge->data;
    st::eddnStarSystem.name = je["StarSystem"].as_string();
    st::eddnStarSystem.addr = je["SystemAddress"].as_int_or();
    auto& jp = je["StarPos"].as_array();
    st::eddnStarSystem.pos = {jp[0].as_real(), jp[1].as_real(), jp[2].as_real()};
    gal::spStarSystem ss = gal::makeStarSystem(st::eddnStarSystem.name, st::eddnStarSystem.addr);
    if (cv::norm(ss->starPos) == 0)
        ss->starPos = st::eddnStarSystem.pos;
    gal::setCurrentStarSystem(ss);
}

void parseEvent_Commander(spGameEvent& ge) {
    auto& je = ge->data;
    setCommander(je["Name"].as_string(), je["FID"].as_string());
    UIManager::updateCommander();
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
    if (je["Odyssey"].is_bool())
        client.isOdyssey = je["Odyssey"].as_bool();
    if (je["Horizons"].is_bool())
        client.isHorizons = je["Horizons"].as_bool();
    client.gameversion = je["gameversion"].as_string();
    client.build = je["build"].as_string();
    client.language = je["language"].as_string();
    Lang lng = st::lng;
    if (client.language == "Russian/RU" || client.language.ends_with("/RU"))
        lng = Lang::RU;
    else if (client.language == "English/EN" || client.language.ends_with("/EN"))
        lng = Lang::EN;
    else if (client.language == "English/UK" || client.language.ends_with("/UK"))
        lng = Lang::EN;
    else {
        LOG(ERROR) << "Unsupported game language: " << client.language;
        lng = Lang::XX;
    }
    LOG(INFO) << "Game language: " << client.language << ": " << enum_name<Lang>(lng);
    Cfg.updateLanguage(lng);

    auto& shipInfo = const_cast<st::ShipInfo&>(st::shipInfo);
    set(shipInfo.shipType, je.at("Ship",""));
    set(shipInfo.shipTypeLocalised, je.at("Ship_Localised", ""));
    set(shipInfo.shipUserName, je.at("ShipName",""));
    set(shipInfo.shipIdent, je.at("ShipIdent",""));
    shipInfo.shipId = je["ShipID"].as_int_or();
    LOG(INFO) << "Ship: " << shipInfo.shipType;

    UIManager::updateCommander();
}

void parseEvent_CarrierLocation(spGameEvent& ge) {
    auto& je = ge->data;

    auto& cmdr = const_cast<st::Commander&>(st::cmdr);
    auto type = je["CarrierType"].as_string_or();
    if (gal::FLEET_CARRIER.match_type(type)) {
        cmdr.fleetCarrierId = je["CarrierID"].as_int_or();
        cmdr.fleetCarrierInSystem = je["StarSystem"].as_string_or();
        cmdr.fleetCarrierAtBodyId = je["BodyID"].as_int_or(-1);
        LOG(INFO) << "Fleet Carrier: " << cmdr.fleetCarrierId << " in system " << cmdr.fleetCarrierInSystem;
        CM.loadCarrierCargo();
    }
    if (gal::SQUADRON_CARRIER.match_type(type)) {
        cmdr.squadronCarrierId = je["CarrierID"].as_int_or();
        cmdr.squadronCarrierInSystem = je["StarSystem"].as_string_or();
        cmdr.squadronCarrierAtBodyId = je["BodyID"].as_int_or(-1);
        LOG(INFO) << "Squadron Carrier: " << cmdr.squadronCarrierId << " in system " << cmdr.squadronCarrierInSystem;
    }
    UIManager::updateCommander();
}

void parseEvent_Location(spGameEvent& ge) {
    auto& je = ge->data;

    setEddnStarSystem(ge);

    if (je["Docked"]) {
        st::dockedAt.marketId = je["MarketID"].as_int_or();
        set(st::dockedAt.stationName, je["StationName"]);
        set(st::dockedAt.stationType, je["StationType"]);
        gal::getCurrentStarSystem()->addStation(ge);
        st::space = {};
    } else {
        if (je["Body"].is_string()) {
            st::space.bodyId = je["BodyID"].as_int_or();
            set(st::space.bodyName, je["Body"]);
            set(st::space.bodyType, je["BodyType"]);
        } else {
            st::space = {};
        }
        st::dockedAt = {};
    }

    if (!ge->expired)
        EDDN::event_Location(ge);
}

void parseEvent_Loadout(spGameEvent& ge) {
    auto& je = ge->data;

    auto& shipInfo = const_cast<st::ShipInfo&>(st::shipInfo);
    set(shipInfo.shipType, je["Ship"]);
    set(shipInfo.shipTypeLocalised, je.at("Ship_Localised", shipInfo.shipType));
    set(shipInfo.shipUserName, je["ShipName"]);
    set(shipInfo.shipIdent, je["ShipIdent"]);
    shipInfo.shipId = je["ShipID"].as_int_or();
    LOG(INFO) << "Ship: " << shipInfo.shipType;

    {
        auto& ss = st::shipStats;
        ss.unladenMass = je["UnladenMass"].as_real_or();
        ss.cargoCapacity = je["CargoCapacity"].as_int_or();
        ss.fuelCapacityMain = je["FuelCapacity"]["Main"].as_real_or();
        ss.fuelCapacityReservoir = je["FuelCapacity"]["Reserve"].as_real_or();
        ss.totalMass = ss.unladenMass + ss.fuelMain + ss.fuelReservoir + ss.cargo;
    }

    st::isNeedRebootRepair = false;
    auto ss = eddb::initShipStats(shipInfo.shipType);
    if (ss && je["Modules"].is_array()) {
        auto& modules = je["Modules"].as_array();
        for (auto& m : modules) {
            ss->setSlotModule(m);
            auto slot_name = m["Slot"].as_string_or();
            auto item_name = m["Item"].as_string_or();
            if (slot_name == "FrameShiftDrive" || item_name == "int_dockingcomputer_advanced") {
                auto health = m["Health"].as_real_or(1.0);
                if (health < 0.05)
                    st::isNeedRebootRepair = true;
            }
        }
        ss->updateStats();
    }
    eddb::setShipStats(ss);
}

void parseEvent_Died(spGameEvent& ge) {
    st::isDead = true;
    if (!ge->expired)
        ai::interrupt(ai::InterruptReason::DEATH);
}

void parseEvent_Resurrect(spGameEvent& ge) {
    st::isDead = false;
}

void parseEvent_Cargo(spGameEvent& ge) {
    CM.loadShipCargo(ge);
}
void parseEvent_Market(spGameEvent& ge) {
    Cfg.loadMarket(ge);
}
void parseEvent_NavRoute(spGameEvent& ge) {
    Cfg.loadNavRoute(ge);
}
void parseEvent_NavRouteClear(spGameEvent& ge) {
    st::currentNavRoute = std::make_shared<NavRoute>();
}

void parseEvent_ShipyardSwap(spGameEvent& ge) {
    auto& je = ge->data;

    auto& shipInfo = const_cast<st::ShipInfo&>(st::shipInfo);
    shipInfo = {};
    set(shipInfo.shipType, je["Ship"]);
    set(shipInfo.shipTypeLocalised, je.at("Ship_Localised", shipInfo.shipType));
    set(shipInfo.shipUserName, je["ShipName"]);
    set(shipInfo.shipIdent, je["ShipIdent"]);
    shipInfo.shipId = je["ShipID"].as_int_or();
    LOG(INFO) << "Ship: " << shipInfo.shipType;
}

void parseEvent_Docked(spGameEvent& ge) {
    Cfg.dockingEvent = ge;

    auto& je = ge->data;

    gal::spStarSystem ss = gal::makeStarSystem(je["StarSystem"].as_string(), je["SystemAddress"].as_int());
    gal::setCurrentStarSystem(ss);
    st::dockedAt.marketId = je["MarketID"].as_int_or();
    set(st::dockedAt.stationName, je["StationName"]);
    set(st::dockedAt.stationType, je["StationType"]);
    gal::getCurrentStarSystem()->addStation(ge);
    st::space = {};

    if (!ge->expired)
        EDDN::event_Docked(ge);
}

void parseEvent_Undocked(spGameEvent& ge) {
    Cfg.dockingEvent.reset();
    auto& je = ge->data;
    st::space.marketId = je["MarketID"].as_int_or(st::dockedAt.marketId);
    st::space.stationName = je["StationName"].as_string_or(st::dockedAt.stationName);
    st::space.stationType = je["StationType"].as_string_or(st::dockedAt.stationType);
    st::dockedAt = {};
}

void parseEvent_Docking(spGameEvent& ge) {
    Cfg.dockingEvent = ge;
    auto& je = ge->data;
    st::space.marketId = je["MarketID"].as_int_or();
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
        auto& name = je["StarSystem"].as_string();
        int64_t address = je["SystemAddress"].as_int();
        gal::spStarSystem ss = gal::makeStarSystem(name, address);
        gal::setCurrentStarSystem(ss);
        st::autopilot.isDestDockTargeted = false;
        st::autopilot.isDestBodyTargeted = false;
        st::autopilot.isDestDockFocused = false;
        st::autopilot.isDestBodyFocused = false;
    }
    ai::resetCompassDetects();
}

void parseEvent_FSDJump(spGameEvent& ge) {
    auto& je = ge->data;

    st::shipAtBody.approachBody = false;
    st::shipAtBody.nearBody = false;
    st::dockedAt = {};
    st::space = {};

    setEddnStarSystem(ge);

    st::space.bodyId = je["BodyID"].as_int_or(-1);
    set(st::space.bodyName, je["Body"]);
    set(st::space.bodyType, je["BodyType"]);
    st::autopilot.isDestDockTargeted = false;
    st::autopilot.isDestBodyTargeted = false;
    st::autopilot.isDestDockFocused = false;
    st::autopilot.isDestBodyFocused = false;
    ai::resetCompassDetects();

    if (!ge->expired)
        EDDN::event_FSDJump(ge);
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
            ss->save();
        } else {
            carrier.reset();
        }
    }
    st::shipAtBody.approachBody = false;
    st::shipAtBody.nearBody = false;

    setEddnStarSystem(ge);

    st::space.bodyId = je["BodyID"].as_int_or(-1);
    set(st::space.bodyName, je["Body"]);
    set(st::space.bodyType, je["BodyType"]);
    if (carrier) {
        auto ss = gal::getCurrentStarSystem();
        std::erase_if(ss->stations, [carrier](auto& st)->bool {
            if (st->marketId == carrier->marketId)
                return true;
            return st->type == TypeNav::FleetCarrier && (st->nameEq(carrier->code) || st->nameEq(carrier->name));
        });
        gal::spEntity old_carrier;
        ss->stations.push_back(carrier);
        carrier->parentBodyId = st::space.bodyId;
        ss->saved = false;
       ss->save();
    }

    if (!ge->expired)
        EDDN::event_CarrierJump(ge);
}

void parseEvent_SupercruiseDestinationDrop(spGameEvent& ge) {
    auto& je = ge->data;

    st::dockedAt = {};
    st::space.marketId = je["MarketID"].as_int_or();
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

    int64_t marketId = je["MarketID"].as_int_or();
    if (st::ship.flags.docked && st::dockedAt.marketId == marketId) {
        // construction complete?
        set(st::dockedAt.stationName, je["Name"]);
        if (st::dockedAt.stationType == "PlanetaryConstructionDepot")
            st::dockedAt.stationType.clear();
    } else {
        st::dockedAt = {};
        st::space.marketId = marketId;
        set(st::space.stationName, je["Name"]);
        st::space.stationType.clear();
        st::space.bodyId = je["BodyID"].as_int_or();
        set(st::space.bodyName, je["BodyName"]);
        set(st::space.bodyType, "Planet");
    }

    if (st::space.stationType.empty() && st::space.stationName.starts_with("Planetary Construction Site"))
        st::space.stationType = "PlanetaryConstructionDepot";
    if (auto ss = gal::getCurrentStarSystem())
        ss->addStation(ge);
}

void parseEvent_SupercruiseExit(spGameEvent& ge) {
    auto& je = ge->data;

    st::dockedAt = {};
    st::space.bodyId = je["BodyID"].as_int_or();
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
            EDDN::event_FSSSignalDiscovered(allFSSSignalEvents);
        }
        allFSSSignalEvents.clear();
        fssSignalSystemAddress = 0;
    } else {
        auto& je = ge->data;

        int64_t address = je["SystemAddress"].as_int_or();
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

void parseEvent_NavBeaconScan(spGameEvent& ge) {
    if (!ge->expired)
        EDDN::event_NavBeaconScan(ge);
}

void parseEvent_SAASignalsFound(spGameEvent& ge) {
    if (!ge->expired)
        EDDN::event_SAASignalsFound(ge);
}

void parseEvent_FSSDiscoveryScan(spGameEvent& ge) {
    if (!ge->expired)
        EDDN::event_FSSDiscoveryScan(ge);
}

void parseEvent_FSSAllBodiesFound(spGameEvent& ge) {
    if (!ge->expired)
        EDDN::event_FSSAllBodiesFound(ge);
}

void parseEvent_FSSBodySignals(spGameEvent& ge) {
    if (!ge->expired)
        EDDN::event_FSSBodySignals(ge);
}

void parseEvent_Scan(spGameEvent& ge) {
    auto& je = ge->data;

    auto starSystem = je["StarSystem"].as_string_or();
    int64_t address = je["SystemAddress"].as_int_or();
    int bodyId = je["BodyID"].as_int_or(-1);
    gal::spStarSystem ss = gal::makeStarSystem(starSystem, address);
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
        if (je["Subclass"].is_int())
            code += std::to_string(je["Subclass"].as_int());
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
                if (jp["Null"].is_int()) {
                    p_type = TypeNav::Barycenter;
                    p_id = jp["Null"].as_int();
                }
                else if (jp["Star"].is_int()) {
                    p_type = TypeNav::Star;
                    p_id = jp["Star"].as_int();
                }
                else if (jp["Planet"].is_int()) {
                    p_type = TypeNav::Planet;
                    p_id = jp["Planet"].as_int();
                }
                else if (jp["Ring"].is_int()) {
                    p_type = TypeNav::Ring;
                    p_id = jp["Ring"].as_int();
                }
                else if (jp["AsteroidCluster"].is_int()) {
                    p_type = TypeNav::AsteroidCluster;
                    p_id = jp["AsteroidCluster"].as_int();
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
        double dist_ls = bd.as_real();
        if (std::round(body->main_star_distance.get_ls()) != std::round(dist_ls)) {
            body->main_star_distance = dist_t(dist_t::LS, dist_ls);
            ss->saved = false;
        }
    }
    if (auto br = je["Radius"]; br.is_number()) {
        double r = std::round(br.as_real()) / 1000.0; // meters->kilometers
        if (body->radius != r) {
            body->radius = r;
            ss->saved = false;
        }
    }
    if (!ss->saved)
        ss->save();

    if (!ge->expired)
        EDDN::event_Scan(ge);
}

void parseEvent_ScanBaryCentre(spGameEvent& ge) {
    auto& je = ge->data;

    auto starSystem = je["StarSystem"].as_string_or();
    int64_t address = je["SystemAddress"].as_int_or();
    int bodyId = je["BodyID"].as_int();
    gal::spStarSystem ss = gal::makeStarSystem(starSystem, address);
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
        ss->save();

    if (!ge->expired)
        EDDN::event_ScanBaryCentre(ge);
}

void parseEvent_ApproachBody(spGameEvent& ge) {
    auto& je = ge->data;

    st::shipAtBody.approachBody = true;
    st::shipAtBody.bodyId = je["BodyID"].as_int_or();
    set(st::shipAtBody.bodyName, je["Body"]);
}

void parseEvent_LeaveBody(spGameEvent& ge) {
    auto& je = ge->data;

    st::shipAtBody.approachBody = false;
    st::shipAtBody.bodyId = je["BodyID"].as_int_or();
    set(st::shipAtBody.bodyName, je["Body"]);
}

void parseEvent_ColonisationConstructionDepot(spGameEvent& ge) {
    auto& je = ge->data;

    auto starSystem = gal::getCurrentStarSystem();
    if (!starSystem)
        return;
    int64_t marketId = je["MarketID"].as_int_or();
    spMarket old_market = gal::getMarket(marketId);
    if (old_market && old_market->timestamp >= ge->timestamp)
        return;
    auto dock = starSystem->getDock(marketId);
    spMarket market = std::make_shared<Market>(Market{
            .timestamp = ge->timestamp,
            .marketId = marketId,
            .stationName = dock ? dock->name : "",
            .stationType = "ConstrDepot",
            .starSystem = starSystem->systemName,
    });
    if (old_market)
        market->raven = old_market->raven;
    bool complete = je["ConstructionComplete"].as_bool();
    bool hasRaven = Cfg.isRavenColonialEnabled() && !market->ravenBuildId().empty();
    bool reportToRaven = false;
    if (hasRaven) {
        if (!ge->expired && market->raven->timestamp.time_since_epoch().count() == 0)
            reportToRaven = true;
        else if (complete && market->raven->status != "complete")
            reportToRaven = true;
        else {
            // check if there was a contribution past last raven project update
            for (auto& ci : market->raven->commanders) {
                if (ci.second.timestamp > market->raven->timestamp)
                    reportToRaven = true;
            }
        }
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
        int provided = jr["ProvidedAmount"].as_int_or();
        int required = jr["RequiredAmount"].as_int_or();
        if (hasRaven && old_market) {
            if (!old_market->items.contains(commodity))
                reportToRaven = true;
            else {
                auto& old_ml = old_market->items.at(commodity);
                if (old_ml.stock != provided || old_ml.demand != required)
                    reportToRaven = true;
            }
        }
        MarketLine ml{};
        ml.sellPrice = jr["Payment"].as_int_or();
        ml.stock = provided;
        ml.demand = required;
        ml.isConsumer = true;
        ml.isProducer = false;
        market->items.emplace(commodity, ml);
    }

    if (hasRaven && reportToRaven) {
        if (market->ravenBuildId().empty() || market->raven->timestamp >= ge->timestamp)
            reportToRaven = false;
        else if (market->raven->status == "complete")
            reportToRaven = false;
        else
            market->raven->timestamp = ge->timestamp;
    }
    gal::setMarketData(market);
    if (hasRaven && reportToRaven) {
        RavenColonial::reportConstructionDepot(ge, market);
    }
    else if (complete && market->raven->status != "complete") {
        market->raven->status = "complete";
        gal::saveMarket(market.get());
    }
 }

void parseEvent_ColonisationContribution(spGameEvent& ge) {
    Cfg.marketEvent = ge;
    RavenColonial::reportContribution(ge);
    CM.processColonisationContribution(ge);
}
void parseEvent_MarketBuy(spGameEvent& ge) {
    CM.processMarketBuy(ge);
    Cfg.marketEvent = ge;
}
void parseEvent_MarketSell(spGameEvent& ge) {
    CM.processMarketSell(ge);
    Cfg.marketEvent = ge;
}

void parseEvent_CargoTransfer(spGameEvent& ge) {
    CM.processCargoTransfer(ge);
}
