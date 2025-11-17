//
// Created by mkizub on 19.06.2025.
//

#include "../pch.h"

#include "TaskTemplate.h"
#include "AIManager.h"
#include "TradeTasks.h"
#include "AutopilotTasks.h"
#include "TaskDebug.h"

namespace ai {

const std::string ED_TASK_REPEAT = "tsk-repeat";
const std::string ED_TASK_MARKET_SELL = "tsk-market-sell";
const std::string ED_TASK_MARKET_SELL_ALL = "tsk-market-sell-all";
const std::string ED_TASK_MARKET_BUY = "tsk-market-buy";
const std::string ED_TASK_MARKET_BUY_CONSTR = "tsk-market-buy-constr";
const std::string ED_TASK_CONSTR_UNLOAD = "tsk-constr-unload";
const std::string ED_TASK_TRADE_AT = "tsk-trade-at";
const std::string ED_TASK_TRADE_LOOP = "tsk-trade-loop";
const std::string ED_TASK_AUTOPILOT = "tsk-autopilot";
const std::string ED_TASK_TRAVEL = "tsk-travel";
const std::string ED_TASK_NAV_SCAN = "tsk-nav-scan";

const std::string ED_TASK_CALIBRATE = "tsk-calibrate";
const std::string ED_TASK_DEBUG_FIND_ALL_COMMODITIES = "tsk-debug-find-all-commodities";
const std::string ED_TASK_DEBUG_FIND_ALL_NAV_POINTS = "tsk-debug-find-all-nav-points";
const std::string ED_TASK_DEBUG_AUTOPILOT = "tsk-debug-autopilot";

static Param dummy_param {};
Param& TaskTemplate::get(const string& pid) {
    for (auto& p : params) {
        if (p.id == pid)
            return p;
    }
    return dummy_param;
}
const Param& TaskTemplate::get(const string& pid) const {
    for (auto& p : params) {
        if (p.id == pid)
            return p;
    }
    return dummy_param;
}
bool TaskTemplate::set(const string& pid, const json5pp::value& value) {
    auto& p = get(pid);
    if (p.type == Param::Void) {
        LOG(ERROR) << "No parameter '" << pid << "' in template " << this->id;
        return false;
    }
    if (p.set(value))
        return true;
    LOG(ERROR) << "Cannot assign int value to parameter '" << pid << "' of type " << enum_name<Param::Type>(p.type) << " in template " << this->id;
    return false;
}
bool Param::set(const json5pp::value& val) {
    if (val.is_null() || (val.is_string() && val.as_string().empty())) {
        value = nullptr;
        return true;
    }
    switch (type) {
    case Void:
        return false;
    case Bool:
        if (val.is_boolean()) {
            value = val;
            return true;
        }
        if (val.is_string()) {
            if (val.as_string() == "true") {
                value = true;
                return true;
            }
            else if (val.as_string() == "false") {
                value = true;
                return true;
            }
        }
        break;
    case Enum:
        if (val.is_string()) {
            auto& text = val.as_string();
            auto arr = meta["values"].as_array();
            for (auto v: arr) {
                if (v.is_string() && text == v.as_string()) {
                    value = v;
                    return true;
                } else if (v["id"].is_string() && text == v["id"].as_string()) {
                    value = v["id"];
                    return true;
                }
            }
            return false;
        }
        if (val.is_integer()) {
            int idx = val.as_integer();
            auto arr = meta["values"].as_array();
            if (idx < 0 || idx >= arr.size())
                return false;
            auto& v = arr[idx];
            if (v.is_string()) {
                value = v;
                return true;
            } else if (v["id"].is_string()) {
                value = v["id"];
                return true;
            }
            return false;
        }
        break;
    case Int:
        if (val.is_integer()) {
            value = val;
            return true;
        }
        if (val.is_string()) {
            try {
                value = std::stoll(val.as_string());
                return true;
            } catch (...) {}
        }
        break;
    case Real:
        if (val.is_number()) {
            value = val;
            return true;
        }
        if (val.is_string()) {
            try {
                value = std::stod(val.as_string());
                return true;
            } catch (...) {}
        }
        break;
    case String:
    case System:
    case Dock:
        if (val.is_string()) {
            value = val;
            return true;
        }
        break;
    case Commodity:
        if (val.is_string()) {
            auto& text = val.as_string();
            auto* commodity = Cfg.getCommodityByName(text, false);
            if (commodity) {
                value = commodity->nameId;
                return true;
            }
        }
        break;
    case Task:
        if (val.is_object() && val["templ"].is_string()) {
            auto& templ_id = val["templ"].as_string();
            auto arr = meta["values"].as_array();
            for (auto v: arr) {
                if (v.is_string() && templ_id == v.as_string()) {
                    value = v;
                    return true;
                }
            }
        }
        break;
    case Array:
        if (val.is_array()) {
            value = val;
            return true;
        }
        break;
    }
    LOG(ERROR) << "Cannot assign to parameter '" << id << "' of type " << enum_name<Param::Type>(type) << " value " << val;
    return false;
}

std::string TaskTemplate::name() const {
    if (!nm.empty())
        return gettext(nm.c_str());
    return id;
}

bool TaskTemplate::validate() const {
    bool valid = true;
    for (auto& p : params) {
        if (!p.valid())
            valid = false;
    }
    return valid;
}

namespace {
std::list<TaskTemplate> AllTasks;
std::list<TaskTemplate> AllTaskTemplates;
std::map<std::string, TaskTemplate *> TaskTemplateMap;
}


std::string Param::name() const {
    if (!nm.empty())
        return gettext(nm.c_str());
    return id;
}

std::string Param::placeholder() const {
    if (meta["placeholder"].is_string())
        return gettext(meta["placeholder"].as_string().c_str());
    return {};
}

bool Param::optional() const {
    return meta["optional"];
}

bool Param::empty() const {
    if (value.is_null())
        return true;
    if (value.is_string() && value.as_string().empty())
        return true;
    if (value.is_object() && value.as_object().empty())
        return true;
    if (value.is_array() && value.as_array().empty())
        return true;
    return false;
}

bool Param::as_boolean() const {
    return bool(value);
}
int Param::as_integer() const {
    if (!value.is_number())
        return 0;
    return value.as_integer();
}
int32_t Param::as_int32() const {
    if (!value.is_number())
        return 0;
    return value.as_int32();
}
int64_t Param::as_int64() const {
    if (!value.is_number())
        return 0;
    return value.as_int64();
}
double Param::as_number() const {
    if (!value.is_number())
        return 0;
    return value.as_number();
}
std::string Param::as_string() const {
    if (value.is_string())
        return value.as_string();
    if (value.is_null())
        return {};
    if (value.is_integer())
        return std::to_string(value.as_int64());
    if (value.is_number())
        return std::to_string(value.as_number());
    if (value.is_boolean())
        return value.as_boolean() ? "true" : "false";
    return {};
}
::Commodity* Param::as_commodity() const {
    if (value.is_string()) {
        auto& text = value.as_string();
        return Cfg.getCommodityByName(text, false);
    }
    return nullptr;
}


bool Param::valid() const {
    if (empty() && optional())
        return true;
    bool valid = false;
    std::string text;
    try {
        switch (type) {
        case Param::Void:
            valid = false;
            break;
        case Param::Bool:
            valid = true;
            break;
        case Param::Enum:
            if (value.is_string()) {
                text = value.as_string();
                if (text.empty()) {
                    valid = optional();
                } else {
                    auto arr = meta["values"].as_array();
                    for (auto v: arr) {
                        if (v.is_string() && text == v.as_string()) {
                            valid = true;
                            break;
                        } else if (v["id"].is_string() && text == v["id"].as_string()) {
                            valid = true;
                            break;
                        }
                    }
                }
            }
            break;
        case Param::Int:
            if (value.is_integer()) {
                int64_t v = value.as_uint64();
                valid = true;
                if (meta["min"].is_integer() && v < meta["min"].as_integer())
                    valid = false;
                if (meta["max"].is_integer() && v > meta["max"].as_integer())
                    valid = false;
            }
            break;
        case Param::Real:
            if (value.is_number()) {
                double v = value.as_number();
                valid = std::isfinite(v) && !std::isnan(v);
                if (meta["min"].is_number() && v < meta["min"].as_number())
                    valid = false;
                if (meta["max"].is_number() && v > meta["max"].as_number())
                    valid = false;
            }
            break;
        case Param::String:
        case Param::System:
        case Param::Dock:
            if (value.is_string())
                valid = !value.as_string().empty();
            break;
        case Param::Commodity:
            if (value.is_string()) {
                text = value.as_string();
                valid = Cfg.getCommodityByName(text, false) != nullptr;
            }
            break;
        case Param::Task:
            if (value.is_object() && value["templ"].is_string()) {
                TaskTemplate test = TaskTemplate::loadTemplate(value);
                if (!test.id.empty() && test.validate()) {
                    for (auto v : meta["values"].as_array()) {
                        if (v.is_string() && test.id == v.as_string()) {
                            return true;
                        }
                    }
                }
            }
            break;
        case Param::Array:
            if (value.is_array()) {
                auto el_meta = meta["elements"];
                Type el_type = enum_cast<Param::Type>(el_meta["type"]).value();
                auto size = value.as_array().size();
                bool ok = true;
                for (auto i=0; i < size; i++) {
                    Param p {el_type, "", "", el_meta, value[i]};
                    if (!p.valid())
                        ok = false;
                }
                valid = ok;
            }
            break;
        }
    } catch (const std::exception &ex) {
        valid = false;
    }
    return valid;
}


json5pp::value META(std::initializer_list<json5pp::value::pair_type> elements) {
    return json5pp::object(elements);
}
json5pp::value META(const char* meta) {
    return json5pp::parse5(meta);
}

#define FACTORY(TYPE) [](const TaskTemplate &templ) { return new TYPE(templ); }

void initTemplates() {
    typedef std::vector<Param> P;
    const json5pp::value OPT {{"optional", true}};
    std::list<TaskTemplate> templates;
    templates.emplace_back(ED_TASK_AUTOPILOT, _lc("Autopilot"), FACTORY(Autopilot));
    templates.emplace_back(ED_TASK_TRADE_AT, _lc("Trade at station"), FACTORY(TaskTradeAt), P{
            { Param::System,   "system", _lc("Star system") },
            { Param::Dock,     "dock",   _lc("Dock") },
            { Param::Commodity,"sell",   _lc("Sell commodity"), META("{optional:true, placeholder:'all'}") },
            { Param::Commodity,"buy",    _lc("Buy commodity"), META("{optional:true, placeholder:'all'}") },
    });
    templates.emplace_back(ED_TASK_TRADE_LOOP, _lc("Trade loop"), FACTORY(TradeLoopTask), P{
            { Param::Array, "markets", _lc("Markets"),
              META(R"({elements:{type:'Task', values: ['tsk-market-trade-at']}})")},
    });
    templates.emplace_back(ED_TASK_MARKET_SELL_ALL, _lc("Sell all cargo commodities"), FACTORY(TaskSellAll), P{
            { Param::Int,   "chunk",  _lc("By chunk"), META("{optional:true, placeholder:'all', max:100}") },
            { Param::Array, "except", _lc("Except"),   META("{optional:true, elements:{type:'Commodity'}}")},
    });
    templates.emplace_back(ED_TASK_MARKET_SELL, _lc("Sell commodity"), FACTORY(TaskSell), P{
            { Param::Commodity, "commodity", _lc("Commodity") },
            { Param::Int,       "amount",    _lc("Amount"),   META("{optional:true, placeholder:'all'}") },
            { Param::Int,       "chunk",     _lc("By chunk"), META("{optional:true, placeholder:'all', max:100}") },
    });
    templates.emplace_back(ED_TASK_MARKET_BUY, _lc("Buy commodity"), FACTORY(TaskBuy), P{
            { Param::Commodity, "commodity", _lc("Commodity") },
            { Param::Int,       "amount",    _lc("Amount"),   META("{optional:true, placeholder:'all'}") },
    });
    templates.emplace_back(ED_TASK_MARKET_BUY_CONSTR, _lc("Buy for construction"), FACTORY(TaskBuyConstr), P{
            { Param::System,   "system", _lc("Star system") },
            { Param::Dock,     "dock",   _lc("Construction depot") },
    });
    templates.emplace_back(ED_TASK_CONSTR_UNLOAD, _lc("Unload cargo at depot"), FACTORY(TaskConstrUnload));
    templates.emplace_back(ED_TASK_TRAVEL, _lc("Travel to dock"), FACTORY(TaskTravel), P{
            { Param::System,   "system", _lc("Star system") },
            { Param::Dock,     "dock",   _lc("Dock") },
    });
    templates.emplace_back(ED_TASK_REPEAT, _lc("Repeat"), FACTORY(TaskRepeat), P{
            { Param::Int, "count",    _lc("Times"),              META("{optional:true, placeholder:'infinit'}")  },
            { Param::Int, "duration", _lc("Duration (minutes)"), META("{optional:true, placeholder:'infinit'}")  },
            { Param::Array, "tasks",  _lc("Tasks"),              META(
                    R"({elements:{type:'Task', values: [
                        'tsk-travel', 'tsk-market-trade-at',
                        'tsk-market-sell-all', 'tsk-market-sell', 'tsk-market-buy',
                        'tsk-market-buy-constr', 'tsk-constr-unload',
                    ]}})")},
    });
    templates.emplace_back(ED_TASK_DEBUG_FIND_ALL_COMMODITIES, _lc("Debug: find all commodities"), FACTORY(TaskDebugFindAllCommodities), P{
            { Param::Bool, "shuffle" },
            { Param::Bool, "dump_images" },
            { Param::Int,  "start_index", "", OPT },
    });
    templates.emplace_back(ED_TASK_DEBUG_FIND_ALL_NAV_POINTS, _lc("Debug: find all nav points"), FACTORY(TaskDebugFindAllNavPoints), P{
            { Param::Bool, "dump_images" },
            { Param::Int,  "ocr_confidence", "", META({{"max",100}}), 90 },
            { Param::Int,  "txt_confidence", "", META({{"max",100}}), 90 },
            { Param::Bool, "resume" },
            { Param::Bool, "unfocused" },
    });
    templates.emplace_back(ED_TASK_DEBUG_AUTOPILOT, _lc("Debug: autopilot steps"), FACTORY(TaskDebugAutopilot), P{
            { Param::Enum, "test", _lc("Test"), META(R"({placeholder:'select the test', values: [
                         "OrientTowards", "OrientAway", "KeepCourse", "Departure", "DockSpaceStation",
                         "DockPlanetPort", "EnterCruise", "HyperJump", "LeaveBody", "FocusDestDock",
                         "FocusDestBody", "FocusNearestBody", "GalMapNavRoute", "FocusTopEntry",
                         "NavDockSelect", "NavBodySelect", "CruiseToDist", "DiveUnderPlanet",
                         "ExitCruiseToSpace", "ExitCruiseToPlanet", "ForwardAccelerate", "ReverseAccelerate"
                       ]})")},
            { Param::String,  "target", _lc("Target"), OPT },
            { Param::Real,    "value",  _lc("Value"),  OPT, 1.0 },
    });

    AllTaskTemplates.swap(templates);
    for (auto& it : AllTaskTemplates) {
        TaskTemplateMap.insert({it.id, &it});
    }

    TaskTemplate::loadSavedTasks();
}

TaskTemplate TaskTemplate::loadTemplate(const json5pp::value& j_task) {
    auto& templ_id = j_task.at("templ","").as_string();
    auto* templ = getTaskTemplate(templ_id);
    if (!templ) {
        LOG(ERROR) << "Task template '" << templ_id << "' not found";
        return {};
    }
    auto task = *templ;
    if (j_task["name"].is_string()) {
        const_cast<std::string&>(task.nm) = j_task["name"].as_string();
    }
    bool ok = true;
    for (auto& p : task.params) {
        auto& jp = j_task[p.id];
        if (jp.is_null()) {
            if (p.optional())
                continue;
            ok = false;
            LOG(ERROR) << "Missing required parameter '" << p.id << "' of task template " << task.id;
            continue;
        }
        else if (!p.set(jp)) {
            LOG(ERROR) << "Failed to set parameter " << p.id << " with " << jp;
            ok = false;
        }
    }
    if (!task.validate()) {
        LOG(ERROR) << "Task id:'" << task.id << "' name:'" << task.nm << "' is not valid";
        return {};
    }
    return task;
}

void TaskTemplate::loadSavedTasks() {
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