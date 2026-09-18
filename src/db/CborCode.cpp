//
// Created by mkizub on 09.09.2026.
//

#include "../pch.h"

#include "DB.h"
#include "JsEnum.h"

#define MSGPACK_NO_BOOST 1
#include <msgpack.hpp>

thread_local std::string tlStarSystemName;
thread_local bool tlDumpFull;

namespace msgpack {
MSGPACK_API_VERSION_NAMESPACE(v3) {
namespace adaptor {

template <typename Stream>
inline msgpack::packer<Stream>& pack_key(const char* nm, msgpack::packer<Stream>& m, uint32_t& key_count, uint32_t k) {
    key_count += 1;
    m.pack_unsigned_int(k);
    //auto s = std::format("{}-key-{}", nm, k);
    //m.pack_str(s.size()).pack_str_body(s.data(), s.size());
    return m;
}

template <typename T>
inline bool need_value(const T& val) {
    if constexpr (std::same_as<T, Timestamp>)
        return val.time_since_epoch().count() != 0;
    else if constexpr (std::same_as<T, bool>)
        return val;
    else if constexpr (std::is_floating_point_v<T>)
        return !std::isnan(val);
    else if constexpr (std::is_integral_v<T>)
        return val != 0;
    else if constexpr (std::same_as<std::string, T>)
        return !val.empty();
    else if constexpr (std::same_as<std::string_view, T>)
        return !val.empty();
    else if constexpr (std::same_as<db::LandingPadsJS, T>)
        return !val.empty();
    return true;
}

template <typename K, typename V>
inline bool need_value(const ed::small_map<K,V>& v) {
    return !v.empty();
}

template <typename V>
inline bool need_value(const std::vector<V>& v) {
    return !v.empty();
}

template <typename V>
inline bool need_value(const std::unique_ptr<V>& v) {
    return v.get() != nullptr;
}

template <typename E>
inline bool need_value(const db::JsEnum<E>& v) {
    return bool(v);
}


#define PK_MAP_START(o)     {   uint32_t key_count = 0;     \
                                msgpack::sbuffer mb;        \
                                msgpack::packer<msgpack::sbuffer> m(mb);

#define PK_MAP_END(o)           o.pack_map(key_count);      \
                                o.pack_bin_body(mb.data(),mb.size());   }

#define UN_MAP_START(m)     uint32_t sz = m.size;                       \
                            msgpack::object_kv* p = m.ptr;              \
                            for (uint32_t j = 0; j < sz; ++j) {         \
                                unsigned key = p[j].key.as<unsigned>(); \
                                auto& val = p[j].val;                   \
                                switch (key) {

#define UN_MAP_END()            }                                       \
                            }


//#define PK(nm, k)           pack_key(nm, m, key_count, k)

#define PK_IF(nm, k, v)     if (need_value(v)) pack_key(nm, m, key_count, k).pack(v)

#define UN_PK(nm, k, v)         case k: v = val.as<decltype(v)>(); continue
#define UN_PK_STAR(nm, k, v, f) case k: v.ensureStarPart()->f = val.as<decltype(db::StarPartJS::f)>(); continue
#define UN_PK_PLNT(nm, k, v, f) case k: v.ensurePlanedPart()->f = val.as<decltype(db::PlanetPartJS::f)>(); continue


template<typename ES>
struct pack<db::JsEnum<ES>> {
    using T = db::JsEnum<ES>;
    template <typename Stream>
    msgpack::packer<Stream>& operator()(msgpack::packer<Stream>& o, const T& v) const {
        if (!v.ptr)
            o.pack_nil();
        else if (!v.ptr->id && !v.ptr->str.empty())
            o.pack_str(v.ptr->str.size()).pack_str_body(v.ptr->str.data(), v.ptr->str.size());
        else
            o.pack_unsigned_int(v.ptr->id);
        return o;
    }
};

template<typename ES>
struct convert<db::JsEnum<ES>> {
    using T = db::JsEnum<ES>;
    msgpack::object const& operator()(msgpack::object const& o, T& v) const {
        if (o.type == msgpack::type::NIL) {
            v = {};
        }
        else if (o.type == msgpack::type::STR) {
            v.ptr = ES::instance.get(o.as<std::string>());
            if (!v.ptr)
                v.ptr = ES::instance.addNewValue(o.as<std::string>());
        }
        else if (o.type == msgpack::type::POSITIVE_INTEGER) {
            v.ptr = ES::instance.get(o.as<unsigned>());
        }
        else {
            throw msgpack::type_error();
        }
        return o;
    }
};


template<>
struct pack<db::MarketLineJS> {
    template <typename Stream>
    msgpack::packer<Stream>& operator()(msgpack::packer<Stream>& o, const db::MarketLineJS& v) const {
        o.pack_array(5);
        o.pack_unsigned_long_long(v.id);
        o.pack_unsigned(v.demand);
        o.pack_unsigned(v.supply);
        o.pack_unsigned(v.buyPrice);
        o.pack_unsigned(v.sellPrice);
        return o;
    }
};

template<>
struct convert<db::MarketLineJS> {
    msgpack::object const& operator()(msgpack::object const& o, db::MarketLineJS& v) const {
        if (o.type != msgpack::type::ARRAY) { throw msgpack::type_error(); }
        if (o.via.array.size != 5) { throw msgpack::type_error(); }
        v.id = o.as<decltype(v.id)>();
        v.demand = o.as<decltype(v.demand)>();
        v.supply = o.as<decltype(v.supply)>();
        v.buyPrice = o.as<decltype(v.buyPrice)>();
        v.sellPrice = o.as<decltype(v.sellPrice)>();
        return o;
    }
};


template<>
struct pack<std::vector<db::BodyParentJS>> {
    using T = std::vector<db::BodyParentJS>;
    template <typename Stream>
    msgpack::packer<Stream>& operator()(msgpack::packer<Stream>& o, const T& v) const {
        o.pack_array(v.size()*2);
        for (int i=0; i < v.size(); i++) {
            o.pack(v[i].type);
            o.pack_unsigned_int(v[i].bodyId);
        }
        return o;
    }
};

template<>
struct convert<std::vector<db::BodyParentJS>> {
    msgpack::object const& operator()(msgpack::object const& o, std::vector<db::BodyParentJS>& v) const {
        if (o.type != msgpack::type::ARRAY) { throw msgpack::type_error(); }
        if (o.via.array.size & 1) { throw msgpack::type_error(); }
        auto sz = o.via.array.size / 2;
        v.reserve(sz);
        for (int i=0; i < sz; i++) {
            db::BodyParentJS bp {};
            bp.type = o.via.array.ptr[i*2].as<decltype(bp.type)>();
            bp.bodyId = o.via.array.ptr[i*2].as<decltype(bp.bodyId)>();
            v.push_back(bp);
        }
        return o;
    }
};


template<typename E, typename V>
struct pack<ed::small_map<db::JsEnum<E>,V>> {
    using T = ed::small_map<db::JsEnum<E>,V>;
    template <typename Stream>
    msgpack::packer<Stream>& operator()(msgpack::packer<Stream>& o, const T& v) const {
        o.pack_map(v.size());
        for (auto& [key,val] : v) {
            o.pack(key);
            o.pack(val);
        }
        return o;
    }
};

template<typename E, typename V>
struct convert<ed::small_map<db::JsEnum<E>,V>> {
    using T = ed::small_map<db::JsEnum<E>,V>;
    msgpack::object const& operator()(msgpack::object const& o, T& v) const {
        if (o.type != msgpack::type::MAP) { throw msgpack::type_error(); }
        uint32_t sz = o.via.map.size;
        msgpack::object_kv* p = o.via.map.ptr;
        for (uint32_t i = 0; i < sz; ++i) {
            auto key = p[i].key.as<db::JsEnum<E>>();
            v[key] = p[i].val.as<V>();
        }
        return o;
    }
};


template<>
struct pack<db::PowerConflictJS> {
    using T = db::PowerConflictJS;
    template <typename Stream>
    msgpack::packer<Stream>& operator()(msgpack::packer<Stream>& o, const T& v) const {
        o.pack_array(2);
        o.pack(v.power);
        o.pack_float(v.progress);
        return o;
    }
};

template<>
struct convert<db::PowerConflictJS> {
    msgpack::object const& operator()(msgpack::object const& o, db::PowerConflictJS& v) const {
        if (o.type != msgpack::type::ARRAY) { throw msgpack::type_error(); }
        if (o.via.array.size != 2) { throw msgpack::type_error(); }
        v.power = o.via.array.ptr[0].as<decltype(v.power)>();
        v.progress = o.via.array.ptr[1].as<decltype(v.progress)>();
        return o;
    }
};


template<>
struct pack<db::FactionStateJS> {
    using T = db::FactionStateJS;
    template <typename Stream>
    msgpack::packer<Stream>& operator()(msgpack::packer<Stream>& o, const T& v) const {
        o.pack_array(2);
        o.pack(v.state);
        if (!std::isnan(v.trend))
            o.pack_float(v.trend);
        else
            o.pack_nil();
        return o;
    }
};

template<>
struct convert<db::FactionStateJS> {
    msgpack::object const& operator()(msgpack::object const& o, db::FactionStateJS& v) const {
        if (o.type != msgpack::type::ARRAY) { throw msgpack::type_error(); }
        if (o.via.array.size != 2) { throw msgpack::type_error(); }
        v.state = o.via.array.ptr[0].as<decltype(v.state)>();
        if (!o.via.array.ptr[1].is_nil())
            v.trend = o.via.array.ptr[1].as<decltype(v.trend)>();
        return o;
    }
};


template<>
struct pack<db::FactionJS> {
    using T = db::FactionJS;
    template <typename Stream>
    msgpack::packer<Stream>& operator()(msgpack::packer<Stream>& o, const T& v) const {
        PK_MAP_START(o)
            PK_IF("fa", 1, v.name);
            PK_IF("fa", 2, v.state);
            PK_IF("fa", 3, v.allegiance);
            PK_IF("fa", 4, v.government);
            PK_IF("fa", 5, v.influence);
            PK_IF("fa", 6, v.activeStates);
            PK_IF("fa", 7, v.pendingStates);
            PK_IF("fa", 8, v.recoveringStates);
        PK_MAP_END(o)
        return o;
    }
};

template<>
struct convert<db::FactionJS> {
    msgpack::object const& operator()(msgpack::object const& o, db::FactionJS& v) const {
        if (o.type != msgpack::type::MAP) { throw msgpack::type_error(); }
        auto& m = o.via.map;
        UN_MAP_START(m)
            UN_PK("fa", 1, v.name);
            UN_PK("fa", 2, v.state);
            UN_PK("fa", 3, v.allegiance);
            UN_PK("fa", 4, v.government);
            UN_PK("fa", 5, v.influence);
            UN_PK("fa", 6, v.activeStates);
            UN_PK("fa", 7, v.pendingStates);
            UN_PK("fa", 8, v.recoveringStates);
        UN_MAP_END()
        return o;
    }
};


template<>
struct pack<db::ThargoidWarJS> {
    using T = db::ThargoidWarJS;
    template <typename Stream>
    msgpack::packer<Stream>& operator()(msgpack::packer<Stream>& o, const T& v) const {
        PK_MAP_START(o)
            PK_IF("tw", 1, v.currentState);
            PK_IF("tw", 2, v.successState);
            PK_IF("tw", 3, v.failureState);
            PK_IF("tw", 4, v.progress);
            PK_IF("tw", 5, v.daysRemaining);
            PK_IF("tw", 6, v.portsRemaining);
            PK_IF("tw", 7, v.successReached);
        PK_MAP_END(o)
        return o;
    }
};

template<>
struct convert<db::ThargoidWarJS> {
    msgpack::object const& operator()(msgpack::object const& o, db::ThargoidWarJS& v) const {
        if (o.type != msgpack::type::MAP) { throw msgpack::type_error(); }
        auto& m = o.via.map;
        UN_MAP_START(m)
            UN_PK("tw", 1, v.currentState);
            UN_PK("tw", 2, v.successState);
            UN_PK("tw", 3, v.failureState);
            UN_PK("tw", 4, v.progress);
            UN_PK("tw", 5, v.daysRemaining);
            UN_PK("tw", 6, v.portsRemaining);
            UN_PK("tw", 7, v.currentState);
            UN_PK("tw", 8, v.successReached);
        UN_MAP_END()
        return o;
    }
};


template<>
struct pack<db::LandingPadsJS> {
    using T = db::LandingPadsJS;
    template <typename Stream>
    msgpack::packer<Stream>& operator()(msgpack::packer<Stream>& o, const T& v) const {
        o.pack_array(3);
        o.pack_unsigned_int(v.large);
        o.pack_unsigned_int(v.medium);
        o.pack_unsigned_int(v.small);
        return o;
    }
};

template<>
struct convert<db::LandingPadsJS> {
    msgpack::object const& operator()(msgpack::object const& o, db::LandingPadsJS& v) const {
        if (o.type != msgpack::type::ARRAY) { throw msgpack::type_error(); }
        if (o.via.array.size != 3) { throw msgpack::type_error(); }
        v.large = o.via.array.ptr[0].as<decltype(v.large)>();
        v.medium = o.via.array.ptr[1].as<decltype(v.medium)>();
        v.small = o.via.array.ptr[2].as<decltype(v.small)>();
        return o;
    }
};


template<>
struct pack<db::StationJS> {

    template <typename Stream>
    msgpack::packer<Stream>& operator()(msgpack::packer<Stream>& o, db::StationJS const& v) const {
        o.pack_array(4);
        o.pack_fix_int64(v.id);
        o.pack(v.type);
        o.pack(v.name);
        PK_MAP_START(o)
            PK_IF("st", 1, v.updated_at);
            PK_IF("st", 2, v.bodyId);
            PK_IF("st", 3, v.controllingFaction);
            PK_IF("st", 4, v.controllingFactionState);
            PK_IF("st", 5, v.primaryEconomy);
            PK_IF("st", 6, v.secondaryEconomy);
            PK_IF("st", 7, v.economies);
            PK_IF("st", 8, v.allegiance);
            PK_IF("st", 9, v.government);
            PK_IF("st", 10, v.state);
            PK_IF("st", 11, v.distanceToArrival);
            PK_IF("st", 12, v.latitude);
            PK_IF("st", 13, v.longitude);
            PK_IF("st", 14, v.landingPads);
            PK_IF("st", 15, v.carrierDockingAccess);
            if (tlDumpFull) {
                PK_IF("st", 16, v.services);
            }
        PK_MAP_END(o)
        return o;
    }
};

template<>
struct convert<db::StationJS> {
    msgpack::object const& operator()(msgpack::object const& o, db::StationJS& v) const {
        if (o.type != msgpack::type::ARRAY) { throw msgpack::type_error(); }
        if (o.via.array.size != 4) { throw msgpack::type_error(); }
        v.id = o.via.array.ptr[0].as<int64_t>();
        v.type = o.via.array.ptr[1].as<decltype(v.type)>();
        v.name = o.via.array.ptr[2].as<decltype(v.name)>();

        auto& m = o.via.array.ptr[3].via.map;
        UN_MAP_START(m)
            UN_PK("st", 1, v.updated_at);
            UN_PK("st", 2, v.bodyId);      // space stations have bodyId
            UN_PK("st", 3, v.controllingFaction);
            UN_PK("st", 4, v.controllingFactionState);
            UN_PK("st", 5, v.primaryEconomy);
            UN_PK("st", 6, v.secondaryEconomy);
            UN_PK("st", 7, v.economies);
            UN_PK("st", 8, v.allegiance);
            UN_PK("st", 9, v.government);
            UN_PK("st", 10, v.state);
            UN_PK("st", 11, v.distanceToArrival);
            UN_PK("st", 12, v.latitude);
            UN_PK("st", 13, v.longitude);
            UN_PK("st", 14, v.landingPads);
            UN_PK("st", 15, v.carrierDockingAccess);
            UN_PK("st", 16, v.services);
        UN_MAP_END()
        return o;
    }
};


//
// db::BodyJS
//

template<>
struct pack<db::BodyJS> {

    template <typename Stream>
    msgpack::packer<Stream>& operator()(msgpack::packer<Stream>& o, db::BodyJS const& v) const {
        if (!v.type) {
            o.pack_nil();
            return o;
        }
        o.pack_array(4);
        o.pack(v.type);
        o.pack_int(v.bodyId);

        std::string v_name = v.name;
        bool is_star = v.type == db::JsBodyType::instance.get("Star");
        bool is_planet = v.type == db::JsBodyType::instance.get("Planet");
        bool is_cluster = v.type == db::JsBodyType::instance.get("Asteroid Cluster");
        if ((is_star || is_planet || is_cluster) && !v.name.empty()) {
            auto &sn = tlStarSystemName;
            if (!sn.empty()) {
                if (is_star && v_name == sn)
                    v_name = "";
                else if (v_name.size() >= sn.size() + 2 && v_name.starts_with(sn) && v_name[sn.size()] == ' ')
                    v_name = v_name.substr(sn.size());
                if (is_cluster) {
                    auto p = v_name.find("Belt Cluster");
                    if (p != std::string::npos)
                        v_name = v_name.replace(p, 12, "$");
                }
            }
            //if (v_name == v.name)
            //    LOG_INFO("Special body name: {}", v_name);
        } else {
            v_name = {};
        }
        o.pack(v_name);

        PK_MAP_START(o)
            PK_IF("bo", 1, v.updated_at);
            PK_IF("bo", 2, v.subType);
            PK_IF("bo", 3, v.orbitalPeriod);
            PK_IF("bo", 4, v.semiMajorAxis);
            PK_IF("bo", 5, v.orbitalEccentricity);
            PK_IF("bo", 6, v.orbitalInclination);
            PK_IF("bo", 7, v.argOfPeriapsis);
            PK_IF("bo", 8, v.meanAnomaly);
            PK_IF("bo", 9, v.ascendingNode);
            PK_IF("bo", 10, v.distanceToArrival);
            PK_IF("bo", 11, v.surfaceTemperature);
            PK_IF("bo", 12, v.rotationalPeriod);
            PK_IF("bo", 13, v.axialTilt);
            PK_IF("bo", 14, v.rotationalPeriodTidallyLocked);
            PK_IF("bo", 15, v.timestamps);
            PK_IF("bo", 16, v.parents);
            PK_IF("bo", 17, v.stations);
//            // "rings"
//            // "belts"
            if (auto* st=v.getStarPart(); st && is_star) {
                PK_IF("bs", 30, st->mainStar);
                PK_IF("bs", 31, st->age);
                PK_IF("bs", 32, st->spectralClass);
                PK_IF("bs", 33, st->luminosity);
                PK_IF("bs", 34, st->absoluteMagnitude);
                PK_IF("bs", 35, st->solarMasses);
                PK_IF("bs", 36, st->solarRadius);
            }
            else if (auto* pl=v.getPlanetPart(); pl && is_planet) {
                PK_IF("bp", 40, pl->isLandable);
                PK_IF("bp", 41, pl->gravity);
                PK_IF("bp", 42, pl->earthMasses);
                PK_IF("bp", 43, pl->radius);
                PK_IF("bp", 44, pl->terraformingState);
                PK_IF("bp", 45, pl->reserveLevel);
                PK_IF("bp", 46, pl->surfacePressure);
                PK_IF("bp", 47, pl->volcanismType);
                PK_IF("bp", 48, pl->atmosphereType);
                if (tlDumpFull) {
                    PK_IF("bp", 49, pl->atmosphereComposition);
                    PK_IF("bp", 50, pl->solidComposition);
                    PK_IF("bp", 51, pl->materials);
                    // "signals"
                }
            }
        PK_MAP_END(o)
        return o;
    }
};

template<>
struct convert<db::BodyJS> {
    msgpack::object const& operator()(msgpack::object const& o, db::BodyJS& v) const {
        if (o.type == msgpack::type::NIL) return o;
        if (o.type != msgpack::type::ARRAY) { throw msgpack::type_error(); }
        if (o.via.array.size != 4) { throw msgpack::type_error(); }

        v.type = o.via.array.ptr[0].as<decltype(v.type)>();
        v.bodyId = o.via.array.ptr[1].as<decltype(v.bodyId)>();
        v.name = o.via.array.ptr[2].as<decltype(v.name)>();
        {
            bool is_star = v.type == db::JsBodyType::instance.get("Star");
            //bool is_planet = v.type == db::JsBodyType::instance.get("Planet");
            bool is_cluster = v.type == db::JsBodyType::instance.get("Asteroid Cluster");
            auto &sn = tlStarSystemName;
            if (!sn.empty()) {
                if (is_star && v.name.empty()) {
                    v.name = sn;
                }
                else if (v.name.size() > 1 && v.name[0] == ' ')
                    v.name = sn + v.name;
                if (is_cluster) {
                    auto p = v.name.find('$');
                    if (p != std::string::npos)
                        v.name = v.name.replace(p, 1, "Belt Cluster");
                }
            }
        }

        auto& m = o.via.array.ptr[3].via.map;
        UN_MAP_START(m)
            UN_PK("st", 1, v.updated_at);
            UN_PK("st", 2, v.subType);
            UN_PK("st", 3, v.orbitalPeriod);
            UN_PK("st", 4, v.semiMajorAxis);
            UN_PK("st", 5, v.orbitalEccentricity);
            UN_PK("st", 6, v.orbitalInclination);
            UN_PK("st", 7, v.argOfPeriapsis);
            UN_PK("st", 8, v.meanAnomaly);
            UN_PK("st", 9, v.ascendingNode);
            UN_PK("st", 10, v.distanceToArrival);
            UN_PK("st", 11, v.surfaceTemperature);
            UN_PK("st", 12, v.rotationalPeriod);
            UN_PK("st", 13, v.axialTilt);
            UN_PK("st", 14, v.rotationalPeriodTidallyLocked);
            UN_PK("st", 15, v.timestamps);
            UN_PK("st", 16, v.parents);
            UN_PK("st", 17, v.stations);
                // "rings"
                // "belts"

            UN_PK_STAR("st", 30, v, mainStar);
            UN_PK_STAR("st", 31, v, age);
            UN_PK_STAR("st", 32, v, spectralClass);
            UN_PK_STAR("st", 33, v, luminosity);
            UN_PK_STAR("st", 34, v, absoluteMagnitude);
            UN_PK_STAR("st", 35, v, solarMasses);
            UN_PK_STAR("st", 36, v, solarRadius);

            UN_PK_PLNT("st", 40, v, isLandable);
            UN_PK_PLNT("st", 41, v, gravity);
            UN_PK_PLNT("st", 42, v, earthMasses);
            UN_PK_PLNT("st", 43, v, radius);
            UN_PK_PLNT("st", 44, v, terraformingState);
            UN_PK_PLNT("st", 45, v, reserveLevel);
            UN_PK_PLNT("st", 46, v, surfacePressure);
            UN_PK_PLNT("st", 47, v, volcanismType);
            UN_PK_PLNT("st", 48, v, atmosphereType);
            UN_PK_PLNT("st", 49, v, atmosphereComposition);
            UN_PK_PLNT("st", 50, v, solidComposition);
            UN_PK_PLNT("st", 51, v, materials);
                // "signals"
        UN_MAP_END()
        return o;
    }
};


//
// db::StarSystemJS
//

template<>
struct pack<db::StarSystemJS> {
    template <typename Stream>
    msgpack::packer<Stream>& operator()(msgpack::packer<Stream>& o, db::StarSystemJS const& v) const {
        o.pack_array(6);
        o.pack_fix_int64(v.id64);
        o.pack(v.name);
        o.pack_double(v.coords.x);
        o.pack_double(v.coords.y);
        o.pack_double(v.coords.z);
        PK_MAP_START(o)
            PK_IF("ss", 1, v.updated_at);
            PK_IF("ss", 2, v.allegiance);
            PK_IF("ss", 3, v.government);
            PK_IF("ss", 4, v.primaryEconomy);
            PK_IF("ss", 5, v.secondaryEconomy);
            PK_IF("ss", 6, v.security);
            PK_IF("ss", 7, v.population);
            PK_IF("ss", 8, v.bodyCount);
            PK_IF("ss", 9, v.controllingFaction);
            PK_IF("ss", 10, v.bodies);
            PK_IF("ss", 11, v.stations);
            if (tlDumpFull) {
                PK_IF("ss", 12, v.factions);
                PK_IF("ss", 13, v.powerState);
                PK_IF("ss", 14, v.powerConflictProgress);
                PK_IF("ss", 15, v.powers);
                PK_IF("ss", 16, v.controllingPower);
                PK_IF("ss", 17, v.powerStateControlProgress);
                PK_IF("ss", 18, v.powerStateReinforcement);
                PK_IF("ss", 19, v.powerStateUndermining);
                PK_IF("ss", 20, v.thargoidWar);
                PK_IF("ss", 21, v.timestamps);
            }
        PK_MAP_END(o)
        return o;
    }
};

template<>
struct convert<db::StarSystemJS> {
    msgpack::object const& operator()(msgpack::object const& o, db::StarSystemJS& v) const {
        if (o.type != msgpack::type::ARRAY) { throw msgpack::type_error(); }
        if (o.via.array.size != 6) { throw msgpack::type_error(); }
        v.id64 = o.via.array.ptr[0].as<int64_t>();
        v.name = o.via.array.ptr[1].as<std::string>();
        v.coords.x = o.via.array.ptr[2].as<double>();
        v.coords.y = o.via.array.ptr[3].as<double>();
        v.coords.z = o.via.array.ptr[4].as<double>();

        tlStarSystemName = v.name;

        auto& m = o.via.array.ptr[5].via.map;
        UN_MAP_START(m)
            UN_PK("ss", 1, v.updated_at);
            UN_PK("ss", 2, v.allegiance);
            UN_PK("ss", 3, v.government);
            UN_PK("ss", 4, v.primaryEconomy);
            UN_PK("ss", 5, v.secondaryEconomy);
            UN_PK("ss", 6, v.security);
            UN_PK("ss", 7, v.population);
            UN_PK("ss", 8, v.bodyCount);
            UN_PK("ss", 9, v.controllingFaction);
            UN_PK("ss", 10, v.bodies);
            UN_PK("ss", 11, v.stations);
            UN_PK("ss", 12, v.factions);
            UN_PK("ss", 13, v.powerState);
            UN_PK("ss", 14, v.powerConflictProgress);
            UN_PK("ss", 15, v.powers);
            UN_PK("ss", 16, v.controllingPower);
            UN_PK("ss", 17, v.powerStateControlProgress);
            UN_PK("ss", 18, v.powerStateReinforcement);
            UN_PK("ss", 19, v.powerStateUndermining);
            UN_PK("ss", 20, v.thargoidWar);
        UN_MAP_END()
        return o;
    }
};

} // namespace adaptor
} // namespace MSGPACK_DEFAULT_API_NS
} // namespace msgpack


