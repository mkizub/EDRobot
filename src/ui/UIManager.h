//
// Created by mkizub on 11.06.2025.
//

#pragma once

#ifndef EDROBOT_UIMANAGER_H
#define EDROBOT_UIMANAGER_H

class UIToast;
class UISellInput;
class UICalibration;
class UISelectRect;

class UIManager {
    UIManager();

public:
    static UIManager& getInstance();

    bool initialize();
    bool shutdown();

    bool showStartupDialog(const std::string& message);
    bool showToast(const std::string& title, const std::string& text);
    bool askSellInput(int& total, int& chunk, Commodity*& commodity);
    bool askCalibrationDialog(const std::string& line1);
    bool askSelectRectWindow();

};


#endif //EDROBOT_UIMANAGER_H
