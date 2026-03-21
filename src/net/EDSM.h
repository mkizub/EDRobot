//
// Created by mkizub on 21.03.2026.
//

#pragma once

#ifndef EDROBOT_EDSM_H
#define EDROBOT_EDSM_H

class EDSM {
    EDSM();
public:
    static std::shared_ptr<EDSM> getInstance();

    virtual ~EDSM();

    js::value loadStarSystem(const std::string& name);

};


#endif //EDROBOT_EDSM_H
