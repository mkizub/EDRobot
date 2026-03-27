//
// Created by mkizub on 10.03.2026.
//

#pragma once

#ifndef EDROBOT_WL_SVG_STATIC_H
#define EDROBOT_WL_SVG_STATIC_H

#include <winlamb/label.h>
#include <winlamb/button.h>
#include <CommCtrl.h>

#include "UIManager.h"

namespace wl {

// Wrapper to native button control.
class svg_static final :
        public wnd,
        public _wli::base_native_ctrl_pubm<svg_static>,
        public _wli::base_focus_pubm<svg_static>
{
private:
    HWND                   _hWnd = nullptr;
    _wli::base_native_ctrl _baseNativeCtrl{_hWnd};

    HBITMAP _hBitmap;
    std::string _iconId;
    int _iconSz;

public:
    // Wraps window style changes done by Get/SetWindowLongPtr.
    _wli::styler<svg_static> style{this};

    svg_static() noexcept :
            wnd(_hWnd), base_native_ctrl_pubm(_baseNativeCtrl),
            base_focus_pubm(_hWnd) { }

    svg_static(svg_static&&) = default;
    svg_static& operator=(svg_static&&) = default; // movable only

    svg_static& create(HWND hParent, int ctrlId, std::string_view iconId, int iconSz, POINT pos, SIZE size)
    {
        _iconId = iconId;
        _iconSz = iconSz;
        _hBitmap = UIManager::makeIconBitmap(_iconId, _iconSz);
        this->_baseNativeCtrl.create(hParent, ctrlId, L"", pos, size, WC_STATIC, (WS_CHILD | WS_VISIBLE | SS_BITMAP | SS_CENTERIMAGE));
        SendMessage(this->_hWnd, STM_SETIMAGE, (WPARAM)IMAGE_BITMAP, (LPARAM)_hBitmap);
        return *this;
    }

    svg_static& create(const wnd* parent, int ctrlId, std::string_view iconId, int iconSz, POINT pos, SIZE size)
    {
        return this->create(parent->hwnd(), ctrlId, iconId, iconSz, pos, size);
    }

    svg_static& set_icon(std::string_view iconId)
    {
        if (_iconId != iconId) {
            _iconId = iconId;
            _hBitmap = UIManager::makeIconBitmap(_iconId, _iconSz);
            SendMessage(this->_hWnd, STM_SETIMAGE, (WPARAM) IMAGE_BITMAP, (LPARAM)_hBitmap);
        }
        return *this;
    }

    svg_static& set_icon_size(int iconSz)
    {
        if (_iconSz != iconSz) {
            _iconSz = iconSz;
            _hBitmap = UIManager::makeIconBitmap(_iconId, _iconSz);
            SendMessage(this->_hWnd, STM_SETIMAGE, (WPARAM) IMAGE_BITMAP, (LPARAM)_hBitmap);
        }
        return *this;
    }
};

}

#endif //EDROBOT_WL_SVG_STATIC_H
