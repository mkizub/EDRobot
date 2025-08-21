//
// Created by mkizub on 11.08.2025.
//

#include "pch.h"

#include "ShipStats.h"

ShipStats::ShipStats(const string &type)
    : type(type)
    , jship(Cfg.getEDDBShip(type))
{
    space_pitch = jship["pitch"].as_number();
    space_yaw = jship["yaw"].as_number();
    space_roll = jship["roll"].as_number();
    auto& cr_p = jship["pitch_cruise"];
    auto& cr_y = jship["yaw_cruise"];
    auto& cr_r = jship["roll_cruise"];
    for (int i=0; i < 3; i++) {
        cruise_pitch[i] = cr_p[i].as_number();
        cruise_yaw[i] = cr_y[i].as_number();
        cruise_roll[i] = cr_r[i].as_number();
    }
}

double ShipStats::getPitch(int speed_percent) const {
    if (Cfg.getCurrentStatus()->flags.cruise) {
        double spd = 0.01 * std::clamp(speed_percent, 0, 100);
        if (spd >= 0.5)
            return std::lerp(cruise_pitch[1], cruise_pitch[2], 2*spd-1);
        return std::lerp(cruise_pitch[0], cruise_pitch[1], 2*spd);
    }
    return space_pitch;
}
double ShipStats::getYaw(int speed_percent) const {
    if (Cfg.getCurrentStatus()->flags.cruise) {
        double spd = 0.01 * std::clamp(speed_percent, 0, 100);
        if (spd >= 0.5)
            return std::lerp(cruise_yaw[1], cruise_yaw[2], 2*spd-1);
        return std::lerp(cruise_yaw[0], cruise_yaw[1], 2*spd);
    }
    return space_yaw;
}
double ShipStats::getRoll(int speed_percent) const {
    if (Cfg.getCurrentStatus()->flags.cruise) {
        double spd = 0.01 * std::clamp(speed_percent, 0, 100);
        if (spd >= 0.5)
            return std::lerp(cruise_roll[1], cruise_roll[2], 2*spd-1);
        return std::lerp(cruise_roll[0], cruise_roll[1], 2*spd);
    }
    return space_roll;
}
