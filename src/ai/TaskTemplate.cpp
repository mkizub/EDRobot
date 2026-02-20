//
// Created by mkizub on 19.06.2025.
//

#include "../pch.h"

#include "TaskTemplate.h"
#include "AIManager.h"
#include "TradeTasks.h"
#include "AutopilotTasks.h"
#include "TaskDebug.h"
#include "ContactsTasks.h"
#include "CarrierTasks.h"
#include "ColonizationTasks.h"

namespace ai {

const std::string ED_TASK_REPEAT = "tsk-repeat";
const std::string ED_TASK_MARKET_SELL = "tsk-market-sell";
const std::string ED_TASK_MARKET_SELL_ALL = "tsk-market-sell-all";
const std::string ED_TASK_MARKET_BUY = "tsk-market-buy";
const std::string ED_TASK_MARKET_BUY_ALL = "tsk-market-buy-all";
const std::string ED_TASK_MARKET_BUY_CONSTR = "tsk-market-buy-constr";
const std::string ED_TASK_CARRIER_UNLOAD = "tsk-carrier-unload";
const std::string ED_TASK_CONSTR_UNLOAD = "tsk-constr-unload";
const std::string ED_TASK_CONSTR_RESERVE = "tsk-constr-reserve";
const std::string ED_TASK_CONSTR_BUILD = "tsk-constr-build";
const std::string ED_TASK_ACQUIRE_PPC = "tsk-contact-acquire-ppc";
const std::string ED_TASK_DELIVER_PPC = "tsk-contact-deliver-ppc";
const std::string ED_TASK_TRADE_AT = "tsk-trade-at";
const std::string ED_TASK_TRADE_LOOP = "tsk-trade-loop";
const std::string ED_TASK_AUTOPILOT = "tsk-autopilot";
const std::string ED_TASK_TRAVEL = "tsk-travel";
const std::string ED_TASK_NAV_SCAN = "tsk-nav-scan";
const std::string ED_TASK_RESURRECT = "tsk-resurrect";

const std::string ED_TASK_DEBUG_FIND_ALL_COMMODITIES = "tsk-debug-find-all-commodities";
const std::string ED_TASK_DEBUG_FIND_ALL_NAV_POINTS = "tsk-debug-find-all-nav-points";
const std::string ED_TASK_DEBUG_AUTOPILOT = "tsk-debug-autopilot";
const std::string ED_TASK_DEBUG_SHIP_STATS = "tsk-debug-ship-stats";

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

bool TaskTemplate::operator==(const TaskTemplate& other) const {
    if (id != other.id)
        return false;
    if (name() != other.name())
        return false;
    for (int i=0; i < params.size(); i++)
        if (params[i] != other.params[i])
            return false;
    return true;
}

bool Param::operator==(const Param& other) const {
    if (type != other.type || id != other.id)
        return false;
    if (value.empty() && other.value.empty())
        return true;
    return (value == other.value);
}

bool Param::set(const json5pp::value& val, bool silent) {
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
            auto& str = val.as_string();
            int64_t result = 0;
            if (parseInt(str, result)) {
                value = result;
                return true;
            }
        }
        break;
    case Real:
        if (val.is_number()) {
            value = val;
            return true;
        }
        if (val.is_string()) {
            auto& str = val.as_string();
            double result = 0;
            if (parseReal(str, result)) {
                value = result;
                return true;
            }
        }
        break;
    case String:
        if (val.is_string()) {
            value = val;
            return true;
        }
        break;
    case Site:
        if (val.is_object()) {
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
            value = val;
            return true;
        }
        break;
    case Array:
        if (val.is_array()) {
            value = val;
            return true;
        }
        break;
    }
    if (!silent)
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
std::list<TaskTemplate> AllUserTasks;
std::list<TaskTemplate> AllTaskTemplates;
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
    return bool(meta["optional"]);
}

