//
// Created by mkizub on 25.06.2025.
//

#include "pch.h"
#include "Configuration.h"

bool Configuration::parseTimestamp(const json::value& value, Timestamp& timestamp) {
    if (value.is_string())
        return parseTimestamp(value.as_string(), timestamp);
    if (!value.is_object() || !value.contains("timestamp"))
        return false;
    return parseTimestamp(value.at("timestamp").as_string(), timestamp);
}

bool Configuration::parseTimestamp(const std::string& str, Timestamp& timestamp) {
    std::istringstream iss(str);
    iss >> std::chrono::parse("%Y-%m-%dT%H:%M:%SZ", timestamp);
    if (iss.fail()) {
        LOG(ERROR) << "Timestamp parse failed, event corrupted?";
        return false;
    }
    return true;
}

bool Configuration::parseEvent(std::string& line, std::string& event, Timestamp& timestamp) {
    std::string error;
    auto jres = json::parse5(line, &error);
    if (!jres.has_value()) {
        LOG(ERROR) << "Error parsing journal file: " << error;
        return false;
    }
    auto& je = jres.value();
    if (!je.is_object() || !je.contains("event")) {
        LOG(ERROR) << "Corrupted journal file, expecting 'Fileheader': " << je;
        return false;
    }

    if (!je.contains("event") || !je.contains("timestamp"))
        return false;

    event = je.at("event").as_string();
    if (!parseTimestamp(je.at("timestamp"), timestamp))
        return false;

    if (event == "Fileheader" || event == "LoadGame")
        return parseEvent_LoadGame(je, event, timestamp);
    if (event == "Shutdown")
        return true;
    if (event == "Commander")
        return parseEvent_Commander(je, event, timestamp);
    if (event == "CarrierLocation")
        return parseEvent_CarrierLocation(je, event, timestamp);
    if (event == "Location")
        return parseEvent_Location(je, event, timestamp);
    if (event == "Loadout")
        return parseEvent_Loadout(je, event, timestamp);
    if (event == "Cargo")
        return parseEvent_Cargo(je, event, timestamp);
    if (event == "ShipyardSwap")
        return parseEvent_ShipyardSwap(je, event, timestamp);

    return true;
}

bool Configuration::parseEvent_Commander(json::value& je, const std::string& event, const Timestamp& timestamp_out) {
    if (!je.contains("Name"))
        return false;
    mCmdrName = je["Name"].as_string();
    return true;
}

bool Configuration::parseEvent_LoadGame(json::value& je, const std::string& event, const Timestamp& timestamp_out) {
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

    return true;
}

bool Configuration::parseEvent_CarrierLocation(json::value& je, const std::string& event, const Timestamp& timestamp_out) {
    return true;
}

bool Configuration::parseEvent_Location(json::value& je, const std::string& event, const Timestamp& timestamp_out) {
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

    return true;
}

bool Configuration::parseEvent_Loadout(json::value& je, const std::string& event, const Timestamp& timestamp_out) {
    mShipType.clear();
    mShipTypeLocalized.clear();
    mShipUserName.clear();

    if (je.contains("Ship"))
        mShipType = je["Ship"].as_string();
    if (je.contains("Ship_Localised"))
        mShipTypeLocalized = je["Ship_Localised"].as_string();
    if (je.contains("ShipName"))
        mShipUserName = je["ShipName"].as_string();

    return true;
}

bool Configuration::parseEvent_Cargo(json::value& je, const std::string& event, const Timestamp& timestamp_out) {
    // TODO: call loadCargo(), with timestamp check
    return true;
}

bool Configuration::parseEvent_ShipyardSwap(json::value& je, const std::string& event, const Timestamp& timestamp_out) {
    mShipType.clear();
    mShipTypeLocalized.clear();
    mShipUserName.clear();

    if (je.contains("ShipType"))
        mShipType = je["ShipType"].as_string();
    if (je.contains("Ship_Localised"))
        mShipTypeLocalized = je["Ship_Localised"].as_string();
    if (je.contains("ShipName"))
        mShipUserName = je["ShipName"].as_string();

    return true;
}
