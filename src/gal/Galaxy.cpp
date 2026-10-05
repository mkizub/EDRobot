//
// Created by mkizub on 22.08.2025.
//

#ifndef PCH_H
#include "../pch.h"
#endif

#include <unordered_set>
#include <boost/pool/pool_alloc.hpp>
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/unordered/unordered_flat_map.hpp>

#include "Galaxy.h"
#include "../db/DB.h"
#include "../net/Spansh.h"

namespace gal {

struct LRUStarSystemKey {
    int64_t  systemAddress;
    std::string_view systemName;
    std::shared_ptr<StarSystem> ptr;
    mutable Timestamp lru;
};

struct by_systemAddress {};
struct by_systemName {};

using StarSystemMap = boost::multi_index_container<
        LRUStarSystemKey,
        boost::multi_index::indexed_by<
                // Index 1: Unique fast hash lookup by ID
                boost::multi_index::hashed_unique<
                        boost::multi_index::tag<by_systemAddress>,
                        boost::multi_index::member<LRUStarSystemKey, int64_t, &LRUStarSystemKey::systemAddress>
                >,
                // Index 2: Unique sorted lookup by Username
                boost::multi_index::hashed_unique<
                        boost::multi_index::tag<by_systemName>,
                        boost::multi_index::member<LRUStarSystemKey, std::string_view, &LRUStarSystemKey::systemName>
                >
        >,
        boost::fast_pool_allocator<LRUStarSystemKey>
>;

struct StarSystemCache {
    StarSystemMap systems;
    const spStarSystem dummyStarSystem;
    alignas(64) std::mutex mutex;

    StarSystemCache() : dummyStarSystem(new StarSystem(0, "Void", Coords{}))
    {
    }

    void cleanup(std::chrono::seconds expired);
    spStarSystem put(int64_t address, std::string_view name, Coords coords);
    spStarSystem get(int64_t address);
    spStarSystem get(std::string_view name);

} theSystemsCache;

void StarSystemCache::cleanup(std::chrono::seconds expired) {
    std::scoped_lock<std::mutex> lock(mutex);
    auto keep = Timestamp::clock::now() - expired;
    for (auto it = systems.begin(); it != systems.end(); /* no increment */) {
        auto& ss = it->ptr;
        bool used = ss.use_count() > 1;
        if (!used && it->lru < keep) {
            // we can safely iterate, because there are no referencies to star system,
            // and noone can load this star system, because we locked cache access
            int used_counter = ss->countEntities([](auto& e) {
                return e.use_count() > 1;
            });
            if (used_counter)
                used = true;
        }
        if (!used && it->lru < keep) {
            it->ptr->save();
            it = systems.erase(it);
        } else {
            ++it;
        }
    }
}

spStarSystem StarSystemCache::put(int64_t address, std::string_view name, Coords coords) {
    std::scoped_lock<std::mutex> lock(mutex);
    if (!address)
        return dummyStarSystem;
    auto& id_index = systems.get<by_systemAddress>();
    auto it = id_index.find(address);
    if (it != id_index.end()) {
        it->lru = Timestamp::clock::now();
        return it->ptr;
    }
    spStarSystem s(new StarSystem(address, name, coords));
    auto p = systems.insert({s->systemAddress, s->systemName, s, Timestamp::clock::now()});
    return s;
}

spStarSystem StarSystemCache::get(int64_t address) {
    std::scoped_lock<std::mutex> lock(mutex);
    if (!address)
        return dummyStarSystem;
    auto& id_index = systems.get<by_systemAddress>();
    auto it = id_index.find(address);
    if (it == id_index.end())
        return {};
    it->lru = Timestamp::clock::now();
    return it->ptr;
}

spStarSystem StarSystemCache::get(std::string_view name) {
    std::scoped_lock<std::mutex> lock(mutex);
    if (name.empty())
        return dummyStarSystem;
    auto& name_index = systems.get<by_systemName>();
    auto it = name_index.find(name);
    if (it == name_index.end())
        return {};
    it->lru = Timestamp::clock::now();
    return it->ptr;
}

struct LRUMarket {
    mutable Timestamp lru;
    spMarket market;
};

struct MarketsCache {
    boost::unordered_flat_map<
            int64_t,
            LRUMarket,
            boost::hash<int64_t>,
            std::equal_to<int64_t>,
            boost::fast_pool_allocator<std::pair<int64_t const, LRUMarket>>
            >
            markets;
    alignas(64) std::mutex mutex;

