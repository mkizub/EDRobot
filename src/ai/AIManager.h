//
// Created by mkizub on 20.06.2025.
//

#pragma once

#ifndef EDROBOT_AIMANAGER_H
#define EDROBOT_AIMANAGER_H

#include "Types.h"
#include "EDState.h"
#include "Task.h"

namespace ai {

class AIManager {
public:

    AIManager();
    ~AIManager();

    bool active();
    void interrupt();
    void resume();
    void new_task(upTask&& task);

    enum class CheckResult {
        Failure,
        Resume,
        Replan,
    };

    //CheckResult checkTaskReqMatch(upTask&);
    const bool detectEDState(DetectLevel level);

    void loop();
    void step();

    Master& master;
    Configuration& cfg;

    EDState edState;
    upTask activeTask;
    std::vector<upTask> archivedTasks;
    std::chrono::milliseconds nextDelay;
    DetectLevel nextDetectLevel {DetectLevel::None};

    std::thread taskThread;
    std::mutex taskMutex;
    std::condition_variable taskCond;
    std::atomic_bool isWorking;
    std::atomic_bool isInterrupted;
    std::atomic_bool isLoopWaiting;
    ResolvedEnv rEnv;
    UIState uiState;
};

}

#endif //EDROBOT_AIMANAGER_H
