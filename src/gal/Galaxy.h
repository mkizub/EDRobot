//
// Created by mkizub on 22.08.2025.
//

#pragma once

#ifndef EDROBOT_GALAXY_H
#define EDROBOT_GALAXY_H

namespace db {
struct StarSystemJS;
struct StationJS;
struct MegashipJS;
struct MarketJS;
struct StarSystem;
}

#include "StarSystem.h"


namespace gal {

bool init_symbols();
bool spansh_systems_dump_test(std::string_view filePath, bool saveDbBlob);
bool game_logs_test(std::string_view dirPath, std::string_view firstFile);

class StarSystemCache;

spStarSystem getStarSystem(std::string_view name, bool blob_load, bool network_load);
spStarSystem getStarSystem(int64_t address, bool blob_load, bool network_load);
spStarSystem makeStarSystem(std::string_view name, int64_t address, Coords coords, bool blob_load, bool network_load);
spStarSystem& getCurrentStarSystem();
void setCurrentStarSystem(spStarSystem ss);
void resetCurrentStarSystem();
void cleanupStarSystemCache(std::chrono::seconds expired);

spMarket getMarket(int64_t marketId);
void saveMarket(Market* market);
void setMarketData(spMarket market);

bool update_star_system(StarSystem* ss, db::StarSystemJS& db_ss);
bool update_megaship(StarSystem* ss, db::MegashipJS& db_ms, spEntity& megaship);
bool fill_star_system_for_db(const StarSystem* ss, db::StarSystemJS& db_ss, std::vector<db::MegashipJS>& db_megaships);
bool fill_megaship_for_db(const StarSystem* ss, const Entity* ms, db::MegashipJS& db_megaships);

void updateFromFSSSignalDiscovered(StarSystem* ss, const std::vector<spGameEvent>& events);
void updateBodyFromSpace(StarSystem* ss, spGameEvent& ge);
void updateFromDestination(StarSystem* ss, spGameEvent& ge);
bool updateFromGameEvent(StarSystem* ss, spGameEvent& ge);
bool updateFromScanEvent(StarSystem* ss, spGameEvent& ge);
bool updateFromDockEvent(StarSystem* ss, spGameEvent& ge);
bool updateFromMarketEvent(StarSystem* ss, spGameEvent& ge);

struct GameVar {
    TypeNav type;
    bool prefix;
    std::string var;
    std::string xx;
    std::string en;
    std::string ru;
    std::size_t hash;
    GameVar(TypeNav tp, bool prf, std::string_view var_, std::string_view xx_, std::string_view en_, std::string_view ru_)
            : type(tp), prefix(prf), var(var_), xx(xx_), en(en_), ru(ru_)
    {
        hash = std::hash<std::string_view>{}(var_);
    }

    struct var_hash {
        using is_transparent = void;
        size_t operator()(const std::string_view& sv) const noexcept {
            return std::hash<std::string_view>{}(sv);
        }
        size_t operator()(const GameVar& gv) const noexcept {
            return gv.hash;
        }
    };

    constexpr bool operator==(std::string_view sv) const noexcept {
        return var == sv;
    }
    constexpr bool operator==(const gal::GameVar& rhs) const noexcept {
        return hash == rhs.hash && var == rhs.var;
    }

    static std::unordered_set<GameVar,var_hash,std::equal_to<void>> allVars;

    static const GameVar* find(std::string_view sv);
    static TypeNav replace_in(std::string_view sv, std::string& eng, std::string& loc);

    [[nodiscard]] std::string_view get_eng() const;
    [[nodiscard]] std::string_view get_loc() const;
    bool match(std::string_view sname) const;
    void localizeName(std::string_view nm, std::string& name, std::string& nloc) const;
};

struct NavType {
    // some nav types share the same charOCR
    const wchar_t charOCR;
    const TypeNav type;
    const std::vector<std::string> navIcons; // some nav types have common icons
    const bool name_pattern;
    const std::vector<GameVar> name_loc;

    static const NavType* findNavType(TypeNav type);
    static TypeNav localizeName(TypeNav type, std::string_view nm, std::string& name, std::string& nloc);
    static TypeNav checkCarrierName(std::string_view nm, std::string* name, std::string* nloc);
    static TypeNav guessTypeFromName(TypeNav tp, std::string_view nm);
    bool match_name(std::string_view name) const;
    bool match_type(std::string_view type) const;
    bool match_icon(wchar_t ch, const std::string& icon) const;
};



extern NavType STAR;
extern NavType BEACON;
extern NavType TOURIST_BEACON;
extern NavType BODY;
extern NavType LAND;
extern NavType BELT;
extern NavType ORBIS;
extern NavType OCELLUS;
extern NavType DODEC;
extern NavType CORIOLIS;
extern NavType MINER_BASE;
extern NavType SPACE_OUTPOST;
extern NavType SPACE_INSTALLATION;
extern NavType SPACE_CONSTR_DEPOT;
extern NavType PLANETARY_PORT;
extern NavType PLANETARY_INSTALLATION;
extern NavType PLANETARY_CONSTR_DEPOT;
extern NavType ODYSSEY_SETTLEMENT;
extern NavType FLEET_CARRIER;
extern NavType SQUADRON_CARRIER;
//extern NavType STRONGHOLD_CARRIER;
extern NavType STATION_MEGASHIP;
extern NavType COLONIZATION_SHIP;
extern NavType ANON_MEGASHIP;
extern NavType MEGASHIP;
extern NavType ENGINEER;
extern NavType UNEXPLORED;
extern NavType SIGNAL;
extern NavType WAR_ZONE;
extern NavType RES_SITE;
extern NavType CODEX;
extern NavType STAR_SYSTEM;

const wchar_t ERROR_MARK    = u'\u2047'; // ⁇
const wchar_t LOCATION_MARK = u'\u2207'; // ∇
const wchar_t SHIELD1_MARK  = u'\u25C7'; // ◇
const wchar_t SHIELD2_MARK  = u'\u2B16'; // ⬖
const wchar_t SHIELD3_MARK  = u'\u25C6'; // ◆

extern const std::vector<const NavType*> ALL_NAV_TYPES;

} // namespace gal

#endif //EDROBOT_GALAXY_H
