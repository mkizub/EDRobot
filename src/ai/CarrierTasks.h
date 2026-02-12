//
// Created by mkizub on 11.02.2026.
//

#pragma once

#ifndef EDROBOT_CARRIERTASKS_H
#define EDROBOT_CARRIERTASKS_H

#include "Types.h"
#include "Task.h"

namespace ai {

class TaskMyCarrierUnload final : public Task {
public:
    explicit TaskMyCarrierUnload(const TaskTemplate& templ);
    bool run() final;

    int contributed {};

    std::string getStatus() override;
    enum {
        READY, TO_TRANSFER, UNLOAD, DONE, DONE_NOTHING
    } status {READY};
};


}

#endif //EDROBOT_CARRIERTASKS_H
