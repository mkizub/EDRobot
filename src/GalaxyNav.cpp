//
// Created by mkizub on 13.11.2025.
//

#include "pch.h"

#include "Galaxy.h"

namespace gal {

// ✦ ☄ ☕ ⚾ ⚽ ⛬ ✇ ⛋ ⭖ ⧰ ⧖ ⛫ ☗ ☖ ✈ ⛴ ♻ ♲ ⏣ ⯐ ⌖ ⛏ ☀ ∇ ◇ ⬖ ◆

NavType STAR {
        u'\u2726', // ✦
        TypeNav::Star,
        {"nav_select_icon_star.png"},
        {"Star"}
};
NavType BEACON {
        u'\u2604', // ☄
        TypeNav::NavBeacon,
        {"nav_select_icon_beacon.png"},
        {"NavBeacon"},
        false,
        {{Lang::EN,"Nav Beacon"},{Lang::RU,"Нав. маяк"},
         {Lang::EN,"Compromised Navigation Beacon"},{Lang::RU,"Навигационный маяк под угрозой"},
        }
};
NavType TOURIST_BEACON {
        u'\u2615', // ☕
        TypeNav::TouristBeacon,
        {"nav_select_icon_tourist_beacon.png"},
        {"TouristBeacon"}
};
NavType BODY {
        u'\u26BE', // ⚾
        TypeNav::Planet,
        {"nav_select_icon_body.png"},
        {"Planet"}
};
NavType LAND {
        u'\u26BD', // ⚽
        TypeNav::Planet,
        {"nav_select_icon_body_land.png"},
        {}, // only detected by icon
};
NavType BELT {
        u'\u26EC', // ⛬
        TypeNav::AsteroidCluster,
        {"nav_select_icon_ast_belt.png"},
        {"AsteroidCluster","Belt Cluster"},
        true
};
NavType ORBIS {
        u'\u2707', // ✇
        TypeNav::Orbis,
        {"nav_select_icon_orbis.png"},
        { "Orbis", "Orbis Starport", "StationONeilOrbis", "StationONeilCylinder", }
};
NavType OCELLUS {
        u'\u2707', // ✇
        TypeNav::Ocellus,
        {"nav_select_icon_ocellus.png"},
        { "Ocellus", "StationBernalSphere", "Ocellus Starport" }
};
NavType CORIOLIS {
        u'\u26CB', // ⛋
        TypeNav::Coriolis,
        {"nav_select_icon_coriolis.png"},
        {"Coriolis", "StationCoriolis", "Coriolis Starport"}
};
NavType MINER_BASE {
        u'\u2B56', // ⭖
        TypeNav::AsteroidBase,
        {"nav_select_icon_miner_base.png"},
        {"AsteroidBase", "Asteroid base"}
};
NavType SPACE_OUTPOST {
        u'\u29F0', // ⧰
        TypeNav::SpaceOutpost,
        {"nav_select_icon_outpost.png"},
        {"Outpost", "Outpost Starport"}
};
NavType SPACE_INSTALLATION {
        u'\u29D6', // ⧖
        TypeNav::SpaceInstallation,
        {"nav_select_icon_installation.png"},
        {"Installation", "Space Installation"}
};
NavType SPACE_CONSTR_DEPOT {
        u'\u0000',
        TypeNav::SpaceConstrDepot,
        {},
        {"SpaceConstructionDepot"},
        true, // "Orbital Construction Site: *"
};
NavType PLANETARY_PORT {
        u'\u26EB', // ⛫
        TypeNav::PlanetaryPort,
        {"nav_select_icon_port.png"},
        {"CraterOutpost", "Planetary Outpost", "Crater Outpost", "Planetary City", "Planetary Port"}
};
NavType PLANETARY_INSTALLATION {
        u'\u2617', // ☗
        TypeNav::PlanetaryInstallation,
        {"nav_select_icon_factory.png","nav_select_icon_factory_war.png"},
        {"Planetary Installation"},
};
NavType PLANETARY_CONSTR_DEPOT {
        u'\u0000',
        TypeNav::PlanetaryConstrDepot,
        {},
        {"PlanetaryConstructionDepot"},
        true, // "Planetary Construction Site: *"
};
NavType ODYSSEY_SETTLEMENT {
        u'\u2616', // ☖
        TypeNav::Settlement,
        {"nav_select_icon_settlement.png"},
        {"Settlement","Odyssey Settlement"}
};
NavType FLEET_CARRIER {
        u'\u2708', // ✈
        TypeNav::FleetCarrier,
        {"nav_select_icon_carrier.png","nav_select_icon_carrier_bad.png"},
        {"FleetCarrier", "Drake-Class Carrier", "Fleet Carrier"}
};
NavType SQUADRON_CARRIER {
        u'\u2708', // ⛴
        TypeNav::SquadronCarrier,
        {"nav_select_icon_squadron.png"},
        {"SquadronCarrier", "Squadron Carrier"}
};
NavType STATION_MEGASHIP {
        u'\u267B', // ♻
        TypeNav::StationMegaShip,
        {"nav_select_icon_station_megaship.png"},
        {"StationMegaShip"}
};
NavType STRONGHOLD_CARRIER {
        u'\u267B', // ♻
        TypeNav::StrongholdCarrier,
        {"nav_select_icon_station_megaship.png"},
        {}, // only by icon or localize name
        false,
        {{Lang::EN,"Stronghold Carrier"},{Lang::RU,"Носитель-база"}},
};
NavType MEGASHIP {
        u'\u2672', // ♲
        TypeNav::Megaship,
        {"nav_select_icon_megaship.png"},
        {"Megaship"}
};
//NavType TRAILBLAZER_DREAM {
//        u'\u0000',
//        TypeNav::TrailblazerDream,
//        {},
//        {"TrailblazerDream","Trailblazer Dream"},
//        false,
//        {{Lang::XX,"Trailblazer Dream"}}
//};
NavType COLONIZATION_SHIP {
        u'\u0000',
        TypeNav::ColonisationShip,
        {},
        {},
        true, // "$EXT_PANEL_ColonisationShip*"
        {{Lang::EN,"System Colonisation Ship"},{Lang::RU,"Колонизационный корабль"}}
};
NavType ENGINEER {
        u'\u23E3', // ⏣
        TypeNav::EngineerPort,
        {"nav_select_icon_engineer.png"},
        {}, // "StationType":"CraterOutpost", "StationGovernment":"$government_Engineer;", "StationGovernment_Localised":"Мастерская"
        true
};
NavType UNEXPLORED {
        u'\u2BD0', // ⯐
        TypeNav::NotExplored,
        {"nav_select_icon_signal.png"},
        {},
        false,
        {{Lang::EN,"Unexplored"},{Lang::RU,"Не исследовано"}}
};
NavType SIGNAL {
        u'\u2BD0', // ⯐
        TypeNav::Signal,
        {"nav_select_icon_signal.png","nav_select_icon_mission.png"},
        {"Generic"}
};
NavType WAR_ZONE {
        u'\u2316', // ⌖
        TypeNav::WarZone,
        {"nav_select_icon_war_zone.png"},
        {"Combat"},
        false,
        {{Lang::EN,"Conflict Zone [Low Intensity]"},{Lang::RU,"Конфликт [низк. напряж.]"},
         {Lang::EN,"Conflict Zone [Medium Intensity]"},{Lang::RU,"Конфликт [средн. напряж.]"},
         {Lang::EN,"Conflict Zone [High Intensity]"},{Lang::RU,"Конфликт [выс. напряж.]"},
        }
};
NavType RES_SITE {
        u'\u26CF', // ⛏
        TypeNav::ResSite,
        {"nav_select_icon_res_ext.png"},
        {"ResourceExtraction"},
        false,
        {{Lang::EN,"Resource Extraction Site"},{Lang::RU,"Место добычи ресурсов"},
         {Lang::EN,"Resource Extraction Site [Low]"},{Lang::RU,"Место добычи ресурсов [низк.]"},
         {Lang::EN,"Resource Extraction Site [High]"},{Lang::RU,"Место добычи ресурсов [высок.]"},
         {Lang::EN,"Resource Extraction Site [Hazardous]"},{Lang::RU,"Место добычи ресурсов [опасн.]"},
        }
};
NavType STAR_SYSTEM {
        u'\u2600', // ☀
        TypeNav::StarSystem,
        {"nav_select_icon_star_system.png"},
        {}
};

const std::vector<NavType*> ALL_NAV_TYPES {
        &STAR, // ✦
        &BEACON, // ☄
        &TOURIST_BEACON, // ☕
        &BODY, // ⚾
        &LAND, // ⚽
        &BELT, // ⛬
        &ORBIS, // ✇
        &OCELLUS, // ✇
        &CORIOLIS, // ⛋
        &MINER_BASE, // ⭖
        &SPACE_OUTPOST, // ⧰
        &SPACE_INSTALLATION, // ⧖
        &SPACE_CONSTR_DEPOT,
        &PLANETARY_PORT, // ⛫
        &PLANETARY_INSTALLATION, // ☗
        &PLANETARY_CONSTR_DEPOT,
        &ODYSSEY_SETTLEMENT, // ☖
        &FLEET_CARRIER, // ✈
        &SQUADRON_CARRIER, // ⛴
        &STATION_MEGASHIP, // ♻
        &STRONGHOLD_CARRIER,
        &MEGASHIP, // ♲
        //&TRAILBLAZER_DREAM,
        &COLONIZATION_SHIP,
        &ENGINEER, // ⏣
        &SIGNAL, // ⯐
        &UNEXPLORED, // ⯐
        &WAR_ZONE, // ⌖
        &RES_SITE, // ⛏
        &STAR_SYSTEM, // ☀
};

bool NavType::match_name(const std::string& sname) const {
    if (sname.empty())
        return false;
    if (!name_loc.empty()) {
        for (auto &p: name_loc) {
            if ((p.first == Lang::XX || p.first == st::lng) && p.second == sname)
                return true;
        }
        return false;
    }
    if (name_pattern) {
        switch (this->type) {
        case TypeNav::AsteroidCluster:
            if (sname.contains("Belt Cluster"))
                return true;
            break;
        case TypeNav::SpaceConstrDepot:
            if (sname.starts_with("Orbital Construction Site:"))
                return true;
            break;
        case TypeNav::PlanetaryConstrDepot:
            if (sname.starts_with("Planetary Construction Site:"))
                return true;
            break;
        case TypeNav::ColonisationShip:
            if (sname.starts_with("$EXT_PANEL_ColonisationShip"))
                return true;
            for (auto &p: name_loc) {
                if ((p.first == Lang::XX || p.first == st::lng) && sname.starts_with(p.second))
                    return true;
            }
            break;
        }
    }
    return false;
}

bool NavType::match_type(const std::string& stype) const {
    if (stype.empty())
        return false;
    if (!stype.empty()) {
        for (auto& alias: typeAliases) {
            if (stype == alias)
                return true;
        }
    }
    return false;
}

bool NavType::match_icon(wchar_t ch, const std::string& icon) const {
    if (!icon.empty()) {
        for (auto& ni: navIcons) {
            if (icon == ni)
                return true;
        }
    }
    if (ch && ch != ERROR_MARK) {
        return ch == charOCR;
    }
    return false;
}

}

