//
// Created by mkizub on 05.10.2025.
//

#pragma once

#ifndef EDROBOT_STATE_H
#define EDROBOT_STATE_H

namespace st {

extern struct DockedAt {
    int64_t marketId;
    std::string stationName;
    std::string stationType;
} dockedAt;

extern struct Space {
    std::string starsystem;
    std::string body;
    int bodyId;
    std::string bodyType;
} space;

extern union NavPanelFilters {
    NavPanelFilters() : mask(0) {}
    struct {
        bool pointOfInterest: 1;
        bool star: 1;
        bool settlement: 1;
        bool station: 1;
        bool signalSource: 1;
        bool asteroidCluster: 1;
        bool landablePlanetOrMoon: 1;
        bool system: 1;
        bool fleetCarrier: 1;
        bool planetOrMoon: 1;
    };
    int mask;
    bool operator==(const NavPanelFilters& other) const { return this->mask == other.mask; }
    bool operator!=(const NavPanelFilters& other) const { return this->mask != other.mask; }
} navFilters;

}


#endif //EDROBOT_STATE_H
