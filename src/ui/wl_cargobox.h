//
// Created by mkizub on 11.02.2026.
//

#pragma once

#ifndef EDROBOT_WL_CARGOBOX_H
#define EDROBOT_WL_CARGOBOX_H

#include <set>
#include <string>

#include <shellapi.h>
#include <winlamb/textbox.h>
#include <winlamb/menu.h>

namespace wl {

const int cargobox_menu_id = 0x1001;

class cargobox :
        public wnd,
        public _wli::base_native_ctrl_pubm<cargobox>,
        public _wli::base_focus_pubm<cargobox>,
        public _wli::base_text_pubm<cargobox>
{
private:
    HWND                   _hWnd = nullptr;
    _wli::base_native_ctrl _baseNativeCtrl{_hWnd};
    std::vector<std::wstring> _entries;
    wl::menu               _menu;

public:
    // Wraps window style changes done by Get/SetWindowLongPtr.
    _wli::styler<cargobox> style{this};

    cargobox() noexcept :
            wnd(_hWnd), base_native_ctrl_pubm(_baseNativeCtrl), base_focus_pubm(_hWnd), base_text_pubm(_hWnd) { }

    cargobox(cargobox&&) = default;
    cargobox& operator=(cargobox&&) = default; // movable only

    cargobox& create(HWND hParent, int ctrlId, POINT pos, SIZE size, LONG dropheight=0)
    {
        this->_baseNativeCtrl.create(hParent, ctrlId, nullptr, pos, size, WC_EDIT,
                                     WS_CHILD | WS_VISIBLE | WS_TABSTOP, WS_EX_CLIENTEDGE);
        return *this;
    }

    cargobox& create(const wnd* parent, int ctrlId, POINT pos, SIZE size)
    {
        return this->create(parent->hwnd(), ctrlId, pos, size);
    }

    cargobox& select(size_t index) noexcept {
        SendMessageW(this->_hWnd, CB_SETCURSEL, index, 0);
        return *this;
    }

    void remove_all() {
        _menu.destroy();
    }

    bool apply_menu(int idx) {
        if (idx < 0 || idx >= _entries.size())
            return false;
        auto len = _entries[idx].size();
        set_text(_entries[idx]);
        SendMessage(hwnd(), EM_SETSEL, (WPARAM)len, (LPARAM)len);
        return true;
    }

    cargobox& auto_drop(HWND parent) noexcept {
        std::wstring text = get_text();
        if (text.size() < 2) {
            _menu.destroy();
            return *this;
        }

        std::set<std::wstring> new_set;
        std::wstring text_l = toLower(text);
        for (auto* c : Cfg.getAllKnownCommodities()) {
            if (toUtf16(c->nameId).starts_with(text_l))
                new_set.insert(c->wide);
            else if (!c->translation[int(Lang::EN)].empty() && toUtf16(toLower(c->translation[int(Lang::EN)])).starts_with(text_l))
                new_set.insert(c->wide);
            else if (st::lng != Lang::EN && !c->translation[int(st::lng)].empty() && toLower(toUtf16(c->translation[int(st::lng)])).starts_with(text_l))
                new_set.insert(c->wide);
        }
        if (new_set.size() == 0 || new_set.size() == 0 > 10) {
            _entries.clear();
            _menu.destroy();
            return *this;
        }
        _entries.assign(new_set.begin(), new_set.end());
        if (new_set.size() == 1 && text == _entries[0]) {
            _menu.destroy();
            return *this;
        }
        _menu = CreatePopupMenu();
        int idx = 0;
        for (auto& s : new_set)
            _menu.append_item(idx++, s);
        MENUINFO mi {sizeof(MENUINFO), MIM_STYLE|MIM_HELPID|MIM_MENUDATA, MNS_AUTODISMISS|MNS_NOCHECK|MNS_NOTIFYBYPOS};
        mi.dwContextHelpID = cargobox_menu_id;
        mi.dwMenuData = (ULONG_PTR)this;
        SetMenuInfo(_menu.hmenu(), &mi);
        _menu.show_at_point(parent, {0,20}, hwnd());
        return *this;
    }
};

} // wl

#endif //EDROBOT_WL_CARGOBOX_H