bool Param::empty() const {
    return value.empty();
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
            if (value.is_string())
                valid = !value.as_string().empty();
            break;
        case Param::Site:
            if (value.is_object()) {
                if (value["system"].is_string() && value["dock"].is_string())
                    valid = !value["system"].empty() && !value["dock"].empty();
            }
            break;
        case Param::Commodity:
            if (value.is_string()) {
                text = value.as_string();
                valid = Cfg.getCommodityByName(text, false) != nullptr;
            }
            break;
        case Param::Task:
            if (value.is_object() && value["templ"].is_string()) {
                TaskTemplate test = TaskTemplate::loadTask(value);
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
                Type el_type = enum_cast<Param::Type>(el_meta["type"].as_string()).value();
                bool empty = true;
                bool ok = true;
                for (auto& v : value.as_array()) {
                    if (v.empty())
                        continue;
                    empty = false;
                    Param p {el_type, "", "", el_meta, v};
                    if (!p.valid())
                        ok = false;
                }
                valid = ok;
                if (empty)
                    valid = optional();
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
            { Param::Site,     "market", _lc("Market") },
            { Param::Array,    "tasks",  _lc("Tasks"), META(
                    R"({optional:true, elements:{type:'Task', values: [
                        'tsk-market-sell-all', 'tsk-market-sell',
                        'tsk-market-buy-all', 'tsk-market-buy',
                        'tsk-market-buy-constr', 'tsk-constr-unload', "tsk-carrier-unload",
                        'tsk-contact-acquire-ppc', 'tsk-contact-deliver-ppc',
                    ]}})")},
    });
    templates.emplace_back(ED_TASK_TRADE_LOOP, _lc("Trade loop"), FACTORY(TradeLoopTask), P{
            { Param::Array, "markets", _lc("Markets"),
              META(R"({elements:{type:'Task', values: ['tsk-trade-at']}})")},
    });
    templates.emplace_back(ED_TASK_MARKET_SELL_ALL, _lc("Sell everything"), FACTORY(TaskSellAll), P{
            { Param::Int,   "chunk",  _lc("By chunk"), META("{optional:true, placeholder:'all', max:100}") },
            { Param::Array, "except", _lc("Except"),   META("{optional:true, elements:{type:'Commodity'}}")},
    });
    templates.emplace_back(ED_TASK_MARKET_SELL, _lc("Sell"), FACTORY(TaskSell), P{
            { Param::Commodity, "commodity", _lc("Commodity") },
            { Param::Int,       "amount",    _lc("Amount"),   META("{optional:true, placeholder:'all'}") },
            { Param::Int,       "chunk",     _lc("By chunk"), META("{optional:true, placeholder:'all', max:100}") },
    });
    templates.emplace_back(ED_TASK_MARKET_BUY_ALL, _lc("Buy all"), FACTORY(TaskBuyAll), P{
            { Param::Array,     "commodity", _lc("Commodity"), META("{elements:{type:'Commodity'}}")},
    });
    templates.emplace_back(ED_TASK_MARKET_BUY, _lc("Buy"), FACTORY(TaskBuy), P{
            { Param::Commodity, "commodity", _lc("Commodity") },
            { Param::Int,       "amount",    _lc("Amount"),   META("{optional:true, placeholder:'all'}") },
    });
    templates.emplace_back(ED_TASK_MARKET_BUY_CONSTR, _lc("Buy for construction"), FACTORY(TaskBuyConstr), P{
            { Param::Site,     "depot",  _lc("Construction depot") },
            { Param::Bool,     "carrier",_lc("Consider Fleet Carrier")  },
            { Param::Enum,     "mode",   _lc("Mode"), META({{"values", json5pp::array({
                json5pp::object({{"id", "ListLittleFirst"},   {"name",  _lc("Listed, then Little first")}}),
                json5pp::object({{"id", "ListBulkFirst"},     {"name",  _lc("Listed, then Bulk first")}}),
                json5pp::object({{"id", "ExceptLittleFirst"}, {"name",  _lc("Except listed, Little first")}}),
                json5pp::object({{"id", "ExceptBulkFirst"},   {"name",  _lc("Except listed, Bulk first")}}),
                json5pp::object({{"id", "OnlyLittleFirst"},   {"name",  _lc("Only listed, Little first")}}),
                json5pp::object({{"id", "OnlyBulkFirst"},     {"name",  _lc("Only listed, Bulk first")}})
            })}}), "ListLittleFirst"},
            { Param::Array,    "commodity", _lc("Commodity"),   META("{optional:true, elements:{type:'Commodity'}}")},
    });
    templates.emplace_back(ED_TASK_CARRIER_UNLOAD, _lc("Unload cargo to own carrier"), FACTORY(TaskMyCarrierUnload));
    templates.emplace_back(ED_TASK_CONSTR_UNLOAD, _lc("Unload cargo at depot"), FACTORY(TaskConstrUnload));
    templates.emplace_back(ED_TASK_CONSTR_RESERVE, _lc("Fill carrier for constructions"), FACTORY(TaskMyCarrierReserve), P{
            { Param::Site,     "carrier",_lc("My carrier location") },
            { Param::Array,    "depots",  _lc("Construction depots"), META(
                    R"({elements:{type:'Site'}})")},
            { Param::Array,    "markets",  _lc("Markets"), META(
                    R"({elements:{type:'Site'}})")},
            { Param::Enum,     "mode",   _lc("Mode"), META({{"values", json5pp::array({
                json5pp::object({{"id", "FirstListed"},  {"name",  _lc("First listed")}}),
                json5pp::object({{"id", "ExceptListed"}, {"name",  _lc("Except listed")}}),
                json5pp::object({{"id", "OnlyListed"},   {"name",  _lc("Only listed")}}),
            })}}), "FirstListed"},
            { Param::Array,    "commodity", _lc("Commodity"),   META("{optional:true, elements:{type:'Commodity'}}")},
    });
    templates.emplace_back(ED_TASK_CONSTR_BUILD, _lc("Build constructions"), FACTORY(TaskConstruction), P{
            { Param::Site,     "depot",   _lc("Construction depot") },
            { Param::Array,    "markets", _lc("Markets"), META(
                    R"({elements:{type:'Site'}})")},
            { Param::Enum,     "mode",   _lc("Mode"), META({{"values", json5pp::array({
                json5pp::object({{"id", "FirstListed"},  {"name",  _lc("First listed")}}),
                json5pp::object({{"id", "ExceptListed"}, {"name",  _lc("Except listed")}}),
                json5pp::object({{"id", "OnlyListed"},   {"name",  _lc("Only listed")}}),
            })}}), "FirstListed"},
            { Param::Array,    "commodity", _lc("Commodity"),   META("{optional:true, elements:{type:'Commodity'}}")},
    });
    templates.emplace_back(ED_TASK_ACQUIRE_PPC, _lc("Acquire PowerPlay resource"), FACTORY(TaskAcquirePPC), P{
            { Param::Commodity, "commodity", _lc("Commodity") },
    });
    templates.emplace_back(ED_TASK_DELIVER_PPC, _lc("Deliver PowerPlay resource"), FACTORY(TaskDeliverPPC), P{
            { Param::Commodity, "commodity", _lc("Commodity") },
    });
    templates.emplace_back(ED_TASK_TRAVEL, _lc("Travel to dock"), FACTORY(TaskTravel), P{
            { Param::Site,     "dock",   _lc("Dock") },
    });
    templates.emplace_back(ED_TASK_REPEAT, _lc("Repeat"), FACTORY(TaskRepeat), P{
            { Param::Int, "count",    _lc("Times"),              META("{optional:true, placeholder:'infinit'}")  },
            { Param::Int, "duration", _lc("Duration (minutes)"), META("{optional:true, placeholder:'infinit'}")  },
            { Param::Array, "tasks",  _lc("Tasks"),              META(
                    R"({elements:{type:'Task', values: [
                        'tsk-travel', 'tsk-trade-at',
                        'tsk-market-sell-all', 'tsk-market-sell', 'tsk-market-buy',
                        'tsk-market-buy-constr', 'tsk-constr-unload',
                    ]}})")},
    });
    templates.emplace_back(ED_TASK_DEBUG_FIND_ALL_COMMODITIES, "Debug: find all commodities", FACTORY(TaskDebugFindAllCommodities), P{
            { Param::Bool, "shuffle" },
            { Param::Bool, "dump_images" },
            { Param::Int,  "start_index", "", OPT },
    });
    templates.emplace_back(ED_TASK_DEBUG_FIND_ALL_NAV_POINTS, "Debug: find all nav points", FACTORY(TaskDebugFindAllNavPoints), P{
            { Param::Bool, "dump_images" },
            { Param::Int,  "ocr_confidence", "", META({{"max",100}}), 90 },
            { Param::Int,  "txt_confidence", "", META({{"max",100}}), 90 },
            { Param::Bool, "resume" },
            { Param::Bool, "unfocused" },
    });
    templates.emplace_back(ED_TASK_DEBUG_AUTOPILOT, "Debug: autopilot steps", FACTORY(TaskDebugAutopilot), P{
            { Param::Enum, "test", _lc("Test"), META(R"({placeholder:'select the test', values: [
                         "Departure", "DockSpaceStation", "DockPlanetPort",
                         "LeaveBody", "EnterCruise", "HyperJump",
                         "FocusDestDock", "FocusDestBody", "FocusNearestBody", "FocusTopEntry", "RecognizeNavList",
                         "GalMapNavRoute", "NavDockSelect", "NavBodySelect", "CruiseToDist", "DiveUnderPlanet",
                         "ExitCruiseToSpace", "ExitCruiseToPlanet",
                       ]})")},
            { Param::String,  "target", _lc("Target"), OPT },
    });
    templates.emplace_back(ED_TASK_DEBUG_SHIP_STATS, "Debug: ship stats", FACTORY(TaskDebugShipStats), P{
            { Param::Enum, "test", _lc("Test"), META(R"({placeholder:'select the test', values: [
                         "OrientTowards", "OrientAway", "KeepCourse",
                         "ForwardAccelerate", "ReverseAccelerate", "ForwardDist",
                         "Pitch", "Yaw", "Roll", "PitchCurve", "YawCurve", "RollCurve",
                       ]})")},
            { Param::Real,    "value",  _lc("Value"),  OPT },
            { Param::Real,    "duration", _lc("Duration"), META({{"min",0},{"optional",true},{"placeholder","seconds"}}) },
            { Param::Int,     "speed", _lc("Speed"), META({{"min",0},{"max",100},{"optional",true},{"placeholder","percents"}}) },
    });

    AllTaskTemplates.swap(templates);

    TaskTemplate::loadUserTasks();
}

