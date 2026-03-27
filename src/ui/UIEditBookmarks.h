//
// Created by mkizub on 26.03.2026.
//

#pragma once

#ifndef EDROBOT_UIEDITBOOKMARKS_H
#define EDROBOT_UIEDITBOOKMARKS_H

#include "UIControl.h"
#include <winlamb/button.h>

class BookmarkCtrl;

class UIEditBookmarks : public UIControl {
public:
    UIEditBookmarks();
    ~UIEditBookmarks();

    const wchar_t* title() const override { return L"EDRobot bookmarks"; };
    void initialize() override;
    void relayout(bool scroll_to_top=false) override;
    void on_update() override;
    void on_ctrl_edit(int id, WORD msg) override;
    bool validate() const override;
    void clear();

    void on_bookmarks_save();

    wl::button btn_save;
    std::deque<std::unique_ptr<BookmarkCtrl>> controls;
};


#endif //EDROBOT_UIEDITBOOKMARKS_H
