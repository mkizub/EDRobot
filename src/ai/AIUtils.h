//
// Created by mkizub on 10.12.2025.
//

#ifndef EDROBOT_AIUTILS_H
#define EDROBOT_AIUTILS_H

namespace ai {

bool clickWidget(const char* btn, int delay_ms, int pause_ms);
bool clickButton(const char* btn);
bool moveToWidget(const char* widget);
bool waitUiState(const std::string& state, std::chrono::seconds duration);
bool waitMarketEvent(std::chrono::seconds duration);
bool selectOnGalaxyMap(const std::string& systemName);
bool leaveScrGalaxy();
void gotoMarketScreen(bool buy);
void gotoLandingPad(bool refuel);

} // ai

#endif //EDROBOT_AIUTILS_H
