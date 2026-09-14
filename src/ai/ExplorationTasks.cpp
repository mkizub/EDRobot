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
#include "../csvparser.hpp"

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
    assert (templ.id == ED_TASK_EXPL_VISIT_SYSTEMS);
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

static std::vector<int> nearest_neighbor(const std::vector<cv::Point3d>& points);
// Вычисление полной длины маршрута (с возвратом в начало)
static double get_total_distance(const std::vector<cv::Point3d>& points, const std::vector<int>& path);
// 2. Локальный поиск: Алгоритм 2-opt (убирает пересечения путей)
void two_opt(const std::vector<cv::Point3d>& points, std::vector<int>& path);

TaskVisitSystems::TaskVisitSystems(const TaskTemplate &templ)
    : Task(templ)
{
    assert (templ.id == ED_TASK_EXPL_VISIT_SYSTEMS);
    for (auto& p : templ.params) {
        if (p.id == "system")
            systemName = p.as_string();
        if (p.id == "list")
            systemList = p.as_string();
    }
}

bool TaskVisitSystems::run() {
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
        status = LOADING;
        std::string spanshPrefix = "https://www.spansh.co.uk/systems/search/";
        if (systemList.starts_with(spanshPrefix)) {
            std::string recallUUID = recallUUID.substr(spanshPrefix.size(), 36);
            if (recallUUID.size() != 36) {
                notify_error("Bad UUID for spansh.co.uk recall: {}", recallUUID);
                return false;
            }
            systems = Spansh::listSpanshSearch(recallUUID, [](gal::spStarSystem ss, js::value& jr)->bool {
                Timestamp updated_at;
                if (jr["updated_at"].is_string())
                    parseTimestampString(jr["updated_at"].as_string(), updated_at);
                if (updated_at < ss->eddn_updated_at || !ss->loaded)
                    Spansh::loadStarSystem(ss->systemAddress);
                if (!ss)
                    return false;
                LOG_INFO("Star system: {} / {} (at x={:.5f} y={:.5f} z={:.5f}) updated at {}",
                         ss->systemName, ss->systemAddress, ss->starPos.x, ss->starPos.y, ss->starPos.z, updated_at);
                return true;
            });
            LOG_INFO("Got {} systems for spansh.co.uk search recall {} request", systems.size(), recallUUID);
        }
        else if (systemList.ends_with(".csv") || systemList.ends_with(".CSV")) {
            ed::DataFrame df;
            if (!df.read_csv(systemList))
                throw_failed("Cannot parse CSV file: {}", systemList);
            std::vector<std::string> list;
            df.get_column<std::string>(list,0);
            totalSystems = list.size();
            for (int i=0; i < list.size(); i++) {
                std::string name = list[i];
                if (name.size() > 2 && name[0] == '\"' && name[name.length()-1] == '\"')
                    name = name.substr(1,name.length()-2);
                nextSystemIdx = i+1;
                nextSystemName = name;
                gal::spStarSystem ss = gal::getStarSystem(name);
                if (ss) {
                    if (std::find(systems.begin(), systems.end(), ss) == systems.end())
                        systems.push_back(ss);
                } else {
                    LOG_WARNING("Star system '{}' not known", name);
                }
            }
        }
        else {
            throw_failed("Unknown file format: {}", systemList);
        }

        // start from current system
        bool found = false;
        for (size_t i=0; i < systems.size(); i++) {
            auto& ss = systems[i];
            if (ss == starSystem) {
                found = true;
                if (i != 0)
                    std::swap(systems[0], systems[i]);
                break;
            }
        }
        if (!found)
            systems.insert(systems.begin(), starSystem);
    }
    if (systems.size() == 1) {
        notify_info("No systems to visit");
        return true;
    }

    if (orderedSystems.empty()) {
        nextSystemIdx = 0;
        nextSystemName = "";
        totalSystems = systems.size();
        status = OPTIMIZING;
        notify_info("Have {} systems to visit", systems.size());

        std::vector<cv::Point3d> points;
        points.reserve(systems.size());
        for (auto& ss : systems)
            points.push_back(ss->starPos);

        // Шаг 1: Получаем быстрое начальное решение за O(N^2)
        std::vector<int> visitOrderPath = nearest_neighbor(points);
        double initial_dist = get_total_distance(points, visitOrderPath);
        notify_info("Initial (Nearest Neighbor) distance: {} ly", int64_t(initial_dist));
        // Шаг 2: Улучшаем его с помощью 2-opt локального поиска
        two_opt(points, visitOrderPath);
        double optimized_dist = get_total_distance(points, visitOrderPath);
        notify_info("Optimized distance: {} ly", int64_t(initial_dist));
        notify_info("Visit route is reported to log file and console");

        for (auto i: visitOrderPath)
            orderedSystems.push(systems[i]);
    }

    notify_info("Current star system: {}", gal::getCurrentStarSystem()->systemName);
    while (!orderedSystems.empty()) {
        status = VISITING;
        auto next = orderedSystems.front();
        if (next == gal::getCurrentStarSystem()) {
            orderedSystems.pop();
            continue;
        }
        nextSystemName = next->systemName;
        nextSystemIdx = systems.size() - orderedSystems.size();
        double dist = cv::norm(next->starPos - gal::getCurrentStarSystem()->starPos);
        notify_info("Next system {} of {}: {} distance {:.1f}ly", nextSystemIdx, totalSystems, next->systemName, dist);

        TaskTemplate impl = getTemplate(ED_TASK_VISIT_SYSTEM);
        impl.nm.clear();
        impl.set("system", next->systemName);
        if (!run_sub_step(impl.factory(impl))) {
            throw_trouble("Failed to visit star system {}", next->systemName);
        }
    }

    status = DONE;
    return true;
}

