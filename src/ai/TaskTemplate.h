//
// Created by mkizub on 19.06.2025.
//

#pragma once

#ifndef EDROBOT_TASKTEMPLATE_H
#define EDROBOT_TASKTEMPLATE_H

#include "Types.h"

namespace ai {

extern const std::string ED_TASK_REPEAT;            // repeat sequence of several tasks
extern const std::string ED_TASK_MARKET_SELL;       // sell specified commodity, maybe by a few items
extern const std::string ED_TASK_MARKET_SELL_ALL;   // sell all commodities, maybe by a few items
extern const std::string ED_TASK_MARKET_BUY;        // buy a list of commodities
extern const std::string ED_TASK_MARKET_BUY_CONSTR; // buy commodities needed for construction
extern const std::string ED_TASK_CONSTR_UNLOAD;     // unload all construction materials at construction depot
extern const std::string ED_TASK_AUTOPILOT;         // fly to current destination
extern const std::string ED_TASK_TRAVEL;            // multistep task to travel somewhere
extern const std::string ED_TASK_NAV_SCAN;          // san navigation map

extern const std::string ED_TASK_CALIBRATE;         // calibrate colors
extern const std::string ED_TASK_DEBUG_FIND_ALL_COMMODITIES;
extern const std::string ED_TASK_DEBUG_FIND_ALL_NAV_POINTS;
extern const std::string ED_TASK_DEBUG_AUTOPILOT;

} // namespace ai

#endif //EDROBOT_TASKTEMPLATE_H
