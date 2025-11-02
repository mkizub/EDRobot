//
// Created by mkizub on 19.06.2025.
//

#pragma once

#ifndef EDROBOT_TASKTEMPLATE_H
#define EDROBOT_TASKTEMPLATE_H

#include "Types.h"

namespace ai {

// Left panel:
// tab-1: System Info/Stats
// tab 0: Navigation: Навигация
// tab 1: Transactions: Транзакции
//    page 0: Missions: Миссии
//    page 2: Passangers: Пассажиры
//    page 3: PP Tasks: Политика: задания
//    page 4: Community Goals: Цели сообщества
//    page 5: Rewards: Награды
//    page 6: Fines and bounties: Штрафы и награды (за голову)
// tab 2: Contacts: Контакты
// tab 3: Target: Цель
//    page 0: Secondary
//    page 2: Primary4

// Right panel:
// tab-1: Pilot Info/Codex/Carrier Controls/etc.
// tab 0: Modules: Модули
// tab 1: Fire groups: Огневые группы
// tab 2: Ship: Корабль (режимы, информация)
//    page 0: Functions: Функции
//    page 1: Autopilot: Автопилот
//    page 2: Preferencies: Предпочтения пилота
//    page 3: Statistics: Статистика
// tab 3: Cargo/Equipment: Снаряжение
//    page 0: Сargo: Груз на корабе
//    page 1: Refinery: Очиститель
//    page 2: Materials: Материалы (инженерные)
//    page 3: Data: Зашифрованные данные (инженерные)
//    page 4: Synthesis: Синетез
//    page 5: Cabins: Каюты
// tab 4: Storage: Хранилище (Одиссей)
//    page 0: Consumables: Расходники
//    page 1: Assets: Активы
//    page 2: Goods: Товары
//    page 3: Data: Данные
// tab 5: Status: Статус
//    page 0: Factions: Фракции системы
//    page 1: Reputation: Репутация
//    page 2: Journal: Журнал сеанса
//    page 3: Finance: Финансы
//    page 4: Permissions: Разрешения (пропуск в систему)
//    page 5: Playlist: Список воспроизведения (GalNet)

// Down panel:
// tab-0: Pilot Info (Space suite, Odissey)
// tab-1: Interceptor / Исстребитель
// tab-2: TRP / ТРП
// tab-3: Crew / Экипаж (Multi-crew)

// Chat panel:
// tab-0: Chat messages
// tab-1: Mail messages
// tab-2: Group/Friends invites
// tab-3: Contacts history
// tab-4: Squadron news
// tab-5: Message Channels settings

// Lamding pad/Dock dialog:
// refuel, repair, rearm, to hangar/landing pad
// station services
// (auto)launch
// disembark



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
