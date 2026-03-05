//
// Created by mkizub on 12.02.2026.
//

#pragma once

#ifndef EDROBOT_RAVENCOLONIAL_H
#define EDROBOT_RAVENCOLONIAL_H

namespace RavenColonial {

bool init();
bool shutdown();

js::value carrierGetCargo(int64_t marketId);
void carrierPostCargo(int64_t marketId, js::value& j);
void carrierPatchCargo(int64_t marketId, js::value& j);
void reportShipCargo();
void reportContribution(spGameEvent& ge);
void reportConstructionDepot(spGameEvent& ge, spMarket market);
spMarket updateConstructionDepot(spMarket market);
js::value queryShipsCargo(spMarket market);

}

#endif //EDROBOT_RAVENCOLONIAL_H
