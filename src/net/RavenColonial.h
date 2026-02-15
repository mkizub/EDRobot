//
// Created by mkizub on 12.02.2026.
//

#pragma once

#ifndef EDROBOT_RAVENCOLONIAL_H
#define EDROBOT_RAVENCOLONIAL_H

namespace RavenColonial {

json5pp::value carrierGetCargo(int64_t marketId);
void carrierPostCargo(int64_t marketId, json5pp::value& j);
void carrierPatchCargo(int64_t marketId, json5pp::value& j);
void reportContribution(spGameEvent& ge);
void reportConstructionDepot(spGameEvent& ge, spMarket market);

}

#endif //EDROBOT_RAVENCOLONIAL_H
