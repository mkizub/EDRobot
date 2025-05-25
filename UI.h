//
// Created by mkizub on 25.05.2025.
//

#pragma once

#ifndef EDROBOT_UI_H
#define EDROBOT_UI_H


class UI {
public:
    static const int RES_OK     = 0;
    static const int RES_CANCEL = 1;
    static const int RES_EXIT   = 2;

    static int showStartupDialog();
    static int askSellInput(int& sells, int& items);

};



#endif //EDROBOT_UI_H