TaskTemplate TaskTemplate::loadTask(const json5pp::value& j_task) {
    auto &templ_id = j_task.at("templ", "").as_string();
    const TaskTemplate *templ_ptr = nullptr;
    for (auto &tt: AllTaskTemplates) {
        if (tt.id == templ_id) {
            templ_ptr = &tt;
            break;
        }
    }
    if (!templ_ptr) {
        LOG(ERROR) << "Task template '" << templ_id << "' not found";
        return {};
    }
    auto task = *templ_ptr;
    if (j_task["name"].is_string()) {
        const_cast<std::string&>(task.nm) = j_task["name"].as_string();
    }
    for (auto& p : task.params) {
        auto& jp = j_task[p.id];
        if (jp.is_null()) {
            if (p.optional())
                continue;
            LOG(ERROR) << "Missing required parameter '" << p.id << "' of task template " << task.id;
            continue;
        }
        else if (!p.set(jp)) {
            LOG(ERROR) << "Failed to set parameter " << p.id << " with " << jp;
        }
    }
    if (!task.validate()) {
        LOG(ERROR) << "Task id:'" << task.id << "' name:'" << task.nm << "' is not valid";
    }
    return task;
}

void TaskTemplate::loadUserTasks() {
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
        TaskTemplate task = loadTask(jtt);
        if (!task.id.empty())
            AllUserTasks.push_back(task);
    }
}

