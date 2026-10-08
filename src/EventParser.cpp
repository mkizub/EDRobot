//
// Created by mkizub on 25.06.2025.
//

#ifndef PCH_H
#include "../pch.h"
#endif

#include "Configuration.h"
#include "CargoManager.h"
#include "ShipStats.h"
#include "gal/Galaxy.h"
#include "ai/AIManager.h"
#include "db/DB.h"
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

enum CleanupReason {
    FILEHEADER, DEATH, SHUTDOWN
};
void cleanup(CleanupReason reason) {
    if (reason == FILEHEADER || reason == SHUTDOWN) {
        auto &cmdr = const_cast<st::Commander &>(st::cmdr);
        cmdr = {};
        auto &client = const_cast<st::GameClient &>(st::client);
        client = {};
    }

    guiFocus = GuiFocus::None;
    isDead = false;
    isNeedRebootRepair = false;

    eddnStarSystem = {};
    gal::resetCurrentStarSystem();

    auto& shipInfo = const_cast<st::ShipInfo&>(st::shipInfo);
    shipInfo = {};

    shipStats = {};
    dockedAt = {};
    space = {};
    navFilters = {};
    destination = {};
    ship = {};
    shipAtBody = {};

    currentNavRoute.reset();
    compass = {};
    autopilot = {};
}

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

inline bool set(std::string& field, const js::value& value) {
    bool changed = false;
    if (!value.is_string()) {
        if (!field.empty()) {
            changed = true;
            field.clear();
        }
        return changed;
    }
    if (field == value.as_string())
        return false;
    field = value.as_string();
    return true;
}

void parseEvent_Fileheader(spGameEvent& ge);
void parseEvent_Shutdown(spGameEvent& ge);
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
void parseEvent_PowerplayCollect(spGameEvent& ge);
void parseEvent_PowerplayDeliver(spGameEvent& ge);
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
void parseEvent_CarrierBuy(spGameEvent& ge);
void parseEvent_CarrierNameChange(spGameEvent& ge);
void parseEvent_CarrierStats(spGameEvent& ge);
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
        {"Shutdown", parseEvent_Shutdown},
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
        {"PowerplayCollect", parseEvent_PowerplayCollect},
        {"PowerplayDeliver", parseEvent_PowerplayDeliver},
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
        {"CarrierBuy", parseEvent_CarrierBuy},
        {"CarrierNameChange", parseEvent_CarrierNameChange},
        {"CarrierStats", parseEvent_CarrierStats},
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
            gameEvent.reset(new GameEvent{std::move(res), ts, *jev.as_string(), expired});
        } catch (const js::syntax_error& ex) {
            return {};
        }
    }

    auto& event = gameEvent->event;
    //LOG_DEBUG("Journal event: {}", event);

    try {
        if (fssSignalSystemAddress) {
            if (event != "FSSSignalDiscovered") {
                spGameEvent empty;
                try {
                    parseEvent_FSSSignalDiscovered(empty);
                } catch (...) {
                    LOG_ERROR("Uncatched exception in events: FSSSignalDiscovered");
                }
            }
        }
        auto it = eventMap.find(event);
        if (it != eventMap.end())
            it->second(gameEvent);
    } catch (...) {
        LOG_ERROR("Uncatched exception in event: {}", line);
    }

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

    //std::optional<js::value> read_result;
    std::stringstream buffer;
    try {
        spGameEvent ge;
        if (!ifs.is_open()) {
            std::wstring filename = mEDLogsPath + L"\\Status.json";
            ifs.open(filename);
            if (!ifs.is_open())
                return false;
        }
        if (!ifs) {
            ifs.clear();
            ifs.seekg(0, std::ios::beg);
        }
        buffer << ifs.rdbuf();
        ifs.clear();
        ifs.seekg(0, std::ios::beg);
        Timestamp latest_log_timestamp{};
        ge = parseEvent(latest_log_timestamp, buffer.str());
        if (!ge || ge->event != "Status") {
            LOG_WARNING("Status.json: {}", buffer.str());
            Sleep(20);
            ifs.close();
            return false;
        }
        //st::ShipStatus old_status = st::ship;
        auto &j = ge->data;
        st::ship.timestamp = ge->timestamp;
        st::ship.flags.all = j["Flags"].as_int_or();
        st::ship.flags2.all = j["Flags2"].as_int_or();
        st::ship.fireGroup = j["FireGroup"].as_int_or();
        auto gf = enum_cast<GuiFocus>(j["GuiFocus"].as_int_or());
        st::guiFocus = gf.has_value() ? gf.value() : GuiFocus::None;
        if (auto &jp = j["Pips"].as_array_or(); jp.size() == 3) {
            st::ship.pips[0] = jp[0].as_unsigned();
            st::ship.pips[1] = jp[1].as_unsigned();
            st::ship.pips[2] = jp[2].as_unsigned();
        }
        {
            auto &ss = st::shipStats;
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
            body_id_t bodyId(jd["Body"].as_int_or(-1));
            if (systemAddress != st::destination.systemAddress || name != st::destination.name || bodyId != st::destination.bodyId) {
                st::destination.systemAddress = systemAddress;
                st::destination.name = name;
                st::destination.bodyId = bodyId;
                gal::spStarSystem ss = gal::getStarSystem(systemAddress, true, false);
                gal::updateFromDestination(ss.get(), ge);
            }
            if (st::autopilot.destBody && st::autopilot.destBody->nameEq(st::destination.name)) {
                st::autopilot.isDestBodyTargeted = true;
                st::autopilot.isDestDockTargeted = false;
            } else if (st::autopilot.destDock && st::autopilot.destDock->nameEq(st::destination.name)) {
                st::autopilot.isDestBodyTargeted = false;
                st::autopilot.isDestDockTargeted = true;
            } else {
                st::autopilot.isDestBodyTargeted = false;
                st::autopilot.isDestDockTargeted = false;
            }
        } else {
            st::destination = {};
            st::autopilot.isDestBodyTargeted = false;
            st::autopilot.isDestDockTargeted = false;
        }
    } catch (...) {
        LOG_ERROR("Uncatched exception while reading Status.json: {}", buffer.str());
    }

    //LOG(INFO) << "Ship status: " << st::ship;
    return true;
}

