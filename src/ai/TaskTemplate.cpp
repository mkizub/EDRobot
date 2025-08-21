//
// Created by mkizub on 19.06.2025.
//

#include "../pch.h"

#include "TaskTemplate.h"
#include "AIManager.h"

namespace nav {

using enum ai::POIType;

// ✦ ☄ ☕ ⚾ ⚽ ⛬ ✇ ⛋ ⭖ ⧰ ⧖ ⛫ ☗ ☖ ✈ ♻ ♲ ⏣ ⯐ ⌖ ⛏ ☀ ∇ ◇ ⬖ ◆

NavType STAR { u'\u2726', // ✦
    Star, {"nav_select_icon_star.png"}, {"Star"}
};
NavType BEACON { u'\u2604', // ☄
    Place, {"nav_select_icon_beacon.png"}, {"NavBeacon"}
};
NavType TOURIST_BEACON { u'\u2615', // ☕
    Place, {"nav_select_icon_tourist_beacon.png"}, {"TouristBeacon"}
};
NavType BODY { u'\u26BE', // ⚾
    Planet, {"nav_select_icon_body.png"}, {"Planet"}
};
NavType LAND { u'\u26BD', // ⚽
    Planet, {"nav_select_icon_body_land.png"}, {"Planet"}
};
NavType BELT { u'\u26EC', // ⛬
    AsteroidBelt, {"nav_select_icon_ast_belt.png"}, {}
};
NavType ORBIS { u'\u2707', // ✇
    Station, {"nav_select_icon_orbis.png", "nav_select_icon_ocellus.png"}, {
        "Orbis", "Orbis Starport", "StationONeilOrbis", "StationONeilCylinder",
        "Ocellus", "StationBernalSphere", "Ocellus Starport"
    }
};
NavType CORIOLIS { u'\u26CB', // ⛋
    Station, {"nav_select_icon_coriolis.png"}, {"Coriolis", "StationCoriolis", "Coriolis Starport"}
};
NavType MINER_BASE { u'\u2B56', // ⭖
    Station, {"nav_select_icon_miner_base.png"}, {"AsteroidBase", "Asteroid base"} // TODO: add the icon!!!
};
NavType SPACE_OUTPOST { u'\u29F0', // ⧰
    Station, {"nav_select_icon_outpost.png"}, {"Outpost"}
};
NavType SPACE_INSTALLATION { u'\u29D6', // ⧖
    Place, {"nav_select_icon_installation.png"}, {"Installation", "Space Installation"}
};
NavType PLANETARY_PORT { u'\u26EB', // ⛫
    Port, {"nav_select_icon_port.png"}, {"Planetary Outpost"}
};
NavType PLANETARY_INSTALLATION { u'\u2617', // ☗
    Place, {"nav_select_icon_factory.png","nav_select_icon_factory_war.png"}, {"Planetary Installation"}
};
NavType ODYSSEY_SETTLEMENT { u'\u2616', // ☖
    Port, {"nav_select_icon_settlement.png"}, {"Settlement","Odyssey Settlement"}
};
NavType CARRIER { u'\u2708', // ✈
    FleetCarrier, {"nav_select_icon_carrier.png","nav_select_icon_carrier_bad.png"}, {"FleetCarrier", "Drake-Class Carrier"}
};
NavType STATION_MEGASHIP { u'\u267B', // ♻
    Place, {"nav_select_icon_station_megaship.png"}, {"StationMegaShip"}
};
NavType MEGASHIP { u'\u2672', // ♲
    Place, {"nav_select_icon_megaship.png"}, {"Megaship","Trailblazer Dream"}
};
NavType ENGINEER { u'\u23E3', // ⏣
    Port, {"nav_select_icon_engineer.png"}, {}
};
NavType SIGNAL { u'\u2BD0', // ⯐
    Signal, {"nav_select_icon_signal.png","nav_select_icon_mission.png"}, {"Generic"}
};
NavType WAR_ZONE { u'\u2316', // ⌖
    Place, {"nav_select_icon_war_zone.png"}, {"Combat"}
};
NavType RES_SITE { u'\u26CF', // ⛏
    Place, {"nav_select_icon_res_ext.png"}, {"ResourceExtraction"}
};
NavType STAR_SYSTEM { u'\u2600', // ☀
    System, {"nav_select_icon_star_system.png"}, {}
};
NavType LOCATION { u'\u2207', // ∇
    None, {}, {}
};
NavType SHIELD1 { u'\u25C7', // ◇
    None, {}, {}
};
NavType SHIELD2 { u'\u2B16', // ⬖
    None, {}, {}
};
NavType SHIELD3 { u'\u25C6', // ◆
    None, {}, {}
};

std::vector<NavType*> ALL_NAV_TYPES {
        &STAR, // ✦
        &BEACON, // ☄
        &TOURIST_BEACON, // ☕
        &BODY, // ⚾
        &LAND, // ⚽
        &BELT, // ⛬
        &ORBIS, // ✇
        &CORIOLIS, // ⛋
        &MINER_BASE, // ⭖
        &SPACE_OUTPOST, // ⧰
        &SPACE_INSTALLATION, // ⧖
        &PLANETARY_PORT, // ⛫
        &PLANETARY_INSTALLATION, // ☗
        &ODYSSEY_SETTLEMENT, // ☖
        &CARRIER, // ✈
        &STATION_MEGASHIP, // ♻
        &MEGASHIP, // ♲
        &ENGINEER, // ⏣
        &SIGNAL, // ⯐
        &WAR_ZONE, // ⌖
        &RES_SITE, // ⛏
        &STAR_SYSTEM, // ☀
//        &LOCATION, // ∇
//        &SHIELD1, // ◇
//        &SHIELD2, // ⬖
//        &SHIELD3, // ◆
};

}

