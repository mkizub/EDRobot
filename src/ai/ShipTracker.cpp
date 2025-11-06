//
// Created by mkizub on 05.11.2025.
//

#include "../pch.h"

#include "Types.h"
#include "../Keyboard.h"
#include "../ShipStats.h"

using namespace std::chrono_literals;

namespace ai {

namespace {
    std::thread turnThread;
    std::mutex turnMutex;
    std::condition_variable turnCond;

    std::atomic_bool isWorking;
    std::atomic_bool isLoopWaiting;
    std::atomic_bool isActive;

    double requestedPitch;
    bool disableRoll;
}

void turn_loop();
void turn_step();

void init_ship_tracker() {
    isWorking = true;
    turnThread = std::thread(&turn_loop);
}
void shutdown_ship_tracker() {
    isWorking = false;
    turnCond.notify_all();
    turnThread.join();
    kbd::reset_vJoy();
}

void resetTurnAxis() {
    const KeyBindings& bind_pitch = Cfg.getGameKeyBindings("PitchAxisRaw");
    const KeyBindings& bind_yaw = Cfg.getGameKeyBindings("YawAxisRaw");
    const KeyBindings& bind_roll = Cfg.getGameKeyBindings("RollAxisRaw");
    kbd::axis(bind_pitch, 0);
    kbd::axis(bind_yaw, 0);
    kbd::axis(bind_roll, 0);
}

void disableAutoTurn() {
    std::unique_lock<std::mutex> lock(turnMutex);
    if (!isActive)
        return;
    isActive = false;
    turnCond.notify_one();
    turnCond.wait_for(lock, 1s, []() {
        return !isWorking || isLoopWaiting;
    });
    resetTurnAxis();
}

void requestPitchRoll(double pitch, bool without_roll) {
    std::unique_lock<std::mutex> lock(turnMutex);
    if (pitch <= -180)
        requestedPitch = 360 + pitch;
    else if (pitch > 180)
        requestedPitch = 360 - pitch;
    else
        requestedPitch = pitch;
    disableRoll = without_roll;
    isActive = true;
    turnCond.notify_one();
}

void set_axis(double speed, double delta, const char* name) {
    double value = 0;
    double seconds = std::abs(delta / speed);
    if (seconds > 0.05)
        value = std::copysign(seconds >= 0.5 ? 1.0 : seconds / 0.5, delta);
    const KeyBindings& bind = Cfg.getGameKeyBindings(name);
    kbd::axis(bind, value);
}

void turn_loop() {
    SetThreadDescription(GetCurrentThread(), L"ShitTracker loop");

    LOG(INFO) << "Starting ship tracker";
    while (isWorking) {
        {
            std::unique_lock<std::mutex> lock(turnMutex);
            isLoopWaiting = true;
            turnCond.wait_for(lock, isActive ? 500ms : 1min);
            isLoopWaiting = false;
            if (!isWorking)
                break;
            if (!isActive)
                continue;
        }
        try {
            turn_step();
        } catch(const std::exception& ex) {
            kbd::reset_vJoy();
            if (auto ir = dynamic_cast<const interrupted_error*>(&ex)) {
                LOG(ERROR) << "Ship tracker interrupted: " << ex.what() << std::endl;
                isActive = false;
            } else {
                LOG(ERROR) << "Exception in ship tracker: " << ex.what() << std::endl;
            }
        }
    }
    LOG(INFO) << "Exiting ship tracker";
}

void turn_step() {
    if (isDebugPause()) {
        resetTurnAxis();
        return;
    }
    if (st::guiFocus != GuiFocus::None)
        return;

    CompassInfo compass = st::compass;
    if (!compass.hemisphere)
        return;

    double precision = 1.0;
    if (!compass.has_nav_target)
        precision = 3.0;

    auto shipStats = eddb::getShipStats();
    if (!shipStats) {
        LOG(ERROR) << "Unsupported or unknown ship";
        isActive = false;
        return;
    }

    double delta_pitch = compass.targetPitch - requestedPitch;
    if (delta_pitch > 180) delta_pitch = 360-delta_pitch;
    if (delta_pitch < -180) delta_pitch = 360+delta_pitch;
    double delta_yaw = compass.targetYaw;

    int speed_set_to = st::autopilot.speed_set_to.has_value() ? st::autopilot.speed_set_to.value() : 50;
    double speed_pitch = shipStats->getPitchSpeed(speed_set_to);
    double speed_yaw = shipStats->getYawSpeed(speed_set_to);

    set_axis(speed_pitch, -delta_pitch, "PitchAxisRaw");
    set_axis(speed_yaw, delta_yaw, "YawAxisRaw");
}

} //namespace ai
