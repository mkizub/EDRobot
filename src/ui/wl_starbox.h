//
// Created by mkizub on 11.02.2026.
//

#pragma once

#ifndef EDROBOT_WL_STARBOX_H
#define EDROBOT_WL_STARBOX_H

#include <set>
#include <string>

#include <shellapi.h>
#include <winlamb/textbox.h>
#include <winlamb/combobox.h>

namespace wl {

class starbox final :
        public wnd,
        public _wli::base_native_ctrl_pubm<starbox>,
        public _wli::base_focus_pubm<textbox>,
        public _wli::base_text_pubm<starbox>
{
public:
    enum class sort { SORTED, UNSORTED };

private:
    HWND                   _hWnd = nullptr;
    _wli::base_native_ctrl _baseNativeCtrl{_hWnd};

public:
    // Wraps window style changes done by Get/SetWindowLongPtr.
    _wli::styler<starbox> style{this};

    starbox() noexcept :
            wnd(_hWnd), base_native_ctrl_pubm(_baseNativeCtrl),
            base_focus_pubm(_hWnd), base_text_pubm(_hWnd)
    { }

    starbox(starbox&&) = default;
    starbox& operator=(starbox&&) = default; // movable only

    starbox& create(HWND hParent, int ctrlId, POINT pos,
                     LONG width, sort sortType)
    {
        this->_baseNativeCtrl.create(hParent, ctrlId, nullptr,
                                     pos, {width, 0}, L"combobox",
                                     WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWN |
                                     (sortType == sort::SORTED ? CBS_SORT : 0), 0);
        return *this;
    }

    starbox& create(const wnd* parent, int ctrlId, POINT pos,
                     LONG width, sort sortType)
    {
        return this->create(parent->hwnd(), ctrlId, pos, width, sortType);
    }

    size_t count() const noexcept {
        return SendMessageW(this->_hWnd, CB_GETCOUNT, 0, 0);
    }

    size_t get_selected_index() const noexcept {
        return static_cast<size_t>(SendMessageW(this->_hWnd, CB_GETCURSEL, 0, 0));
    }

    starbox& remove_all() noexcept {
        SendMessageW(this->_hWnd, CB_RESETCONTENT, 0, 0);
        return *this;
    }

    starbox& add(const wchar_t* entries, wchar_t delimiter = L'|') {
        wchar_t delim[2]{delimiter, L'\0'};
        std::vector<std::wstring> vals = str::split(entries, delim);
        for (const std::wstring& s : vals) {
            SendMessageW(this->_hWnd, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(s.c_str()));
        }
        return *this;
    }

    starbox& add(std::initializer_list<const wchar_t*> entries) noexcept {
        for (const wchar_t* s : entries) {
            SendMessageW(this->_hWnd, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(s));
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

    starbox& select(size_t index) noexcept {
        SendMessageW(this->_hWnd, CB_SETCURSEL, index, 0);
        return *this;
    }
};

} // wl

#endif //EDROBOT_WL_STARBOX_H
