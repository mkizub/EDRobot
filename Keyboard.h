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

const std::vector<std::string>& getNamesForKey(const std::string& key);
void intercept(const std::vector<std::string>& keys);
void start(KeyboardCollbackFn callback);
void stop();

bool sendKeyDown(const std::string& key_name);
bool sendKeyUp(const std::string& key_name);
bool sendMouseMoveTo(int x, int y, bool absolute, bool virtualDesk);
bool sendMouseDown(int buttons);
bool sendMouseUp(int buttons);
bool sendMouseWheel(int count); // positive - forward, away from the user; negative - backward, toward the user

}


#endif //EDROBOT_KEYBOARD_H
