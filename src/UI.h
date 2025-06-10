//
// Created by mkizub on 25.05.2025.
//

#pragma once

#ifndef EDROBOT_UI_H
#define EDROBOT_UI_H


namespace UI {

bool initializeUI();
void shutdownUI();
bool showToast(const std::string& title, const std::string& text);
bool showStartupDialog(const std::string& line1, const std::string& line2);
bool showCalibrationDialog(const std::string& line1);
bool askSellInput(int& total, int& chunk, std::string& commodity);
bool askSelectRectWindow();


}

#endif //EDROBOT_UI_H
