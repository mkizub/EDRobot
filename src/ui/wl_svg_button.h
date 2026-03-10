//
// Created by mkizub on 10.03.2026.
//

#pragma once

#ifndef EDROBOT_WL_IBUTTON_H
#define EDROBOT_WL_IBUTTON_H

#include <winlamb/button.h>
#include <winlamb/icon.h>

#include "UIManager.h"

namespace wl {

// Wrapper to native button control.
class svg_button final :
        public wnd,
        public _wli::base_native_ctrl_pubm<svg_button>,
        public _wli::base_focus_pubm<svg_button>
{
private:
    HWND                   _hWnd = nullptr;
    _wli::base_native_ctrl _baseNativeCtrl{_hWnd};

    HBITMAP _hBitmap;
    std::string _iconId;
    int _iconSz;

public:
    // Wraps window style changes done by Get/SetWindowLongPtr.
    _wli::styler<svg_button> style{this};

    svg_button() noexcept :
            wnd(_hWnd), base_native_ctrl_pubm(_baseNativeCtrl),
            base_focus_pubm(_hWnd) { }

    svg_button(svg_button&&) = default;
    svg_button& operator=(svg_button&&) = default; // movable only

    svg_button& create(HWND hParent, int ctrlId, std::string_view iconId, int iconSz, POINT pos, SIZE size)
    {
        _iconId = iconId;
        _iconSz = iconSz;
        _hBitmap = UIManager::makeIconBitmap(_iconId, _iconSz);
        this->_baseNativeCtrl.create(hParent, ctrlId, L"", pos, size, L"Button", (WS_CHILD | WS_VISIBLE | BS_BITMAP));
        SendMessage(this->_hWnd, BM_SETIMAGE, (WPARAM)IMAGE_BITMAP, (LPARAM)_hBitmap);
        return *this;
    }

    svg_button& create(const wnd* parent, int ctrlId, std::string_view iconId, int iconSz, POINT pos, SIZE size)
    {
        return this->create(parent->hwnd(), ctrlId, iconId, iconSz, pos, size);
    }

    svg_button& set_icon(std::string_view iconId)
    {
        if (_iconId != iconId) {
            _iconId = iconId;
            _hBitmap = UIManager::makeIconBitmap(_iconId, _iconSz);
            SendMessage(this->_hWnd, BM_SETIMAGE, (WPARAM) IMAGE_BITMAP, (LPARAM)_hBitmap);
        }
        return *this;
    }

    svg_button& set_icon_size(int iconSz)
    {
        if (_iconSz != iconSz) {
            _iconSz = iconSz;
            _hBitmap = UIManager::makeIconBitmap(_iconId, _iconSz);
            SendMessage(this->_hWnd, BM_SETIMAGE, (WPARAM) IMAGE_BITMAP, (LPARAM)_hBitmap);
        }
        return *this;
    }
};

}

#endif //EDROBOT_WL_IBUTTON_H