void setGameVersion(spGameEvent& ge) {
    auto& je = ge->data;
    auto& client = const_cast<st::GameClient&>(st::client);
    client.majorVersion = 0;
    if (je["Odyssey"].is_bool())
        client.isOdyssey = je["Odyssey"].as_bool();
    if (je["Horizons"].is_bool())
        client.isHorizons = je["Horizons"].as_bool();
    if (je["gameversion"].is_string()) {
        client.gameversion = je["gameversion"].as_string();
        auto spl = split(client.gameversion, '.');
        if (spl.size() >= 2) {
            client.majorVersion = atoi(spl[0].c_str());
            client.minorVersion = atoi(spl[1].c_str());
        }
    }
    if (je["build"].is_string())
        client.build = je["build"].as_string();

    if (je["language"].is_string()) {
        client.language = *je["language"].as_string();
        std::replace(client.language.begin(), client.language.end(), '\\', '/');
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
        if (lng != st::lng) {
            LOG(INFO) << "Game language: " << client.language << ": " << enum_name<Lang>(lng);
            Cfg.updateLanguage(lng);
        }
    }

    Cfg.loadPlayerOptions(false);
}

void parseEvent_Fileheader(spGameEvent& ge) {
    st::cleanup(st::FILEHEADER);
    setGameVersion(ge);
}

void parseEvent_Shutdown(spGameEvent& ge) {
    LOG_INFO("Game shutdown");
    ai::stop();
    st::cleanup(st::SHUTDOWN);
    gal::resetCurrentStarSystem();
    gal::cleanupStarSystemCache({});
    db::checkpoint(db::CheckPoint::FULL);
}

static void setCommander(std::string_view name, std::string_view fid) {
    if (name != st::cmdr.name || fid != st::cmdr.fid) {
        LOG(INFO) << "CMDR: " << name;
        auto& cmdr = const_cast<st::Commander&>(st::cmdr);
        cmdr = {};
        cmdr.name = *name;
        cmdr.fid = *fid;
        cmdr.ravenKey = Cfg.getRavenColonialKey(*name);
        {
            std::string key = *name + ":" + *fid;
            unsigned char digestMD5[MD5_DIGEST_LENGTH];
            MD5((unsigned char*)key.data(), key.size(), digestMD5);
            char output[((MD5_DIGEST_LENGTH + 2) / 3 * 4) + 1];
            EVP_EncodeBlock((unsigned char*)output, digestMD5, MD5_DIGEST_LENGTH);
            cmdr.uploaderId = output;
            std::erase(cmdr.uploaderId, '=');
        }
    }
}

static gal::spStarSystem setEddnStarSystem(spGameEvent& ge) {
    auto& je = ge->data;
    st::eddnStarSystem.name = je["StarSystem"].as_string();
    st::eddnStarSystem.addr = je["SystemAddress"].as_int_or();
    auto& jp = je["StarPos"].as_array();
    st::eddnStarSystem.coords = {jp[0].as_real(), jp[1].as_real(), jp[2].as_real()};
    gal::spStarSystem ss = gal::makeStarSystem(st::eddnStarSystem.name, st::eddnStarSystem.addr, st::eddnStarSystem.coords, true, false);
    assert ((!ss->systemCoords.empty() || st::eddnStarSystem.name == "Sol") && !ss->needCoreSave);
    gal::setCurrentStarSystem(ss);
    return ss;
}

void parseEvent_Commander(spGameEvent& ge) {
    auto& je = ge->data;
    setCommander(je["Name"].as_string(), je["FID"].as_string());
    UIManager::updateCommander();
}

void printShipInfo() {
    std::string str = "Ship: ";
    if (st::shipInfo.shipTypeLocalised == st::shipInfo.shipType)
        str.append(st::shipInfo.shipType).append(" ");
    else
        str.append(st::shipInfo.shipTypeLocalised).append(" (").append(st::shipInfo.shipType).append(") ");
    if (!st::shipInfo.shipUserName.empty())
        str.append(st::shipInfo.shipUserName).append(" ");
    str.append("(id ").append(std::to_string(st::shipInfo.shipId)).append(")");
}

void parseEvent_LoadGame(spGameEvent& ge) {
    {
        // for relogin
        Cfg.dockingEvents.clear();
        Cfg.marketEvents.clear();
        Cfg.scanEvents.clear();
        st::compass = {};
    }
    auto& je = ge->data;
    if (je["Commander"].is_string())
        setCommander(je["Commander"].as_string(), je["FID"].as_string());

    setGameVersion(ge);

    bool shipChanged = false;
    auto& shipInfo = const_cast<st::ShipInfo&>(st::shipInfo);
    shipChanged |= set(shipInfo.shipType, je["Ship"]);
    shipChanged |= set(shipInfo.shipTypeLocalised, je.at("Ship_Localised", shipInfo.shipType));
    shipChanged |= set(shipInfo.shipUserName, je["ShipName"]);
    shipChanged |= set(shipInfo.shipIdent, je["ShipIdent"]);
    shipInfo.shipId = je["ShipID"].as_int_or();
    if (shipChanged)
        printShipInfo();

    UIManager::updateCommander();
}

