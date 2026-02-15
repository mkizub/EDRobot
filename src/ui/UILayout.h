//
// Created by mkizub on 25.11.2025.
//

#pragma once

#ifndef EDROBOT_UILAYOUT_H
#define EDROBOT_UILAYOUT_H

const int LO_DLG_BORDER     = 10;
const int LO_BTN_W          = 80;
const int LO_BTN_H          = 24;
const int LO_V_GAP          = 2;
const int LO_X_GAP          = 5;
const int LO_H_GAP          = 10;
const int LO_V_ROW          = 20;
const int LO_TXT_20_W       = 100;
const int LO_TXT_50_W       = 250;
const int LO_TXT_6_W        = 40;

extern void loCreateFont(wl::font& font, int uiDpi, int uiPercent);

#endif //EDROBOT_UILAYOUT_H
