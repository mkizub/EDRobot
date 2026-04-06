//
// Created by mkizub on 05.11.2025.
//

#include "../pch.h"

#include "Types.h"
#include "AutopilotTasks.h"
#include "AIManager.h"
#include "../Keyboard.h"
#include "../ShipStats.h"

#include <boost/circular_buffer.hpp>

Axis pitchAxis(Axis::Pitch);
Axis yawAxis(Axis::Yaw);
Axis rollAxis(Axis::Roll);

static GameKey mouseResetKey;

void Axis::resetAll(bool reset_mouse) {
    kbd::axis(pitchAxis.bindings, 0, true);
    pitchAxis.value = 0;
    pitchAxis.start = {};
    pitchAxis.stop = {};
    kbd::axis(yawAxis.bindings, 0, true);
    yawAxis.value = 0;
    yawAxis.start = {};
    yawAxis.stop = {};
    kbd::axis(rollAxis.bindings, 0, true);
    rollAxis.value = 0;
    rollAxis.start = {};
    rollAxis.stop = {};
    if (reset_mouse && mouseResetKey.device == GameKey::Device::vJoy)
        kbd::post(mouseResetKey, Cfg.getDefaultKeyHoldTime()); // post to not wait
}

double Axis::timeScaleFor(double val) const {
    auto ship = eddb::getShipStats();
    if (!ship)
        return 1;
    auto abs_val = std::abs(val);
    if (abs_val >= 1)
        return 1;
    double rot_scale = ship->getRotationScale(type, st::autopilot.speed_set_to.value_or(50));
    double time_scale = rot_scale * std::pow(abs_val, -0.2);
    return std::max(time_scale, abs_val);
}

double Axis::valueScaleFor(double val) const {
    auto ship = eddb::getShipStats();
    if (!ship)
        return val;
    auto abs_val = std::abs(val);
    double rot_scale = ship->getRotationScale(type, st::autopilot.speed_set_to.value_or(50));
    double val_scale = std::pow(abs_val*rot_scale, -0.2)*rot_scale;
    return std::clamp(val_scale*val, -1.0, +1.0);
}


void Axis::set(double val, int duration) {
    if (val == 0) {
        reset();
        return;
    }
    auto utc_now = std::chrono::utc_clock::now();
    if (!start.time_since_epoch().count())
        start = utc_now;
    value = std::clamp(val, -1.0, +1.0);
#if 1
    kbd::axis(bindings, valueScaleFor(value), false);
    stop = utc_now + std::chrono::milliseconds(duration);
#else
    kbd::axis(bindings, value, false);
    stop = utc_now + std::chrono::milliseconds(duration*timeScaleFor(value));
#endif
}

void Axis::setRaw(double val, int duration) {
    if (val == 0) {
        reset();
        return;
    }
    value = std::clamp(val, -1.0, +1.0);
    kbd::axis(bindings, value, false);
    auto utc_now = std::chrono::utc_clock::now();
    if (!start.time_since_epoch().count())
        start = utc_now;
    stop = utc_now + std::chrono::milliseconds(duration);
}

void Axis::reset() {
    value = 0;
    kbd::axis(bindings, 0, false);
    start = {};
    stop = {};
}

bool Axis::active() const {
    return value != 0 && start.time_since_epoch().count() && stop.time_since_epoch().count();
}


namespace ai {

extern std::thread turnThread;
extern std::mutex turnMutex;
extern std::condition_variable turnCond;

namespace {
    std::atomic_bool isWorking;
    std::atomic_bool isLoopWaiting;
    std::atomic_bool isActive;