void TaskTemplate::saveUserTasks() {
    LOG(INFO) << "Saving tasks.json5";
    json5pp::value j_tasks = json5pp::array({});
    for (auto& tt : AllUserTasks) {
        json5pp::value jt = json5pp::object({
            {"templ", tt.id},
            {"name", tt.nm},
        });
        for (auto& p : tt.params) {
            if (p.value.is_array())
                std::erase_if(p.value.as_array(),[](auto& v) { return v.empty(); });
            if (p.empty())
                continue;
            jt.as_object().emplace(p.id, p.value);
        }
        j_tasks.as_array().push_back(jt);
    }
    if (std::filesystem::exists("tasks.json5")) {
        if (std::filesystem::exists("tasks.bak.json5"))
            std::filesystem::remove("tasks.bak.json5");
        std::filesystem::rename("tasks.json5", "tasks.bak.json5");
    }
    try {
        std::ofstream tasksFile("tasks.json5", std::ofstream::out|std::ofstream::trunc);
        if (tasksFile.fail()) {
            LOG(ERROR) << "Cannot write file: tasks.json5";
        } else {
            tasksFile << json5pp::rule::json5() << json5pp::rule::space_indent<2>() << j_tasks;
        }
        tasksFile.close();
    } catch (...) {
        LOG(ERROR) << "Failed to write tasks.json5";
    }
}

const std::list<TaskTemplate>& getUserTasks() {
    return AllUserTasks;
}

const std::list<TaskTemplate>& getTemplates() {
    return AllTaskTemplates;
}

const TaskTemplate& getTemplate(const std::string& id) {
    for (auto& tt : AllTaskTemplates)
        if (tt.id == id)
            return tt;
    throw std::out_of_range(id);
}

bool saveUserTask(TaskTemplate& templ) {
    for (auto it=AllUserTasks.begin(); it != AllUserTasks.end(); it++) {
        if (it->id == templ.id && it->nm == templ.nm) {
            for (int p=0; p < templ.params.size(); p++)
                it->params[p].value = templ.params[p].value;
            TaskTemplate::saveUserTasks();
            return true;
        }
    }
    AllUserTasks.emplace_front(templ);
    TaskTemplate::saveUserTasks();
    return true;
}

bool delUserTask(TaskTemplate& templ) {
    for (auto it=AllUserTasks.begin(); it != AllUserTasks.end(); it++) {
        if (it->id == templ.id && it->nm == templ.nm) {
            AllUserTasks.erase(it);
            TaskTemplate::saveUserTasks();
            return true;
        }
    }
    return false;
}

bool delUserTask(int index) {
    if (index >= 0 && index < AllUserTasks.size()) {
        auto it = AllUserTasks.begin();
        std::advance(it, index);
        AllUserTasks.erase(it);
        TaskTemplate::saveUserTasks();
        return true;
    }
    return false;
}

} // namespace ai