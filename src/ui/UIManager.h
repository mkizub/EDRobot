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

    static bool initialize();
    static bool shutdown();

    static bool showStartupDialog(const std::string& message);
    static bool showToast(const std::string& title, const std::string& text);
    static bool showDebugWindow();
    static bool postToDebugWindow(const cv::Mat& image);
    static bool askSellInput(int& total, int& chunk, Commodity*& commodity);
    static bool askCalibrationDialog(const std::string& line1);
    static bool askSelectRectWindow();

};


#endif //EDROBOT_UIMANAGER_H
