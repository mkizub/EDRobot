//
// Created by mkizub on 19.06.2025.
//

#include "../pch.h"

#include "TaskTemplate.h"
#include "AIManager.h"
#include "TradeTasks.h"
#include "AutopilotTasks.h"
#include "TaskDebug.h"

namespace nav {

using enum ai::POIType;

// ✦ ☄ ☕ ⚾ ⚽ ⛬ ✇ ⛋ ⭖ ⧰ ⧖ ⛫ ☗ ☖ ✈ ⛴ ♻ ♲ ⏣ ⯐ ⌖ ⛏ ☀ ∇ ◇ ⬖ ◆

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
NavType FLEET_CARRIER { u'\u2708', // ✈
    FleetCarrier, {"nav_select_icon_carrier.png","nav_select_icon_carrier_bad.png"}, {"FleetCarrier", "Drake-Class Carrier"}
};
NavType SQUADRON_CARRIER { u'\u2708', // ⛴
    FleetCarrier, {"nav_select_icon_squadron.png"}, {"SquadronCarrier"}
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
NavType ERROR { u'\u2047', // ⁇
    None, {}, {}
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
        &FLEET_CARRIER, // ✈
        &SQUADRON_CARRIER, // ⛴
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

const std::string ED_TASK_REPEAT = "tsk-repeat";
const std::string ED_TASK_MARKET_SELL = "tsk-market-sell";
const std::string ED_TASK_MARKET_SELL_ALL = "tsk-market-sell-all";
const std::string ED_TASK_MARKET_BUY = "tsk-market-buy";
const std::string ED_TASK_MARKET_BUY_CONSTR = "tsk-market-buy-constr";
const std::string ED_TASK_CONSTR_UNLOAD = "tsk-constr-unload";
const std::string ED_TASK_AUTOPILOT = "tsk-autopilot";
const std::string ED_TASK_TRAVEL = "tsk-travel";
const std::string ED_TASK_NAV_SCAN = "tsk-nav-scan";

const std::string ED_TASK_CALIBRATE = "tsk-calibrate";
const std::string ED_TASK_DEBUG_FIND_ALL_COMMODITIES = "tsk-debug-find-all-commodities";
const std::string ED_TASK_DEBUG_FIND_ALL_NAV_POINTS = "tsk-debug-find-all-nav-points";
const std::string ED_TASK_DEBUG_AUTOPILOT = "tsk-debug-autopilot";

bool TaskTemplate::set(const string& pname, bool value) {
    for (auto& p : params) {
        if (p.name == pname) {
            if (p.type == Param::Bool) {
                p.value = value;
                return true;
            }
            LOG(ERROR) << "Cannot assign bool value to parameter '" << p.name << "' of type " << enum_name<Param::Type>(p.type) << " in template " << this->id;
            return false;
        }
    }
    LOG(ERROR) << "Parameter '" << pname << "' not found in template " << this->id;
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
            LOG(ERROR) << "Cannot assign int value to parameter '" << p.name << "' of type " << enum_name<Param::Type>(p.type) << " in template " << this->id;
            return false;
        }
    }
    LOG(ERROR) << "Parameter '" << pname << "' not found in template " << this->id;
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
            LOG(ERROR) << "Cannot assign real value to parameter '" << p.name << "' of type " << enum_name<Param::Type>(p.type) << " in template " << this->id;
            return false;
        }
    }
    LOG(ERROR) << "Parameter '" << pname << "' not found in template " << this->id;
    return false;
}
bool TaskTemplate::set(const string& pname, const string& value) {
    for (auto& p : params) {
        if (p.name == pname) {
            if (!(p.type == Param::Int || p.type == Param::Real)) {
                p.value = value;
                return true;
            }
            LOG(ERROR) << "Cannot assign string value to parameter '" << p.name << "' of type " << enum_name<Param::Type>(p.type) << " in template " << this->id;
            return false;
        }
    }
    LOG(ERROR) << "Parameter '" << pname << "' not found in template " << this->id;
    return false;
}

