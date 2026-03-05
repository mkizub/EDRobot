//
// Created by mkizub on 04.03.2026.
//

#pragma once

#ifndef EDROBOT_CARGOMANAGER_H
#define EDROBOT_CARGOMANAGER_H

class CargoManager {
    friend class Configuration;
    friend class Master;
    friend void parseEvent_CarrierLocation(spGameEvent& ge);

    CargoManager();

    std::mutex cargoMutex;

    Timestamp timestampShip;
    Timestamp timestampFC;
    //Timestamp timestampSRV;

    spShipCargo shipCargo;
    spShipCargo carrierCargo;
    //spShipCargo srvCargo;

    bool loadCarrierCargo();
public:
    static CargoManager& getInstance();

    spShipCargo getShipCargo() { return shipCargo; }
    spShipCargo getCarrierCargo() { return carrierCargo; }

    bool loadShipCargo(spGameEvent ge);
    bool saveCarrierCargo(Timestamp timestamp, const std::map<Commodity*,int>& patch);
    bool processMarketBuy(spGameEvent ge);
    bool processMarketSell(spGameEvent ge);
    bool processColonisationContribution(spGameEvent ge);
    bool processCargoTransfer(spGameEvent ge);

};

extern CargoManager& CM;


#endif //EDROBOT_CARGOMANAGER_H
