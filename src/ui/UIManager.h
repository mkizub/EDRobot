//
// Created by mkizub on 11.06.2025.
//

#pragma once

#ifndef EDROBOT_UIMANAGER_H
#define EDROBOT_UIMANAGER_H

class UIToast;
class UISelectRect;
class UIMainDialog;

class UIManager {
    UIManager(UIMainDialog&);
    ~UIManager();

public:
    static UIManager& getInstance();

    static bool initialize();
    static bool shutdown();

    static bool showStartupDialog(const std::string& message, std::string latest_version, std::string latest_url);
    static bool showMainDialog();
    static bool hideMainDialog(bool force);
    static bool showToast(const std::string& title, const std::string& text);
    static bool hasDebugWindow();
    static bool showDebugWindow();
    static bool postToDebugWindow(const XMat& image);
    static bool postToDebugWindow(const XMat& image, const cv::Mat& overlay);
    static bool askSelectRectWindow();

    UIMainDialog& uiMain;

private:
    void uiThreadLoop();

    std::thread uiThread;
};


#endif //EDROBOT_UIMANAGER_H
