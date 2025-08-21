//
// Created by mkizub on 22.05.2025.
//

#pragma once

#ifndef EDROBOT_KEYBOARD_H
#define EDROBOT_KEYBOARD_H

#include <functional>
#include <thread>

namespace keyboard {

typedef std::function<void(int code, int scancode, int flags, const std::string& name)> KeyboardCollbackFn;

const int SHIFT = 0x01;
const int CTRL  = 0x02;
const int ALT   = 0x04;
const int WIN   = 0x08;

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
//bool sendKeyDown(const GameKey& gk);
//bool sendKeyUp(const GameKey& gk);
//bool sendKeyDown(const std::string& key_name);
//bool sendKeyUp(const std::string& key_name);
bool sendMouseMoveTo(int x, int y, bool absolute, bool virtualDesk);
//bool sendMouseDown(int buttons);
//bool sendMouseUp(int buttons);
bool sendMouseWheel(int count); // positive - forward, away from the user; negative - backward, toward the user
// return inputId that can be used to clearInput()
unsigned sendKeyDown(const GameKey& gk, int hold, int pause, HANDLE event);
bool clearInput(unsigned inputId);
const vJoyAxisInfo* getJoyAxis(const std::string& axis_name);
bool sendJoyAxis(const KeyBindings& gk, double value); // value -1..1 for full-range axes, or 0..1 for others
bool sendJoyAxis(const std::string& axis_name, double value); // value -1..1 for full-range axes, or 0..1 for others

}


#endif //EDROBOT_KEYBOARD_H
