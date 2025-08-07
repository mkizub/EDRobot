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
class Main;

class UIManager {
    UIManager(Main&);

public:
    static UIManager& getInstance();

    static bool initialize();
    static bool shutdown();

    static bool showStartupDialog(const std::string& message);
    static bool showMainDialog();
    static bool showToast(const std::string& title, const std::string& text);
    static bool hasDebugWindow();
    static bool showDebugWindow();
    static bool postToDebugWindow(const XMat& image);
    static bool postToDebugWindow(const XMat& image, const cv::Mat& overlay);
    static bool askCalibrationDialog(const std::string& line1);
    static bool askSelectRectWindow();

private:
    void uiThreadLoop();

    Main& uiMain;
    std::thread uiThread;
};


#endif //EDROBOT_UIMANAGER_H
