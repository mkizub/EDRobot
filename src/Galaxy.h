//
// Created by mkizub on 22.08.2025.
//

#pragma once

#ifndef EDROBOT_GALAXY_H
#define EDROBOT_GALAXY_H

namespace gal {

class Item : public std::enable_shared_from_this<Item> {
public:
    virtual ~Item() = default;
    virtual bool isBody() { return false; }
    virtual bool isSite() { return false; }
    virtual bool nameEq(const std::string& nm);
    virtual bool nameEq(const std::wstring& nm);
    TypeNav typeNav : 16 {TypeNav::Other};
    short parentBodyId {-1}; // on orbit of
    std::string name;
    std::optional<short> bodyId;
};

class Body : public Item {
public:
    bool isBody() override { return true; }
    dist_t distance; // approximate distance to arrival
    double radius {0}; // KM
};

class Planet : public Body {
public:
    bool isLandable {false};
};

class Star : public Body {
public:
    bool isMainStar {false};
    bool isScoopable {false};
    std::string spectralClass;
};

class Site : public Item {
public:
    bool isSite() override { return true; }
    bool nameEq(const std::string& nm) override;
    bool nameEq(const std::wstring& nm) override;
    TypeSite typeSite : 16 {TypeSite::Other};
    int64_t marketId {0};
    std::string nloc;
};

typedef std::shared_ptr<Item> spItem;
typedef std::shared_ptr<Body> spBody;
typedef std::shared_ptr<Site> spSite;

struct StarSystem : public std::enable_shared_from_this<StarSystem> {
    StarSystem() = default;
    virtual ~StarSystem() = default;
    int64_t address {0};
    std::string name;
    cv::Point3d pos;

    std::vector<spBody> bodies;
    std::vector<spSite> stations;
    bool saved {false};

    spItem getBodyById(int bodyId);
    spBody getBody(const std::string& bname);
    spSite getDock(const std::string& sname);
    spSite getDock(int64_t marketId);
    spItem addFSSSignalDiscovered(std::shared_ptr<GameEvent> event);
    spSite addStation(int64_t marketId, const std::string& sname, const std::string& stype);
    void addDestination();

};

typedef std::shared_ptr<StarSystem> spStarSystem;

spStarSystem getStarSystem(const std::string& name);
spStarSystem getStarSystem(const std::string& name, int64_t address);
spStarSystem& getCurrentStarSystem();
void setCurrentStarSystem(spStarSystem ss);
void saveStarSystem(StarSystem* ss);

} // namespace gal

#endif //EDROBOT_GALAXY_H
