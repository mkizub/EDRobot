//
// Created by mkizub on 05.10.2025.
//

#pragma once

#ifndef EDROBOT_STATE_H
#define EDROBOT_STATE_H

namespace st {

extern Lang lng;
extern std::string currentStarSystem;

extern GuiFocus guiFocus;


extern struct Commander {
    std::string name;
    std::string fid;
} const cmdr;

extern struct GameClient {
    bool isOdyssey;
    bool isHorizons;
    std::string language;
    std::string gameversion;
    std::string build;
} const client;

extern struct ShipInfo {
    std::string shipType;
    std::string shipTypeLocalized;
    std::string shipUserName;
    std::string shipIdent;
    int shipId;
} const shipInfo;

extern struct ShipStats {
    float unladenMass;
    float totalMass;

    float fuelCapacityMain;
    float fuelCapacityReservoir;
    float cargoCapacity;

    float fuelMain;
    float fuelReservoir;
    float cargo;
} shipStats;

extern struct DockedAt {
    int64_t marketId;
    std::string stationName;
    std::string stationType;
} dockedAt;

extern struct Space {
    // if at station cruise exit
    int64_t marketId;
    std::string stationName;
    std::string stationType;
    // nearest body
    int bodyId;
    std::string bodyName;
    std::string bodyType;
} space;

extern union NavPanelFilters {
    NavPanelFilters() : mask(0) {}
    struct {
        bool pointOfInterest: 1;
        bool star: 1;
        bool settlement: 1;
        bool station: 1;
        bool signalSource: 1;
        bool asteroidCluster: 1;
        bool landablePlanetOrMoon: 1;
        bool system: 1;
        bool fleetCarrier: 1;
        bool planetOrMoon: 1;
    };
    int mask;
    bool operator==(const NavPanelFilters& other) const { return this->mask == other.mask; }
    bool operator!=(const NavPanelFilters& other) const { return this->mask != other.mask; }
} navFilters;

extern struct Destination {
    int64 system;
    int bodyId;
    std::string name;
} destination;

extern struct ShipStatus {
    enum class LegalState {
        Clean, IllegalCargo, Speeding, Wanted, Hostile, PassengerWanted, Warrant, Allied, Thargoid
    };

    Timestamp timestamp;
    union {
        struct {
            bool docked : 1;
            bool landed : 1;
            bool landing_gear_down : 1;
            bool shields_up : 1;
            bool cruise : 1;
            bool fa_off : 1;
            bool weapon_on : 1;
            bool in_wing : 1;
            bool lights_on : 1;
            bool cargo_scoop_on : 1;
            bool silent_run : 1;
            bool fuel_scooping : 1;
            bool srv_handbrake : 1;
            bool srv_turret_view : 1;
            bool srv_turret_retructed : 1;
            bool srv_drive_assist : 1;
            bool fsd_masslocked : 1;
            bool fsd_charging : 1;
            bool fsd_cooldown : 1;
            bool fuel_low : 1;
            bool overheating : 1;
            bool has_lat_lon : 1;
            bool in_danger : 1;
            bool in_interdiction : 1;
            bool in_ship : 1;
            bool in_fighter : 1;
            bool in_srv : 1;
            bool hud_in_analysis : 1;
            bool night_vision : 1;
            bool alt_from_avr_radius : 1;
            bool fsd_jump : 1;
            bool srv_high_beam : 1;
        };
        uint32_t all;
    } flags;
    union {
        struct {
            bool on_foot : 1;
            bool in_taxy : 1;
            bool in_multicrew : 1;
            bool on_foot_in_station : 1;
            bool on_foot_on_planet : 1;
            bool aim_down_sight : 1;
            bool low_oxygen : 1;
            bool low_health : 1;
            bool cold : 1;
            bool hot : 1;
            bool very_cold : 1;
            bool very_hot : 1;
            bool glide_mode : 1;
            bool on_foot_in_hangar : 1;
            bool on_foot_social_space : 1;
            bool on_foot_exterior : 1;
            bool breathable_atmosphere : 1;
            bool telepresence_multicrew : 1;
            bool physical_multicrew : 1;
            bool fsd_hyperdrive_charging : 1;
        };
        uint32_t all;
    } flags2;
    uint8_t pips[3];
    uint8_t fireGroup;
    uint64_t balance;
    LegalState legalState;
} ship;

extern struct ShipAtBody {
    // "ApproachBody"/"LeaveBody" events
    bool approachBody;
    int bodyId;
    std::string bodyName; // both event and Status.json
    // if on or near a planet
    bool nearBody;
    double latitude;
    double altitude;
    double longitude;
    double heading;
    double planetRadius;
} shipAtBody;

}

extern std::ostream& operator<<(std::ostream& os, const st::ShipStatus& obj);

#endif //EDROBOT_STATE_H
