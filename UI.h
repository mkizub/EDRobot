//
// Created by mkizub on 25.05.2025.
//

#pragma once

#ifndef EDROBOT_UI_H
#define EDROBOT_UI_H


namespace UI {

bool showStartupDialog();
bool askSellInput(int& sells, int& items);
bool showSceneEditorDialog();
void selectRectDialog(HWND = nullptr);


}

#endif //EDROBOT_UI_H
