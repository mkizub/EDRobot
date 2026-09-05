//
// Created by mkizub on 26.08.2026.
//
#include "../pch.h"

#include "AIManager.h"
#include "ExplorationTasks.h"
#include "NavList.h"
#include "AIUtils.h"
#include "../widget/List.h"
#include "../Keyboard.h"
#include "../net/Spansh.h"

namespace ai {

TaskDebugExploration::TaskDebugExploration(const ai::TaskTemplate &templ)
    : Task(templ)
{
    assert (templ.id == ED_TASK_DEBUG_EXPLORATION);
    for (auto& p : templ.params) {
        if (p.id == "test")
            test = p.as_string();
        if (p.id == "begin")
            systemBegin = p.as_string();
        if (p.id == "end")
            systemEnd = p.as_string();
    }
}

bool TaskDebugExploration::run() {
    if (status == DONE)
        return true;
    status = DONE;
    if (systemBegin.empty())
        systemBegin = gal::getCurrentStarSystem()->systemName;
    if (test == "ListNearest") {
        Spansh::listNearestSystems(systemBegin, systemEnd, distance);
    }
    if (test == "ScanNearest") {
        TaskTemplate tt {ED_TASK_NAV_SCAN_SYSTEMS, _lc("Scan Star Systems"), [](const TaskTemplate &templ) { return new NavListScanSystemsTask(templ); }};
        run_sub_step(new NavListScanSystemsTask(tt));
    }
    return true;
}


TaskSystemsAround::TaskSystemsAround(const TaskTemplate &templ)
    : Task(templ)
{
    assert (templ.id == ED_TASK_EXPL_SYSTEMS_AROUND);
    for (auto& p : templ.params) {
        if (p.id == "system")
            systemName = p.as_string();
    }
}

bool TaskSystemsAround::run() {
    if (!starSystem) {
        if (systemName.empty()) {
            starSystem = gal::getCurrentStarSystem();
            if (!starSystem)
                throw_failed("Current star system not known");
            systemName = starSystem->systemName;
        } else {
            starSystem = gal::getStarSystem(systemName);
            if (!starSystem)
                throw_failed("Star system '{}' is not known", systemName);
        }
    }
    if (systems.empty()) {
        std::vector<gal::spStarSystem> knownSystems = Spansh::listNearestSystems(gal::getCurrentStarSystem()->systemName, "", 20);
        TaskTemplate tt {ED_TASK_NAV_SCAN_SYSTEMS, _lc("Scan Star Systems"), [](const TaskTemplate &templ) { return new NavListScanSystemsTask(templ); }};
        auto scan_task = spTask(new NavListScanSystemsTask(tt));
        if (!run_sub_step(scan_task)) {
            throw_failed("Failed to scan systems around '{}'", systemName);
            return false;
        }
        LOG_INFO("Scanned around systems address {:14d} name \"{}\" is unknown",
                 starSystem->systemAddress, starSystem->systemName);
        auto& foundSystems = std::static_pointer_cast<NavListScanSystemsTask>(scan_task)->foundSystems;
        for (int sidx=0; sidx < foundSystems.size(); sidx++) {
            auto& ss = foundSystems[sidx];
            if (ss->starPos == cv::Point3d{}) {
                if (ss->systemName != "Sol") {
                    LOG_INFO("Selected system[{:2d}] address {:14d} name \"{}\" is unknown", sidx, ss->systemAddress,
                             ss->systemName);
                    systems.push_back(ss);
                }
            } else {
                auto eddn_updated_at = formatTimestampString(ss->eddn_updated_at);
                if (ss->game_body_count <= 0) {
                    LOG_INFO("Selected system[{:2d}] address {:14d} name \"{}\" not scanned: updated at {}",
                             sidx, ss->systemAddress, ss->systemName, eddn_updated_at);
                    systems.push_back(ss);
                } else {
                    int known_body_count = 0;
                    for (auto b: ss->bodies) {
                        if (b->type == TypeNav::Star || b->type == TypeNav::Planet)
                            known_body_count += 1;
                    }
                    if (known_body_count == ss->game_body_count) {
                        LOG_INFO("Selected system[{:2d}] address {:14d} name \"{}\" fully scanned with {} bodies: updated at {}",
                                 sidx, ss->systemAddress, ss->systemName, known_body_count, eddn_updated_at);
                    }
                    else {
                        LOG_INFO("Selected system[{:2d}] address {:14d} name \"{}\" has only {} known bodies out of {}: updated at {}",
                                 sidx, ss->systemAddress, ss->systemName, known_body_count, ss->game_body_count, eddn_updated_at);
                        systems.push_back(ss);
                    }
                }
            }
        }
    }
    if (systems.empty()) {
        notify_info("No systems to explore");
        return true;
    }

    return true;
}

std::string TaskSystemsAround::getStatus() {
    return {};
}
} // namespace ai
