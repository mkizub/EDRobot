//
// Created by mkizub on 01.09.2026.
//

#pragma once

#ifndef EDROBOT_DB_H
#define EDROBOT_DB_H

namespace db {

struct StarSystem {
    int64_t id;
    std::string name;
    double x, y, z;
    int64_t population;
    int64_t blobId;
};

bool init();
bool shutdown();
StarSystem loadStarSystem(std::string_view name);
StarSystem loadStarSystem(int64_t address);
bool saveStarSystem(const StarSystem& starSystem);
//enum class JsBlobTable : int {
//    Systems = 1,
//    Bodies = 2,
//    Stations = 3
//};
//void loadJsBlobs(int64_t blobId);

}

#endif //EDROBOT_DB_H
