//
// Created by mkizub on 25.11.2025.
//

#pragma once

#ifndef EDROBOT_UILAYOUT_H
#define EDROBOT_UILAYOUT_H

namespace wl {
class font;
}

const int LO_DLG_BORDER     = 10;
const int LO_BTN_W          = 80;
const int LO_BTN_H          = 24;
const int LO_ICN_S          = 16;
const int LO_V_GAP          = 2;
const int LO_X_GAP          = 5;
const int LO_H_GAP          = 10;
const int LO_V_ROW          = 20;
const int LO_TXT_20_W       = 100;
const int LO_TXT_50_W       = 250;
const int LO_TXT_6_W        = 40;

extern void loCreateFont(wl::font& font, UINT uiDpi, UINT uiPercent);

struct UILayout {
    UILayout(int uiDpi, int uiPercent, RECT& rect);
    int hgap, vgap, xgap, vrow, icsz, btnh, btnw, txt6w, txt20w, txt50w, border;
    int width, height;
    int left, top;
    wl::font *font;
    HDWP wpi;
};


#endif //EDROBOT_UILAYOUT_H