namespace db {

std::ofstream dbg_cbor;

void test_cbor_start() {
    dbg_cbor.open("cache/dbg.cbor", std::ios::out|std::ios::trunc|std::ios::binary);
}
void test_cbor_end() {
    dbg_cbor.close();
}

int test_cbor(StarSystemJS& ss_js) {
    tlStarSystemName = ss_js.name;
    tlDumpFull = true;
    try {
        msgpack::sbuffer buffer;
        msgpack::pack(buffer, ss_js);

        if (dbg_cbor.is_open())
            dbg_cbor.write(buffer.data(), buffer.size());

        StarSystemJS ss_back;
        auto obj = msgpack::unpack(buffer.data(), buffer.size());
        obj->convert(ss_back);

        return (int)buffer.size();
    } catch (const std::exception& e) {
        LOG_ERROR("Packing error");
        return 0;
    }
}

js::value decode_system_blob(const std::string& system_name, const void* data, int size) {
    tlStarSystemName = system_name;
    try {
        msgpack::sbuffer buffer;
        StarSystemJS ss;
        auto obj = msgpack::unpack(buffer.data(), buffer.size());
        obj->convert(ss);

        js::value res = js::object({});
        if (ss.allegiance) res["allegiance"] = ss.allegiance.sv();
        if (ss.government) res["government"] = ss.government.sv();
        if (ss.primaryEconomy) res["primaryEconomy"] = ss.primaryEconomy.sv();
        if (ss.secondaryEconomy) res["secondaryEconomy"] = ss.secondaryEconomy.sv();
        if (ss.security) res["security"] = ss.security.sv();
        if (ss.population) res["population"] = ss.population;
        if (ss.bodyCount) res["bodyCount"] = ss.bodyCount;
        if (ss.powerState) res["powerState"] = ss.powerState.sv();
        if (ss.controllingPower) res["controllingPower"] = ss.controllingPower.sv();
        if (!std::isnan(ss.powerStateControlProgress)) res["powerStateControlProgress"] = ss.powerStateControlProgress;
        if (!std::isnan(ss.powerStateReinforcement)) res["powerStateReinforcement"] = ss.powerStateReinforcement;
        if (!std::isnan(ss.powerStateUndermining)) res["powerStateUndermining"] = ss.powerStateUndermining;
        if (!ss.powers.empty()) {
            res["powers"] = js::array({});
            auto& powers = res["powers"].as_array();
            powers.reserve(ss.powers.size());
            for (auto& pw : ss.powers)
                powers.push_back(pw.sv());
        }
        if (!ss.timestamps.empty()) {
            res["timestamps"] = js::object({});
            js::value::object_type& timestamps = res["timestamps"].as_object();
            for (auto& [key,val] : ss.timestamps)
                res["timestamps"][key.sv()] = formatTimestampString(val);
        }
//        std::unique_ptr<FactionJS> controllingFaction;
//        std::vector<FactionJS> factions;
//        std::vector<PowerConflictJS> powerConflictProgress;
//        std::unique_ptr<ThargoidWarJS> thargoidWar {};

        return res;
    } catch (const std::exception& e) {
        LOG_ERROR("decode_system_blob error");
        return {};
    }
}

bool encode_system_blob(const std::string& system_name, std::stringstream& buffer, const js::value& ext) {
    return false;
}


}