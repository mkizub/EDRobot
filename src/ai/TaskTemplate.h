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



extern const std::string ED_STATE_VOID;    // unknown state
extern const std::string ED_STATE_SPACE;   // in conventional space
extern const std::string ED_STATE_JUMP;    // jumping from one system to another
extern const std::string ED_STATE_CRUISE;  // in hyper-cruise space mode
extern const std::string ED_STATE_CRUISE_INT; // intercepted during hyper-cruise
extern const std::string ED_STATE_CRUISE_JET; // int neutron star jet
extern const std::string ED_STATE_DOCKED;  // docked at station/settlement
extern const std::string ED_STATE_LANDED;  // landed outside of station/settlement
extern const std::string ED_STATE_DEPART;  // departure from docked/landed state

extern const std::string ED_UI_MODE_NORM;  // default, not viewing any info panel
extern const std::string ED_UI_MODE_LEFT;  // viewing navigation (left-1) panel
extern const std::string ED_UI_MODE_RIGHT; // viewing ship information (right-4) panel
extern const std::string ED_UI_MODE_DOWN;  // viewing command (bottom-3) panel
extern const std::string ED_UI_MODE_CHAT;  // viewing communication (up-2) panel
extern const std::string ED_UI_MODE_DOCK;  // docked at station (departure, goto services, refuel, etc.)
extern const std::string ED_UI_MODE_STATION; // viewing docked station services screens (trading/etc.)
extern const std::string ED_UI_MODE_GAL_MAP; // viewing galaxy map
extern const std::string ED_UI_MODE_SYS_MAP; // viewing star system map

extern const std::string ED_TASK_SEQ;               // sequence of several tasks
extern const std::string ED_TASK_LOOP;              // repeat task a few times
extern const std::string ED_TASK_GOTO_SERVICES;     // just press 'services' button at dock
extern const std::string ED_TASK_GOTO_MARKET;       // select 'market' button at dock
extern const std::string ED_TASK_MARKET_SELL_ALL;   // sell all commodities, maybe by a few items
extern const std::string ED_TASK_MARKET_SELL;       // sell specified commodity, maybe by a few items
extern const std::string ED_TASK_MARKET_BUY;        // buy a list of commodities
extern const std::string ED_TASK_TRAVEL;            // multistep task to travel somewhere
extern const std::string ED_TASK_GAL_MAP_SELECT;    // select destination star system on galaxy map
extern const std::string ED_TASK_SYS_MAP_SELECT;    // select destination POI on system map
extern const std::string ED_TASK_DEPART;            // press 'departure' button at dock and wait autopilot to complete
extern const std::string ED_TASK_CRUISE_AVOID;      // avoid stars and planets during cruise
extern const std::string ED_TASK_SPACE_AVOID;       // avoid station, fleet carrier, ship, etc. during normal space fly
extern const std::string ED_TASK_JUMP_TO_SYSTEM;    // jump to selected star in nav route
extern const std::string ED_TASK_TO_CRUISE;         // from normal space to cruise (if destination in the same system or to avoid obscured jump point)
extern const std::string ED_TASK_CRUISE_TO_STATION; // cruise to station, exit cruise
extern const std::string ED_TASK_CRUISE_TO_POI;     // cruise to point of interest (planet, beacon, etc.)
extern const std::string ED_TASK_CRUISE_TO_PORT;    // cruise to planet to see the port at right angle, then fly toward until cruise mode end
extern const std::string ED_TASK_NIGH;              // fly toward port/station after cruise exit
extern const std::string ED_TASK_DOCK;              // ask permission to land, then autopilot, and wait docking
extern const std::string ED_TASK_STEP;              // some trivial step, like button press or wait for UI update

extern const std::string ED_TASK_CALIBRATE;         // calibrate colors
extern const std::string ED_TASK_DEBUG_FIND_ALL_COMMODITIES;
extern const std::string ED_TASK_DEBUG_FIND_ALL_NAV_POINTS;
extern const std::string ED_TASK_DEBUG_FIX_OCR;

class UIModePage {
    const std::string name;
};
class UIModeTab {
    const std::string name;
    std::vector<UIModePage> pages;
};
struct UIMode {
    const std::string name;
    std::vector<UIModeTab> tabs;
};


} // namespace ai

#endif //EDROBOT_TASKTEMPLATE_H
