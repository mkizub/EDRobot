//
// Created by mkizub on 26.08.2026.
//

#pragma once

#ifndef EDROBOT_EXPLORATIONTASKS_H
#define EDROBOT_EXPLORATIONTASKS_H

namespace ai {

class TaskDebugExploration : public Task {
public:
    explicit TaskDebugExploration(const TaskTemplate& templ);
    bool run() final;

    enum {
        READY, DONE
    } status {READY};
    std::string test;
    std::string systemBegin;
    std::string systemEnd;
    double distance {20};
};

class TaskSystemsAround final : public Task {
public:
    explicit TaskSystemsAround(const TaskTemplate& templ);
    bool run() final;

    std::string getStatus() override;
    enum {
        READY, DONE
    } status {READY};
    std::string systemName;
    gal::spStarSystem starSystem;
    std::vector<gal::spStarSystem> systems;
};

class TaskVisitPlanets final : public BaseAutopilotTask {
public:
    explicit TaskVisitPlanets(const TaskTemplate& templ);
    bool run() final;

    std::string getStatus() override;
    enum {
        READY, FSS, VISITING, DONE
    } status {READY};
    std::string nextSystemName;
    std::string scanFireGroup {"A1"};

private:
    bool selectFireGroup();
    bool fireScan();
    bool selectUnexploredBody(gal::spEntity& selected);

    double scanProgress {};
    int bodyCount {};
    int nonBodyCount {};
};

class TaskVisitSystems final : public Task {
public:
    explicit TaskVisitSystems(const TaskTemplate& templ);
    bool run() final;
    std::string getTitle() override;

    std::string getStatus() override;
    enum {
        READY, LOADING, OPTIMIZING, VISITING, DONE
    } status {READY};
    std::string systemName;
    std::string systemList;
    gal::spStarSystem starSystem;
    std::vector<gal::spStarSystem> systems;
    std::queue<gal::spStarSystem> orderedSystems;
    int totalSystems {};
    int nextSystemIdx {};
    std::string nextSystemName;
};



}

#endif //EDROBOT_EXPLORATIONTASKS_H