bool TaskTemplate::validate() {
    bool valid = true;
    for (auto& param : params) {
        bool ok = false;
        std::string text;
        try {
            switch (param.type) {
            case Param::Bool:
                ok = true;
                break;
            case Param::Enum:
                text = std::get<std::string>(param.value);
                ok = (text.empty() && param.optional) || std::ranges::contains(split(param.meta, '|'), text);
                break;
            case Param::Int:
                ok = std::get<int64_t>(param.value) >= 0 || param.optional;
                break;
            case Param::Real:
                ok = std::isfinite(std::get<double>(param.value)) || param.optional;
                break;
            case Param::String:
            case Param::System:
            case Param::POI:
            case Param::Dock:
                text = std::get<std::string>(param.value);
                ok = !text.empty() || param.optional;
                break;
            case Param::Commodity:
                text = std::get<std::string>(param.value);
                if (text.empty())
                    ok = param.optional;
                else
                    ok = Cfg.getCommodityByName(text, false) != nullptr;
                break;
            }
        } catch (const std::exception &ex) {
            ok = false;
        }
        if (!ok)
            valid = false;
    }
    return valid;
}

namespace {
std::list<TaskTemplate> AllTasks;
std::list<TaskTemplate> AllTaskTemplates;
std::map<std::string, TaskTemplate *> TaskTemplateMap;
}

TaskTemplate loadTemplate(const json5pp::value& j_task);
void loadSavedTasks();

void initTemplates() {
    std::list<TaskTemplate> templates = {
            {
                .id = ED_TASK_AUTOPILOT,
                .name = _("Autopilot"),
                .factory = [](const TaskTemplate& templ) { return new Autopilot(templ); }
            },
            {
                .id = ED_TASK_MARKET_SELL_ALL,
                .name = _("Sell all cargo commodities"),
                .params = {{Param::Int, "chunk", 0 }},
                .factory = [](const TaskTemplate& templ) { return new TaskSellAll(templ); }
            },
            {
                .id = ED_TASK_MARKET_SELL,
                .name = _("Sell commodity"),
                .params = {{Param::Commodity, "commodity", ""}, {Param::Int, "amount", 0 }, {Param::Int, "chunk", 0 }},
                .factory = [](const TaskTemplate& templ) { return new TaskSell(templ); }
            },
            {
                .id = ED_TASK_MARKET_BUY,
                .name = _("Buy commodity"),
                .params = {{Param::Commodity, "commodity", ""}, {Param::Int, "amount", 0 }},
                .factory = [](const TaskTemplate& templ) { return new TaskBuy(templ); }
            },
            {
                .id = ED_TASK_MARKET_BUY_CONSTR,
                .name = _("Buy all for construction"),
                .params = {{Param::System, "system", ""}, {Param::Dock, "depot", ""}},
                .factory = [](const TaskTemplate& templ) { return new TaskBuyConstr(templ); }
            },
            {
                .id = ED_TASK_CONSTR_UNLOAD,
                .name = _("Unload cargo at depot"),
                .factory = [](const TaskTemplate& templ) { return new TaskConstrUnload(templ); }
            },
            {
                .id = ED_TASK_TRAVEL,
                .name = _("Travel to dock"),
                .params = {{Param::System, "system", ""}, {Param::Dock, "dock", ""}},
                .factory = [](const TaskTemplate& templ) { return new TaskTravel(templ); }
            },
            {
                .id = ED_TASK_REPEAT,
                .name = _("Repeat"),
                .params = {{Param::Int, "count", 0, "", true}, {Param::Int, "duration", 0, "", true}},
                .factory = [](const TaskTemplate& templ) { return new TaskRepeat(templ); }
            },
            {
                .id = ED_TASK_DEBUG_FIND_ALL_COMMODITIES,
                .name = _("Debug: find all commodities"),
                .params = {{Param::Bool, "shuffle", false}, {Param::Bool, "dump_images", false}, {Param::Int, "start_index", 0 }},
                .factory = [](const TaskTemplate& templ) { return new TaskDebugFindAllCommodities(templ); }
            },
            {
                .id = ED_TASK_DEBUG_FIND_ALL_NAV_POINTS,
                .name = _("Debug: find all nav points"),
                .params = {{Param::Bool, "dump_images", true}, {Param::Int, "ocr_confidence", 90 }, {Param::Int, "txt_confidence", 90 }, {Param::Bool, "resume", false}, {Param::Bool, "unfocused", false}},
                .factory = [](const TaskTemplate& templ) { return new TaskDebugFindAllNavPoints(templ); }
            },
            {
                .id = ED_TASK_DEBUG_AUTOPILOT,
                .name = _("Debug: autopilot steps"),
                .params = {
                    {Param::Enum, "test", "", "OrientTowards|OrientAway|KeepCourse|Departure|DockSpaceStation|DockPlanetPort|"
                                              "EnterCruise|HyperJump|LeaveBody|FocusDestDock|FocusDestBody|FocusNearestBody|GalMapNavRoute|FocusTopEntry|"
                                              "NavDockSelect|NavBodySelect|CruiseToDist|DiveUnderPlanet|ExitCruiseToSpace|ExitCruiseToPlanet" },
                    {Param::String, "target", "", "", true},
                    {Param::Real, "precision", 1.0, "", true }
                },
                .factory = [](const TaskTemplate& templ) { return new TaskDebugAutopilot(templ); }
            }
    };

    AllTaskTemplates.swap(templates);
    for (auto& it : AllTaskTemplates) {
        TaskTemplateMap.insert({it.id, &it});
    }

    loadSavedTasks();
}

