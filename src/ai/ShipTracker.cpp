//
// Created by mkizub on 05.11.2025.
//

#include "../pch.h"

#include "Types.h"
#include "AutopilotTasks.h"
#include "../Keyboard.h"
#include "../ShipStats.h"

namespace ai {

namespace trk {
    struct Axis {
        KeyBindings bindings;
        double value;
    };
    std::thread turnThread;
    std::mutex turnMutex;
    std::condition_variable turnCond;

    std::atomic_bool isWorking;
    std::atomic_bool isLoopWaiting;
    std::atomic_bool isActive;

    double requestedPitch;
    bool disableRoll;

    Axis pitchAxis;
    Axis yawAxis;
    Axis rollAxis;
}

using namespace trk;

void turn_loop();
void turn_step();
void resetTurnAxis();
void request_pitch_roll(double pitch, bool without_roll);

CourseLocker::CourseLocker(double pitch, bool without_roll) {
    request_pitch_roll(pitch, without_roll);
}
CourseLocker::~CourseLocker() {
    disableAutoTurn();
}
void CourseLocker::requestPitchRoll(double pitch, bool without_roll) {
    request_pitch_roll(pitch, without_roll);
}

CourseLockerPause::CourseLockerPause()
    : wasActive(isActive)
{
    if (wasActive)
        disableAutoTurn();
}
CourseLockerPause::~CourseLockerPause() {
    if (wasActive) {
        isActive = true;
        turnCond.notify_one();
    }
}


void setAxisBindings(Axis& axis, const char* name, bool invert) {
    const KeyBindings& orig = Cfg.getGameKeyBindings(name);
    axis.bindings = orig;
    if (!(orig.mode == KeyBindings::Axis || orig.mode == KeyBindings::AxisInv))
        throw std::domain_error(std::format("Bad bindings for {}, vJoy axis required", name));
    if (invert) {
        if (axis.bindings.mode == KeyBindings::Axis)
            axis.bindings.mode = KeyBindings::AxisInv;
        else
            axis.bindings.mode = KeyBindings::Axis;
    }
}

void init_ship_tracker() {
    isWorking = true;
    setAxisBindings(pitchAxis, "PitchAxisRaw", true);
    setAxisBindings(yawAxis, "YawAxisRaw", false);
    setAxisBindings(rollAxis, "RollAxisRaw", false);
    resetTurnAxis();
    turnThread = std::thread(&turn_loop);
}
void shutdown_ship_tracker() {
    isWorking = false;
    turnCond.notify_all();
    if (turnThread.joinable())
        turnThread.join();
    kbd::reset_vJoy();
}

void resetTurnAxis() {
    kbd::axis(pitchAxis.bindings, 0, true);
    pitchAxis.value = 0;
    kbd::axis(yawAxis.bindings, 0, true);
    yawAxis.value = 0;
    kbd::axis(rollAxis.bindings, 0, true);
    rollAxis.value = 0;
}

void disableAutoTurn() {
    isActive = false;
    resetTurnAxis();
    turnCond.notify_one();
}

void request_pitch_roll(double pitch, bool without_roll) {
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

void set_axis(double speed, double delta, Axis& axis) {
    double value = 0;
    double seconds = std::abs(delta / speed);
    if (seconds > 0.03125)
        value = std::copysign(std::min(1.0, seconds), delta);
    kbd::axis(axis.bindings, value, axis.value == 0);
    axis.value = value;
}

void turn_loop() {
    SetThreadDescription(GetCurrentThread(), L"ShitTracker loop");

    LOG(INFO) << "Starting ship tracker";
    while (isWorking) {
        {
            std::unique_lock<std::mutex> lock(turnMutex);
            isLoopWaiting = true;
            turnCond.wait_for(lock, isActive ? 650ms : 1min);
            isLoopWaiting = false;
            if (!isWorking)
                break;
            if (!isActive) {
                resetTurnAxis();
                continue;
            }
        }
        try {
            turn_step();
        } catch(const std::exception& ex) {
            kbd::reset_vJoy();
            if (dynamic_cast<const interrupted_error*>(&ex)) {
                LOG(ERROR) << "Ship tracker interrupted";
                isActive = false;
            } else {
                LOG(ERROR) << "Exception in ship tracker: " << ex.what();
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

    auto shipStats = eddb::getShipStats();
    if (!shipStats) {
        LOG(ERROR) << "Unsupported or unknown ship";
        resetTurnAxis();
        isActive = false;
        return;
    }

    if (st::guiFocus != GuiFocus::None) {
        resetTurnAxis();
        return;
    }

    if (st::compass.timestamp < std::chrono::utc_clock::now() - 150ms) {
        std::promise<bool> promise;
        std::future<bool> future = promise.get_future();
        Mgr.pushDetectRequest(std::move(promise), { DetectLevel::Screen });
        auto status = future.wait_for(250ms);
        if (status != std::future_status::ready) {
            resetTurnAxis();
            return;
        }
    }
    CompassInfo compass = st::compass;
    auto compassElapsed = std::chrono::utc_clock::now() - compass.timestamp;
    if (!compass.hemisphere || compassElapsed > 250ms) {
        resetTurnAxis();
        return;
    }

    int speed_set_to = st::autopilot.speed_set_to.has_value() ? st::autopilot.speed_set_to.value() : 50;
    double pitchSpeed = shipStats->getPitchSpeed(speed_set_to);
    double yawSpeed = shipStats->getYawSpeed(speed_set_to);
    //double rollSpeed = shipStats->getRollSpeed(speed_set_to);

    double timeDelta = std::chrono::duration_cast<std::chrono::duration<double>>(compassElapsed).count();
    double expectedPitch = compass.targetPitch - timeDelta * pitchSpeed * pitchAxis.value;
    double expectedYaw = compass.targetYaw - timeDelta * yawSpeed * yawAxis.value;
    //double expectedRoll = compass.targetRoll - timeDelta * rollSpeed * rollAxis.value;

    double delta_pitch = expectedPitch - requestedPitch;
    if (delta_pitch > 180) delta_pitch = 360-delta_pitch;
    if (delta_pitch < -180) delta_pitch = 360+delta_pitch;
    double delta_yaw = expectedYaw;

    if (!compass.has_nav_target) {
        if (std::abs(delta_pitch) < 4)
            delta_pitch = 0;
        if (std::abs(delta_yaw) < 4)
            delta_yaw = 0;
    }

//    LOG(INFO) << std::format("KeepCourse: time delta: {}ms, {}/{}, p {:.1f}, y {:.1f}, r {:.1f}, a {:.1f}",
//                             int(timeDelta*1000), (compass.hemisphere > 0 ? "front" : "back"), compass.has_nav_target,
//                             compass.targetPitch, compass.targetYaw, compass.targetRoll, compass.targetAngle);
//    LOG(INFO) << std::format("KeepCourse: pitch from compass {:.1f}, approximated value {:.1f} with speed {:.1f}/{:.3f}",
//                             compass.targetPitch, expectedPitch, pitchSpeed, pitchAxis.value);

    set_axis(pitchSpeed, delta_pitch, pitchAxis);
    set_axis(yawSpeed, delta_yaw, yawAxis);
    //set_axis(rollSpeed, 0, rollAxis);
}

} //namespace ai
