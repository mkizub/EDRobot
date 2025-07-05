//
// Created by mkizub on 25.06.2025.
//

#include "pch.h"
#include "Configuration.h"


static inline bool parseTimestampString(const std::string& str, Timestamp& timestamp) {
    std::istringstream iss(str);
    iss >> std::chrono::parse("%Y-%m-%dT%H:%M:%SZ", timestamp);
    if (iss.fail()) {
        LOG(ERROR) << "Timestamp parse failed, event corrupted?";
        return false;
    }
    return true;
}

static inline bool parseTimestamp(const json::value& value, Timestamp& timestamp) {
    if (value.is_string())
        return parseTimestampString(value.as_string(), timestamp);
    if (!value.is_object() || !value.contains("timestamp"))
        return false;
    return parseTimestampString(value.at("timestamp").as_string(), timestamp);
}

GameEvent::GameEvent(json::value&& j) : data(std::move(j.as_object())) {
    if (!data.contains("event") || !data.contains("timestamp"))
        return;
    if (parseTimestamp(data, timestamp))
        event = data["event"].as_string();
}

spGameEvent Configuration::parseEvent(const std::string& line) {
    spGameEvent gameEvent;
    {
        std::string error;
        auto res = json::parse5(line, &error);
        if (!res.has_value()) {
            LOG(ERROR) << "Error parsing journal file: " << error;
            return {};
        }
        gameEvent.reset(new GameEvent(std::move(res.value())));
        if (gameEvent->event.empty())
            return {};
    }

    auto& event = gameEvent->event;
    LOG(DEBUG) << "Journal event: " << event;

    if (event == "Fileheader" || event == "LoadGame")
        parseEvent_LoadGame(gameEvent);
    else if (event == "Commander")
        parseEvent_Commander(gameEvent);
    else if (event == "CarrierLocation")
        parseEvent_CarrierLocation(gameEvent);
    else if (event == "Location")
        parseEvent_Location(gameEvent);
    else if (event == "Loadout")
        parseEvent_Loadout(gameEvent);
    else if (event == "Cargo")
        parseEvent_Cargo(gameEvent);
    else if (event == "ShipyardSwap")
        parseEvent_ShipyardSwap(gameEvent);
    else if (event == "Docked")
        parseEvent_Docked(gameEvent);
    else if (event == "Undocked" || event == "Liftoff")
        parseEvent_Undocked(gameEvent);
    else if (event.starts_with("Docking"))
        parseEvent_Docking(gameEvent);

    return gameEvent;
}

void Configuration::parseEvent_Commander(spGameEvent& ge) {
    auto& je = ge->data;
    if (je.contains("Name")) {
        mCmdrName = je["Name"].as_string();
    }
}

void Configuration::parseEvent_LoadGame(spGameEvent& ge) {
    auto& je = ge->data;
    if (je.contains("Commander"))
        mCmdrName = je["Commander"].as_string();

    if (je.contains("Odyssey"))
        const_cast<bool&>(this->isOdyssey) = je["Odyssey"].as_boolean();
    else
        const_cast<bool&>(this->isOdyssey) = false;

    if (je.contains("language")) {
        auto gameLang = je["language"].as_string();
        if (gameLang == "Russian/RU" || gameLang.ends_with("/RU"))
            const_cast<Lang &>(this->lng) = RU;
        else if (gameLang == "English/EN" || gameLang.ends_with("/EN"))
            const_cast<Lang &>(this->lng) = EN;
        else if (gameLang == "English/UK" || gameLang.ends_with("/UK"))
            const_cast<Lang &>(this->lng) = EN;
        else {
            LOG(ERROR) << "Unsupported game language: " << gameLang;
            const_cast<Lang &>(this->lng) = XX;
        }
    }

    if (je.contains("Ship"))
        mShipType = je["Ship"].as_string();
    if (je.contains("Ship_Localised"))
        mShipTypeLocalized = je["Ship_Localised"].as_string();
    if (je.contains("ShipName"))
        mShipUserName = je["ShipName"].as_string();
}

void Configuration::parseEvent_CarrierLocation(spGameEvent& ge) {
}

void Configuration::parseEvent_Location(spGameEvent& ge) {
    auto& je = ge->data;

    if (je.contains("Docked") && je["Docked"].as_boolean()) {
        currentDock.marketId = je["MarketID"].as_unsigned_long_long();
        currentDock.stationType = je["StationType"].as_string();
        currentDock.name = je["StationName"].as_string();
    }
    if (je.contains("StarSystem")) {
        currentStarSystem.address = je["SystemAddress"].as_unsigned_long_long();
        currentStarSystem.name = je["StarSystem"].as_string();
        auto& jpos = je["StarPos"].as_array();
        currentStarSystem.pos.x = jpos[0].as_double();
        currentStarSystem.pos.y = jpos[1].as_double();
        currentStarSystem.pos.z = jpos[2].as_double();
    }
}

void Configuration::parseEvent_Loadout(spGameEvent& ge) {
    auto& je = ge->data;

    mShipType.clear();
    mShipTypeLocalized.clear();
    mShipUserName.clear();

    if (je.contains("Ship"))
        mShipType = je["Ship"].as_string();
    if (je.contains("Ship_Localised"))
        mShipTypeLocalized = je["Ship_Localised"].as_string();
    if (je.contains("ShipName"))
        mShipUserName = je["ShipName"].as_string();
}

void Configuration::parseEvent_Cargo(spGameEvent& ge) {
    // TODO: call loadCargo(), with timestamp check
}

void Configuration::parseEvent_ShipyardSwap(spGameEvent& ge) {
    auto& je = ge->data;

    mShipType.clear();
    mShipTypeLocalized.clear();
    mShipUserName.clear();

    if (je.contains("ShipType"))
        mShipType = je["ShipType"].as_string();
    if (je.contains("Ship_Localised"))
        mShipTypeLocalized = je["Ship_Localised"].as_string();
    if (je.contains("ShipName"))
        mShipUserName = je["ShipName"].as_string();
}

void Configuration::parseEvent_Docked(spGameEvent& ge) {
    dockingEvent = ge;
}

void Configuration::parseEvent_Undocked(spGameEvent& ge) {
    dockingEvent.reset();
}

void Configuration::parseEvent_Docking(spGameEvent& ge) {
    dockingEvent = ge;
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
