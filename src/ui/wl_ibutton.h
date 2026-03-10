//
// Created by mkizub on 10.03.2026.
//

#pragma once

#ifndef EDROBOT_WL_IBUTTON_H
#define EDROBOT_WL_IBUTTON_H

#include <winlamb/button.h>
#include <winlamb/icon.h>

namespace wl {

// Wrapper to native button control.
class ibutton final :
        public wnd,
        public _wli::base_native_ctrl_pubm<ibutton>,
        public _wli::base_focus_pubm<ibutton>
{
private:
    HWND                   _hWnd = nullptr;
    _wli::base_native_ctrl _baseNativeCtrl{_hWnd};

    wl::icon _icon;
    int _iconId {};
    int _iconSz {};

public:
    // Wraps window style changes done by Get/SetWindowLongPtr.
    _wli::styler<ibutton> style{this};

    ibutton() noexcept :
            wnd(_hWnd), base_native_ctrl_pubm(_baseNativeCtrl),
            base_focus_pubm(_hWnd) { }

    ibutton(ibutton&&) = default;
    ibutton& operator=(ibutton&&) = default; // movable only

    ibutton& create(HWND hParent, int ctrlId,
                    int iconId, int iconSz, POINT pos, SIZE size)
    {
        _iconId = iconId;
        _iconSz = iconSz;
        this->_icon.load_from_resource(_iconId, {_iconSz,_iconSz});
        this->_baseNativeCtrl.create(hParent, ctrlId, L"", pos, size, L"Button", (WS_CHILD | WS_VISIBLE | BS_ICON));
        SendMessage(this->_hWnd, BM_SETIMAGE, (WPARAM)IMAGE_ICON, (LPARAM)_icon.hicon());
        return *this;
    }

    ibutton& create(const wnd* parent, int ctrlId,
                    int iconId, int iconSz, POINT pos, SIZE size)
    {
        return this->create(parent->hwnd(), ctrlId, iconId, iconSz, pos, size);
    }

    ibutton& set_icon_resource(int iconId)
    {
        if (_iconId != iconId) {
            _iconId = iconId;
            this->_icon.load_from_resource(_iconId, {_iconSz, _iconSz});
            SendMessage(this->_hWnd, BM_SETIMAGE, (WPARAM) IMAGE_ICON, (LPARAM) _icon.hicon());
        }
        return *this;
    }

    ibutton& set_icon_size(int iconSz)
    {
        if (_iconSz != iconSz) {
            _iconSz = iconSz;
            this->_icon.load_from_resource(_iconId, {_iconSz, _iconSz});
            SendMessage(this->_hWnd, BM_SETIMAGE, (WPARAM) IMAGE_ICON, (LPARAM) _icon.hicon());
        }
        return *this;
    }
};

}

#endif //EDROBOT_WL_IBUTTON_H
