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

#include "wl_popup_menu.h"

namespace wl {

class cargobox :
        public wnd,
        public _wli::base_native_ctrl_pubm<cargobox>,
        public _wli::base_focus_pubm<cargobox>,
        public _wli::base_text_pubm<cargobox>
{
private:
    HWND                   _hWnd = nullptr;
    _wli::base_native_ctrl _baseNativeCtrl{_hWnd};
    wl::popup_menu         _menu;
    std::function<void(int)> _apply;

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

    cargobox& auto_drop(HWND parent) noexcept {
        _menu.destroy();
        std::wstring text = get_text();
        if (text.size() < 2)
            return *this;

        std::set<Commodity*> commodities;
        std::wstring text_l = toLower(text);
        for (auto* c : Cfg.getAllKnownCommodities()) {
            if (toUtf16(c->nameId).starts_with(text_l))
                commodities.insert(c);
            else if (!c->translation[int(Lang::EN)].empty() && toUtf16(toLower(c->translation[int(Lang::EN)])).starts_with(text_l))
                commodities.insert(c);
            else if (st::lng != Lang::EN && !c->translation[int(st::lng)].empty() && toLower(toUtf16(c->translation[int(st::lng)])).starts_with(text_l))
                commodities.insert(c);
        }
        if (commodities.size() == 0 || commodities.size() == 0 > 10)
            return *this;
        if (commodities.size() == 1 && text == (*commodities.begin())->wide)
            return *this;
        for (auto* c : commodities) {
            std::string tooltip = c->category->name;
            if (st::lng != Lang::EN && !c->category->translation[int(Lang::EN)].empty())
                tooltip += "\r\n(" + c->category->translation[int(Lang::EN)]+")";
            for (auto& tr : c->translation) {
                if (!tr.empty()) {
                    tooltip += "\r\n";
                    tooltip += tr;
                }
            }
            _menu.append_item(c, c->wide, toUtf16(tooltip));
        }
        _menu.show_at_point(parent, hwnd(), {0,20}, [this](int idx, wl::popup_menu::item& item) {
            auto& name = ((Commodity*)item.data)->wide;
            this->set_text(name);
            SendMessage(this->hwnd(), EM_SETSEL, (WPARAM)name.length(), (LPARAM)name.length());
            return true;
        });
        return *this;
    }
};

} // wl

#endif //EDROBOT_WL_CARGOBOX_H