namespace ai {

const std::string ED_STATE_VOID = "eds-void";
const std::string ED_STATE_SPACE = "eds-space";
const std::string ED_STATE_JUMP = "eds-jump";
const std::string ED_STATE_CRUISE = "eds-cruise";
const std::string ED_STATE_CRUISE_INT = "eds-cruise-int";
const std::string ED_STATE_CRUISE_JET = "eds-cruise-jet";
const std::string ED_STATE_DOCKED = "eds-dock";
const std::string ED_STATE_LANDED = "eds-land";
const std::string ED_STATE_DEPART = "eds-depart";

const std::string ED_UI_MODE_NORM = "uim-n";
const std::string ED_UI_MODE_LEFT = "uim-1";
const std::string ED_UI_MODE_RIGHT = "uim-4";
const std::string ED_UI_MODE_DOWN = "uim-3";
const std::string ED_UI_MODE_CHAT = "uim-2";
const std::string ED_UI_MODE_DOCK = "uim-dock";
const std::string ED_UI_MODE_STATION = "uim-st";
const std::string ED_UI_MODE_GAL_MAP = "uim-gal";
const std::string ED_UI_MODE_SYS_MAP = "uim-sys";

const std::string ED_TASK_SEQ = "tsk-seq";
const std::string ED_TASK_LOOP = "tsk-loop";
const std::string ED_TASK_GOTO_SERVICES = "tsk-goto-services";
const std::string ED_TASK_GOTO_MARKET = "tsk-goto-market";
const std::string ED_TASK_MARKET_SELL_ALL = "tsk-market-sell-all";
const std::string ED_TASK_MARKET_SELL = "tsk-market-sell";
const std::string ED_TASK_MARKET_BUY = "tsk-market-buy";
const std::string ED_TASK_TRAVEL = "tsk-travel";
const std::string ED_TASK_GAL_MAP_SELECT = "tsk-star-select";
const std::string ED_TASK_SYS_MAP_SELECT = "tsk-poi-select";
const std::string ED_TASK_DEPART = "tsk-departure";
const std::string ED_TASK_CRUISE_AVOID = "tsk-cruise-avoid";
const std::string ED_TASK_SPACE_AVOID = "tsk-space-avoid";
const std::string ED_TASK_JUMP_TO_SYSTEM = "tsk-jump-to-system";
const std::string ED_TASK_TO_CRUISE = "tsk-to_cruise";
const std::string ED_TASK_CRUISE_TO_STATION = "tsk-cruise-to-station";
const std::string ED_TASK_CRUISE_TO_POI = "tsk-cruise-to-poi";
const std::string ED_TASK_CRUISE_TO_PORT = "tsk-cruise-to-port";
const std::string ED_TASK_NIGH = "tsk-nigh";
const std::string ED_TASK_DOCK = "tsk-dock";
const std::string ED_TASK_STEP = "tsk-step";

const std::string ED_TASK_CALIBRATE = "tsk-calibrate";
const std::string ED_TASK_DEBUG_FIND_ALL_COMMODITIES = "tsk-debug-find-all-commodities";
const std::string ED_TASK_DEBUG_FIND_ALL_NAV_POINTS = "tsk-debug-find-all-nav-points";
const std::string ED_TASK_DEBUG_FIX_OCR = "tsk-debug-fix-ocr";
const std::string ED_TASK_DEBUG_ORIENT_NAV_TARGET = "tsk-debug-orient-to-nav-target";

const std::string PATH_SEP = ":";

UIMode ED_UI_Mode_Norm{ED_UI_MODE_NORM, {}};
UIMode ED_UI_Mode_Left{ED_UI_MODE_LEFT, {

}};
UIMode ED_UI_Mode_Right{ED_UI_MODE_RIGHT, {

}};
UIMode ED_UI_Mode_Down{ED_UI_MODE_DOWN, {

}};
UIMode ED_UI_Mode_Chat{ED_UI_MODE_CHAT, {

}};
UIMode ED_UI_Mode_Dock{ED_UI_MODE_DOCK, {

}};
UIMode ED_UI_Mode_Station{ED_UI_MODE_STATION, {

}};
UIMode ED_UI_Mode_Gal_Map{ED_UI_MODE_GAL_MAP, {

}};
UIMode ED_UI_Mode_Sys_Map{ED_UI_MODE_SYS_MAP, {

}};

TaskTemplate ED_Task_Seq {
    .name = ED_TASK_SEQ
};
TaskTemplate ED_Task_Loop {
    .name = ED_TASK_LOOP,
    .params = { {Param::Int, "count", 0} }
};
TaskTemplate ED_Task_Goto_Services {
    .name = ED_TASK_GOTO_SERVICES
};
TaskTemplate ED_Task_Goto_Market {
    .name = ED_TASK_GOTO_MARKET
};
TaskTemplate ED_Task_Market_Sell_All {
        .name = ED_TASK_MARKET_SELL_ALL,
        .params = { {Param::Int, "chunk", 0 } }
};
TaskTemplate ED_Task_Market_Sell {
    .name = ED_TASK_MARKET_SELL,
    .params = { {Param::Commodity, "commodity", ""}, {Param::Int, "amount", 0 }, {Param::Int, "chunk", 0 } },
    .maxMisses = 3
};
TaskTemplate ED_Task_Market_Buy {
    .name = ED_TASK_MARKET_BUY,
    .params = { {Param::Commodity, "commodity", ""}, {Param::Int, "amount", 0 } },
    .maxMisses = 3
};
TaskTemplate ED_Task_Gal_Map_Select {
    .name = ED_TASK_GAL_MAP_SELECT,
    .params = { {Param::System, "system", ""} }
};
TaskTemplate ED_Task_Sys_Map_Select {
    .name = ED_TASK_SYS_MAP_SELECT,
    .params = { {Param::POI, "poi", ""} }
};
TaskTemplate ED_Task_Depart {
    .name = ED_TASK_DEPART
};
TaskTemplate ED_Task_Cruise_Avoid {
    .name = ED_TASK_CRUISE_AVOID
};
TaskTemplate ED_Task_Space_Avoid {
    .name = ED_TASK_SPACE_AVOID
};
TaskTemplate ED_Task_To_Cruise {
    .name = ED_TASK_TO_CRUISE
};
TaskTemplate ED_Task_Cruise_To_PIO {
    .name = ED_TASK_CRUISE_TO_POI,
    .params = { {Param::POI, "poi", ""} }
};
TaskTemplate ED_Task_Cruise_To_Port{
    .name = ED_TASK_CRUISE_TO_PORT,
    .params = { {Param::Dock, "port", ""} }
};
TaskTemplate ED_Task_Nigh {
    .name = ED_TASK_NIGH,
    .params = { {Param::POI, "poi", ""} }
};
TaskTemplate ED_Task_Dock {
    .name = ED_TASK_DOCK,
    .params = { {Param::Dock, "dock", ""} }
};
TaskTemplate ED_Task_Step {
    .name = ED_TASK_STEP,
    .params = { {Param::String, "command", ""} }
};


bool TaskTemplate::set(const string& pname, bool value) {
    for (auto& p : params) {
        if (p.name == pname) {
            if (p.type == Param::Bool) {
                p.value = value;
                return true;
            }
            LOG(ERROR) << "Cannot assign bool value to parameter '" << p.name << "' of type " << enum_name<Param::Type>(p.type) << " in template " << this->name;
            return false;
        }
    }
    LOG(ERROR) << "Parameter '" << pname << "' not found in template " << this->name;
    return false;
}
bool TaskTemplate::set(const string& pname, int64_t value) {
    for (auto& p : params) {
        if (p.name == pname) {
            if (p.type == Param::Int) {
                p.value = value;
                return true;
            }
            if (p.type == Param::Real) {
                p.value = (double)value;
                return true;
            }
            LOG(ERROR) << "Cannot assign int value to parameter '" << p.name << "' of type " << enum_name<Param::Type>(p.type) << " in template " << this->name;
            return false;
        }
    }
    LOG(ERROR) << "Parameter '" << pname << "' not found in template " << this->name;
    return false;
}
bool TaskTemplate::set(const string& pname, double value) {
    for (auto& p : params) {
        if (p.name == pname) {
            if (p.type == Param::Int) {
                p.value = (int64_t)value;
                return true;
            }
            if (p.type == Param::Real) {
                p.value = value;
                return true;
            }
            LOG(ERROR) << "Cannot assign real value to parameter '" << p.name << "' of type " << enum_name<Param::Type>(p.type) << " in template " << this->name;
            return false;
        }
    }
    LOG(ERROR) << "Parameter '" << pname << "' not found in template " << this->name;
    return false;
}
bool TaskTemplate::set(const string& pname, const string& value) {
    for (auto& p : params) {
        if (p.name == pname) {
            if (!(p.type == Param::Int || p.type == Param::Real)) {
                p.value = value;
                return true;
            }
            LOG(ERROR) << "Cannot assign string value to parameter '" << p.name << "' of type " << enum_name<Param::Type>(p.type) << " in template " << this->name;
            return false;
        }
    }
    LOG(ERROR) << "Parameter '" << pname << "' not found in template " << this->name;
    return false;
}


void AIManager::initTemplates() {
    std::vector<TaskTemplate> templates {
            { .name = ED_TASK_MARKET_SELL_ALL,
              .params = {{Param::Int, "chunk", 0 }},
              .maxMisses = 3 },
            { .name = ED_TASK_MARKET_SELL,
              .params = {{Param::Commodity, "commodity", ""}, {Param::Int, "amount", 0 }, {Param::Int, "chunk", 0 }},
              .maxMisses = 3 },
            { .name = ED_TASK_DEPART, .maxMisses = 5 },
            { .name = ED_TASK_DOCK, .maxMisses = 5 },
            { .name = ED_TASK_TRAVEL,
              .params = {{Param::System, "system", ""}, {Param::Dock, "dock", ""}},
              .maxMisses = 5 },
            { .name = ED_TASK_JUMP_TO_SYSTEM,
              .params = {{Param::System, "system", ""}},
              .maxMisses = 5 },
            { .name = ED_TASK_CRUISE_TO_STATION,
              .params = {{Param::Dock, "dock", ""}},
              .maxMisses = 5 },

            { ED_TASK_CALIBRATE },
            { .name = ED_TASK_DEBUG_FIND_ALL_COMMODITIES,
              .params = {{Param::Bool, "shuffle", false}, {Param::Bool, "dump_images", false}, {Param::Int, "start_index", 0 }},
              .maxMisses = 3 },
            { .name = ED_TASK_DEBUG_FIND_ALL_NAV_POINTS,
              .params = {{Param::Bool, "dump_images", true}, {Param::Int, "ocr_confidence", 90 }, {Param::Int, "txt_confidence", 90 }, {Param::Bool, "resume", false}},
              .maxMisses = 3 },
            { .name = ED_TASK_DEBUG_ORIENT_NAV_TARGET,
              .params = {{Param::Real, "precision", 1.5 }},
              .maxMisses = 3 },

            //{ .name = ED_TASK_DEBUG_FIX_OCR,
            //  .params = {{Param::Bool, "use_tess", true}, {Param::Bool, "use_lstm", true}, {Param::Bool, "use_edr", true}, {Param::Bool, "rus+eng", true}},
            //  },
    };
    AllImplementedTasks.swap(templates);
    AllImplementedTaskRefs.reserve(AllImplementedTasks.size());
    for (auto& it : AllImplementedTasks) {
        AllImplementedTaskRefs.push_back(&it);
        AllImplementedTaskMap.insert({it.name, &it});
    }
}

const std::vector<TaskTemplate*>& AIManager::getTaskTemplates() {
    return AllImplementedTaskRefs;
}

const TaskTemplate& AIManager::getTaskTemplate(const std::string& name) {
    static TaskTemplate dummy;
    auto it = AllImplementedTaskMap.find(name);
    if (it == AllImplementedTaskMap.end())
        return dummy;
    return *(it->second);
}

} // namespace ai