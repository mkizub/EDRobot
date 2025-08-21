//
// Created by mkizub on 28.06.2025.
//

#ifndef EDROBOT_AUTOPILOTTASKS_H
#define EDROBOT_AUTOPILOTTASKS_H


#include "Types.h"
#include "Task.h"

namespace ai {

class BaseAutopilotTask : public Task {
protected:
    BaseAutopilotTask(Task* parent, AIManager& mgr, const TaskTemplate& templ);
    void relogin();
    int  getNavPageIndex(const std::string& page_name);
    bool gotoNavPage(const std::string& page_name);
    bool selectNavFilters(LocationPanelFilters filters);

    struct StationRowInfo {
        wchar_t type {L'\0'};  // ✦ / ☄ / ✇ / etc.
        bool isTarget {false}; // < Name >
        bool isLocation {false}; // Name∇
        uint8_t size {0}; // Name ++
        wchar_t danger {L'\0'}; // ◇ / ⬖ / ◆
        std::wstring name;
        dist_t dist;
        const nav::NavType* navType {nullptr};
        bool dockable;
    };
    bool parseNavNameDist(std::wstring text, std::wstring dist, StationRowInfo& rowInfo);
    bool parseNavRow(const cv::Mat& grayImage, const ClassifiedRect& cr, StationRowInfo& rowInfo);
    bool parseFocusedNavRow(const cv::Mat& grayImage, StationRowInfo& rowInfo);
    bool getFocusedNavRow(StationRowInfo& rowInfo);

    bool setSpeed(int percents);
    bool orientTowardTargetStep(double precision);
    bool orientTowardTarget(double precision, const char* dropSpeedButton=nullptr);

    int speed_set_to {0};
    dist_t distanceToDock; // distance to the dock, in km
};

class TaskDebugAutopilot : public BaseAutopilotTask {
public:
    TaskDebugAutopilot(Task* parent, AIManager& mgr, const TaskTemplate& templ);
    Result run() final;

    double orient_precision {0.5};
};


class TaskDepart : public BaseAutopilotTask {
public:
    TaskDepart(Task* parent, AIManager& mgr, const TaskTemplate& templ);
    Result run() final;
};

class TaskDock : public BaseAutopilotTask {
public:
    TaskDock(Task* parent, AIManager& mgr, const TaskTemplate& templ);
    Result run() final;

    spGameEvent requestDockingPermit();
    bool selectDockingFilters();
    bool lockDockingStation();
    bool flyTowardsTarget();

    std::wstring dockName; // ocr form
};

class TaskJumpToSystem : public BaseAutopilotTask {
public:
    TaskJumpToSystem(Task* parent, AIManager& mgr, const TaskTemplate& templ);
    Result run() final;

    bool selectDestSystem();

    std::string destSystem;
};

class TaskCruiseToDock : public BaseAutopilotTask {
public:
    TaskCruiseToDock(Task* parent, AIManager& mgr, const TaskTemplate& templ);
    Result run() final;

    bool selectDestDock();

    std::string destDock;

    std::wstring dockNameOCR; // ocr form
};

class TaskTravel : public BaseAutopilotTask {
public:
    TaskTravel(Task* parent, AIManager& mgr, const TaskTemplate& templ);
    Result run() final;

    void plan();

    std::string destSystem;
    std::string destDock;
};

} // namespace ai

#endif //EDROBOT_AUTOPILOTTASKS_H
