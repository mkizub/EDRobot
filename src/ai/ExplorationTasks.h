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



}

#endif //EDROBOT_EXPLORATIONTASKS_H
