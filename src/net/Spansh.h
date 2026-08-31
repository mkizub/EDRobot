//
// Created by mkizub on 10.07.2026.
//

#ifndef EDROBOT_SPANSH_H
#define EDROBOT_SPANSH_H

namespace Spansh {
    gal::spStarSystem loadStarSystem(const std::string& systemName);
    gal::spStarSystem loadStarSystem(int64_t systemAddress);
    std::vector<gal::spStarSystem> listNearestSystems(const std::string& systemBegin, const std::string& systemEnd, double distance);
}

#endif //EDROBOT_SPANSH_H