TaskTemplate loadTemplate(const json5pp::value& j_task) {
    auto& templ_id = j_task.at("templ","").as_string();
    auto* templ = getTaskTemplate(templ_id);
    if (!templ) {
        LOG(ERROR) << "Task template '" << templ_id << "' not found";
        return {};
    }
    auto task = *templ;
    if (j_task["name"].is_string()) {
        task.name = j_task["name"].as_string();
    }
    bool ok = true;
    for (auto& p : task.params) {
        auto& jp = j_task[p.name];
        if (jp.is_null()) {
            if (p.optional)
                continue;
            ok = false;
            LOG(ERROR) << "Missing required parameter '" << p.name << "' of task template " << task.id;
            continue;
        }
        else if (jp.is_boolean()) {
            if (!task.set(p.name, jp.as_boolean()))
                ok = false;
        }
        else if (jp.is_integer()) {
            if (!task.set(p.name, jp.as_int64()))
                ok = false;
        }
        else if (jp.is_number()) {
            if (!task.set(p.name, jp.as_number()))
                ok = false;
        }
        else if (jp.is_string()) {
            if (!task.set(p.name, jp.as_string()))
                ok = false;
        }
        else {
            LOG(ERROR) << "Expecting parameter '" << p.name << "' with type " << enum_name<Param::Type>(p.type) << " of task template " << task.id;
            ok = false;
        }
    }
    if (!ok)
        return {};
    if (!task.validate())
        return {};
    if (j_task["steps"].is_array()) {
        for (auto& j_step : j_task["steps"].as_array()) {
            TaskTemplate step = loadTemplate(j_step);
            if (step.id.empty())
                return {};
            task.steps.push_back(step);
        }
    }
    return task;
}

void loadSavedTasks() {
    LOG(INFO) << "Loading tasks.json5";
    json5pp::value j_tasks;
    try {
        std::ifstream tasksFile("tasks.json5", std::ifstream::in);
        if (tasksFile.fail()) {
            LOG(ERROR) << "Cannot read file: tasks.json5";
        } else {
            j_tasks = json5pp::parse5(tasksFile);
        }
        tasksFile.close();
    } catch (...) {
        LOG(ERROR) << "Failed to read/parse tasks.json5";
    }
    if (!j_tasks || !j_tasks.is_array())
        return;
    for (const auto& jtt : j_tasks.as_array()) {
        TaskTemplate task = loadTemplate(jtt);
        if (!task.id.empty())
            AllTasks.push_back(task);
    }
}

const std::list<TaskTemplate>& getUserTasks() {
    return AllTasks;
}

const std::list<TaskTemplate>& getTemplates() {
    return AllTaskTemplates;
}

const TaskTemplate* getTaskTemplate(const std::string& name) {
    auto it = TaskTemplateMap.find(name);
    if (it == TaskTemplateMap.end())
        return nullptr;
    return it->second;
}

} // namespace ai