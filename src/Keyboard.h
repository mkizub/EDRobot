//
// Created by mkizub on 22.05.2025.
//

#pragma once

#ifndef EDROBOT_KEYBOARD_H
#define EDROBOT_KEYBOARD_H

#include <functional>
#include <thread>

namespace kbd {

typedef std::function<void(int code, int scancode, int flags, const std::string& name)> KeyboardCollbackFn;

const int LSHIFT = 0x01;
const int RSHIFT = 0x02;
const int LCTRL  = 0x04;
const int RCTRL  = 0x08;
const int LALT   = 0x10;
const int RALT   = 0x20;
const int LWIN   = 0x40;
const int RWIN   = 0x80;

const int MOUSE_L_BUTTON = 0x1;
const int MOUSE_R_BUTTON = 0x2;
const int MOUSE_M_BUTTON = 0x4;

struct vJoyAxisInfo {
    std::string name;
    UINT devID;
    UINT axisID;
    LONG min;
    LONG max;
    bool full; // full range is -1 .. +1, otherwise 0 .. +1
};

const std::vector<std::string>& getNamesForKey(const std::string& key);
void intercept(const std::vector<std::string>& keys);
bool acquire_vJoy();
bool reset_vJoy();
bool release_vJoy();
void start(KeyboardCollbackFn callback);
void stop();

int getScanCode(const std::string& key_name);
bool send(const std::string& name, int delay_ms= 0, int pause_ms= 0, bool precise= false);
bool sendMouseMove(const cv::Point& point, int pause_ms, bool absolute=true);
bool sendMouseClick(const cv::Point& point, int delay_ms, int pause_ms);
bool sendMouseWheel(int count);
bool sendMouseMoveTo(int x, int y, bool absolute, bool virtualDesk);
bool sendMouseWheel(int count); // positive - forward, away from the user; negative - backward, toward the user
cv::Point getMouseDesktopPos();
// return inputId that can be used to clearInput()
unsigned post(const std::string& name, int hold_ms);
unsigned post(const GameKey& gk, int hold_ms);
bool clearInput(unsigned inputId);
bool axis(const KeyBindings& gk, double value, bool background=false); // value -1..1 for full-range axes, or 0..1 for others

}


#endif //EDROBOT_KEYBOARD_H
