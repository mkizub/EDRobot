//
// Created by mkizub on 10.07.2026.
//

#ifndef EDROBOT_SPANSH_H
#define EDROBOT_SPANSH_H

namespace Spansh {
    gal::spStarSystem loadStarSystem(std::string_view systemName);
    gal::spStarSystem loadStarSystem(int64_t systemAddress);

    using listCallback = std::function<bool(gal::spStarSystem, js::value&)>;
    std::vector<gal::spStarSystem> listSystemsUsingRequest(js::value j_request, int max_pages, listCallback systemCallback);
    std::vector<gal::spStarSystem> listNearestSystems(const std::string& systemBegin, const std::string& systemEnd, double distance);
    std::vector<gal::spStarSystem> listSpanshSearch(const std::string& uuid, listCallback systemCallback);
}

#endif //EDROBOT_SPANSH_H