    void cleanup(std::chrono::seconds expired);
    spMarket put(spMarket market);
    spMarket get(int64_t marketId);

} theMarketsCache;

void MarketsCache::cleanup(std::chrono::seconds expired) {
    std::scoped_lock<std::mutex> lock(mutex);
    auto keep = Timestamp::clock::now() - expired;
    for (auto it = markets.begin(); it != markets.end(); /* no increment */) {
        if (it->second.market.use_count() == 1 && it->second.lru < keep) {
            it = markets.erase(it);
        } else {
            ++it;
        }
    }
}

spMarket MarketsCache::put(spMarket market) {
    std::scoped_lock<std::mutex> lock(mutex);
    if (market && market->marketId) {
        auto now = Timestamp::clock::now();
        markets[market->marketId] = LRUMarket{now, market};
    }
    return market;
}

spMarket MarketsCache::get(int64_t marketId) {
    if (!marketId)
        return {};
    std::scoped_lock<std::mutex> lock(mutex);
    auto it = markets.find(marketId);
    if (it == markets.end())
        return {};
    it->second.lru = Timestamp::clock::now();
    return it->second.market;
}


spStarSystem gCurrentStarSystem = theSystemsCache.dummyStarSystem;

void saveMarket(Market* market) {
    if (!market || !market->marketId || !market->systemAddress)
        return;

    db::MarketJS db_m {
        .marketId = market->marketId,
        .systemId = market->systemAddress,
        .updated = market->timestamp,
        .stationName = market->stationName,
        .stationType = market->stationType,
        .starSystem = market->starSystem
    };
    if (market->raven) {
        db_m.ravenBuildId = market->raven->buildId;
        db_m.ravenStatus = market->raven->status;
        db_m.ravenUpdated = market->raven->timestamp;
        for (auto& [cmdr,info] : market->raven->commanders) {
            js::vector<int64_t> info_assigned;
            for (auto* c : info.assigned)
                info_assigned.push_back(c->intId);
            db_m.ravenCommanders[cmdr] = db::RavenCmdrInfoJS {
                .timestamp = info.timestamp,
                .deliveries = info.deliveries,
                .contributed = info.contributed,
                .assigned = info_assigned,
            };
        }
    }

    db_m.commodities.reserve(market->items.size());
    for (auto& [c,ml] : market->items) {
        if (!c->intId)
            continue;
        db_m.commodities.emplace_back(db::MarketLineJS{
            .commodityId = c->intId,
            .demand = (unsigned)ml.demand,
            .supply = (unsigned)ml.stock,
            .buyPrice = (unsigned)ml.buyPrice,
            .sellPrice = (unsigned)ml.sellPrice,
            .isConsumer = ml.isConsumer,
            .isProducer = ml.isProducer,
        });
    }
    db::saveMarket(db_m);

//    js::value jm = js::object({
//        {"timestamp", formatTimestampString(market->timestamp)},
//        {"MarketID", market->marketId},
//        {"StationName", market->stationName},
//        {"StationType", market->stationType},
//        {"StarSystem", market->starSystem},
//        {"Items", js::array({})},
//        });
//    if (market->raven && (!market->raven->buildId.empty() || !market->raven->status.empty())) {
//        jm["RavenColonial"] = js::object({
//            {"buildId",market->raven->buildId},
//            {"status",market->raven->status},
//            {"timestamp",formatTimestampString(market->raven->timestamp, true)},
//        });
//        if (!market->raven->commanders.empty()) {
//            auto& jcommanders = jm["RavenColonial"]["commanders"].deref();
//            for (auto cmdr: market->raven->commanders) {
//                auto &name = cmdr.first;
//                auto &ci = cmdr.second;
//                auto& jcmdr = jcommanders[name].deref();
//                if (ci.timestamp.time_since_epoch().count())
//                    jcmdr["timestamp"] = formatTimestampString(ci.timestamp);
//                if (ci.deliveries)
//                    jcmdr["deliveries"] = ci.deliveries;
//                if (ci.contributed)
//                    jcmdr["contributed"] = ci.contributed;
//                js::value ja = js::array({});
//                for (Commodity* c : ci.assigned)
//                    ja.as_array().push_back(c->nameId);
//                jcmdr["assigned"] = ja;
//                jcmdr.set_no_indent();
//            }
//        }
//    }
//    auto& jarr = jm["Items"].as_array();
//    for (auto it : market->items) {
//        MarketLine& ml = it.second;
//        if (!ml.isConsumer && !ml.isProducer && !ml.stock && !ml.demand)
//            continue;
//        js::value& jv = jarr.emplace_back(js::object({{"Name", it.first->nameId}}));
//        if (ml.stock)
//            jv["Stock"] = ml.stock;
//        if (ml.demand)
//            jv["Demand"] = ml.demand;
//        if (ml.buyPrice)
//            jv["BuyPrice"] = ml.buyPrice;
//        if (ml.sellPrice)
//            jv["SellPrice"] = ml.sellPrice;
//        if (ml.isConsumer)
//            jv["Consumer"] = ml.isConsumer;
//        if (ml.isProducer)
//            jv["Producer"] = true;
//        jv.set_no_indent();
//    }
//
//    std::filesystem::path fp(std::format(L"cache/markets/{}.json", market->marketId));
//    std::ofstream ofs(fp);
//    ofs << js::rule::ecma404() << js::rule::space_indent<1>() << jm;
//    ofs.close();
}

spMarket loadMarket(int64_t marketId) {
    if (!marketId)
        return {};

    if (!marketId)
        return {};

    db::MarketJS db_m {marketId};
    if (!db::loadMarket(db_m))
        return {};
    spMarket market = std::make_shared<Market>(Market{
        .timestamp = db_m.updated,
        .marketId = marketId,
        .systemAddress = db_m.systemId,
        .stationName = db_m.stationName,
        .stationType = db_m.stationType,
        .starSystem = db_m.starSystem,
    });
    if (!db_m.ravenBuildId.empty()) {
        market->raven = std::make_shared<RavenProj>();
        market->raven->buildId = db_m.ravenBuildId;
        market->raven->status = db_m.ravenStatus;
        market->raven->timestamp = db_m.ravenUpdated;
        for (auto& [cmdr,info] : db_m.ravenCommanders) {
            std::vector<Commodity*> info_assigned;
            for (auto& c_id : info.assigned) {
                Commodity* c = Cfg.getCommodityById(c_id);
                if (c)
                    info_assigned.push_back(c);
            }
            market->raven->commanders[cmdr] = RavenProj::CmdrInfo {
                    .timestamp = info.timestamp,
                    .deliveries = info.deliveries,
                    .contributed = info.contributed,
                    .assigned = info_assigned,
            };
        }
    }
    market->items.reserve(db_m.commodities.size());
    for (auto& ln : db_m.commodities) {
        Commodity* commodity = Cfg.getCommodityById(ln.commodityId);
        if (!commodity)
            continue;
        MarketLine ml {
            .buyPrice = (int)ln.buyPrice,
            .sellPrice = (int)ln.sellPrice,
            .stock = (int)ln.supply,
            .demand = (int)ln.demand,
            .isConsumer = ln.isConsumer,
            .isProducer = ln.isProducer,
        };
        market->items[commodity] = ml;
    }
    return market;

//    const js::value jm = parseJsonFile(std::format(L"cache/markets/{}.json", marketId));
//
//    Timestamp timestamp;
//    if (!parseTimestamp(jm["timestamp"], timestamp))
//        return {};
//    if (marketId != jm["MarketID"].as_int_or())
//        return {};
//    spMarket market = std::make_shared<Market>(Market{
//        .timestamp = timestamp,
//        .marketId = marketId,
//        .systemAddress = ...,
//        .stationName = *jm["StationName"].as_string_or(),
//        .stationType = *jm["StationType"].as_string_or(),
//        .starSystem = *jm["StarSystem"].as_string_or(),
//    });
//    if (jm["RavenColonial"].is_object()) {
//        market->raven = std::make_shared<RavenProj>();
//        market->raven->buildId = jm["RavenColonial"]["buildId"].as_string_or();
//        market->raven->status = jm["RavenColonial"]["status"].as_string_or();
//        parseTimestamp(jm["RavenColonial"]["timestamp"], market->raven->timestamp);
//        if (auto commanders=jm["RavenColonial"]["commanders"]; commanders.is_object())
//        for (auto [name,jcmdr] : commanders.key_value()) {
//            RavenProj::CmdrInfo ci {};
//            if (!jcmdr["timestamp"].empty())
//                parseTimestamp(jcmdr["timestamp"], ci.timestamp);
//            ci.deliveries = jcmdr["deliveries"].as_int_or();
//            ci.contributed = jcmdr["contributed"].as_int_or();
//            if (jcmdr["assigned"].is_array()) {
//                for (auto& jc : jcmdr["assigned"].as_array()) {
//                    Commodity* c = Cfg.getCommodityById(jc.as_string_or());
//                    if (c)
//                        ci.assigned.push_back(c);
//                }
//            }
//            market->raven->commanders.emplace(name, ci);
//        }
//    }
//    auto& items = jm["Items"].as_array();
//    for (auto it : items) {
//        Commodity* commodity = Cfg.getCommodityById(it["Name"].as_string());
//        if (!commodity)
//            continue;
//        MarketLine ml {};
//        ml.buyPrice = it["BuyPrice"].as_int_or();
//        ml.sellPrice = it["SellPrice"].as_int_or();
//        ml.stock = it["Stock"].as_int_or();
//        ml.demand = it["Demand"].as_int_or();
//        ml.isConsumer = it["Consumer"].as_bool_or();
//        ml.isProducer = it["Producer"].as_bool_or();
//        market->items.emplace(commodity, ml);
//    }
//
//    return market;
}

//static void parseUpdated(spEntity& entity, const js::value& j) {
//    auto upd = j["updateTime"];
//    if (upd.is_int()) {
//        std::chrono::seconds sec_duration(upd.as_int());
//        std::chrono::sys_time<std::chrono::seconds> sys_time(sec_duration);
//        entity->updated = std::chrono::utc_clock::from_sys(sys_time);
//    }
//    else if (upd.is_string()) {
//        parseTimestampString(upd.as_string(), entity->updated);
//    }
//}
//static void parseBodyId(const spStarSystem& ss, spEntity& entity, const js::value& j) {
//    if (j.at("bodyId").is_int())
//        entity->bodyId = j.at("bodyId").as_int();
//    if (j["parentBodyId"].is_int())
//        entity->parentBodyId = j["parentBodyId"].as_int();
//    else if (j["parents"].is_array() && !j["parents"].as_array().empty()) {
//        auto& jp = j["parents"].as_array()[0];
//        if (jp.is_object())
//            entity->parentBodyId = jp.key_value().begin().value().as_int();
//    }
//    else if (j["body"].is_object() && j["body"]["name"].is_string()) {
//        const auto body = j["body"]["name"].as_string();
//        for (auto& b : ss->entities) {
//            if (isBody(b->type) && b->name == body && b->bodyId >= 0) {
//                entity->parentBodyId = b->bodyId;
//                break;
//            }
//        }
//    }
//}

spStarSystem getStarSystem(std::string_view name, bool blob_load, bool network_load) {
    spStarSystem ss = theSystemsCache.get(name);
    if (!ss) {
        db::StarSystem s = db::loadStarSystem(name);
        if (!s.name.empty()) {
            ss = theSystemsCache.put(s.id, s.name, s.coords);
            ss->needCoreSave = false;
            ss->isBlobLoaded = false;
            ss->needBlobSave = false;

            ss->updated_at = s.updated;
            ss->eddn_updated_at = s.eddn_updated;
        }
    }
    if (!ss && network_load) {
        ss = Spansh::loadStarSystem(name);
    }
    if (ss && blob_load && !ss->isBlobLoaded)
        ss->load();
    if (ss && !ss->isBlobLoaded && network_load)
        Spansh::loadStarSystem(ss);
    if (ss)
        ss->save();
    return ss;
}

spStarSystem getStarSystem(int64_t address, bool blob_load, bool network_load) {
    spStarSystem ss = theSystemsCache.get(address);
    if (!ss) {
        db::StarSystem s = db::loadStarSystem(address);
        if (!s.name.empty()) {
            ss = theSystemsCache.put(s.id, s.name, s.coords);
            ss->needCoreSave = false;
            ss->isBlobLoaded = false;
            ss->needBlobSave = false;

            ss->updated_at = s.updated;
            ss->eddn_updated_at = s.eddn_updated;
        }
    }
    if (!ss && network_load) {
        ss = Spansh::loadStarSystem(address);
    }
    if (ss && blob_load && !ss->isBlobLoaded)
        ss->load();
    if (ss && !ss->isBlobLoaded && network_load)
        Spansh::loadStarSystem(ss);
    if (ss)
        ss->save();
    return ss;
}


spStarSystem makeStarSystem(std::string_view name, int64_t address, Coords coords, bool blob_load, bool network_load) {
    assert (!name.empty() && address != 0);
    if (name.empty() || address == 0) {
        LOG_ERROR("Attempt to create star system: address={}, name='{}', coords=[{},{},{}]",
                  address, name, coords.x, coords.y, coords.z);
        return {};
    }
    spStarSystem ss = theSystemsCache.get(address);
    if (!ss) {
        db::StarSystem s = db::loadStarSystem(address);
        if (!s.name.empty()) {
            ss = theSystemsCache.put(address, name, s.coords);
            ss->needCoreSave = false; // just loaded from DB
            ss->isBlobLoaded = false;
            ss->needBlobSave = false;
            ss->updated_at = s.updated;
            ss->eddn_updated_at = s.eddn_updated;
        } else {
            ss = theSystemsCache.put(address, name, coords);
            ss->needCoreSave = !coords.empty();
        }
    }
    if (!coords.empty() && ss->systemCoords != coords) {
        const_cast<Coords&>(ss->systemCoords) = coords;
        ss->needCoreSave = true;
    }
    if (ss->needCoreSave) {
        db::StarSystem s {
                .id = ss->systemAddress,
                .name = ss->systemName,
                .coords = ss->systemCoords,
                .updated = ss->updated_at,
                .eddn_updated = ss->eddn_updated_at,
        };
        db::saveStarSystem(s);
        ss->needCoreSave = false;
    }
    if (blob_load || network_load)
        ss->load();
    if (network_load && !Cfg.isNetworkDisabled() &&
        (!ss->isBlobLoaded || (!ss->has_entities() && !ss->eddn_updated_at)))
        Spansh::loadStarSystem(ss);
    return ss;
}

spStarSystem& getCurrentStarSystem() {
    return gCurrentStarSystem;
}
void setCurrentStarSystem(spStarSystem ss) {
    assert(ss);
    gCurrentStarSystem.swap(ss);
}
void resetCurrentStarSystem() {
    gCurrentStarSystem = theSystemsCache.dummyStarSystem;
}


void cleanupStarSystemCache(std::chrono::seconds expired) {
    theSystemsCache.cleanup(expired);
}

spMarket getMarket(int64_t marketId) {
    if (!marketId)
        return {};
    auto market = theMarketsCache.get(marketId);
    if (market && !market->items.empty())
        return market;
    market = loadMarket(marketId);
    if (market)
        theMarketsCache.put(market);
    return market;
}

void setMarketData(spMarket market) {
    if (!market || !market->marketId || !market->systemAddress)
        return;
    if (auto old = theMarketsCache.get(market->marketId); old && old->timestamp > market->timestamp && old->raven == market->raven) {
        if (!old->raven || old->raven->timestamp == market->raven->timestamp)
            return;
    }
    theMarketsCache.put(market);
    saveMarket(market.get());
}


} // namespace gal
