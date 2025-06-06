//
// Created by mkizub on 04.06.2025.
//

#pragma once

#ifndef EDROBOT_CONFIGURATION_H
#define EDROBOT_CONFIGURATION_H

enum class Command;

enum class ButtonState : int { Normal, Focused, Activated, Disabled };

class CReadDirectoryChanges;

class Configuration {
public:
    enum class FullScreenMode : int { Window, FullScreen, Borderless };

    Configuration();
    ~Configuration();

    bool load();
    void setCalibrationResult(const std::array<cv::Vec3b,4>& luv);
    bool saveCalibration() const;
    bool checkResolutionSupported(cv::Size gameSize);
    bool checkNeedColorCalibration();
    std::string getShortcutFor(Command cmd) const;

private:
    friend class Master;

    void parseShortcutConfig(Command command, const std::string& name, json5pp::value cfg);
    bool loadCalibration();
    bool loadGameSettings();
    bool checkReloadGameSettings();

    std::unique_ptr<CReadDirectoryChanges> changeDirListener;

    int defaultKeyHoldTime = 35;
    int defaultKeyAfterTime = 50;
    int searchRegionExtent = 10;
    std::wstring mEDSettingsPath;
    std::wstring mEDLogsPath;
    std::map<std::pair<std::string,unsigned>, Command> keyMapping;

    double configDashboardGUIBrightness = 0; // Options/Player/Custom.?.misc: <DashboardGUIBrightness Value="0.49999991" />
    double configGammaOffset = 0; // Options/Graphics/Settings.xml: <GammaOffset>1.200000</GammaOffset>
    double configFOV = 56.249001; // Options/Graphics/Settings.xml: <FOV>1.200000</FOV>
    int configScreenWidth = 0;    // Options\Graphics\DisplaySettings.xml: <ScreenWidth>1920</ScreenWidth>
    int configScreenHeight = 0;   // Options\Graphics\DisplaySettings.xml: <ScreenHeight>1080</ScreenHeight>
    FullScreenMode configFullScreen = FullScreenMode::Window; // Options\Graphics\DisplaySettings.xml: <FullScreen>0</FullScreen>

    double calibrationDashboardGUIBrightness = -1;
    double calibrationGammaOffset = 0;
    int calibrationScreenWidth = 0;
    int calibrationScreenHeight = 0;
    FullScreenMode calibrationFullScreen = FullScreenMode::Window;

    std::array<cv::Vec3b, 4> mButtonLuv;
};


#endif //EDROBOT_CONFIGURATION_H
