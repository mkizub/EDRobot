//
// Created by mkizub on 12.02.2026.
//

#pragma once

#ifndef EDROBOT_RAVENCOLONIAL_H
#define EDROBOT_RAVENCOLONIAL_H

namespace RavenColonial {

bool init();
bool shutdown();

gal::spEntity importConstructionProject(const std::string& systemName, const std::string& fullName, const std::string& shortName);

js::value carrierGetCargo(int64_t marketId);
void carrierPostCargo(int64_t marketId, js::value& j);
void carrierPatchCargo(int64_t marketId, js::value& j);
void reportShipCargo();
js::value queryShipsCargo(const spMarket& market);
void reportContribution(spGameEvent& ge);
void reportConstructionDepot(spGameEvent& ge, const spMarket& market);
spMarket updateConstructionDepot(spMarket market);

}

#endif //EDROBOT_RAVENCOLONIAL_H
