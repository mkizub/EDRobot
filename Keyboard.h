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
const int EXT   = 0x10;

void start(KeyboardCollbackFn callback);
void stop();

bool sendDown(const std::string& key_name);
bool sendUp(const std::string& key_name);

}


#endif //EDROBOT_KEYBOARD_H