void parseEvent_CarrierLocation(spGameEvent& ge) {
    auto& je = ge->data;

    auto& cmdr = const_cast<st::Commander&>(st::cmdr);
    const auto type = je["CarrierType"].as_string_or("FleetCarrier");
    if (gal::FLEET_CARRIER.match_type(type)) {
        cmdr.fleetCarrierId = je["CarrierID"].as_int_or();
        cmdr.fleetCarrierInSystem = je["StarSystem"].as_string_or();
        cmdr.fleetCarrierAtBodyId = je["BodyID"].as_int_or(-1);
        auto starSystem = gal::getStarSystem(cmdr.fleetCarrierInSystem, true, false);
        if (starSystem) {
            auto body = starSystem->getBodyById(cmdr.fleetCarrierAtBodyId);
            if (body)
                cmdr.fleetCarrierAtBody = body->name;
            auto carrier = starSystem->moveMegashipHere(cmdr.fleetCarrierId, cmdr.fleetCarrierCode, cmdr.fleetCarrierName, cmdr.fleetCarrierAtBodyId, true);
            if (carrier) {
                cmdr.fleetCarrierCode = carrier->name;
                cmdr.fleetCarrierName = carrier->getLocalisedName();
            }
        }
        LOG_INFO("Fleet Carrier: {}[{}]({}) in system '{}' at '{}' ({})", cmdr.fleetCarrierCode, cmdr.fleetCarrierId, cmdr.fleetCarrierName,
                 cmdr.fleetCarrierInSystem, cmdr.fleetCarrierAtBody, cmdr.fleetCarrierAtBodyId.val);
        CM.loadCarrierCargo();
    }
    if (gal::SQUADRON_CARRIER.match_type(type)) {
        cmdr.squadronCarrierId = je["CarrierID"].as_int_or();
        cmdr.squadronCarrierInSystem = je["StarSystem"].as_string_or();
        cmdr.squadronCarrierAtBodyId = je["BodyID"].as_int_or(-1);
        auto starSystem = gal::getStarSystem(cmdr.squadronCarrierInSystem, true, false);
        if (starSystem) {
            auto body = starSystem->getBodyById(cmdr.squadronCarrierAtBodyId);
            if (body)
                cmdr.squadronCarrierAtBody = body->name;
            auto carrier = starSystem->moveMegashipHere(cmdr.squadronCarrierId, cmdr.squadronCarrierCode, cmdr.squadronCarrierName, cmdr.squadronCarrierAtBodyId, true);
            if (carrier) {
                cmdr.squadronCarrierCode = carrier->name;
                cmdr.squadronCarrierName = carrier->getLocalisedName();
            }
        }
        LOG_INFO("Squad Carrier: {}[{}]({}) in system '{}' at '{}' ({})", cmdr.squadronCarrierCode, cmdr.squadronCarrierId, cmdr.squadronCarrierName,
                 cmdr.squadronCarrierInSystem, cmdr.squadronCarrierAtBody, cmdr.squadronCarrierAtBodyId.val);
    }
    UIManager::updateCommander();
}

void parseEvent_Location(spGameEvent& ge) {
    auto& je = ge->data;

    auto starSystem = setEddnStarSystem(ge);

    if (je["Docked"].as_bool_or()) {
        st::dockedAt.marketId = je["MarketID"].as_int_or();
        set(st::dockedAt.stationName, je["StationName"]);
        set(st::dockedAt.stationType, je["StationType"]);
        st::space = {};
    } else {
        if (je["Body"].is_string()) {
            st::space.bodyId = je["BodyID"].as_int_or(-1);
            set(st::space.bodyName, je["Body"]);
            set(st::space.bodyType, je["BodyType"]);
            gal::updateBodyFromSpace(starSystem.get(), ge);
        } else {
            st::space = {};
        }
        st::dockedAt = {};
    }

    if (!ge->expired) {
        EDDN::event_Location(ge);
        gal::updateFromGameEvent(starSystem.get(), ge);
        if (je["Docked"].as_bool_or())
            gal::updateFromDockEvent(starSystem.get(), ge);
    }
}

void parseEvent_Loadout(spGameEvent& ge) {
    auto& je = ge->data;

    bool shipChanged = false;
    auto& shipInfo = const_cast<st::ShipInfo&>(st::shipInfo);
    shipChanged |= set(shipInfo.shipType, je["Ship"]);
    shipChanged |= set(shipInfo.shipTypeLocalised, je.at("Ship_Localised", shipInfo.shipType));
    shipChanged |= set(shipInfo.shipUserName, je["ShipName"]);
    shipChanged |= set(shipInfo.shipIdent, je["ShipIdent"]);
    shipInfo.shipId = je["ShipID"].as_int_or();
    if (shipChanged)
        printShipInfo();

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
            const auto slot_name = m["Slot"].as_string_or();
            const auto item_name = m["Item"].as_string_or();
            if (slot_name == "FrameShiftDrive" || item_name == "int_dockingcomputer_advanced") {
                auto health = m["Health"].as_real_or(1.0);
                if (health < 0.05) {
                    st::isNeedRebootRepair = true;
                    LOG_WARNING("Slot '{}' item '{}' health {:.2f} - need repair", slot_name, item_name, health);
                }
            }
        }
        ss->updateStats();
    }
    eddb::setShipStats(ss);
}

void parseEvent_Died(spGameEvent& ge) {
    st::cleanup(st::DEATH);
    st::isDead = true;
    if (!ge->expired)
        ai::interrupt(ai::InterruptReason::DEATH);
}

void parseEvent_Resurrect(spGameEvent& ge) {
    st::cleanup(st::DEATH);
    st::isDead = false;
}

