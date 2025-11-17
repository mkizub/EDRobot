//
// Created by mkizub on 19.06.2025.
//

#pragma once

#ifndef EDROBOT_TASKTEMPLATE_H
#define EDROBOT_TASKTEMPLATE_H

#include "Types.h"

namespace ai {

struct TaskTemplate;

struct Param {
    enum Type { Void, Bool, Enum, Int, Real, String, System, Dock, Commodity, Task, Array };

    const Type        type;
    const std::string id;
    const std::string nm;
    json5pp::value    meta;
    json5pp::value    value;

    std::string name() const;
    std::string placeholder() const;
    bool optional() const;
    bool empty() const;
    bool valid() const;

    bool as_boolean() const;
    int as_integer() const;
    int32_t as_int32() const;
    int64_t as_int64() const;
    double as_number() const;
    std::string as_string() const;
    ::Commodity* as_commodity() const;

    bool set(const json5pp::value& value);
};

struct TaskTemplate {
    const std::string id;
    const std::string nm;
    const std::function<Task*(const TaskTemplate& templ)> factory;
    std::vector<Param> params;

    Param& get(const string& pid);
    const Param& get(const string& pid) const;
    bool set(const string& pid, const json5pp::value& value);

    std::string name() const;
    bool validate() const;

    static TaskTemplate loadTemplate(const json5pp::value& task);
    static void loadSavedTasks();
};


extern const std::string ED_TASK_REPEAT;            // repeat sequence of several tasks
extern const std::string ED_TASK_MARKET_SELL;       // sell specified commodity, maybe by a few items
extern const std::string ED_TASK_MARKET_SELL_ALL;   // sell all commodities, maybe by a few items
extern const std::string ED_TASK_MARKET_BUY;        // buy a list of commodities
extern const std::string ED_TASK_MARKET_BUY_CONSTR; // buy commodities needed for construction
extern const std::string ED_TASK_CONSTR_UNLOAD;     // unload all construction materials at construction depot
extern const std::string ED_TASK_TRADE_AT;          // sell/buy at specified dock
extern const std::string ED_TASK_TRADE_LOOP;        // trade loop between stations
extern const std::string ED_TASK_AUTOPILOT;         // fly to current destination
extern const std::string ED_TASK_TRAVEL;            // multistep task to travel somewhere
extern const std::string ED_TASK_NAV_SCAN;          // san navigation map

extern const std::string ED_TASK_CALIBRATE;         // calibrate colors
extern const std::string ED_TASK_DEBUG_FIND_ALL_COMMODITIES;
extern const std::string ED_TASK_DEBUG_FIND_ALL_NAV_POINTS;
extern const std::string ED_TASK_DEBUG_AUTOPILOT;

} // namespace ai

#endif //EDROBOT_TASKTEMPLATE_H
