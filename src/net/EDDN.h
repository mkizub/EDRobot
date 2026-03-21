//
// Created by mkizub on 20.03.2026.
//

#pragma once

#ifndef EDROBOT_EDDN_H
#define EDROBOT_EDDN_H

class EDDN {
    EDDN();
public:
    static std::shared_ptr<EDDN> getInstance();

    virtual ~EDDN();

    void event_Location(spGameEvent& ge);
    void event_FSDJump(spGameEvent& ge);
    void event_CarrierJump(spGameEvent& ge);
    void event_Docked(spGameEvent& ge);
    void event_NavRoute(spGameEvent& ge);
    void event_FSSSignalDiscovered(const std::vector<spGameEvent>& events);
    void event_NavBeaconScan(spGameEvent& ge);
    void event_FSSDiscoveryScan(spGameEvent& ge);
    void event_FSSAllBodiesFound(spGameEvent& ge);
    void event_FSSBodySignals(spGameEvent& ge);
    void event_Scan(spGameEvent& ge);
    void event_ScanBaryCentre(spGameEvent& ge);

    void event_Market(spGameEvent& ge);
private:
    void scanEvent(spGameEvent& ge, const char* event, const char* schema);

};


#endif //EDROBOT_EDDN_H