    double requestedPitch;
    bool disableRoll;
}

void turn_loop();
std::chrono::duration<double> turn_step();
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


bool setAxisBindings(Axis& axis, bool invert) {
    const KeyBindings& orig = Cfg.getGameKeyBindings(std::string(axis.name()) + "AxisRaw");
    axis.bindings = orig;
    if (!(orig.mode == KeyBindings::Axis || orig.mode == KeyBindings::AxisInv)) {
        LOG_ERROR("Bad bindings for {}, vJoy axis required", axis.name());
        return false;
    }
    if (invert) {
        if (axis.bindings.mode == KeyBindings::Axis)
            axis.bindings.mode = KeyBindings::AxisInv;
        else
            axis.bindings.mode = KeyBindings::Axis;
    }
    return true;
}

bool init_ship_tracker() {
    isWorking = true;
    bool ok = true;
    ok &= setAxisBindings(pitchAxis, true);
    ok &= setAxisBindings(yawAxis, false);
    ok &= setAxisBindings(rollAxis, false);

    auto& kb = Cfg.getGameKeyBindings("MouseReset");
    if (kb.primary.device == GameKey::vJoy)
        mouseResetKey = kb.primary;
    else if (kb.secondary.device == GameKey::vJoy)
        mouseResetKey = kb.secondary;
    else {
        LOG_ERROR("Bad bindings for MouseReset, vJoy button required");
        ok = false;
    }
    if (!ok)
        return false;

    Axis::resetAll();
    turnThread = std::thread(&turn_loop);
    return true;
}
bool shutdown_ship_tracker() {
    isWorking = false;
    turnCond.notify_all();
    if (turnThread.joinable())
        turnThread.join();
    kbd::reset_vJoy();
    return true;
}

void disableAutoTurn() {
    isActive = false;
    Axis::resetAll(true);
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
    Axis::resetAll(true);
    isActive = true;
    turnCond.notify_one();
}

struct CompassDist {
    Timestamp timestamp;
    dist_t dist;
};
boost::circular_buffer<CompassDist> buffer(6);
std::string buffer_target_name;

void resetCompassDetects() {
    buffer.clear();
    buffer_target_name = st::destination.name;
    st::autopilot.distanceToDock = {};
    st::autopilot.distanceToBody = {};
    st::autopilot.distanceToTarget = {};
}

static inline bool in_range(dist_t d1, dist_t d2) {
    if (!d1 || !d2)
        return false;
    double rel = d1.get_km() / d2.get_km();
    return (rel >= 0.5 && rel <= 2.0);
}

void approximateCompassDistance(CompassInfo& compass) {

    if (st::destination.name != buffer_target_name) {
        buffer.clear();
        if (st::autopilot.destDock && st::autopilot.destDock->nameEq(buffer_target_name))
            st::autopilot.distanceToDock = {};
        else if (st::autopilot.destBody && st::autopilot.destBody->nameEq(buffer_target_name))
            st::autopilot.distanceToBody = {};
        st::autopilot.distanceToTarget = {};
        buffer_target_name = st::destination.name;
    }

    // clear outdated entries
    while (!buffer.empty() && (buffer.front().timestamp+5s) < compass.timestamp)
        buffer.pop_front();

    dist_t& d2t = st::autopilot.distanceToTarget;
    dist_t d = compass.nav_target_dist;
    if (d) {
        bool in_prev_range = in_range(d, d2t);

        // check it match prev entries
        int buffer_rate = d.conf;
        for (int i=int(buffer.size())-1; i >= 0; i--) {
            auto& cd = buffer[i];
            if (in_range(d, cd.dist)) {
                if (d > d2t)
                    buffer_rate += cd.dist.conf/2;
                else
                    buffer_rate += cd.dist.conf;
            }
            else
                buffer_rate -= cd.dist.conf;
        }

        buffer.push_back({compass.timestamp, d});

        if (in_prev_range || buffer_rate >= 0) {
            if (in_prev_range && buffer_rate > 0)
                LOG_INFO("Target dist (both ): {} (conf {})", d.to_string(), int(d.conf));
            else if (buffer_rate > 0) {
                //std::string history;
                //history += d2t.to_string();
                //for (int i=int(buffer.size())-1; i >= 0; i--) {
                //    history += " " + buffer[i].dist.to_string();
                //}
                //LOG_INFO("Target dist history: {}", history);
                LOG_INFO("Target dist (ACCUM): {} (conf {})", d.to_string(), int(d.conf));
            }
            else if (in_prev_range)
                LOG_INFO("Target dist (prev ): {} (conf {})", d.to_string(), int(d.conf));
            else
                LOG_INFO("Target dist (first): {} (conf {})", d.to_string(), int(d.conf));

            d2t = d; // st::autopilot.distanceToTarget = d;

            if (st::autopilot.destDock && st::autopilot.destDock->nameEq(buffer_target_name))
                st::autopilot.distanceToDock = d2t;
            else if (st::autopilot.destBody && st::autopilot.destBody->nameEq(buffer_target_name))
                st::autopilot.distanceToBody = d2t;
        } else {
            LOG_INFO("Target dist (ignored): {} (conf {})", d.to_string(), int(d.conf));
        }
    }
}

void reportCompassDetect(CompassInfo& compass) {
    approximateCompassDistance(compass);
    if (isActive)
        turnCond.notify_one();
}

static int compassLostCounter;

void turn_loop() {
    SetThreadDescription(GetCurrentThread(), L"ShipTracker loop");

    LOG_INFO("Starting ship tracker");
    std::chrono::duration<double> wait_dur = 0ms;
    while (isWorking) {
        {
            std::unique_lock<std::mutex> lock(turnMutex);
            isLoopWaiting = true;
            if (!isActive)
                wait_dur = 1min;
            else if (ai::uiState.autopilot)
                wait_dur = 5s;
            else if (st::autopilot.distanceToDock < 3_Mm && wait_dur > 250ms)
                wait_dur = 250ms;
            else if (wait_dur > 650ms)
                wait_dur = 650ms;
            turnCond.wait_for(lock, wait_dur);
            isLoopWaiting = false;
            if (!isWorking)
                break;
            if (!isActive)
                continue;
        }
        try {
            wait_dur = turn_step();
        } catch(const std::exception& ex) {
            kbd::reset_vJoy();
            if (dynamic_cast<const interrupted_error*>(&ex)) {
                LOG_ERROR("Ship tracker interrupted");
                isActive = false;
                Axis::resetAll(true);
                compassLostCounter = 0;
            } else {
                LOG_ERROR("Exception in ship tracker: {}", ex.what());
            }
        }
    }
    LOG_INFO("Exiting ship tracker");
}

inline double normalizeAngle(double angle) {
    if (angle > 180) angle =  360 - angle;
    if (angle < -180) angle = 360 + angle;
    return angle;
}

double update_axis(Axis& axis, double delta, double max_speed) {
    if (std::abs(delta) < 0.1) {
        axis.reset();
        return 2.0;
    }

    double time_at_max_speed = std::abs(delta) / max_speed;

    auto utc_now = std::chrono::utc_clock::now();
    double time = std::chrono::duration_cast<std::chrono::duration<double>>(axis.stop - utc_now).count();

    if (!axis.active() || std::signbit(delta) != std::signbit(axis.value) || time < 0) {
        time = std::max({2.0, time_at_max_speed});
        double value = std::copysign(time_at_max_speed / time, delta);
//        LOG_INFO("KeepCourse: set {} {:.3f}, start seconds {:.2f}", axis.name, value, time);
        axis.set(value, int(time * 1000));
        return time;
    }

    double miss = delta - time_at_max_speed * std::abs(axis.value) * time;
    if (std::abs(miss) < 0.5)
        return time;

    double max_value = std::abs(delta) < 5 ? 0.5 : 1.0;
    // change axis.value keeping the same time if possible
    double new_speed = delta / time;
    if (std::abs(new_speed) <= (max_speed*max_value)) {
        double value = std::clamp(new_speed / max_speed, -max_value, +max_value);
//        LOG_INFO("KeepCourse: set {} {:.3f}, keep seconds {:.2f}", axis.name, value, time);
        axis.set(value, int(time * 1000));
        return time;
    }
    // set new axis.value and time
    time = std::abs(delta) / (max_speed * max_value);
    new_speed = delta / time;
    double value = std::clamp(new_speed / max_speed, -max_value, +max_value);
//    LOG_INFO("KeepCourse: set {} {:.3f}, seconds {:.2f}", axis.name, value, time);
    axis.set(value, int(time * 1000));
    return time;
}

std::chrono::duration<double> turn_step() {
    bool forceScreenDetect = false;

repeat_step:
    if (isDebugPause()) {
        Axis::resetAll();
        compassLostCounter = 0;
        return 1s;
    }

    auto shipStats = eddb::getShipStats();
    if (!shipStats) {
        LOG_ERROR("Unsupported or unknown ship");
        isActive = false;
        Axis::resetAll();
        compassLostCounter = 0;
        return 1min;
    }

    if (st::guiFocus != GuiFocus::None) {
        Axis::resetAll();
        compassLostCounter = 0;
        return 1s;
    }

    bool hasActiveAxis = pitchAxis.active() || yawAxis.active() || rollAxis.active();
    const auto compass_age = (hasActiveAxis && useOpenCL()) ? 500ms : 1500ms;

#ifdef NDEBUG
    const auto max_wait_age = (hasActiveAxis && useOpenCL()) ? 500ms : 1500ms;
#else
    const auto max_wait_age = 1000ms;
#endif
    auto utc_now = std::chrono::utc_clock::now();
    if (forceScreenDetect || !st::compass.hemisphere || st::compass.timestamp < utc_now - compass_age) {
        std::promise<bool> promise;
        std::future<bool> future = promise.get_future();
        Mgr.pushDetectRequest(std::move(promise), { DetectLevel::Screen });
        auto status = future.wait_for(max_wait_age);
        if (status != std::future_status::ready) {
            LOG_WARNING("KeepCourse: screen detect request status: {}", status);
            Axis::resetAll();
            return compass_age;
        }
        utc_now = std::chrono::utc_clock::now();
    }
    CompassInfo compass = st::compass;
    auto compassElapsed = utc_now - compass.timestamp;
//    LOG_INFO("KeepCourse: time delta: {}ms, hemispere {}",
//                             std::chrono::duration_cast<std::chrono::milliseconds>(compassElapsed).count(),
//                             compass.hemisphere);
    if (!compass.hemisphere || compassElapsed > max_wait_age) {
        compassLostCounter += 1;
        //LOG_INFO("KeepCourse: compass trouble {}",compassLostCounter);
        if (compassLostCounter >= 3) {
            LOG_WARNING("KeepCourse: compass lost");
            Axis::resetAll();
            compassLostCounter = 1;
        }
        return compass_age;
    } else {
        compassLostCounter = 0;
    }

//    if (kbd::isCapsLocked()) {
//        Axis::resetAll();
//        return 250ms;
//    }

    int speed_set_to = st::autopilot.speed_set_to.value_or(50);
    double pitchSpeed = shipStats->getPitchSpeed(speed_set_to);
    double yawSpeed = shipStats->getYawSpeed(speed_set_to);
    //double rollSpeed = shipStats->getRollSpeed(speed_set_to);

    double timeDelta = std::chrono::duration_cast<std::chrono::duration<double>>(compassElapsed).count();
    double expectedPitch = compass.targetPitch - timeDelta * pitchSpeed * pitchAxis.value;
    double expectedYaw = compass.targetYaw - timeDelta * yawSpeed * yawAxis.value;
    //double expectedRoll = compass.targetRoll - timeDelta * rollSpeed * rollAxis.value;

    double pitchDelta = normalizeAngle(expectedPitch - requestedPitch);
    double hemiYaw = expectedYaw;
    if (compass.hemisphere < 0) {
        if (hemiYaw > 0)
            hemiYaw = 180 - hemiYaw;
        else
            hemiYaw = -180 - hemiYaw;
    }
    double yawDelta = normalizeAngle(hemiYaw);

    if (compass.hemisphere < 0) {
        if (std::abs(pitchDelta) < 5)
            pitchDelta = 0;
        if (std::abs(yawDelta) < 5)
            yawDelta = 0;
    }

//    LOG_INFO("KeepCourse: time delta: {}ms, {}/{}, p {:.1f}, y {:.1f}, r {:.1f}, a {:.1f}",
//                             int(timeDelta*1000), (compass.hemisphere > 0 ? "front" : "back"), compass.has_nav_target,
//                             compass.targetPitch, compass.targetYaw, compass.targetRoll, compass.targetAngle);
//    LOG_INFO("KeepCourse: pitch from compass {:.1f}, approximated value {:.1f} with speed {:.1f}/{:.3f}",
//                             compass.targetPitch, expectedPitch, pitchSpeed, pitchAxis.value);
//    LOG_INFO("KeepCourse: yaw   from compass {:.1f}, approximated value {:.1f} with speed {:.1f}/{:.3f}",
//                             compass.targetYaw, expectedYaw, yawSpeed, yawAxis.value);

    double pitchSleep = update_axis(pitchAxis, pitchDelta, pitchSpeed);
    double yawSleep = update_axis(yawAxis, yawDelta, yawSpeed);
    rollAxis.reset();

    double sleep = std::min({2.0, pitchSleep, yawSleep});
    if (std::abs(pitchDelta) <= 8 || std::abs(yawDelta) <= 8)
        sleep *= 0.5;
    if (sleep < 0.15) {
        forceScreenDetect = true;
        goto repeat_step;
    }

    return std::chrono::duration<double>(sleep-0.05);
}

} //namespace ai
