//
// Created by mkizub on 13.11.2025.
//

#ifndef PCH_H
#include "../pch.h"
#endif

#include "Galaxy.h"

namespace gal {

// ✦ ☄ ☕ ⚾ ⚽ ⛬ ✇ ⛋ ⭖ ⧰ ⧖ ⛫ ☗ ☖ ✈ ⛴ ♻ ♲ ⏣ ⯐ ⌖ ⛏ ☀ ∇ ◇ ⬖ ◆

NavType STAR {
        u'\u2726', // ✦
        TypeNav::Star,
        {"nav_select_icon_star.png"},
};
NavType BEACON {
        u'\u2604', // ☄
        TypeNav::NavBeacon,
        {"nav_select_icon_beacon.png"},
        true,
//        {{false, "$MULTIPLAYER_SCENARIO42_TITLE",      "", "Nav Beacon", "Нав. маяк"},
//         {false, "$MULTIPLAYER_SCENARIO80_TITLE",      "", "Compromised Navigation Beacon", "Навигационный маяк под угрозой"},
//         {false, "$EXT_PANEL_ColonisationBeacon_Site", "", "System Colonisation Beacon", "Маяк колонизации системы"},
//         {false, "$EXT_PANEL_ColonisationBeacon_DeploymentSite", "", "System Colonisation Beacon Deployment Site", "Место размещения маяка"},
//        }
};
NavType TOURIST_BEACON {
        u'\u2615', // ☕
        TypeNav::TouristBeacon,
        {"nav_select_icon_tourist_beacon.png"},
};
NavType BODY {
        u'\u26BE', // ⚾
        TypeNav::Planet,
        {"nav_select_icon_body.png"},
};
NavType LAND {
        u'\u26BD', // ⚽
        TypeNav::Planet,
        {"nav_select_icon_body_land.png"},
};
NavType BELT {
        u'\u26EC', // ⛬
        TypeNav::AsteroidCluster,
        {"nav_select_icon_ast_belt.png"},
        true
};
NavType ORBIS {
        u'\u2707', // ✇
        TypeNav::Orbis,
        {"nav_select_icon_orbis.png"},
};
NavType OCELLUS {
        u'\u2707', // ✇
        TypeNav::Ocellus,
        {"nav_select_icon_ocellus.png"},
};
NavType DODEC {
        u'\u2707', // ✇
        TypeNav::Dodec,
        {"nav_select_icon_dodec.png"},
};
NavType CORIOLIS {
        u'\u26CB', // ⛋
        TypeNav::Coriolis,
        {"nav_select_icon_coriolis.png"},
};
NavType MINER_BASE {
        u'\u2B56', // ⭖
        TypeNav::AsteroidBase,
        {"nav_select_icon_miner_base.png"},
};
NavType SPACE_OUTPOST {
        u'\u29F0', // ⧰
        TypeNav::SpaceOutpost,
        {"nav_select_icon_outpost.png"},
};
NavType SPACE_INSTALLATION {
        u'\u29D6', // ⧖
        TypeNav::SpaceInstallation,
        {"nav_select_icon_installation.png"},
};
NavType SPACE_CONSTR_DEPOT {
        u'\u0000',
        TypeNav::SpaceConstrDepot,
        {},
        true, // "Orbital Construction Site: *"
};
NavType PLANETARY_PORT {
        u'\u26EB', // ⛫
        TypeNav::PlanetaryPort,
        {"nav_select_icon_port.png"},
};
NavType PLANETARY_INSTALLATION {
        u'\u2617', // ☗
        TypeNav::PlanetaryInstallation,
        {"nav_select_icon_factory.png","nav_select_icon_factory_war.png"},
};
NavType PLANETARY_CONSTR_DEPOT {
        u'\u0000',
        TypeNav::PlanetaryConstrDepot,
        {},
        true, // "Planetary Construction Site: *"
};
NavType ODYSSEY_SETTLEMENT {
        u'\u2616', // ☖
        TypeNav::Settlement,
        {"nav_select_icon_settlement.png"},
};
NavType FLEET_CARRIER {
        u'\u2708', // ✈
        TypeNav::FleetCarrier,
        {"nav_select_icon_carrier.png","nav_select_icon_carrier_bad.png"},
        true, // have special suffix "XXX-XXX"
};
NavType SQUADRON_CARRIER {
        u'\u26F4', // ⛴
        TypeNav::SquadronCarrier,
        {"nav_select_icon_squadron.png"},
        true, // have special suffix "| XXXX"
};
NavType STATION_MEGASHIP {
        u'\u267B', // ♻
        TypeNav::StationMegaShip,
        {"nav_select_icon_station_megaship.png"},
};
//NavType STRONGHOLD_CARRIER {
//        u'\u267B', // ♻
//        TypeNav::StrongholdCarrier,
//        {"nav_select_icon_station_megaship.png"},
//        false,
////        {{false, "", "Stronghold Carrier", "", "Носитель-база"}},
//};
NavType MEGASHIP {
        u'\u2672', // ♲
        TypeNav::Megaship,
        {"nav_select_icon_megaship.png"},
};
NavType ANON_MEGASHIP {
        u'\u2672', // ♲
        TypeNav::AnonMegaship,
//        {"nav_select_icon_megaship.png"},
};
NavType COLONIZATION_SHIP {
        u'\u267B', // ♻
        TypeNav::ColonisationShip,
        {},
        true, // "$EXT_PANEL_ColonisationShip; *" "$EXT_PANEL_ColonisationShip:#index=1;"
//        {{true, "$EXT_PANEL_ColonisationShip", "System Colonisation Ship", "", "Колонизационный корабль"}}
};
NavType ENGINEER {
        u'\u23E3', // ⏣
        TypeNav::EngineerPort,
        {"nav_select_icon_engineer.png"},
        //true // need a list of all engeneers to compare station name
};
NavType UNEXPLORED {
        u'\u2BD0', // ⯐
        TypeNav::NotExplored,
        {"nav_select_icon_signal.png"},
        false,
//        {{false, "Unexplored","","Не исследовано"}}
};
NavType SIGNAL {
        u'\u2BD0', // ⯐
        TypeNav::Signal,
        {"nav_select_icon_signal.png","nav_select_icon_mission.png"},
        true,
};
NavType WAR_ZONE {
        u'\u2316', // ⌖
        TypeNav::WarZone,
        {"nav_select_icon_war_zone.png"},
        true,
//        {{false, "$Warzone_PointRace_Low", "", "Conflict Zone [Low Intensity]",   "Конфликт [низк. напряж.]"},
//         {false, "$Warzone_PointRace_Med", "", "Conflict Zone [Medium Intensity]","Конфликт [средн. напряж.]"},
//         {false, "$Warzone_PointRace_High","", "Conflict Zone [High Intensity]",  "Конфликт [выс. напряж.]"},
//        }
};
NavType RES_SITE {
        u'\u26CF', // ⛏
        TypeNav::ResSite,
        {"nav_select_icon_res_ext.png"},
        true,
//        {{false, "$MULTIPLAYER_SCENARIO14_TITLE", "","Resource Extraction Site","Место добычи ресурсов"},
//         {false, "$MULTIPLAYER_SCENARIO77_TITLE", "","Resource Extraction Site [Low]","Место добычи ресурсов [низк.]"},
//         {false, "$MULTIPLAYER_SCENARIO78_TITLE", "","Resource Extraction Site [High]","Место добычи ресурсов [высок.]"},
//         {false, "$MULTIPLAYER_SCENARIO79_TITLE", "","Resource Extraction Site [Hazardous]","Место добычи ресурсов [опасн.]"},
//        }
};
NavType CODEX {
        u'\u270D', // ✍
        TypeNav::Codex,
        {},
        true,
//        {{false, "$Fixed_Event_Life", "","Notable Stellar phenomena","Примечательные звездные феномены"}}
};
NavType STAR_SYSTEM {
        u'\u2600', // ☀
        TypeNav::StarSystem,
        {"nav_select_icon_star_system.png"},
};

const std::vector<const NavType*> ALL_NAV_TYPES {
        &STAR, // ✦
        &BEACON, // ☄
        &TOURIST_BEACON, // ☕
        &BODY, // ⚾
        &LAND, // ⚽
        &BELT, // ⛬
        &ORBIS, // ✇
        &OCELLUS, // ✇
        &DODEC, // ✇
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
        //&STRONGHOLD_CARRIER,
        &MEGASHIP, // ♲
        //&TRAILBLAZER_DREAM,
        &COLONIZATION_SHIP,
        &ANON_MEGASHIP,
        &ENGINEER, // ⏣
        &SIGNAL, // ⯐
        &UNEXPLORED, // ⯐
        &WAR_ZONE, // ⌖
        &RES_SITE, // ⛏
        &CODEX, // ✍
        &STAR_SYSTEM, // ☀
};

const NavType* NavType::findNavType(TypeNav tp) {
    if (tp == TypeNav::Other)
        return nullptr;
    for (auto nt: ALL_NAV_TYPES) {
        if (tp == nt->type)
            return nt;
    }
    return nullptr;
}

std::unordered_set<GameVar,GameVar::var_hash,std::equal_to<void>> GameVar::allVars;

const GameVar* GameVar::find(std::string_view sv) {
    auto start = sv.find('$');
    if (start == std::string_view::npos)
        return nullptr;
    start += 1;
    auto end = sv.find(';', start+1);
    if (end == std::string_view::npos)
        return nullptr;
    auto col= sv.find(':', start+1);
    if (col == std::string_view::npos || col >= end) {
        std::string_view svar = sv.substr(start, end-start);
        auto it = allVars.find(svar);
        if (it == allVars.end()) {
            LOG_WARNING("Unknown substitution var: \"{}\"", svar);
            return nullptr;
        }
        return &*it;
    } else {
        std::string_view svar = sv.substr(start, col-start);
        auto it = allVars.find(svar);
        if (it == allVars.end()) {
            LOG_WARNING("Unknown substitution var: \"{}\"", sv.substr(start, end-start));
            return nullptr;
        }
        return &*it;
    }
}

TypeNav GameVar::replace_in(std::string_view sv, std::string& eng, std::string& loc) {
    TypeNav detected = TypeNav::Other;
    size_t pos = 0;
    while (pos < sv.size()) {
        auto start = sv.find('$', pos);
        if (start == std::string_view::npos)
            break;
        start += 1;
        auto end = sv.find(';', start+1);
        if (end == std::string_view::npos)
            break;
        auto col = sv.find(':', start+1);
        if (col == std::string_view::npos || col >= end) {
            std::string_view svar = sv.substr(start, end - start);
            auto it = allVars.find(svar);
            if (it == allVars.end()) {
                LOG_WARNING("Unknown substitution var: \"{}\"", svar);
                eng.append(sv, pos, end-pos);
                loc.append(sv, pos, end-pos);
            } else {
                eng.append(it->get_eng());
                loc.append(it->get_loc());
                if (detected == TypeNav::Other)
                    detected = it->type;
            }
        } else {
            auto arg = sv.substr(col+1, end-col-1);
            std::string_view lvl;
            if (arg.starts_with("#threatLevel="))
                lvl = arg.substr(13);
            else if (arg.starts_with("#index="))
                lvl = arg.substr(7);
            else
                LOG_WARNING("Unknown substitution var arg: \"{}\" in \"{}\"", arg, sv);
            std::string_view svar = sv.substr(start, col - start);
            auto it = allVars.find(svar);
            if (it == allVars.end()) {
                LOG_WARNING("Unknown substitution var: \"{}\"", sv.substr(start, end - start));
                eng.append(sv, pos, end-pos);
                loc.append(sv, pos, end-pos);
            } else {
                eng.append(it->get_eng());
                loc.append(it->get_loc());
                if (!lvl.empty()) {
                    if (auto p = eng.find("{}"); p != std::string::npos)
                        eng.replace(p, 2, lvl);
                    if (auto p = loc.find("{}"); p != std::string::npos)
                        loc.replace(p, 2, lvl);
                }
                if (detected == TypeNav::Other)
                    detected = it->type;
            }
        }
        pos = end + 1;
    }
    eng.append(sv, pos, sv.size()-pos);
    loc.append(sv, pos, sv.size()-pos);
    return detected;
}

std::string_view GameVar::get_eng() const {
    if (!en.empty())
        return en;
    return xx;
}

std::string_view GameVar::get_loc() const {
    if (st::lng == Lang::XX) {
        if (!xx.empty()) return xx;
        return en;
    }
    if (st::lng == Lang::EN) {
        if (!en.empty()) return en;
        return xx;
    }
    if (st::lng == Lang::RU) {
        if (!ru.empty()) return ru;
        return xx;
    }
    return {};
}

bool GameVar::match(std::string_view sname) const {
    if (sname.empty())
        return false;
    if (!var.empty() && sname.size() > var.size()+2 && sname[0] == '$' && sname.substr(1).starts_with(var))
        return true;
    if (!prefix) {
        if (xx == sname) return true;
        if (en == sname) return true;
        if (ru == sname) return true;
    } else {
        if (!xx.empty() && sname.starts_with(xx)) return true;
        if (!en.empty() && sname.starts_with(en)) return true;
        if (!ru.empty() && sname.starts_with(ru)) return true;
    }
    return false;
}

void GameVar::localizeName(std::string_view sname, std::string& name, std::string& nloc) const {
    assert (match(sname));

    if (sname[0] == '$') {
        replace_in(sname, name, nloc);
        return;
    }

    if (!prefix) {
        name = *get_eng();
        if (st::lng == Lang::RU)
            nloc = *get_loc();
    } else {
        if (!xx.empty() && sname.starts_with(xx)) {
            auto suffix = sname.substr(xx.size());
            name = *get_eng();
            name.append(suffix);
            if (st::lng == Lang::RU) {
                nloc = *get_loc();
                nloc.append(suffix);
            }
            return;
        }
        if (!en.empty() && sname.starts_with(en)) {
            auto suffix = sname.substr(en.size());
            name = *get_eng();
            name.append(suffix);
            if (st::lng == Lang::RU) {
                nloc = *get_loc();
                nloc.append(suffix);
            }
            return;
        }
        if (!ru.empty() && sname.starts_with(ru)) {
            auto suffix = sname.substr(ru.size());
            name = *get_eng();
            name.append(suffix);
            if (st::lng == Lang::RU) {
                nloc = *get_loc();
                nloc.append(suffix);
            }
            return;
        }
    }
}

// capital alpha-numeric
inline bool CAN(char c) {
    return (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
}

bool NavType::match_name(std::string_view sname) const {
    if (sname.empty())
        return false;
    if (!name_loc.empty()) {
        for (auto &nl: name_loc) {
            if (nl.match(sname))
                return true;
        }
    }
    if (name_pattern) {
        switch (this->type) {
        case TypeNav::FleetCarrier:
        case TypeNav::SquadronCarrier:
            if (this->type == NavType::checkCarrierName(sname, nullptr, nullptr))
                return true;
            return false;
        case TypeNav::AsteroidCluster:
            if (sname.contains(" Belt Cluster "))
                return true;
            return false;
        case TypeNav::SpaceConstrDepot:
            if (sname.starts_with("Orbital Construction Site:"))
                return true;
            return false;
        case TypeNav::PlanetaryConstrDepot:
            if (sname.starts_with("Planetary Construction Site:"))
                return true;
            return false;
        case TypeNav::ColonisationShip:
            for (auto &p: name_loc) {
                if (p.match(sname))
                    return true;
            }
            break;
        case TypeNav::AnonMegaship:
            for (auto &p: name_loc) {
                if (p.match(sname))
                    return true;
            }
            break;
        case TypeNav::NavBeacon:
        case TypeNav::Signal:
        case TypeNav::WarZone:
        case TypeNav::ResSite:
        case TypeNav::USS:
        case TypeNav::Codex:
        case TypeNav::Titan:
            break;
        default:
            LOG_WARNING("Unknown name pattern for NavType {}", enum_name(this->type));
            break;
        }
        auto* gvar = GameVar::find(sname);
        if (gvar && gvar->type == this->type)
            return true;
    }
    return false;
}

const std::unordered_set<std::string> NonCarriers {
        "LP", "L", "LFT", "LHS", "OW94", "Luyten", "Furuhjelm",

        "Gordon Class Bulk Cargo Ship",
        "James Class Bulk Cargo Ship",
        "Samson Class Bulk Cargo Ship",

        "Amaethon Class Agricultural Vessel",
        "Demeter Class Agricultural Vessel",
        "Dionysus Class Agricultural Vessel",

        "Lowell Class Science Vessel",
        "Aquarius Class Tanker",
        "Naphtha Class Tanker",
        "Alcatraz Class Prison Ship",
        "Riker Class Prison Ship",
        "Freedom Class Survey Vessel",
        "Sagan Class Tourist Ship",
};

//const std::string_view NonCarriers[] {
//    "LP", "L", "LFT", "LHS", "OW94", "Luyten", "Furuhjelm",
//};

TypeNav NavType::checkCarrierName(std::string_view nm, std::string* name, std::string* nloc) {
    if (nm.size() == 7) {
        if (CAN(nm[0]) && CAN(nm[1]) && CAN(nm[2]) && nm[3] == '-' && CAN(nm[4]) && CAN(nm[5]) && CAN(nm[6])) {
            if (name) *name = *nm;
            if (nloc) *nloc = *nm;
            return TypeNav::FleetCarrier;
        }
    }
    if (nm.size() > 8 && nm[nm.size() - 8] == ' ') {
        auto s = nm.substr(nm.size() - 7);
        if (CAN(s[0]) && CAN(s[1]) && CAN(s[2]) && s[3] == '-' && CAN(s[4]) && CAN(s[5]) && CAN(s[6])) {
            std::string_view prefix = nm.substr(0,nm.size()-8);
            //for (auto& nc : NonCarriers) {
            //    if (prefix == nc)
            //        return TypeNav::Other;
            //}
            if (NonCarriers.contains(trim(prefix)))
                return TypeNav::Other;
            if (name) *name = *s;
            if (nloc) *nloc = *nm;
            return TypeNav::FleetCarrier;
        }
    }

    if (nm.size() == 4 && CAN(nm[0]) && CAN(nm[1]) && CAN(nm[2]) && CAN(nm[3])) {
        if (name) *name = *nm;
        if (nloc) *nloc = *nm;
        return TypeNav::SquadronCarrier;
    }
    if (nm.size() > 7) {
        auto s = nm.substr(nm.size() - 7);
        if (s[0] == ' ' && s[1] == '|' && s[2] == ' ' && CAN(s[3]) && CAN(s[4]) && CAN(s[5]) && CAN(s[6])) {
            if (name) *name = *s.substr(3);
            if (nloc) *nloc = *nm;
            return TypeNav::SquadronCarrier;
        }
    }

    return TypeNav::Other;
}

TypeNav NavType::localizeName(TypeNav type, std::string_view nm, std::string& name, std::string& nloc) {
    TypeNav tn = checkCarrierName(nm, &name, &nloc);
    if (isCarrier(tn))
        return tn;

    std::string repl_en, repl_ru;
    tn = GameVar::replace_in(nm, repl_en, repl_ru);
    if (tn != TypeNav::Other) {
        name = repl_en;
        if (st::lng == Lang::RU)
            nloc = repl_ru;
        else
            nloc = repl_en;
        if (type == TypeNav::ColonisationShip) {
            if (auto pos = name.find('['); pos != std::string::npos)
                name = trim(name.substr(0, pos));
        }
        return tn;
    }

    for (auto nt: ALL_NAV_TYPES) {
        if (!nt->name_loc.empty()) {
            for (auto &p: nt->name_loc) {
                if (p.match(nm)) {
                    p.localizeName(nm, name, nloc);
                    return nt->type;
                }
            }
        }
    }

    name = nm;
    nloc = "";
    return TypeNav::Other;
}

TypeNav NavType::guessTypeFromName(TypeNav tp, std::string_view nm) {
    if (const GameVar* gv = GameVar::find(nm); gv && gv->type != TypeNav::Other)
        return gv->type;
    for (auto nt: ALL_NAV_TYPES) {
        if (nt->match_name(nm))
            return nt->type;
    }

    return tp;
}

bool NavType::match_type(std::string_view stype) const {
    if (stype.empty())
        return false;
    return type == toTypeNav(JsNavType::get(stype));
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

