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
#include <winlamb/combobox.h>

namespace wl {

class cargobox :
        public wnd,
        public _wli::base_native_ctrl_pubm<cargobox>,
        public _wli::base_text_pubm<cargobox>
{
private:
    HWND                   _hWnd = nullptr;
    _wli::base_native_ctrl _baseNativeCtrl{_hWnd};
    std::set<std::wstring> _entries;

public:
    // Wraps window style changes done by Get/SetWindowLongPtr.
    _wli::styler<cargobox> style{this};

    cargobox() noexcept :
            wnd(_hWnd), base_native_ctrl_pubm(_baseNativeCtrl), base_text_pubm(_hWnd) { }

    cargobox(cargobox&&) = default;
    cargobox& operator=(cargobox&&) = default; // movable only

    cargobox& create(HWND hParent, int ctrlId, POINT pos, SIZE size, LONG dropheight)
    {
        this->_baseNativeCtrl.create(hParent, ctrlId, nullptr,
                                     pos, {size.cx, dropheight}, WC_COMBOBOX,
                                     WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWN | CBS_SORT | WS_VSCROLL, 0);
        SendMessage(this->_hWnd, CB_SETITEMHEIGHT, 1, (LPARAM)size.cy);
        return *this;
    }

    cargobox& create(const wnd* parent, int ctrlId, POINT pos, SIZE size, LONG dropheight)
    {
        return this->create(parent->hwnd(), ctrlId, pos, size, dropheight);
    }

    size_t count() const noexcept {
        return SendMessageW(this->_hWnd, CB_GETCOUNT, 0, 0);
    }

    size_t get_selected_index() const noexcept {
        return static_cast<size_t>(SendMessageW(this->_hWnd, CB_GETCURSEL, 0, 0));
    }

    cargobox& remove_all() noexcept {
        SendMessageW(this->_hWnd, CB_RESETCONTENT, 0, 0);
        _entries.clear();
        return *this;
    }

    cargobox& set_list(const std::set<std::wstring>& entries) noexcept {
        for (auto it=_entries.begin(); it != _entries.end(); ) {
            auto& t = *it;
            if (!entries.contains(t)) {
                LRESULT index = SendMessage(this->_hWnd, CB_FINDSTRINGEXACT, (WPARAM)-1, (LPARAM)t.c_str());
                if (index != CB_ERR)
                    SendMessage(this->_hWnd, CB_DELETESTRING, (WPARAM)index, 0);
                it = _entries.erase(it);
            } else {
                ++it;
            }
        }
        for (auto& t : entries) {
            if (!_entries.contains(t)) {
                SendMessage(this->_hWnd, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(t.c_str()));
                _entries.insert(t);
            }
        }
        return *this;
    }

    std::wstring get_entry_text(size_t index) const {
        std::wstring buf;
        size_t len = SendMessageW(this->_hWnd, CB_GETLBTEXTLEN, index, 0);
        if (len) {
            buf.resize(len, L'\0');
            SendMessageW(this->_hWnd, CB_GETLBTEXT, index, reinterpret_cast<LPARAM>(&buf[0]));
            buf.resize(len);
        }
        return buf;
    }

    std::wstring get_selected_text() const {
        return this->get_entry_text(this->get_selected_index());
    }

    cargobox& select(size_t index) noexcept {
        SendMessageW(this->_hWnd, CB_SETCURSEL, index, 0);
        return *this;
    }
};

} // wl

#endif //EDROBOT_WL_CARGOBOX_H
