//
// Created by mkizub on 25.05.2025.
//

#pragma once

#ifndef EDROBOT_UI_H
#define EDROBOT_UI_H

#include "ui/UIManager.h"

namespace UI {

inline bool initializeUI() { return UIManager::getInstance().initialize(); }
inline void shutdownUI() { UIManager::getInstance().shutdown(); }
inline bool showToast(const std::string& title, const std::string& text) { return UIManager::getInstance().showToast(title, text); }
inline bool showStartupDialog(const std::string& text) { return UIManager::getInstance().showStartupDialog(text); }
inline bool showCalibrationDialog(const std::string& text) { return UIManager::getInstance().askCalibrationDialog(text); }
inline bool askSellInput(int& total, int& chunk, Commodity*& commodity) { return UIManager::getInstance().askSellInput(total, chunk, commodity); }
inline bool askSelectRectWindow() { return UIManager::getInstance().askSelectRectWindow(); }

}

#endif //EDROBOT_UI_H