std::string TaskVisitSystems::getTitle() {
    std::string title = Task::getTitle();
    if (status == VISITING)
        title += std::format(" {}/{}", nextSystemIdx, totalSystems);
    return title;
}

std::string TaskVisitSystems::getStatus() {
    switch (status) {
    case READY:
    case DONE:
        return {};
    case LOADING:
        return lc_format("Loading star system list {0} of {1}: {2}", nextSystemIdx, totalSystems, nextSystemName);
    case OPTIMIZING:
        return lc_format("Optimizing route");
    case VISITING:
        return lc_format("Visiting system {0} of {1}: {2}", nextSystemIdx, totalSystems, nextSystemName);
    }
    return {};
}


//
// AI-generated code
//

// Функция вычисления 3D расстояния
inline double dist(const cv::Point3d& a, const cv::Point3d& b) {
    return std::sqrt((a.x - b.x) * (a.x - b.x) +
                     (a.y - b.y) * (a.y - b.y) +
                     (a.z - b.z) * (a.z - b.z));
}

// Вычисление полной длины маршрута (с возвратом в начало)
double get_total_distance(const std::vector<cv::Point3d>& points, const std::vector<int>& path) {
    double total = 0.0;
    for (size_t i = 0; i < path.size(); ++i) {
        total += dist(points[path[i]], points[path[(i + 1) % path.size()]]);
    }
    return total;
}

// 1. Жадный алгоритм: поиск Ближайшего Соседа
std::vector<int> nearest_neighbor(const std::vector<cv::Point3d>& points) {
    int n = points.size();
    std::vector<int> path;
    path.reserve(n);
    std::vector<bool> visited(n, false);

    // Стартуем со случайной или нулевой точки
    int current = 0;
    path.push_back(current);
    visited[current] = true;

    for (int step = 1; step < n; ++step) {
        int next_node = -1;
        double min_d = 1e18;

        for (int i = 0; i < n; ++i) {
            if (!visited[i]) {
                double d = dist(points[current], points[i]);
                if (d < min_d) {
                    min_d = d;
                    next_node = i;
                }
            }
        }
        current = next_node;
        path.push_back(current);
        visited[current] = true;
    }
    return path;
}

// 2. Локальный поиск: Алгоритм 2-opt (убирает пересечения путей)
void two_opt(const std::vector<cv::Point3d>& points, std::vector<int>& path) {
    int n = path.size();
    bool improvement = true;

    while (improvement) {
        improvement = false;
        for (int i = 1; i < n - 1; ++i) {
            for (int j = i + 1; j < n; ++j) {
                // Текущие ребра: (i-1 -> i) и (j -> j+1)
                int a = path[i - 1];
                int b = path[i];
                int c = path[j];
                int d = path[(j + 1) % n];

                // Текущая стоимость vs Стоимость после переворота участка пути
                double current_dist = dist(points[a], points[b]) + dist(points[c], points[d]);
                double new_dist = dist(points[a], points[c]) + dist(points[b], points[d]);

                if (new_dist < current_dist) {
                    // Переворачиваем подмассив от i до j
                    std::reverse(path.begin() + i, path.begin() + j + 1);
                    improvement = true;
                }
            }
        }
    }
}

//int main() {
//    // Генерация 1000 случайных 3D точек
//    const int N = 1000;
//    std::vector<Point3d> points(N);
////    std::mt19937 rng(42); // Фиксированный seed для воспроизводимости
////    std::uniform_real_distribution<double> dist_gen(0.0, 100.0);
//
////    for (int i = 0; i < N; ++i) {
////        points[i] = {dist_gen(rng), dist_gen(rng), dist_gen(rng)};
////    }
//
//    auto start_time = std::chrono::high_resolution_clock::now();
//
//    // Шаг 1: Получаем быстрое начальное решение за O(N^2)
//    std::vector<int> path = nearest_neighbor(points);
//    double initial_dist = get_total_distance(points, path);
//    std::cout << "Initial (Nearest Neighbor) distance: " << initial_dist << "\n";
//
//    // Шаг 2: Улучшаем его с помощью 2-opt локального поиска
//    two_opt(points, path);
//    double optimized_dist = get_total_distance(points, path);
//
//    auto end_time = std::chrono::high_resolution_clock::now();
//    std::chrono::duration<double, std::milli> duration = end_time - start_time;
//
//    std::cout << "Optimized (2-opt) distance: " << optimized_dist << "\n";
//    std::cout << "Execution time: " << duration.count() << " ms\n";
//
//    return 0;
//}

} // namespace ai
