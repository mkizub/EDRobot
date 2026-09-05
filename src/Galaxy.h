//
// Created by mkizub on 22.08.2025.
//

#pragma once

#ifndef EDROBOT_GALAXY_H
#define EDROBOT_GALAXY_H

namespace gal {

class Entity {
public:
    virtual ~Entity() = default;
    bool nameEq(std::string_view nm) const;
    bool setName(std::string_view nm);
    TypeNav type {TypeNav::Other};
    Timestamp updated {};
    short bodyId {-1};
    short parentBodyId {-1}; // on orbit of
    int64_t marketId {0};
    double radius {0};
    dist_t main_star_distance; // approximate distance to arrival from main star
    dist_t parent_distance; // approximate distance to parent body center
    std::string name;
    std::string nloc; // localized name
    std::string code; // star class or planet type, fleet carrier code, etc
    bool special {false}; // main star, dockable for stations, landable for planets
};

typedef std::shared_ptr<Entity> spEntity;

struct StarSystem {
    StarSystem(int64_t address, std::string_view name)
        : systemAddress(address)
        , systemName(name)
    {}
    StarSystem(int64_t address, std::string_view name, double x, double y, double z, int64_t blobId, bool savedDbBase)
            : systemAddress(address)
            , systemName(name)
            , starPos(x,y,z)
            , dbBlobId(blobId)
            , savedDbBase(savedDbBase)
    {}
    virtual ~StarSystem() = default;
    const int64_t systemAddress;
    const std::string systemName;
    cv::Point3d starPos;
    int64_t dbBlobId {};
    Timestamp eddn_updated_at {};
    int game_body_count {};
    bool loaded {};

    std::vector<spEntity> bodies;
    std::vector<spEntity> stations;
    std::vector<spEntity> signals;
    bool saved {false};
    bool savedDbBase {false};
    void save();

    spEntity getMainStar();
    spEntity getEntity(const std::string& bname);
    spEntity getBodyById(int bodyId);
    spEntity getBody(std::string_view bname);
    spEntity getDock(std::string_view sname);
    spEntity getDock(int64_t marketId);
    void addFSSSignalDiscovered(const std::vector<std::shared_ptr<GameEvent>>& events);
    spEntity addNavListEntry(wchar_t charOCR, const std::string& nav_icon, const std::string& name, int bodyId);
    spEntity addStation(spGameEvent& ge);
    spEntity addStation(spEntity station);
    spEntity addSignal(spEntity signal);
    void addDestination();
    void removeEntity(const spEntity& entity);

private:
    void checkType(spEntity& site, TypeNav type, Timestamp timestamp);
    void checkName(spEntity& site, std::string_view name, Timestamp timestamp);
    void checkNloc(spEntity& site, std::string_view nloc, Timestamp timestamp);
};

typedef std::shared_ptr<StarSystem> spStarSystem;

spStarSystem getStarSystem(std::string_view name, bool network_load=true);
spStarSystem makeStarSystem(std::string_view name, int64_t address, cv::Point3d* starPos, bool network_load=true);
spStarSystem& getCurrentStarSystem();
void setCurrentStarSystem(spStarSystem ss);

spMarket getMarket(int64_t marketId);
void saveMarket(Market* market);
void setMarketData(spMarket market);

struct NavType {
    // some nav types share the same charOCR
    const wchar_t charOCR;
    const TypeNav type;
    const std::vector<std::string> navIcons; // some nav types have common icons
    const std::vector<std::string> typeAliases;
    const bool name_pattern;
    const std::vector<std::pair<Lang,std::string>> name_loc;

    static NavType* findNavType(TypeNav type);
    static bool expandName(std::string_view nm, std::string& name, std::string& nloc);
    std::string get_nloc() const;
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
extern NavType STRONGHOLD_CARRIER;
extern NavType STATION_MEGASHIP;
//extern NavType TRAILBLAZER_DREAM;
extern NavType COLONIZATION_SHIP;
extern NavType MEGASHIP;
extern NavType ENGINEER;
extern NavType UNEXPLORED;
extern NavType SIGNAL;
extern NavType WAR_ZONE;
extern NavType RES_SITE;
extern NavType STAR_SYSTEM;

const wchar_t ERROR_MARK    = u'\u2047'; // ⁇
const wchar_t LOCATION_MARK = u'\u2207'; // ∇
const wchar_t SHIELD1_MARK  = u'\u25C7'; // ◇
const wchar_t SHIELD2_MARK  = u'\u2B16'; // ⬖
const wchar_t SHIELD3_MARK  = u'\u25C6'; // ◆

extern const std::vector<NavType*> ALL_NAV_TYPES;

} // namespace gal

#endif //EDROBOT_GALAXY_H