void parseEvent_Cargo(spGameEvent& ge) {
    CM.loadShipCargo(ge);
    if (!ge->expired)
        Cfg.marketEvents.push(ge);
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

void parseEvent_PowerplayCollect(spGameEvent& ge) {
    if (!ge->expired)
        Cfg.marketEvents.push(ge);
}

void parseEvent_PowerplayDeliver(spGameEvent& ge) {
    if (!ge->expired)
        Cfg.marketEvents.push(ge);
}


void parseEvent_ShipyardSwap(spGameEvent& ge) {
    auto& je = ge->data;

    auto& shipInfo = const_cast<st::ShipInfo&>(st::shipInfo);
    shipInfo = {};
    set(shipInfo.shipType, je["ShipType"]);
    set(shipInfo.shipTypeLocalised, je.at("ShipType_Localised", shipInfo.shipType));
    shipInfo.shipUserName.clear();
    shipInfo.shipIdent.clear();
    shipInfo.shipId = je["ShipID"].as_int_or();
    printShipInfo();
}

void parseEvent_Docked(spGameEvent& ge) {
    auto& je = ge->data;

    auto systemName = je["StarSystem"].as_string();
    auto systemAddress = je["SystemAddress"].as_int_or();
    Coords coords = (systemName == st::eddnStarSystem.name && systemAddress == st::eddnStarSystem.addr)
            ? st::eddnStarSystem.coords : Coords{};
    gal::spStarSystem ss = gal::makeStarSystem(systemName, systemAddress, coords, true, true);
    gal::setCurrentStarSystem(ss);
    st::dockedAt.marketId = je["MarketID"].as_int_or();
    set(st::dockedAt.stationName, je["StationName"]);
    set(st::dockedAt.stationType, je["StationType"]);
    st::space = {};

    if (!ge->expired) {
        gal::updateFromDockEvent(ss.get(), ge);
        EDDN::event_Docked(ge);
        Cfg.dockingEvents.push(ge);
    }
}

void parseEvent_Undocked(spGameEvent& ge) {
    auto& je = ge->data;
    st::space.marketId = je["MarketID"].as_int_or(st::dockedAt.marketId);
    st::space.stationName = je["StationName"].as_string_or(st::dockedAt.stationName);
    st::space.stationType = je["StationType"].as_string_or(st::dockedAt.stationType);
    st::dockedAt = {};
    if (!ge->expired) {
        gal::updateFromDockEvent(gal::getCurrentStarSystem().get(), ge);
        Cfg.dockingEvents.push(ge);
    }
}

void parseEvent_Docking(spGameEvent& ge) {
    auto& je = ge->data;
    st::space.marketId = je["MarketID"].as_int_or();
    st::space.stationName = je["StationName"].as_string_or();
    st::space.stationType = je["StationType"].as_string_or(st::dockedAt.stationType);
    auto ss = gal::getCurrentStarSystem();
    auto dock = ss->getDock(st::space.marketId);
    if (!dock)
        dock = ss->getDock(st::space.stationName);
    auto dt = toTypeNav(JsNavType::get(st::space.stationType));
    //if (dock && isSite(dt) && dock->type != dt) {
    //    dock->setType(dt);
    //    dock->setMarketId(st::space.marketId);
    //    ss->needBlobSave = true;
    //}
    if (dock && dock->marketId() != st::space.marketId) {
        dock->setMarketId(st::space.marketId);
        ss->needBlobSave = true;
    }
    ss->save();
    if (!ge->expired) {
        gal::updateFromDockEvent(ss.get(), ge);
        Cfg.dockingEvents.push(ge);
    }
}

void parseEvent_StartJump(spGameEvent& ge) {
    auto& je = ge->data;

    st::shipAtBody.approachBody = false;
    st::shipAtBody.nearBody = false;
    st::dockedAt = {};
    st::space = {};
    // "Hyperspace" or "Supercruise"
    if (je["JumpType"].as_string_or() == "Hyperspace") {
        auto name = je["StarSystem"].as_string();
        int64_t address = je["SystemAddress"].as_int();
        gal::spStarSystem ss = gal::makeStarSystem(name, address, {}, true, true);
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

    auto starSystem = setEddnStarSystem(ge);

    st::space.bodyId = je["BodyID"].as_int_or(-1);
    set(st::space.bodyName, je["Body"]);
    set(st::space.bodyType, je["BodyType"]);
    gal::updateBodyFromSpace(starSystem.get(), ge);

    st::autopilot.isDestDockTargeted = false;
    st::autopilot.isDestBodyTargeted = false;
    st::autopilot.isDestDockFocused = false;
    st::autopilot.isDestBodyFocused = false;
    ai::resetCompassDetects();

    if (!ge->expired) {
        EDDN::event_FSDJump(ge);
        gal::updateFromGameEvent(starSystem.get(), ge);
    }
}

void parseEvent_CarrierBuy(spGameEvent& ge) {
    auto& je = ge->data;

    auto& cmdr = const_cast<st::Commander&>(st::cmdr);
    auto stype = je["CarrierType"].as_string_or("FleetCarrier");
    TypeNav type = toTypeNav(JsNavType::get(stype));
    if (type == TypeNav::FleetCarrier) {
        cmdr.fleetCarrierId = je["CarrierID"].as_int();
        cmdr.fleetCarrierCode = je["Callsign"].as_string();
        cmdr.fleetCarrierName = cmdr.fleetCarrierCode;
        cmdr.fleetCarrierInSystem = je["Location"].as_string();
        cmdr.fleetCarrierAtBody = {};
        cmdr.fleetCarrierAtBodyId = -1;
    }
    else if (type == TypeNav::SquadronCarrier) {
        cmdr.squadronCarrierId = je["CarrierID"].as_int();
        cmdr.squadronCarrierCode = je["Callsign"].as_string();
        cmdr.squadronCarrierName = cmdr.squadronCarrierName;
        cmdr.squadronCarrierInSystem = je["Location"].as_string();
        cmdr.squadronCarrierAtBody = {};
        cmdr.squadronCarrierAtBodyId = -1;
    }
    else {
        LOG_ERROR("Bought carrier of unknown type {}", stype);
        return;
    }

    auto starSystem = gal::makeStarSystem(je["Location"].as_string(), je["SystemAddress"].as_int(), {}, true, false);
    if (starSystem) {
        gal::spEntity carrier;
        if (type == TypeNav::FleetCarrier)
            carrier = starSystem->addEntity(type, cmdr.fleetCarrierCode, "", {.market_id=cmdr.fleetCarrierId, .ts=ge->timestamp});
        else
            carrier = starSystem->addEntity(type, cmdr.squadronCarrierCode, "", {.market_id=cmdr.squadronCarrierId, .ts=ge->timestamp});
        carrier->setMarketId(je["CarrierID"].as_int());
        starSystem->save();
    }
}

void parseEvent_CarrierNameChange(spGameEvent& ge) {
    auto& je = ge->data;

    auto ss = gal::getCurrentStarSystem();
    if (!ss || !ss->systemAddress)
        return;

    auto& cmdr = const_cast<st::Commander&>(st::cmdr);
    auto stype = je["CarrierType"].as_string_or("FleetCarrier");
    int64_t carrierId = je["CarrierID"].as_int_or();
    auto carrierCode = je["Callsign"].as_string_or();
    auto carrierName = je["Name"].as_string_or();
    TypeNav type = toTypeNav(JsNavType::get(stype));
    if (type == TypeNav::FleetCarrier) {
        if (cmdr.fleetCarrierId && cmdr.fleetCarrierId != carrierId) {
            LOG_ERROR("CarrierStats event: carrier id {} != {}", cmdr.fleetCarrierId, carrierId);
            return;
        }
        if (!cmdr.fleetCarrierCode.empty() && cmdr.fleetCarrierCode != carrierCode) {
            LOG_ERROR("CarrierStats event: carrier callsign {} != {}", cmdr.fleetCarrierCode, carrierCode);
            return;
        }
        if (!cmdr.fleetCarrierId)
            cmdr.fleetCarrierId = carrierId;
        if (cmdr.fleetCarrierCode.empty())
            cmdr.fleetCarrierCode = carrierCode;
        auto nloc = *carrierName +" "+ *cmdr.fleetCarrierCode;
        if (nloc != cmdr.fleetCarrierName)
            cmdr.fleetCarrierName = nloc;
        auto carrier = ss->getDock(carrierId);
        if (!carrier)
            carrier = ss->getDock(carrierCode);
        if (!carrier)
            carrier = ss->moveMegashipHere(carrierId, cmdr.fleetCarrierCode, cmdr.fleetCarrierName, body_id_t{}, true);
        if (!carrier)
            carrier = ss->addEntity(TypeNav::FleetCarrier, cmdr.fleetCarrierCode, cmdr.fleetCarrierName,
                                    {.market_id=cmdr.fleetCarrierId, .ts=ge->timestamp});
        else
            carrier = ss->checkTypeAndName(carrier, TypeNav::FleetCarrier, cmdr.fleetCarrierCode, cmdr.fleetCarrierName,
                                           {.market_id=cmdr.fleetCarrierId, .ts=ge->timestamp});
        if (carrier && carrier->marketId() != carrierId) {
            carrier->setMarketId(carrierId);
            ss->needBlobSave = true;
        }
    }
    else if (type == TypeNav::SquadronCarrier) {
        if (cmdr.squadronCarrierId && cmdr.squadronCarrierId != carrierId) {
            LOG_ERROR("CarrierStats event: carrier id {} != {}", cmdr.squadronCarrierId, carrierId);
            return;
        }
        if (!cmdr.squadronCarrierCode.empty() && cmdr.squadronCarrierCode != carrierCode) {
            LOG_ERROR("CarrierStats event: carrier callsign {} != {}", cmdr.squadronCarrierCode, carrierCode);
            return;
        }
        if (!cmdr.squadronCarrierId)
            cmdr.squadronCarrierId = carrierId;
        if (cmdr.squadronCarrierCode.empty())
            cmdr.squadronCarrierCode = carrierCode;
        auto nloc = *carrierName +" | "+ *cmdr.squadronCarrierCode;
        if (nloc != cmdr.squadronCarrierName)
            cmdr.squadronCarrierName = nloc;
        auto carrier = ss->getDock(carrierId);
        if (!carrier)
            carrier = ss->getDock(carrierCode);
        if (!carrier)
            carrier = ss->moveMegashipHere(carrierId, cmdr.squadronCarrierCode, cmdr.squadronCarrierName, body_id_t{}, true);
        if (!carrier)
            carrier = ss->addEntity(TypeNav::SquadronCarrier, cmdr.squadronCarrierCode, cmdr.squadronCarrierName,
                                    {.market_id=cmdr.squadronCarrierId, .ts=ge->timestamp});
        else
            carrier = ss->checkTypeAndName(carrier, TypeNav::SquadronCarrier, cmdr.squadronCarrierCode, cmdr.squadronCarrierName,
                                           {.market_id=cmdr.squadronCarrierId, .ts=ge->timestamp});
        if (carrier && carrier->marketId() != carrierId) {
            carrier->setMarketId(carrierId);
            ss->needBlobSave = true;
        }
    }
    else {
        LOG_ERROR("CarrierStats of unknown type {}", stype);
        return;
    }
    ss->save();
}

void parseEvent_CarrierStats(spGameEvent& ge) {
    auto& je = ge->data;

    auto ss = gal::getCurrentStarSystem();
    if (!ss || !ss->systemAddress)
        return;

    auto& cmdr = const_cast<st::Commander&>(st::cmdr);
    auto stype = je["CarrierType"].as_string_or("FleetCarrier");
    int64_t carrierId = je["CarrierID"].as_int_or();
    auto carrierCode = je["Callsign"].as_string_or();
    auto carrierName = je["Name"].as_string_or();
    TypeNav type = toTypeNav(JsNavType::get(stype));
    if (type == TypeNav::FleetCarrier) {
        if (cmdr.fleetCarrierId && cmdr.fleetCarrierId != carrierId) {
            LOG_ERROR("CarrierStats event: carrier id {} != {}", cmdr.fleetCarrierId, carrierId);
            return;
        }
        if (!cmdr.fleetCarrierCode.empty() && cmdr.fleetCarrierCode != carrierCode) {
            LOG_ERROR("CarrierStats event: carrier callsign {} != {}", cmdr.fleetCarrierCode, carrierCode);
            return;
        }
        if (!cmdr.fleetCarrierId)
            cmdr.fleetCarrierId = carrierId;
        if (cmdr.fleetCarrierCode.empty())
            cmdr.fleetCarrierCode = carrierCode;
        auto nloc = *carrierName +" "+ *cmdr.fleetCarrierCode;
        if (nloc != cmdr.fleetCarrierName)
            cmdr.fleetCarrierName = nloc;
        auto carrier = ss->getDock(carrierId);
        if (!carrier)
            carrier = ss->getDock(carrierCode);
        if (!carrier)
            carrier = ss->moveMegashipHere(carrierId, cmdr.fleetCarrierCode, cmdr.fleetCarrierName, body_id_t{}, true);
        if (!carrier)
            carrier = ss->addEntity(TypeNav::FleetCarrier, cmdr.fleetCarrierCode, cmdr.fleetCarrierName,
                                    {.market_id=cmdr.fleetCarrierId, .ts=ge->timestamp});
        else
            carrier = ss->checkTypeAndName(carrier, TypeNav::FleetCarrier, cmdr.fleetCarrierCode, cmdr.fleetCarrierName,
                                           {.market_id=cmdr.fleetCarrierId, .ts=ge->timestamp});
        if (carrier && carrier->marketId() != carrierId) {
            carrier->setMarketId(carrierId);
            ss->needBlobSave = true;
        }
    }
    else if (type == TypeNav::SquadronCarrier) {
        if (cmdr.squadronCarrierId && cmdr.squadronCarrierId != carrierId) {
            LOG_ERROR("CarrierStats event: carrier id {} != {}", cmdr.squadronCarrierId, carrierId);
            return;
        }
        if (!cmdr.squadronCarrierCode.empty() && cmdr.squadronCarrierCode != carrierCode) {
            LOG_ERROR("CarrierStats event: carrier callsign {} != {}", cmdr.squadronCarrierCode, carrierCode);
            return;
        }
        if (!cmdr.squadronCarrierId)
            cmdr.squadronCarrierId = carrierId;
        if (cmdr.squadronCarrierCode.empty())
            cmdr.squadronCarrierCode = carrierCode;
        auto nloc = *carrierName +" | "+ *cmdr.squadronCarrierCode;
        if (nloc != cmdr.squadronCarrierName)
            cmdr.squadronCarrierName = nloc;
        auto carrier = ss->getDock(carrierId);
        if (!carrier)
            carrier = ss->getDock(carrierCode);
        if (!carrier)
            carrier = ss->moveMegashipHere(carrierId, cmdr.squadronCarrierCode, cmdr.squadronCarrierName, body_id_t{}, true);
        if (!carrier)
            carrier = ss->addEntity(TypeNav::SquadronCarrier, cmdr.squadronCarrierCode, cmdr.squadronCarrierName,
                                    {.market_id=cmdr.squadronCarrierId, .ts=ge->timestamp});
        else
            carrier = ss->checkTypeAndName(carrier, TypeNav::SquadronCarrier, cmdr.squadronCarrierCode, cmdr.squadronCarrierName,
                                           {.market_id=cmdr.squadronCarrierId, .ts=ge->timestamp});
        if (carrier && carrier->marketId() != carrierId) {
            carrier->setMarketId(carrierId);
            ss->needBlobSave = true;
        }
    }
    else {
        LOG_ERROR("CarrierStats of unknown type {}", stype);
        return;
    }
    ss->save();
}

void parseEvent_CarrierJump(spGameEvent& ge) {
    auto& je = ge->data;

    st::shipAtBody.approachBody = false;
    st::shipAtBody.nearBody = false;

    auto starSystem = setEddnStarSystem(ge);

    auto& cmdr = const_cast<st::Commander&>(st::cmdr);
    if (!cmdr.fleetCarrierId && je["Docked"].as_bool_or()) {
        cmdr.fleetCarrierId = je["MarketID"].as_int();
        cmdr.fleetCarrierCode = je["StationName"].as_string();
    }
    cmdr.fleetCarrierInSystem = st::eddnStarSystem.name;
    cmdr.fleetCarrierAtBody = je["Body"].as_string_or();
    cmdr.fleetCarrierAtBodyId = je["BodyID"].as_int_or(-1);

    if (cmdr.fleetCarrierId) {
        auto carrier = starSystem->moveMegashipHere(cmdr.fleetCarrierId, cmdr.fleetCarrierCode, "", st::space.bodyId, false);
        if (carrier) {
            cmdr.fleetCarrierName = carrier->getLocalisedName();
        }
    }

    LOG_INFO("Fleet Carrier Jump: now {}[{}]({}) in system '{}' at '{}' ({})", cmdr.fleetCarrierCode, cmdr.fleetCarrierId, cmdr.fleetCarrierName,
             cmdr.fleetCarrierInSystem, cmdr.fleetCarrierAtBody, cmdr.fleetCarrierAtBodyId.val);

    if (je["Docked"].as_bool_or()) {
        st::space.bodyId = je["BodyID"].as_int_or(-1);
        set(st::space.bodyName, je["Body"]);
        set(st::space.bodyType, je["BodyType"]);
        gal::updateBodyFromSpace(starSystem.get(), ge);
    }

    if (!ge->expired) {
        EDDN::event_CarrierJump(ge);
        gal::updateFromGameEvent(starSystem.get(), ge);
    }
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
    if (auto ss = gal::getCurrentStarSystem(); ss && ss->systemAddress) {
        gal::updateFromDockEvent(ss.get(), ge);
        auto dock = ss->getDock(st::space.marketId);
        if (dock && !isGeneric(dock->type))
            st::space.stationType = toJsNavType(dock->type).sv();
    }
    ai::resetCompassDetects();
}

void parseEvent_ApproachSettlement(spGameEvent& ge) {
    if (!st::eddnStarSystem.addr)
        return;
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
        st::space.bodyId = je["BodyID"].as_int_or(-1);
        set(st::space.bodyName, je["BodyName"]);
        set(st::space.bodyType, "Planet");
        gal::updateBodyFromSpace(gal::getCurrentStarSystem().get(), ge);
    }

    gal::updateFromDockEvent(gal::getCurrentStarSystem().get(), ge);
}

void parseEvent_SupercruiseExit(spGameEvent& ge) {
    auto& je = ge->data;

    auto starSystem = gal::getCurrentStarSystem();

    st::dockedAt = {};
    st::space.bodyId = je["BodyID"].as_int_or(-1);
    set(st::space.bodyName, je["Body"]);
    set(st::space.bodyType, je["BodyType"]);
    gal::updateBodyFromSpace(starSystem.get(), ge);
    if (st::space.bodyType == "Station") {
        auto dock = starSystem->getDock(st::space.bodyName);
        if (dock && !dock->bodyId.has_value()) {
            dock->bodyId = st::space.bodyId;
            dock->updated = ge->timestamp;
            starSystem->needBlobSave = true;
            starSystem->save();
        }
    }
    ai::resetCompassDetects();
}

void parseEvent_FSSSignalDiscovered(spGameEvent& ge) {
    if (!ge || ge->event != "FSSSignalDiscovered") {
        // stop FSSSignalDiscovered chain
        gal::spStarSystem ss = gal::getCurrentStarSystem();
        if (ss && fssSignalSystemAddress && ss->systemAddress == fssSignalSystemAddress && !allFSSSignalEvents.empty()) {
            gal::updateFromFSSSignalDiscovered(ss.get(), allFSSSignalEvents);
            EDDN::event_FSSSignalDiscovered(allFSSSignalEvents);
        }
        allFSSSignalEvents.clear();
        fssSignalSystemAddress = 0;
    }
    else if (!ge->expired) {
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

static void saveBodyCount(spGameEvent& ge, const char* prop) {
    if (!ge->data[prop].is_int())
        return;
    int body_count = ge->data[prop].as_int();
    auto ss = gal::getCurrentStarSystem();
    if (ss->ext.bodyCount != body_count) {
        ss->ext.bodyCount = body_count;
        ss->needBlobSave = true;
        ss->save();
    }
}

void parseEvent_NavBeaconScan(spGameEvent& ge) {
    saveBodyCount(ge, "NumBodies");
    if (!ge->expired) {
        EDDN::event_NavBeaconScan(ge);
        gal::updateFromGameEvent(gal::getCurrentStarSystem().get(), ge);
        Cfg.scanEvents.push(ge);
    }
}

void parseEvent_SAASignalsFound(spGameEvent& ge) {
    if (!ge->expired) {
        EDDN::event_SAASignalsFound(ge);
        Cfg.scanEvents.push(ge);
    }
}

void parseEvent_FSSDiscoveryScan(spGameEvent& ge) {
    saveBodyCount(ge, "BodyCount");
    if (!ge->expired) {
        EDDN::event_FSSDiscoveryScan(ge);
        gal::updateFromGameEvent(gal::getCurrentStarSystem().get(), ge);
        Cfg.scanEvents.push(ge);
    }
}

void parseEvent_FSSAllBodiesFound(spGameEvent& ge) {
    saveBodyCount(ge, "Count");
    if (!ge->expired) {
        EDDN::event_FSSAllBodiesFound(ge);
        Cfg.scanEvents.push(ge);
    }
}

void parseEvent_FSSBodySignals(spGameEvent& ge) {
    if (!ge->expired) {
        EDDN::event_FSSBodySignals(ge);
        Cfg.scanEvents.push(ge);
    }
}

void parseEvent_Scan(spGameEvent& ge) {
    auto& je = ge->data;

    const auto systemName = je["StarSystem"].as_string_or();
    int64_t systemAddress = je["SystemAddress"].as_int_or();
    Coords coords = (systemName == st::eddnStarSystem.name && systemAddress == st::eddnStarSystem.addr)
                    ? st::eddnStarSystem.coords : Coords{};
    gal::spStarSystem ss;
    if (!systemName.empty()) {
        ss = gal::makeStarSystem(systemName, systemAddress, coords, true, true);
    } else {
        // old game
        ss = gal::getCurrentStarSystem();
    }

    if (!ge->expired) {
        EDDN::event_Scan(ge);
        gal::updateFromScanEvent(ss.get(), ge);
        Cfg.scanEvents.push(ge);
    }
}

void parseEvent_ScanBaryCentre(spGameEvent& ge) {
    auto& je = ge->data;

    const auto systemName = je["StarSystem"].as_string_or();
    int64_t systemAddress = je["SystemAddress"].as_int_or();
    Coords coords = (systemName == st::eddnStarSystem.name && systemAddress == st::eddnStarSystem.addr)
                    ? st::eddnStarSystem.coords : Coords{};
    gal::spStarSystem ss;
    if (!systemName.empty()) {
        ss = gal::makeStarSystem(systemName, systemAddress, coords, true, true);
    } else {
        // old game
        ss = gal::getCurrentStarSystem();
    }

    if (!ge->expired) {
        EDDN::event_ScanBaryCentre(ge);
        gal::updateFromScanEvent(ss.get(), ge);
        Cfg.scanEvents.push(ge);
    }
}

void parseEvent_ApproachBody(spGameEvent& ge) {
    auto& je = ge->data;

    st::shipAtBody.approachBody = true;
    st::shipAtBody.bodyId = je["BodyID"].as_int_or(-1);
    set(st::shipAtBody.bodyName, je["Body"]);
}

void parseEvent_LeaveBody(spGameEvent& ge) {
    auto& je = ge->data;

    st::shipAtBody.approachBody = false;
    st::shipAtBody.bodyId = je["BodyID"].as_int_or(-1);
    set(st::shipAtBody.bodyName, je["Body"]);
}

void parseEvent_ColonisationConstructionDepot(spGameEvent& ge) {
    if (!ge->expired) {
        gal::updateFromMarketEvent(gal::getCurrentStarSystem().get(), ge);
    }
}

void parseEvent_ColonisationContribution(spGameEvent& ge) {
    if (!ge->expired) {
        RavenColonial::reportContribution(ge);
        CM.processColonisationContribution(ge);
        Cfg.marketEvents.push(ge);
    }
}
void parseEvent_MarketBuy(spGameEvent& ge) {
    if (!ge->expired) {
        CM.processMarketBuy(ge);
        Cfg.marketEvents.push(ge);
    }
}
void parseEvent_MarketSell(spGameEvent& ge) {
    if (!ge->expired) {
        CM.processMarketSell(ge);
        Cfg.marketEvents.push(ge);
    }
}

void parseEvent_CargoTransfer(spGameEvent& ge) {
    if (!ge->expired) {
        CM.processCargoTransfer(ge);
        Cfg.marketEvents.push(ge);
    }
}


void GameEventQueue::clear() {
    std::scoped_lock<std::mutex> lock(mutex);
    events.clear();
}

void GameEventQueue::clear(std::chrono::milliseconds expired) {
    std::scoped_lock<std::mutex> lock(mutex);
    Timestamp old = Timestamp::clock::now() - expired;
    while (!events.empty()) {
        if (events.front()->timestamp > old)
            break;
        events.pop_front();

    }
}

void GameEventQueue::push(spGameEvent& ge) {
    std::scoped_lock<std::mutex> lock(mutex);
    Timestamp old = Timestamp::clock::now() - std::chrono::seconds(seconds_to_keep);
    while (!events.empty()) {
        if (events.front()->timestamp > old)
            break;
        events.pop_front();
    }
    events.push_back(ge);
    condvar.notify_all();
}

spGameEvent GameEventQueue::pop(std::chrono::milliseconds timeout) {
    std::unique_lock<std::mutex> lock(mutex);
    if (events.empty()) {
        if (timeout.count() <= 0)
            return {};
        if (timeout > 1s && ai::is_ai_thread()) {
            auto total_until = Timestamp::clock::now() + timeout;
            while (events.empty() && Timestamp::clock::now() < total_until) {
                auto until = Timestamp::clock::now() + 250ms;
                if (until > total_until)
                    until = total_until;
                condvar.wait_until(lock, until, [this]() -> bool { return !events.empty(); });
                ai::check_interrupted();
            }
        } else {
            condvar.wait_for(lock, timeout, [this]() -> bool { return !events.empty(); });
        }
        if (events.empty())
            return {};
    }
    auto ge = events.front();
    events.pop_front();
    return ge;
}

std::deque<spGameEvent>::iterator GameEventQueue::find_event_locked(const std::initializer_list<std::string_view>& lst) {
    std::deque<spGameEvent>::iterator it;
    for (it = events.begin(); it != events.end(); ++it) {
        if (std::find(lst.begin(), lst.end(), (*it)->event) != lst.end())
            return it;
    }
    return events.end();
}

spGameEvent GameEventQueue::wait_event(std::chrono::milliseconds timeout, bool pop, std::initializer_list<std::string_view> ids) {
    std::unique_lock<std::mutex> lock(mutex);
    Timestamp old = Timestamp::clock::now() - std::chrono::seconds(seconds_to_keep);
    while (!events.empty()) {
        if (events.front()->timestamp > old)
            break;
        events.pop_front();
    }
    auto it = find_event_locked(ids);
    if (it != events.end())
        return (*it);
    auto total_until = Timestamp::clock::now() + timeout;
    while (Timestamp::clock::now() < total_until) {
        auto until = Timestamp::clock::now() + 250ms;
        if (until > total_until)
            until = total_until;
        condvar.wait_until(lock, until);
        if (ai::is_ai_thread())
            ai::check_interrupted();
        it = find_event_locked(ids);
        if (it != events.end())
            break;
    }
    spGameEvent ge;
    if (it == events.end())
        return ge;
    ge = *it;
    if (pop) {
        events.erase(events.begin(), it);
        assert (it == events.begin());
        events.pop_front();
    }
    return ge;
}
