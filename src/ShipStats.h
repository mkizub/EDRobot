//
// Created by mkizub on 11.08.2025.
//

#pragma once

#ifndef EDROBOT_SHIPSTATS_H
#define EDROBOT_SHIPSTATS_H


class ShipStats {
public:
    ShipStats(const std::string& type);

    double getPitch(int speed_percent) const;
    double getYaw(int speed_percent) const;
    double getRoll(int speed_percent) const;

private:
    const std::string type;
    const json5pp::value& jship;
    float space_pitch;
    float space_yaw;
    float space_roll;
    float cruise_pitch[3]; // at speed [0,50,100]
    float cruise_yaw[3]; // at speed [0,50,100]
    float cruise_roll[3]; // at speed [0,50,100]
};


#endif //EDROBOT_SHIPSTATS_H
