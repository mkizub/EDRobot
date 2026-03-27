//
// Created by mkizub on 26.03.2026.
//

#pragma once

#ifndef EDROBOT_WL_POPUP_MENU_H
#define EDROBOT_WL_POPUP_MENU_H

#include <winlamb/menu.h>

namespace wl {

const static int popup_menu_id = 0x1001;

class popup_menu {
public:
    struct item {
        void* data;
        std::wstring text;
        std::wstring tooltip;
    };
    wl::menu _menu;
    std::vector<item> _items;
    std::function<bool(int idx, item& item)> _apply;

    popup_menu() = default;

    void destroy() {
        _menu.destroy();
        _items.clear();
        _apply = nullptr;
    }

    void append_item(void* data, std::wstring text, std::wstring tooltip={}) {
        _items.emplace_back(data, std::move(text), std::move(tooltip));
    }

    void append_separator() {
        _items.emplace_back(nullptr);
    }

    void show_at_point(HWND hParent, HWND hWnd, POINT pt, std::function<bool(int idx, item& item)> lambda) {
        _apply = lambda;
        _menu = CreatePopupMenu();
        for (int idx=0; idx < _items.size(); idx++) {
            auto& e  = _items[idx];
            if (e.text.empty())
                _menu.append_separator();
            else
                _menu.append_item(idx, e.text);
        }
        MENUINFO mi {sizeof(MENUINFO), MIM_STYLE|MIM_HELPID|MIM_MENUDATA, MNS_AUTODISMISS|MNS_NOCHECK|MNS_NOTIFYBYPOS};
        mi.dwContextHelpID = popup_menu_id;
        mi.dwMenuData = (ULONG_PTR)this;
        SetMenuInfo(_menu.hmenu(), &mi);
        _menu.show_at_point(hParent, pt, hWnd);
    }

    std::wstring_view tooltip(int idx) {
        if (idx < 0 || idx >= _items.size())
            return {};
        auto& item = _items[idx];
        if (item.text.empty()) // separator
            return {};
        return item.tooltip;
    }

    bool apply(int idx) {
        if (!_apply)
            return false;
        if (idx < 0 || idx >= _items.size())
            return false;
        auto& item = _items[idx];
        if (item.text.empty()) // separator
            return false;
        return _apply(idx, item);
    }

};

} // namespace wl

#endif //EDROBOT_WL_POPUP_MENU_H
