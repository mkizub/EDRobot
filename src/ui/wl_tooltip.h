//
// Created by mkizub on 26.03.2026.
//

#pragma once

#ifndef EDROBOT_WL_TOOLTIP_H
#define EDROBOT_WL_TOOLTIP_H

#include <winlamb/label.h>
#include <winlamb/button.h>
#include <CommCtrl.h>

#include "UIManager.h"

namespace wl {

// Wrapper to native button control.
class tooltip final :
    public wl::wnd
{
private:
    HWND                   _hWnd = nullptr;
    HWND                   _hParent = nullptr;
    TOOLINFO               _ti {};
    std::wstring           _text;

public:
    // Wraps window style changes done by Get/SetWindowLongPtr.
    _wli::styler<tooltip> style{this};

    tooltip() noexcept :
            wnd(_hWnd) { }

    tooltip(tooltip&&) = default;
    tooltip& operator=(tooltip&&) = default; // movable only

    ~tooltip() {
        HWND hWnd = this->_hWnd;
        if (this->_hWnd) {
            this->_hWnd = nullptr;
            DestroyWindow(hWnd);
        }
    }

    tooltip& create(HWND hParent)
    {
        if (_hWnd)
            return *this;
        HINSTANCE hInst = reinterpret_cast<HINSTANCE>(GetWindowLongPtrW(hParent, GWLP_HINSTANCE));
        _hWnd = CreateWindowEx(WS_EX_TOPMOST, TOOLTIPS_CLASS, NULL,
                WS_POPUP | TTS_NOPREFIX, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
                hParent, // Parent window handle
                NULL,
                hInst, // Application instance handle
                NULL
        );
        _hParent = hParent;
        _ti.cbSize = sizeof(TOOLINFO);
        _ti.hwnd = _hParent;
        _ti.uId = (UINT_PTR)_hWnd;
        _ti.uFlags = TTF_IDISHWND;
        _ti.lpszText = (LPWSTR)L"My tooltip";
        SendMessage(_hWnd, TTM_ADDTOOL, 0, (LPARAM)&_ti);
        int uiDpi = GetDpiForWindow(_hWnd);
        int maxWidth = MulDiv(600, uiDpi, USER_DEFAULT_SCREEN_DPI);
        SendMessage(_hWnd, TTM_SETMAXTIPWIDTH, 0, maxWidth);
        return *this;
    }

    tooltip& create(const wnd* parent)
    {
        return this->create(parent->hwnd());
    }

    tooltip& hide() {
        if (this->_hWnd) {
            SendMessage(this->_hWnd, TTM_TRACKACTIVATE, FALSE, (LPARAM)&_ti);
        }
        return *this;
    }

    tooltip& show(std::wstring_view text) {
        if (this->_hWnd) {
            _text = text;
            _ti.lpszText = _text.data();
            SendMessage(_hWnd, TTM_UPDATETIPTEXT, 0, (LPARAM)&_ti);
            POINT pt;
            GetCursorPos(&pt);
            SendMessage(this->_hWnd, TTM_TRACKPOSITION, 0, (LPARAM)MAKELONG(pt.x, pt.y));
            SendMessage(this->_hWnd, TTM_TRACKACTIVATE, TRUE, (LPARAM)&_ti);
        }
        return *this;
    }

};

}

#endif //EDROBOT_WL_TOOLTIP_H
