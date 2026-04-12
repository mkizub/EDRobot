//
// Created by mkizub on 12.02.2026.
//

#pragma once

#ifndef EDROBOT_RAVENCOLONIAL_H
#define EDROBOT_RAVENCOLONIAL_H

class RavenColonial {
    static std::shared_ptr<RavenColonial> gInstance;

    std::atomic<bool> shipCargoReportEnabled;
    RavenColonial();
public:
    static std::shared_ptr<RavenColonial> getInstance();
    static std::shared_ptr<RavenColonial> newInstance();
    static void shutdown();

    virtual ~RavenColonial();

    static gal::spEntity importConstructionProject(
            const std::string& systemName, const std::string& fullName, const std::string& shortName);

    static bool linkCmdr(int64_t marketId);
    static js::value carrierGetCargo(int64_t marketId);
    static void carrierPostCargo(int64_t marketId, js::value& j);
    static void carrierPatchCargo(int64_t marketId, const std::map<Commodity*,int>& patch);
    static void reportContribution(spGameEvent& ge);
    static void reportConstructionDepot(spGameEvent& ge, const spMarket& market);
    static spMarket updateConstructionDepot(spMarket market);

    void setShipCargoReport(bool on);
    void reportShipCargo();
    js::value queryShipsCargo(const spMarket& market);
};

#endif //EDROBOT_RAVENCOLONIAL_H
