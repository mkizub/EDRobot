//
// Created by mkizub on 27.09.2026.
//

#ifndef PCH_H
#include "../pch.h"
#endif

#include "Galaxy.h"
#include "../db/DB.h"

namespace gal {

void StarSystem::load() {
    if (!systemAddress) {
        LOG_ERROR("dummy StarSystem::load()");
        return;
    }
    if (isBlobLoaded)
        return;
    db::StarSystemJS db_ss {};
    if (db::loadStarSystemBlob(db_ss, systemName, systemAddress)) {
        auto saved_needBlobSave = needBlobSave;
        update_star_system(this, db_ss);
        needBlobSave = saved_needBlobSave;
        std::vector<db::MegashipJS> megaships;
        if (db::loadMegaShips(systemAddress, megaships)) {
            for (auto& db_ms : megaships) {
                assert (db_ms.systemId == this->systemAddress);
                spEntity megaship;
                TypeNav tp = toTypeNav(JsNavType::get(db_ms.blob.type));
                if (isCarrier(tp))
                    megaship = addMegashipEntity(tp, db_ms.code, db_ms.name, db_ms.marketId, db_ms.updated);
                else
                    megaship = addMegashipEntity(tp, db_ms.name, "", db_ms.marketId, db_ms.updated);
                update_megaship(this, db_ms, megaship);
            }
        }
    }
    isBlobLoaded = true;
}

void StarSystem::save() {
    if (!systemAddress) {
        LOG_ERROR("dummy StarSystem::load()");
        return;
    }
    if (needCoreSave) {
        db::saveStarSystem({systemAddress, systemName, systemCoords, updated_at, eddn_updated_at});
        needCoreSave = false;
    }
    if (needBlobSave) {
        db::StarSystemJS db_ss {};
        std::vector<db::MegashipJS> db_megaships;
        if (fill_star_system_for_db(this, db_ss, db_megaships)) {
            db::saveStarSystemBlob(db_ss, systemName, systemAddress);
            for (auto& ms : db_megaships)
                db::saveMegaShip(ms);
        }
        needBlobSave = false;
        isBlobLoaded = true;
    }
}

void StarSystem::wipe() {
    if (!systemAddress) {
        LOG_ERROR("dummy StarSystem::wipe()");
        return;
    }
    std::vector<spEntity> megaships;
    {
        EntitiesLocker lock(this);
        updated_at = {};
        eddn_updated_at = {};
        ext = {};
        for (auto &entity: entities) {
            if (entity && isMegaship(entity->type))
                megaships.push_back(entity);
        }
        entities.clear();
        needCoreSave = true;
        isBlobLoaded = true;
        needBlobSave = true;
    }
    save();
    for (auto& ms : megaships) {
        db::MegashipJS db_ms {};
        gal::fill_megaship_for_db(this, ms.get(), db_ms);
        db::removeMegaShip(db_ms);
    }
}

spEntity StarSystem::getMainStar() const {
    if (!systemAddress) {
        LOG_ERROR("dummy StarSystem::getMainStar()");
        return {};
    }
    EntitiesLocker lock(this);
    for (auto& b : this->entities) {
        if (b && b->type == TypeNav::Star) {
            auto* sd = b->getStarData();
            if (sd && sd->mainStar.value_or())
                return b;
        }
    }
    return {};
}

spEntity StarSystem::getEntity(const std::string& nm) const {
    if (!systemAddress) {
        LOG_ERROR("dummy StarSystem::getEntity()");
        return {};
    }
    if (nm.empty())
        return {};
    EntitiesLocker lock(this);
    for (auto& e : this->entities) {
        if (e && e->nameEq(nm))
            return e;
    }
    std::string xx_name;
    std::string ru_name;
    NavType::localizeName(TypeNav::Other, nm, xx_name, ru_name);
    for (auto& s : this->entities) {
        if (s && (s->nameEq(xx_name) || s->nameEq(ru_name)))
            return s;
    }
    return {};
}

spEntity StarSystem::getSignal(std::string_view sig_name) const {
    if (!systemAddress) {
        LOG_ERROR("dummy StarSystem::getSignal()");
        return {};
    }
    if (sig_name.empty())
        return {};
    EntitiesLocker lock(this);
    if (BEACON.match_name(sig_name)) {
        for (auto& s : this->entities) {
            if (s && s->type == TypeNav::NavBeacon)
                return s;
        }
    } else {
        for (auto& s : this->entities) {
            if (s && isSignal(s->type)) {
                if (s->nameEq(sig_name))
                    return s;
            }
        }
    }
    return {};
}

void StarSystem::removeEntity(const spEntity& entity) {
    if (!systemAddress) {
        LOG_ERROR("dummy StarSystem::removeEntity()");
        return;
    }
    if (!entity)
        return;
    {
        bool erased = false;
        EntitiesLocker lock(this);
        for (auto &e: this->entities) {
            if (e && e.get() == entity.get()) {
                e.reset();
                erased = true;
            }
        }
        if (!erased)
            return;
    }
    updated_at = Timestamp::clock::now();
    needCoreSave = true;
    if (isMegaship(entity->type)) {
        db::MegashipJS db_ms {};
        gal::fill_megaship_for_db(this, entity.get(), db_ms);
        db::removeMegaShip(db_ms);
    } else {
        needBlobSave = true;
    }
}

spEntity StarSystem::moveMegashipHere(int64_t marketId, std::string_view code, std::string_view name, body_id_t parentId, bool silent) {
    if (!systemAddress) {
        LOG_ERROR("dummy StarSystem::moveCarrierHere()");
        return {};
    }
    if (!marketId && code.empty() && name.empty())
        return {};
    spEntity megaship;
    if (marketId)
        megaship = getDock(marketId);
    if (!megaship && !name.empty())
        megaship = getDock(name);
    if (!megaship && !code.empty())
        megaship = getDock(code);

    if (megaship && megaship->parentBodyId == parentId)
        return megaship;

    db::MegashipJS db_ms {};
    db_ms.marketId = marketId;
    db_ms.code = code;
    db_ms.name = name;
    if (!db::loadMegaShip(db_ms, silent))
        return {};
    if (!marketId)
        marketId = db_ms.marketId;
    if (db_ms.systemId == systemAddress) {
        update_megaship(this, db_ms, megaship);
        if (db_ms.blob.parentId != parentId) {
            db_ms.blob.parentId = parentId;
            db::saveMegaShip(db_ms);
        }
        if (marketId)
            megaship = getDock(marketId);
        else
            megaship = getDock(db_ms.name);
        return megaship;
    }
    auto fromSystem = gal::getStarSystem(db_ms.systemId, false, false);
    if (fromSystem) {
        if (marketId)
            megaship = fromSystem->getDock(marketId);
        if (!megaship && !name.empty())
            megaship = fromSystem->getDock(name);
        if (!megaship && !code.empty())
            megaship = fromSystem->getDock(code);
    }

    if (!fromSystem || !megaship) {
        db_ms.systemId = systemAddress;
        db_ms.blob.parentId = parentId;
        update_megaship(this, db_ms, megaship);
        db::saveMegaShip(db_ms);
        if (marketId)
            megaship = getDock(marketId);
        else
            megaship = getDock(db_ms.name);
        return megaship;
    }
    {
        EntitiesLocker lock(fromSystem.get());
        for (auto &e: fromSystem->entities) {
            if (e && e.get() == megaship.get())
                e.reset();
        }
    }
    addEntity(megaship);
    db_ms.systemId = systemAddress;
    db_ms.blob.parentId = parentId;
    update_megaship(this, db_ms, megaship);
    db::saveMegaShip(db_ms);
    return megaship;
}

spEntity StarSystem::moveMegashipHere(db::MegashipJS& db_ms, body_id_t parentId) {
    assert (systemAddress);
    assert (db_ms.systemId && db_ms.systemId != systemAddress);
    assert (db_ms.marketId || !db_ms.code.empty() || !db_ms.name.empty());
    spEntity megaship;
    auto fromSystem = gal::getStarSystem(db_ms.systemId, false, false);
    if (fromSystem) {
        if (db_ms.marketId)
            megaship = fromSystem->getDock(db_ms.marketId);
        if (!megaship && !db_ms.name.empty())
            megaship = fromSystem->getDock(db_ms.name);
        if (!megaship && !db_ms.code.empty())
            megaship = fromSystem->getDock(db_ms.code);
        if (megaship) {
            EntitiesLocker lock(fromSystem.get());
            for (auto &e: fromSystem->entities) {
                if (e && e.get() == megaship.get())
                    e.reset();
            }
        }
    }

    // prevent recursive addMegashipEntity->update_megaship->addEntity->addMegashipEntity
    TypeNav tp = toTypeNav(JsNavType::get(db_ms.blob.type));
    if (db_ms.code.empty() || db_ms.name == db_ms.code)
        megaship = addEntity(spEntity(new MegashipEntity(tp, "", db_ms.name, "", db_ms.marketId)));
    else
        megaship = addEntity(spEntity(new MegashipEntity(tp, "", db_ms.code, db_ms.name, db_ms.marketId)));
    megaship->updated = db_ms.updated;
    db_ms.systemId = systemAddress;
    db_ms.blob.parentId = parentId;
    update_megaship(this, db_ms, megaship);
    db::saveMegaShip(db_ms);
    return megaship;
}


spEntity StarSystem::checkTypeAndName(spEntity entity, TypeNav tp, std::string_view sname, std::string_view snloc, entity_args args) {
    if (!systemAddress) {
        LOG_ERROR("dummy StarSystem::checkTypeAndName()");
        return entity;
    }
    if (!entity) {
        LOG_ERROR("StarSystem::checkTypeAndName on nullptr");
        return entity;
    }

    {
        EntitiesLocker lock(this);

        decltype(entities)::iterator entityIt = entities.end();
        for (auto it = entities.begin(); it != entities.end(); it++) {
            if (it->get() == entity.get()) {
                entityIt = it;
                break;
            }
        }
        if (entityIt == entities.end()) {
            LOG_ERROR("StarSystem::checkTypeAndName on entity that is not in this system");
            return entity;
        }
    }

    // special care needed to convert from->to megaships, because we need to check names
    // and update database table for megaships
    //
    // do not allow type conversion for bodies, sine they are safely created from logs,
    // but I have to think about wrong information from network (spansh, edsm)
    //
    // signals (usually obtained from FSSDiscovery) often have wrong type,
    // so can safely be converted to AnySite (stations and megaships)
    //
    // FSSDiscovery and ApproachSettlement often have wrong type for space/planetary types
    // For new constructions game reports wrong surface station even for space stations.
    // So we freely convert any site type to any other site type, but generally we
    // should not freely downgrade definitely detected site type.
    // Manual conversion is allowed in any case.

    if (entity->type != tp) {
        if (entity->type == TypeNav::Other) {
            // TypeNav::Other can be upgraded to any other type
            removeEntity(entity);
            if (!entity->getUnexpandedName().empty())
                entity = addEntity(tp, entity->getUnexpandedName(), snloc, args);
            else
                entity = addEntity(tp, entity->name, snloc, args);
            needBlobSave = true;
        }
        else if (entity->type == TypeNav::AnySite && isAnySite(tp)) {
            if (isMegaship(tp)) {
                removeEntity(entity);
                if (!entity->getUnexpandedName().empty())
                    entity = addEntity(tp, entity->getUnexpandedName(), snloc, args);
                else
                    entity = addMegashipEntity(tp, entity->name, snloc, args.market_id, args.ts);
            } else {
                const_cast<TypeNav&>(entity->type) = tp;
            }
            entity->updated = std::max(entity->updated, args.ts);
            this->updated_at = std::max(this->updated_at, args.ts);
            needBlobSave = true;
        }
        else if (isSpaceStation(entity->type) && isSpaceStation(tp) ||
                 isPlanetarySite(entity->type) && isPlanetarySite(tp) ||
                 isMegaship(entity->type) && isMegaship(tp) ||
                 isSignal(entity->type) && isSignal(tp))
        {
            const_cast<TypeNav&>(entity->type) = tp;
            entity->updated = std::max(entity->updated, args.ts);
            this->updated_at = std::max(this->updated_at, args.ts);
            needBlobSave = true;
        }
        else if (entity->type == TypeNav::AnySite && isSignal(tp)) {
            // AnySite -> TouristBeacon, for example
            removeEntity(entity);
            if (!entity->getUnexpandedName().empty())
                entity = addEntity(tp, entity->getUnexpandedName(), snloc, args);
            else
                entity = addEntity(tp, entity->name, entity->getLocalisedName(), args);
            needBlobSave = true;
        }
        else if (!isGeneric(tp)) {
            LOG_ERROR("StarSystem::checkTypeAndName set incompatible type, from {} to {}", entity->type, tp);
            return entity;
        }
    }

    if (isBody(entity->type)) {
        if (entity->name != sname && BodyEntity::setBodyName(entity, sname)) {
            if (*args.ts) {
                entity->updated = args.ts;
                this->updated_at = std::max(this->updated_at, args.ts);
            }
            needBlobSave = true;
        }
        return entity;
    }

    // try set both name and nloc
    if (entity->name == sname && entity->getLocalisedName() == snloc)
        return entity;

    if (isSignal(entity->type)) {
        if (SignalEntity::setSignalName(entity, sname, snloc)) {
            if (*args.ts) {
                entity->updated = args.ts;
                this->updated_at = std::max(this->updated_at, args.ts);
            }
            needBlobSave = true;
        }
        return entity;
    }
    else if (isCarrier(entity->type)) {
        std::string code, full;
        TypeNav res;
        if (!snloc.empty())
            res = NavType::checkCarrierName(snloc, &code, &full);
        else
            res = NavType::checkCarrierName(sname, &code, &full);
        if (isCarrier(res)) {
            if (entity->name != code) {
                LOG_WARNING("StarSystem::checkTypeAndName attempt to change carrier callsign '{}' != '{}'", code, entity->name);
                return entity;
            }
            if (!snloc.empty() && entity->getLocalisedName() != full) {
                if (MegashipEntity::setCarrierName(entity, code, full)) {
                    if (*args.ts) {
                        entity->updated = args.ts;
                        this->updated_at = std::max(this->updated_at, args.ts);
                    }
                    needBlobSave = true;
                }
            }
            return entity;
        } else {
            LOG_WARNING("StarSystem::checkTypeAndName bad carrier name '{}'/'{}'", sname, snloc);
            return entity;
        }
    }
    else if (isMegaship(entity->type)) {
        if (MegashipEntity::setMegashipName(entity, sname, snloc)) {
            if (*args.ts) {
                entity->updated = args.ts;
                this->updated_at = std::max(this->updated_at, args.ts);
            }
            needBlobSave = true;
        }
        return entity;
    }
    else if (isAnySite(entity->type)) {
        if (StationEntity::setStationName(entity, sname, snloc)) {
            if (*args.ts) {
                entity->updated = args.ts;
                this->updated_at = std::max(this->updated_at, args.ts);
            }
            needBlobSave = true;
        }
        return entity;
    }

    LOG_WARNING("StarSystem::checkTypeAndName on unknown entity type '{}'", entity->type);
    return entity;
}

spEntity StarSystem::getBodyById(body_id_t bodyId) const {
    if (!systemAddress) {
        LOG_ERROR("dummy StarSystem::getBodyById()");
        return {};
    }
    if (!bodyId.has_value())
        return {};
    EntitiesLocker lock(this);
    for (auto& b : this->entities) {
        if (b && b->bodyId == bodyId)
            return b;
    }
    return {};
}

spEntity StarSystem::getBody(std::string_view bname) const {
    if (!systemAddress) {
        LOG_ERROR("dummy StarSystem::getBody()");
        return {};
    }
    if (bname.empty())
        return {};
    EntitiesLocker lock(this);
    for (auto& b : this->entities) {
        if (b && isBody(b->type) && b->nameEq(bname))
            return b;
    }
    return {};
}
spEntity StarSystem::getDock(std::string_view sname) const {
    if (!systemAddress) {
        LOG_ERROR("dummy StarSystem::getDock()");
        return {};
    }
    if (sname.empty())
        return {};
    EntitiesLocker lock(this);
    for (auto& s : this->entities) {
        if (!s || s->type < TypeNav::SpaceStation)
            continue;
        if (s->nameEq(sname))
            return s;
    }
    return {};
}

spEntity StarSystem::getDock(int64_t marketId) const {
    if (!systemAddress) {
        LOG_ERROR("dummy StarSystem::getDock()");
        return {};
    }
    if (marketId == 0)
        return {};
    EntitiesLocker lock(this);
    for (auto& s : this->entities) {
        if (!s || s->type < TypeNav::SpaceStation)
            continue;
        if (s->marketId() == marketId && s->type != TypeNav::AnonMegaship)
            return s;
    }
    return {};
}


spEntity StarSystem::addNavListEntry(wchar_t charOCR, const std::string& nav_icon, const std::string& sname, body_id_t bodyId) {
    if (!systemAddress) {
        LOG_ERROR("dummy StarSystem::addNavListEntry()");
        return {};
    }

    TypeNav typeNav = NavType::guessTypeFromName(TypeNav::Other, sname);
    spEntity entity = getEntity(sname);
    if (!entity) {
        entity = addEntity(typeNav, sname, "", {});
    }

    if (typeNav == TypeNav::Other) {
        TypeNav tp = TypeNav::Other;
        for (auto nt: ALL_NAV_TYPES) {
            if (nt->match_icon(charOCR, nav_icon)) {
                if (entity->type == TypeNav::Other || nt->type == entity->type) {
                    typeNav = nt->type;
                    break;
                }
                if (tp == TypeNav::Other)
                    tp = nt->type;
            }
        }
        if (typeNav != entity->type && tp != TypeNav::Other)
            typeNav = tp;
    }

    if (typeNav != TypeNav::Other && entity->type != typeNav || entity->name.empty() || !entity->nameEq(sname)) {
        entity = checkTypeAndName(entity, typeNav, sname, "", {.ts=entity->updated});
    }

    if (bodyId.has_value()) {
        if (isBody(entity->type) || isSpaceStation(entity->type)) {
            if (!entity->bodyId.has_value()) {
                entity->bodyId = bodyId;
                needBlobSave = true;
            }
        } else {
            if (!entity->parentBodyId.has_value()) {
                entity->parentBodyId = bodyId;
                needBlobSave = true;
            }
        }
    }

    save();
    return entity;
}

//spEntity StarSystem::addStation(spGameEvent& ge) {
//    if (!systemAddress) {
//        LOG_ERROR("dummy StarSystem::addStation()");
//        return {};
//    }
//    auto& je = ge->data;
//
//    if (ge->event=="SupercruiseDestinationDrop") {
//        int64_t marketId = je["MarketID"].as_int_or();
//        if (!marketId)
//            return {};
//        auto name = je["Type"].as_string_or();
//        auto nloc = je["Type_Localised"].as_string_or();
//        gal::spEntity dock = getDock(marketId);
//        if (!dock)
//            dock = getDock(name);
//        if (!dock) {
//            dock = addEntity(gal::Entity::create(TypeNav::AnySite, name, nloc));
//        } else {
//            dock = checkTypeAndName(dock, dock->type, name, "", ge->timestamp);
//        }
//        if (!dock->marketId()) {
//            dock->setMarketId(marketId);
//            needBlobSave = true;
//        }
//        save();
//        return dock;
//    }
//
//
//    if (!(ge->event=="ApproachSettlement" || ge->event=="Docked" || (ge->event=="Location") && ge->data["Docked"]))
//        return {};
//
//    body_id_t stationBodyId;
//    if (ge->event == "Location") {
//        body_id_t bodyId;
//        auto bodyName = je["Body"].as_string_or();
//        auto bodyType = je["BodyType"].as_string_or();
//        bodyId = je["BodyID"].as_int_or(-1);
//        if (bodyType == "Star" || bodyType == "Planet") {
//            TypeNav tp = toTypeNav(JsNavType::get(bodyType));
//            auto body = getBodyById(bodyId);
//            if (!body)
//                body = getBody(bodyName);
//            if (!body)
//                body = addEntity(Entity::create(tp, bodyName));
//            body = checkTypeAndName(body, tp, bodyName, "", ge->timestamp);
//            if (body->bodyId != bodyId) {
//                body->bodyId = bodyId;
//                body->updated = ge->timestamp;
//                needBlobSave = true;
//            }
//        }
//        else if (bodyType == "Station") {
//            stationBodyId = bodyId;
//        }
//    }
//
//    std::string sname;
//    std::string snloc;
//    std::string stype;
//    int64_t marketId = je["MarketID"].as_int_or();
//    if (ge->event=="ApproachSettlement") {
//        sname = je["Name"].as_string_or();
//        snloc = je["Name_Localised"].as_string_or();
//        if (st::client.majorVersion <= 3) {
//            while (sname.back() == '+') sname = sname.substr(0,sname.size()-1);
//            while (sname.back() == ' ') sname = sname.substr(0,sname.size()-1);
//        }
//    } else {
//        sname = je["StationName"].as_string_or();
//        snloc = je["StationName_Localised"].as_string_or();
//        stype = je["StationType"].as_string_or();
//    }
//
//    spEntity dock = getDock(marketId);
//    if (!dock) {
//        dock = getDock(sname);
//        if (!dock)
//            dock = getSignal(sname, snloc);
//        if (dock) {
//            if (dock->marketId()) {
//                assert (dock->marketId() != marketId);
//                dock.reset();
//            } else {
//                dock->setMarketId(marketId);
//                needBlobSave = true;
//            }
//        }
//    }
//
//    TypeNav typeNav = toTypeNav(JsNavType::get(stype));
//    if (typeNav == TypeNav::Other)
//        typeNav = TypeNav::AnySite;
//    if (!dock && isMegaship(typeNav)) {
//        db::MegashipJS db_ms {};
//        db_ms.marketId = marketId;
//        db_ms.name = sname;
//        if (db::loadMegaShip(db_ms, true) && db_ms.marketId) {
//            if (db_ms.systemId != systemAddress) {
//                dock = moveMegashipHere(db_ms.marketId, db_ms.code, db_ms.name, body_id_t());
//            }
//        }
//    }
//    if (!dock) {
//        dock = Entity::create(typeNav, sname, snloc);
//        dock = addEntity(dock);
//    }
//
//    for (auto nt: ALL_NAV_TYPES) {
//        if (nt->match_name(sname)) {
//            typeNav = nt->type;
//            break;
//        }
//    }
//    if (isGeneric(typeNav)) {
//        TypeNav tp = TypeNav::Other;
//        if (!stype.empty()) {
//            for (auto nt: ALL_NAV_TYPES) {
//                if (nt->match_type(stype)) {
//                    if (nt->type == dock->type || dock->type == TypeNav::Other) {
//                        typeNav = nt->type;
//                        break;
//                    }
//                    tp = nt->type;
//                }
//            }
//            if (tp != TypeNav::Other) {
//                typeNav = tp;
//            }
//        }
////        else if (dock->type == TypeNav::Other || dock->type == TypeNav::AnySite) {
////            if (ge->event=="ApproachSettlement") {
////                if (je["StationServices"].as_array_or().empty())
////                    typeNav = TypeNav::PlanetaryInstallation;
////                else
////                    typeNav = TypeNav::PlanetaryThing;
////            }
////            else
////                typeNav = TypeNav::SpaceStation;
////        }
////        else if (dock->type == TypeNav::PlanetaryThing && ge->event=="ApproachSettlement") {
////            if (je["StationServices"].as_array_or().empty())
////                typeNav = TypeNav::PlanetaryInstallation;
////        }
//    }
//    if (typeNav == TypeNav::PlanetaryPort || typeNav == TypeNav::PlanetaryThing) {
//        if (je["StationGovernment"].is_string() && je["StationGovernment"].as_string() == "$government_Engineer;")
//            typeNav = TypeNav::EngineerPort;
//    }
//    dock = checkTypeAndName(dock, typeNav, sname, snloc, ge->timestamp);
////    if (dock->setTypeAndName(typeNav, sname, snloc)) {
////        needBlobSave = true;
////        dock->updated = ge->timestamp;
////    }
//    if (stationBodyId.has_value()) {
//        if (dock->bodyId != stationBodyId) {
//            dock->bodyId = stationBodyId;
//            dock->updated = ge->timestamp;
//            needBlobSave = true;
//        }
//    }
//    if (!dock->parentBodyId.has_value()) {
//        if (st::space.bodyType == "Planet" || st::space.bodyType == "Star") {
//            dock->parentBodyId = st::space.bodyId;
//            dock->updated = ge->timestamp;
//            needBlobSave = true;
//        }
//    }
//    if (!dock->marketId() && marketId) {
//        dock->setMarketId(marketId);
//        dock->updated = ge->timestamp;
//        needBlobSave = true;
//    }
//    save();
//    return dock;
//}

spEntity StarSystem::addEntity(spEntity entity) {
    if (!systemAddress) {
        LOG_ERROR("dummy StarSystem::addEntity()");
        return {};
    }
    if (!entity)
        return entity;
    EntitiesLocker lock(this);

    // check for duplicated unique entities
    if (entity->type == TypeNav::NavBeacon) {
        for (auto& item : entities) {
            if (item && item->type == TypeNav::NavBeacon) {
                LOG_WARNING("Attempt to add second NavBeacon");
                return item;
            }
        }
    }
    if (entity->type == TypeNav::AnonMegaship) {
        for (auto& item : entities) {
            if (item && item->type == TypeNav::AnonMegaship && item->name == entity->name) {
                LOG_WARNING("Attempt to add second '{}'", entity->name);
                return item;
            }
        }
    }
    // do not add generic signals
    //if (isSignal(entity->type)) {
    //    LOG_WARNING("Attempt to add signal: {}", enum_name(entity->type));
    //    return entity;
    //}
    for (auto &e: this->entities) {
        if (e && e.get() == entity.get())
            return entity;
    }
    bool added = false;
    for (auto &e: this->entities) {
        if (e && e.get() == nullptr) {
            e = entity;
            added = true;
            break;
        }
    }
    if (!added)
        entities.push_back(entity);
    if (!isSignal(entity->type) && !isMegaship(entity->type))
        needBlobSave = true;
    return entity;
}

//
//
//Other
//      -> Signals:
//        Error, NotExplored, StarSystem, Signal, WarZone
//          -> USS
//        ResSite, NavBeacon, TouristBeacon, Codex
//      -> Body:
//          Barycenter, Ring, AsteroidCluster
//          -> Star
//          -> Planet
//      -> AnySite
//          SpaceStation,
//          Orbis, Ocellus, Dodec, Coriolis, AsteroidBase, SpaceOutpost, SpaceInstallation, SpaceConstrDepot
//          -> Megaship
//              StationMegaShip, FleetCarrier, SquadronCarrier, StrongholdCarrier, ColonisationShip
//          PlanetaryThing, PlanetaryPort, EngineerPort, Settlement, PlanetaryInstallation, PlanetaryConstrDepot
//
// Safe upgrade/dopwngrade:
// Other <-> any type
//  Signal <-> WarZone, ResSite, NavBeacon, TouristBeacon, Codex
//    USS - cannot change
//  Body -> Barycenter, Ring, AsteroidCluster
//       -> Star
//       -> Planet
//  AnySite -> SpaceStation
//      SpaceStation -> Orbis, Ocellus, Dodec, Coriolis, AsteroidBase, SpaceOutpost, SpaceInstallation, SpaceConstrDepot
//  AnySite -> Megaship
//      Megaship -> StationMegaShip, FleetCarrier, SquadronCarrier, StrongholdCarrier, ColonisationShip
//  AnySite -> PlanetaryThing
//      PlanetaryThing -> PlanetaryPort, EngineerPort, Settlement, PlanetaryInstallation, PlanetaryConstrDepot

//spEntity StarSystem::convertEntity(spEntity entity, TypeNav to_type) {
//    if (!entity)
//        return {};
//
//    TypeNav from_type = entity->type;
//    if (from_type == to_type)
//        return entity;
//
//    if (isSignal(to_type)) {
//        if (from_type == TypeNav::USS)
//            return {};
//        if (isSignal(to_type)) {
//            const_cast<TypeNav&>(entity->type) = to_type;
//            // TODO: remove from entities depending on type
//            return entity;
//        } else {
//            return replaceEntity(entity, makeSignalEntity(to_type, entity->name, entity->nloc));
//        }
//    }
//
//}

spEntity StarSystem::addEntity(TypeNav tp, std::string_view name, std::string_view nloc, entity_args args) {
    if (isGeneric(tp) && !nloc.empty())
        tp = NavType::guessTypeFromName(tp, nloc);
    if (isGeneric(tp) && !name.empty())
        tp = NavType::guessTypeFromName(tp, name);

    if (isSignal(tp)) {
        if (tp == TypeNav::USS) {
            LOG_ERROR("Use addSignalUSS instead of addEntity");
            return makeSignalUSS("", name, "", nloc);
        }
        return addEntity(makeSignalEntity(tp, name, nloc));
    }
    if (isBody(tp)) {
        if (!nloc.empty())
            LOG_ERROR("BodyEntity '{}' created with localized name '{}'", name, nloc);
        if (tp == TypeNav::Star) {
            if (name.empty())
                LOG_ERROR("StarEntity created with empty name");
            return addEntity(spEntity{new StarEntity(*name, args.body_id)});
        }
        if (tp == TypeNav::Planet) {
            if (name.empty())
                LOG_ERROR("PlanetEntity created with empty name");
            return addEntity(spEntity{new PlanetEntity(*name, args.body_id)});
        }
        return addEntity(spEntity{new BodyEntity(tp, *name, args.body_id)});
    }

    assert (isAnySite(tp));
    TypeNav tp_guess = NavType::guessTypeFromName(tp, name);
    if (isAnySite(tp_guess) && tp_guess != tp)
        tp = tp_guess;

    if (isMegaship(tp)) {
        // megaships are automatically added to this system or moved from another system,
        // and this is stored in database, so we cannot separately create and then add megaships
        spEntity megaship = addMegashipEntity(tp, name, nloc, args.market_id, args.ts);
        return megaship;
    }

    if (isAnySite(tp)) {
        if (name.empty())
            LOG_ERROR("StationEntity created with empty name");
        return addEntity(makeStationEntity(tp, name, nloc, args.market_id, args.ts));
    }

    LOG_ERROR("Unknown system entity type '{}' with name '{}'", enum_name(tp), name);
    // do not add it, since it's wrong
    return makeSignalEntity(tp, name, nloc);
}

spEntity StarSystem::makeStationEntity(TypeNav tp, std::string_view nm, std::string_view nl, int64_t marketId, Timestamp ts) {
    assert (isAnySite(tp) && !isMegaship(tp));

    if (nm.find('$') != std::string::npos) {
        std::string xx_name, ru_name;
        TypeNav tn = GameVar::replace_in(nm, xx_name, ru_name);
        if (tn != tp) {
            LOG_ERROR("Creating station '{}' with wrong type {}, must be {}", nm, enum_name(tp), enum_name(tn));
        }
        spEntity station(new StationEntity(tp, *nm, xx_name, ru_name, marketId));
        station->updated = ts;
        return station;
    }

    for (auto nt: ALL_NAV_TYPES) {
        if (!nt->name_loc.empty()) {
            for (auto &p: nt->name_loc) {
                if (p.match(nm) || p.match(nl)) {
                    if (p.type != tp) {
                        LOG_ERROR("Creating station '{}'/'{}' with wrong type {}, must be {}", nm, nl, enum_name(tp), enum_name(p.type));
                    }
                    std::string var;
                    if (!p.var.empty())
                        var = "$" + p.var + ";";
                    std::string eng = *p.get_eng();
                    std::string loc = *p.get_loc();
                    spEntity station(new StationEntity(tp, var, eng, loc, marketId));
                    station->updated = ts;
                    return station;
                }
            }
        }
    }

    spEntity station;
    if (!nm.empty() && nl.empty()) {
        station.reset(new StationEntity(tp, *nm, marketId));
        station->updated = ts;
        return station;
    }

    LOG_ERROR("Creating station '{}'/'{}' of type {} with unknown localization", nm, nl, enum_name(tp));
    if (nl.empty()) {
        bool ascii = true;
        for (auto ch : toUtf16(nm)) {
            if (ch > 255)
                ascii = false;
        }
        if (ascii)
            station.reset(new StationEntity(tp, "", *nm, "", marketId));
        else
            station.reset(new StationEntity(tp, "", "", *nm, marketId));
    } else {
        station.reset(new StationEntity(tp, *nm, marketId));
    }
    station->updated = ts;
    return station;
}

spEntity StarSystem::makeSignalEntity(TypeNav tp, std::string_view name, std::string_view nloc) {
    assert (isSignal(tp));
    if (!isSignal(tp)) {
        LOG_ERROR("SignalEntity '{}'/'{}' created with non-signal type: {}", name, nloc, enum_name(tp));
        return nullptr;
    }

    if (name.find('$') != std::string::npos) {
        std::string xx_name, ru_name;
        TypeNav tn = GameVar::replace_in(name, xx_name, ru_name);
        if (tn != tp) {
            LOG_ERROR("Creating signal '{}' with wrong type {}, must be {}", name, enum_name(tp), enum_name(tn));
        }
        if (!nloc.empty())
            ru_name = nloc;
        spEntity entity(new SignalEntity(tp, *name, *xx_name, *ru_name));
        return entity;
    }

    for (auto nt: ALL_NAV_TYPES) {
        if (!nt->name_loc.empty()) {
            for (auto &p: nt->name_loc) {
                if (p.match(name) || p.match(nloc)) {
                    if (p.type != tp) {
                        LOG_ERROR("Creating signal '{}'/'{}' with wrong type {}, must be {}", name, nloc, enum_name(tp), enum_name(p.type));
                    }
                    std::string var;
                    if (!p.var.empty())
                        var = "$" + p.var + ";";
                    std::string eng = *p.get_eng();
                    std::string loc = *p.get_loc();
                    spEntity entity(new SignalEntity(tp, var, eng, loc));
                    return entity;
                }
            }
        }

    }

    if (tp != TypeNav::TouristBeacon && tp != TypeNav::Titan
        && (st::client.majorVersion * 100 + st::client.minorVersion) >= 401)
        LOG_ERROR("Creating signal '{}'/'{}' with unknown localization", name, nloc, enum_name(tp));
    spEntity entity;
    if (nloc.empty()) {
        bool ascii = true;
        for (auto ch : toUtf16(name)) {
            if (ch > 255)
                ascii = false;
        }
        if (ascii)
            entity.reset(new SignalEntity(tp, "", *name, *nloc));
        else
            entity.reset(new SignalEntity(tp, "", "", *name));
    } else {
        entity.reset(new SignalEntity(tp, "", *name, *nloc));
    }
    return entity;
}

spEntity StarSystem::makeSignalUSS(std::string_view sname, std::string_view snloc, std::string_view uss_type, std::string_view uss_tloc) {
    std::string name;
    std::string nloc;

    {
        if (snloc.empty())
            snloc = sname;
        std::string xx_name;
        std::string ru_name;
        if (!snloc.empty())
            NavType::localizeName(TypeNav::USS, snloc, xx_name, ru_name);
        else
            NavType::localizeName(TypeNav::USS, sname, xx_name, ru_name);
        if (!ru_name.empty())
            name = *ru_name;
        else
            name = *xx_name;
    }
    {
        std::string xx_nloc;
        std::string ru_nloc;
        if (!uss_tloc.empty())
            NavType::localizeName(TypeNav::USS, uss_tloc, xx_nloc, ru_nloc);
        else
            NavType::localizeName(TypeNav::USS, uss_type, xx_nloc, ru_nloc);
        if (!ru_nloc.empty())
            nloc = *ru_nloc;
        else
            nloc = *xx_nloc;
    }

    spEntity entity(new SignalUUSEntity(name, nloc));
    return entity;
}

spEntity StarSystem::addMegashipEntity(TypeNav tp, std::string_view name, std::string_view nloc, int64_t marketId, Timestamp ts) {
    spEntity megaship;
    assert (isMegaship(tp));

    std::string var_name, xx_name, ru_name;
    if (name.starts_with("$")) {
        var_name = name;
        TypeNav tp_guess = NavType::localizeName(tp, name, xx_name, ru_name);
        if (tp_guess != TypeNav::Other && tp_guess != tp)
            tp = tp_guess;
        if (nloc.empty() && st::lng == Lang::RU) nloc = ru_name;
        name = xx_name;
    }

    db::MegashipJS db_ms {};
    if (isCarrier(tp)) {
        if (name.empty())
            LOG_ERROR("MegashipEntity created with empty name");
        if (nloc.empty())
            nloc = name;
        if (tp != NavType::checkCarrierName(nloc, &db_ms.code, &db_ms.name)) {
            LOG_ERROR("Carrier created with bad name '{}'", nloc);
            return {};
        }
        name = db_ms.code;
        nloc = db_ms.name;
        db_ms.marketId = marketId;
    }
    else if (isAnonMegaship(tp)) {
        NavType::localizeName(tp, name, xx_name, ru_name);
        db_ms.name = xx_name;
        db_ms.anon = true;
        if (nloc.empty() && st::lng == Lang::RU) nloc = ru_name;
        name = xx_name;
        marketId = 0;
    }
    else if (name == "Trailblazer Dream" ||
             name == "System Colonisation Ship" ) {
        db_ms.name = name;
        db_ms.anon = true;
        marketId = 0;
    }
    else {
        db_ms.name = name;
        db_ms.marketId = marketId;
    }
    if (db::loadMegaShip(db_ms, true)) {
        if (db_ms.systemId != systemAddress) {
            db_ms.updated = std::max(db_ms.updated, ts);
            megaship = moveMegashipHere(db_ms, body_id_t());
            if (megaship)
                return megaship;
        } else {
            // prevent recursive addMegashipEntity->update_megaship->addEntity->addMegashipEntity
            if (db_ms.code.empty() || db_ms.name == db_ms.code)
                megaship = addEntity(spEntity(new MegashipEntity(tp, var_name, db_ms.name, "", marketId)));
            else
                megaship = addEntity(spEntity(new MegashipEntity(tp, var_name, db_ms.code, db_ms.name, marketId)));
            db_ms.updated = std::max(db_ms.updated, ts);
            update_megaship(this, db_ms, megaship);
            db::saveMegaShip(db_ms);
            return megaship;
        }
    }
    if (nloc == name)
        nloc = "";
    megaship.reset(new MegashipEntity(tp, var_name, *name, *nloc, marketId));
    megaship->updated = ts;
    addEntity(megaship);
    gal::fill_megaship_for_db(this, megaship.get(), db_ms);
    db::saveMegaShip(db_ms);
    return megaship;
}

Entity::Entity(TypeNav tp, std::string nm)
    : type(tp)
    , name(std::move(nm))
{
    if (isBody(tp))
        LOG_ERROR("Body entity of type '{}' must be added with another constructor", enum_name(tp));
    if (type == TypeNav::Barycenter || type == TypeNav::Ring) {
        if (!name.empty())
            LOG_WARNING("Non-empty name '{}' for type: {}", name, enum_name(type));
    }
    else if (name.empty() && !isSignal(type))
        LOG_WARNING("Empty name for type: {}", enum_name(type));
}

Entity::Entity(TypeNav tp, std::string nm, body_id_t bId)
        : type(tp)
        , name(std::move(nm))
        , bodyId(std::move(bId))
{
    if (!isBody(tp))
        LOG_ERROR("Entity of type '{}' must be added with another constructor", enum_name(tp));
    if (type == TypeNav::Barycenter || type == TypeNav::Ring) {
        if (!name.empty())
            LOG_WARNING("Non-empty name '{}' for type: {}", name, enum_name(type));
    }
    else if (name.empty())
        LOG_WARNING("Empty name for type: {}", enum_name(type));
    if (!bodyId.has_value())
        LOG_WARNING("Body '{}' of type {} added without body id", name, enum_name(type));
}

bool Entity::nameEq(std::string_view nm) const {
    if (nm.empty())
        return false;
    return name == nm;
}

//bool Entity::setName(std::string_view sname, std::string_view snloc) {
//    if (name == sname && nloc == snloc)
//        return false;
//
//    std::string xx_name;
//    std::string ru_name;
//    NavType* navType = nullptr;
//    switch (type) {
//    case TypeNav::Barycenter:
//        LOG_ERROR("Cannot set name for TypeNav::Barycenter");
//        return false;
//    case TypeNav::Ring:
//        LOG_ERROR("Cannot set name for TypeNav::Ring");
//        return false;
//    case TypeNav::AsteroidCluster:
//        if (!sname.contains("Belt Cluster")) {
//            LOG_ERROR("Bad name for TypeNav::AsteroidCluster: '{}'", sname);
//            return false;
//        }
//        if (!snloc.empty()) {
//            LOG_ERROR("Bad localized name for TypeNav::AsteroidCluster: '{}'", snloc);
//        }
//        const_cast<std::string&>(name) = *sname;
//        const_cast<std::string&>(nloc) = "";
//        return false;
//    case TypeNav::Star:
//    case TypeNav::Planet:
//        if (!snloc.empty()) {
//            LOG_ERROR("Bad localized name for TypeNav::{}: '{}'", enum_name(type), snloc);
//        }
//        const_cast<std::string&>(name) = *sname;
//        const_cast<std::string&>(nloc) = "";
//        return false;
//    case TypeNav::Other:
//    case TypeNav::Error:
//    case TypeNav::Signal:
//    case TypeNav::USS:
//    case TypeNav::Codex:
//    case TypeNav::StarSystem:
//    case TypeNav::Body:
//    case TypeNav::TouristBeacon:
//    case TypeNav::AnySite:
//    case TypeNav::SpaceStation:
//    case TypeNav::Orbis:
//    case TypeNav::Ocellus:
//    case TypeNav::Dodec:
//    case TypeNav::Coriolis:
//    case TypeNav::AsteroidBase:
//    case TypeNav::SpaceOutpost:
//    case TypeNav::SpaceInstallation:
//    case TypeNav::SpaceConstrDepot:
//    case TypeNav::PlanetaryThing:
//    case TypeNav::PlanetaryPort:
//    case TypeNav::EngineerPort:
//    case TypeNav::Settlement:
//    case TypeNav::PlanetaryInstallation:
//    case TypeNav::PlanetaryConstrDepot:
//        const_cast<std::string&>(name) = sname;
//        const_cast<std::string&>(nloc) = snloc;
//        return true;
//    case TypeNav::Megaship:
//    case TypeNav::StationMegaShip:
//    case TypeNav::FleetCarrier:
//    case TypeNav::SquadronCarrier:
//    case TypeNav::ColonisationShip:
//    case TypeNav::AnonMegaship:
//        // handled on overriden method
//        assert(false);
//        return false;
//
//    case TypeNav::NotExplored:
//        navType = &UNEXPLORED;
//        break;
//    case TypeNav::WarZone:
//        navType = &WAR_ZONE;
//        break;
//    case TypeNav::ResSite:
//        navType = &RES_SITE;
//        break;
//    case TypeNav::NavBeacon:
//        navType = &BEACON;
//        break;
//    }
//
//    for (auto& p : navType->name_loc) {
//        if (p.match(sname)) {
//            p.localizeName(sname, xx_name, ru_name);
//            if (name == xx_name && nloc == ru_name)
//                return false;
//            if (!snloc.empty() && snloc != ru_name) {
//                if (!(st::lng == Lang::EN && snloc == xx_name && ru_name.empty()))
//                    LOG_ERROR("Cannot set localized name '{}' to entity type {}", snloc, enum_name(type));
//            }
//            const_cast<std::string&>(name) = xx_name;
//            const_cast<std::string&>(nloc) = ru_name;
//            return true;
//        }
//    }
//    LOG_ERROR("Cannot set name '{}'/'{}' to entity type {}", sname, snloc, enum_name(type));
//    return false;
//}


SignalEntity::SignalEntity(TypeNav tp, std::string var, std::string nm, std::string nloc)
    : Entity(tp, std::move(nm))
{
    if (!isSignal(type))
        LOG_ERROR("SignalEntity '{}'/'{}'/'{}' created with non-signal type: {}", var, name, nloc, enum_name(type));
    if (name.empty() && nloc.empty())
        LOG_WARNING("Empty signal name and localised name for: '{}' type {}", var, enum_name(type));
    signal_ext.unexpanded_name = js::symbol(var);
    signal_ext.localised_name = js::symbol(nloc);
    if (!signal_ext.localised_name.empty())
        signal_ext.localised = true;
}

bool SignalEntity::nameEq(std::string_view nm) const {
    if (!signal_ext.localised) {
        if (!signal_ext.unexpanded_name.empty()) {
            std::string eng, loc;
            NavType::localizeName(type, signal_ext.unexpanded_name.sv(), eng, loc);
            const_cast<js::symbol&>(signal_ext.localised_name) = js::symbol(loc);
            const_cast<bool&>(signal_ext.localised) = true;
        } else {
            for (auto nt: ALL_NAV_TYPES) {
                for (auto &p: nt->name_loc) {
                    if (p.match(name)) {
                        if (!p.var.empty()) {
                            std::string var = "$" + p.var + ";";
                            const_cast<js::symbol&>(signal_ext.localised_name) = js::symbol(var);
                        }
                        const_cast<js::symbol&>(signal_ext.localised_name) = js::symbol(*p.get_loc());
                        const_cast<bool&>(signal_ext.localised) = true;
                        goto localized;
                    }
                }
            }
        }
    }
localized:
    return name == nm || signal_ext.localised_name == nm || signal_ext.unexpanded_name == nm;
}

bool SignalEntity::setSignalName(spEntity& entity, std::string_view nm, std::string_view nl) {
    TypeNav type = entity->type;
    if (type == TypeNav::USS)
        return false;
    assert (isSignal(type));
    SignalEntity* signal = (SignalEntity*)entity.get();

    std::string var_name, xx_name, ru_name;
    if (nm.starts_with("$")) {
        var_name = nm;
        TypeNav tp_guess = NavType::localizeName(type, nm, xx_name, ru_name);
        if (tp_guess != TypeNav::Other && tp_guess != type)
            type = tp_guess;
        if (nl.empty() && st::lng == Lang::RU) nl = ru_name;
        nm = xx_name;
    }
    if (xx_name.empty()) {
        for (auto nt: ALL_NAV_TYPES) {
            for (auto& p : nt->name_loc) {
                if (p.match(nm) || p.match(nl)) {
                    var_name = "$" + p.var + ";";
                    xx_name = p.get_eng();
                    if (xx_name.empty())
                        xx_name = var_name;
                    if (nl.empty())
                        ru_name = p.get_loc();
                    else
                        ru_name = nl;
                    goto localized;
                }
            }
        }
    }

    localized:
    if (signal->name == nm && signal->signal_ext.unexpanded_name == var_name && signal->signal_ext.localised_name == nl)
        return false;

    if (signal->name != xx_name)
        const_cast<std::string &>(signal->name) = xx_name;
    if (signal->signal_ext.localised_name != ru_name) {
        const_cast<js::symbol&>(signal->signal_ext.localised_name) = js::symbol(ru_name);
        const_cast<bool&>(signal->signal_ext.localised) = true;
    }
    return true;
}

bool BodyEntity::setBodyName(spEntity& entity, std::string_view nm) {
    TypeNav type = entity->type;
    assert (isBody(type));
    BodyEntity* body = (BodyEntity*)entity.get();
    if (body->name == nm)
        return false;
    if (type == TypeNav::Barycenter || type == TypeNav::Ring) {
        if (!nm.empty())
            LOG_ERROR("Attempt to set name '{}' for body type {}", nm, enum_name(type));
        return false;
    }
    if (nm.empty()) {
        LOG_ERROR("Set empty name for body type {}", enum_name(type));
        return false;
    }
    if (body->name[0] == '\t' && (type == TypeNav::Star || type == TypeNav::Planet)) {
        const_cast<std::string&>(body->name) = *nm;
        return true;
    }
    LOG_ERROR("Change name from '{}' to '{}' for body type {}", body->name, nm, enum_name(type));
    const_cast<std::string&>(body->name) = *nm;
    return true;
}

StationEntity::StationEntity(TypeNav tp, std::string nm, int64_t market_id)
        : Entity(tp, std::move(nm))
{
    station_ext.localised = true;
}

StationEntity::StationEntity(TypeNav tp, std::string var, std::string nm, std::string nl, int64_t market_id)
        : Entity(tp, std::move(nm))
{
    station_ext.unexpanded_name = js::symbol(var);
    station_ext.localised_name = js::symbol(nl);
}

bool StationEntity::setMarketId(int64_t marketId) {
    station_ext.marketId = marketId;
    return true;
}

bool StationEntity::nameEq(std::string_view nm) const {
    if (!station_ext.localised) {
        if (!station_ext.unexpanded_name.empty()) {
            std::string eng, loc;
            NavType::localizeName(type, station_ext.unexpanded_name.sv(), eng, loc);
            const_cast<js::symbol&>(station_ext.localised_name) = js::symbol(loc);
            const_cast<bool&>(station_ext.localised) = true;
        } else {
            for (auto nt: ALL_NAV_TYPES) {
                for (auto &p: nt->name_loc) {
                    if (p.match(name)) {
                        if (!p.var.empty()) {
                            std::string var = "$" + p.var + ";";
                            const_cast<js::symbol&>(station_ext.localised_name) = js::symbol(var);
                        }
                        const_cast<js::symbol&>(station_ext.localised_name) = js::symbol(*p.get_loc());
                        const_cast<bool&>(station_ext.localised) = true;
                        goto localized;
                    }
                }
            }
        }
    }
localized:
    return name == nm || station_ext.localised_name == nm || station_ext.unexpanded_name == nm;
}

bool StationEntity::setStationName(spEntity& entity, std::string_view nm, std::string_view nl) {
    TypeNav type = entity->type;
    assert (isAnySite(type) && !isMegaship(type));
    StationEntity* station = (StationEntity*)entity.get();

    std::string var_name, xx_name, ru_name;
    if (nm.starts_with("$")) {
        var_name = nm;
        TypeNav tp_guess = NavType::localizeName(type, nm, xx_name, ru_name);
        if (tp_guess != TypeNav::Other && tp_guess != type)
            type = tp_guess;
        nm = xx_name;
        if (nl.empty() && st::lng == Lang::RU)
            nl = ru_name;
    }
    if (nm.empty()) {
        for (auto nt: ALL_NAV_TYPES) {
            for (auto& p : nt->name_loc) {
                if (p.match(nm) || p.match(nl)) {
                    var_name = "$" + p.var + ";";
                    nm = p.get_eng();
                    if (nm.empty())
                        nm = var_name;
                    if (nl.empty() && st::lng == Lang::RU)
                        nl = p.get_loc();
                    goto localized;
                }
            }
        }
    }

    localized:

    if (nm.empty()) {
        LOG_ERROR("Empty station name");
        return false;
    }

    if (station->name == nm && station->station_ext.unexpanded_name == var_name && station->station_ext.localised_name == nl)
        return false;

    if (station->name != nm)
        const_cast<std::string &>(station->name) = nm;
    if (station->station_ext.localised_name != nl) {
        const_cast<js::symbol&>(station->station_ext.localised_name) = js::symbol(nl);
        const_cast<bool&>(station->station_ext.localised) = true;
    }
    return true;
}


MegashipEntity::MegashipEntity(TypeNav tp, std::string var, std::string nm, std::string nl, int64_t market_id)
    : StationEntity(tp, std::move(var), std::move(nm), std::move(nl), isAnonMegaship(type) ? 0 : market_id)
{
    if (tp == TypeNav::FleetCarrier) {
        assert (var.empty());
        assert (name.size() == 7);
        auto full = station_ext.localised_name;
        assert (full.empty() || (full.size() > 8 && full.sv().ends_with(name)));
    }
    else if (tp == TypeNav::SquadronCarrier) {
        assert (var.empty());
        assert (name.size() == 4);
        auto full = station_ext.localised_name;
        assert (full.empty() || (full.size() > 7 && full.sv().ends_with(" | " + name)));
    }
    else if (isAnonMegaship(tp)) {
        assert (!name.empty() && tp == NavType::guessTypeFromName(TypeNav::Other,name));
        assert (station_ext.localised_name.empty() || tp == NavType::guessTypeFromName(TypeNav::Other,station_ext.localised_name));
    }
    else if (tp == TypeNav::ColonisationShip || name == "System Colonisation Ship") {
        assert (name.starts_with("System Colonisation Ship"));
        assert (station_ext.localised_name.empty() || station_ext.localised_name.sv().starts_with("Колонизационный корабль"));
    }
    else {
        assert (!name.empty());
        assert (station_ext.unexpanded_name.empty());
        assert (station_ext.localised_name.empty());
    }
}

bool MegashipEntity::setMarketId(int64_t marketId) {
    if (isAnonMegaship(type))
        return false;
    station_ext.marketId = marketId;
    return true;
}

bool MegashipEntity::setCarrierName(spEntity& entity, std::string_view code, std::string_view full) {
    TypeNav type = entity->type;
    assert (isCarrier(type));
    MegashipEntity* carrier = (MegashipEntity*)entity.get();
    if (carrier->name == code && carrier->station_ext.localised_name == full)
        return false;

    if (type == TypeNav::FleetCarrier) {
        if (code.empty() && full.empty())
            return false;
        if (code.empty()) {
            code = full;
            full = "";
        }
        if (NavType::checkCarrierName(code, nullptr, nullptr) != TypeNav::FleetCarrier) {
            LOG_ERROR("Set wrong fleet carrier name as: '{}'", code);
            return false;
        }
        if (!full.empty() && !full.ends_with(code)) {
            LOG_ERROR("Set wrong fleet carrier name as: '{}'", full);
            return false;
        }
        if (!full.empty() && NavType::checkCarrierName(full, nullptr, nullptr) != TypeNav::FleetCarrier) {
            LOG_ERROR("Set wrong fleet carrier name as: {}", full);
            return false;
        }
        if (code.size() != 7) {
            if (full.empty())
                full = code;
            code = full.substr(full.size()-7);
        }
    }
    else if (type == TypeNav::SquadronCarrier) {
        if (code.empty() && full.empty())
            return false;
        if (code.empty()) {
            code = full;
            full = "";
        }
        if (NavType::checkCarrierName(code, nullptr, nullptr) != TypeNav::SquadronCarrier) {
            LOG_ERROR("Set wrong squadron carrier name as: '{}'", code);
            return false;
        }
        if (!full.empty() && !full.ends_with(code)) {
            LOG_ERROR("Set wrong squadron carrier name as: '{}'", full);
            return false;
        }
        if (!full.empty() && NavType::checkCarrierName(full, nullptr, nullptr) != TypeNav::SquadronCarrier) {
            LOG_ERROR("Set wrong squadron carrier name as: {}", full);
            return false;
        }
        if (code.size() != 4) {
            if (full.empty())
                full = code;
            code = full.substr(full.size()-4);
        }

    }
    else
        return false;

    bool changed = false;
    if (carrier->name.empty()) {
        const_cast<std::string&>(carrier->name) = *code;
        changed = true;
    }
    if (!full.empty() && carrier->station_ext.localised_name != full) {
        const_cast<js::symbol&>(carrier->station_ext.localised_name) = js::symbol(*full);
        const_cast<bool&>(carrier->station_ext.localised) = true;
        changed = true;
    }
    return changed;
}


bool MegashipEntity::setMegashipName(spEntity& entity, std::string_view nm, std::string_view nl) {
    TypeNav type = entity->type;
    assert (isMegaship(type) && !isCarrier(type));
    MegashipEntity* megaship = (MegashipEntity*)entity.get();

    std::string var_name, xx_name, ru_name;
    if (nm.starts_with("$")) {
        var_name = nm;
        TypeNav tp_guess = NavType::localizeName(type, nm, xx_name, ru_name);
        if (tp_guess != TypeNav::Other && tp_guess != type)
            type = tp_guess;
        nm = xx_name;
        if (nl.empty())
            nl = ru_name;
    }
    if (xx_name.empty()) {
        for (auto nt: ALL_NAV_TYPES) {
            for (auto& p : nt->name_loc) {
                if (p.match(nm) || p.match(nl)) {
                    if (!p.var.empty())
                        var_name = "$" + p.var + ";";
                    if (type == TypeNav::ColonisationShip) {
                        xx_name = p.get_eng();
                    } else {
                        xx_name = p.get_eng();
                        if (xx_name.empty())
                            xx_name = var_name;
                    }
                    if (nl.empty())
                        nl = nm;
                    nm = xx_name;
                    if (nl.empty())
                        nl = p.get_loc();
                    goto localized;
                }
            }
        }
    }

localized:
    if (nl == nm)
        nl = "";
    if (megaship->name == nm && megaship->station_ext.unexpanded_name == var_name && megaship->station_ext.localised_name == nl)
        return false;

    if (type == TypeNav::Megaship || type == TypeNav::StationMegaShip) {
        if (!nl.empty()) {
            LOG_ERROR("TypeNav::Megaship and TypeNav::StationMegaShip cannot have localized name: '{}'", nl);
            nl = {};
        }
        if (megaship->name == nm)
            return false;
        const_cast<std::string&>(megaship->name) = *nm;
        return true;
    }

    if (megaship->name != nm)
        const_cast<std::string &>(megaship->name) = nm;
    if (!var_name.empty() && megaship->station_ext.unexpanded_name != var_name)
        const_cast<js::symbol&>(megaship->station_ext.unexpanded_name) = js::symbol(var_name);
    if (megaship->station_ext.localised_name != nl) {
        const_cast<js::symbol&>(megaship->station_ext.localised_name) = js::symbol(nl);
        const_cast<bool&>(megaship->station_ext.localised) = true;
    }
    return true;
}

bool MegashipEntity::nameEq(std::string_view nm) const {
    if (isCarrier(type)) {
        return name == nm || station_ext.localised_name == nm;
    }
    return StationEntity::nameEq(nm);
}


} // namespace gal