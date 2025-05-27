//
// Created by mkizub on 22.05.2025.
//

#pragma once

#ifndef EDROBOT_TRADEITEMS_H
#define EDROBOT_TRADEITEMS_H


#include <string>
#include <atomic>
#include <stdexcept>

class TradeItems {
public:

private:
    class ExitException : public std::runtime_error {
    public:
        ExitException(const std::string& what) : std::runtime_error(what) {}
    };

    void exit(std::string msg = {});
    bool is_selling();

    std::atomic<bool> shutdown;
};


#endif //EDROBOT_TRADEITEMS_H
