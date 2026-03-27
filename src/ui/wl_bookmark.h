//
// Created by mkizub on 10.03.2026.
//

#pragma once

#ifndef EDROBOT_WL_BOOKMARK_H
#define EDROBOT_WL_BOOKMARK_H

#include <winlamb/label.h>
#include <winlamb/button.h>
#include <winlamb/textbox.h>
#include "wl_popup_menu.h"
#include <CommCtrl.h>
#include <memory>
#include <vector>

#include "../Types.h"
#include "../Utils.h"
#include "../Configuration.h"
#include "UIManager.h"
#include "UIControl.h"

namespace wl {

// Wrapper to native button control.
class bookmark final :
        public wnd,
        public _wli::base_native_ctrl_pubm<bookmark>,
        public _wli::base_focus_pubm<bookmark>
{
private:
    HWND                   _hWnd = nullptr;
    _wli::base_native_ctrl _baseNativeCtrl{_hWnd};
    wl::popup_menu         _menu;

    HBITMAP _hBitmap;
    std::string _iconId;
    int _iconSz;

public:
    // Wraps window style changes done by Get/SetWindowLongPtr.
    _wli::styler<bookmark> style{this};

    bookmark() noexcept :
            wnd(_hWnd), base_native_ctrl_pubm(_baseNativeCtrl),
            base_focus_pubm(_hWnd) { }

    bookmark(bookmark&&) = default;
    bookmark& operator=(bookmark&&) = default; // movable only

    bookmark& create(HWND hParent, int ctrlId, std::string_view iconId, int iconSz, POINT pos, SIZE size)
    {
        _iconId = iconId;
        _iconSz = iconSz;
        _hBitmap = UIManager::makeIconBitmap(_iconId, _iconSz);
        this->_baseNativeCtrl.create(hParent, ctrlId, L"", pos, size, WC_STATIC, (WS_CHILD | WS_VISIBLE | SS_BITMAP | SS_CENTERIMAGE));
        SendMessage(this->_hWnd, STM_SETIMAGE, (WPARAM)IMAGE_BITMAP, (LPARAM)_hBitmap);
        return *this;
    }

    bookmark& create(const wnd* parent, int ctrlId, std::string_view iconId, int iconSz, POINT pos, SIZE size)
    {
        return this->create(parent->hwnd(), ctrlId, iconId, iconSz, pos, size);
    }

    bookmark& set_icon(std::string_view iconId)
    {
        if (_iconId != iconId) {
            _iconId = iconId;
            _hBitmap = UIManager::makeIconBitmap(_iconId, _iconSz);
            SendMessage(this->_hWnd, STM_SETIMAGE, (WPARAM) IMAGE_BITMAP, (LPARAM)_hBitmap);
        }
        return *this;
    }

    bookmark& set_icon_size(int iconSz)
    {
        if (_iconSz != iconSz) {
            _iconSz = iconSz;
            _hBitmap = UIManager::makeIconBitmap(_iconId, _iconSz);
            SendMessage(this->_hWnd, STM_SETIMAGE, (WPARAM) IMAGE_BITMAP, (LPARAM)_hBitmap);
        }
        return *this;
    }

    bookmark& show_drop(UIControl* parent, wl::textbox& tb_system, wl::textbox& tb_dock) noexcept {
        _menu.destroy();
        auto system_name = toUtf8(tb_system.get_text());
        auto dock_name = toUtf8(tb_dock.get_text());
        std::vector<spBookmark> bookmarks = Cfg.getBookmarks();
        void* modeADD = reinterpret_cast<void*>(1);
        void* modeDEL = reinterpret_cast<void*>(2);
        spBookmark have_bm;
        for (auto& bm : bookmarks) {
            if (system_name == bm->system && dock_name == bm->dock) {
                have_bm = bm;
                break;
            }
        }
        if (have_bm) {
            if (have_bm->comment.empty()) {
                _menu.append_item(modeDEL, toUtf16(_gt("Remove from bookmarks")));
                _menu.append_separator();
            }
        }
        else if (!system_name.empty() && !dock_name.empty()) {
            _menu.append_item(modeADD, toUtf16(_gt("Add to bookmarks")));
            _menu.append_separator();
        }
        for (auto& bm : Cfg.getBookmarks()) {
            std::string tooltip = std::format("{}: {}\r\n{}: {}", _gt("Star system"), bm->system, _gt("Dock"), bm->dock);
            if (!bm->comment.empty()) {
                tooltip += "\r\n";
                tooltip += bm->comment;
            }
            _menu.append_item(bm.get(), toUtf16(bm->name), toUtf16(tooltip));
        }
        _menu.show_at_point(parent->hwnd(), hwnd(), {0,20}, [this, parent, modeADD, modeDEL, &tb_system, &tb_dock](int idx, wl::popup_menu::item& item) {
            if (item.data == nullptr) {
                return false;
            }
            if (item.data == modeADD) {
                std::wstring system = tb_system.get_text();
                std::wstring dock = tb_dock.get_text();
                Cfg.addBookmark(0, toUtf8(system), toUtf8(dock));
                parent->validate();
                return true;
            }
            if (item.data == modeDEL) {
                std::wstring system = tb_system.get_text();
                std::wstring dock = tb_dock.get_text();
                Cfg.delBookmark(toUtf8(system), toUtf8(dock));
                parent->validate();
                return true;
            }
            Bookmark* bm = static_cast<Bookmark *>(item.data);
            tb_system.set_text(toUtf16(bm->system));
            tb_dock.set_text(toUtf16(bm->dock));
            return true;
        });
        return *this;
    }
};

}

#endif //EDROBOT_WL_BOOKMARK_H
