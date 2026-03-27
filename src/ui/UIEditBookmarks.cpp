//
// Created by mkizub on 26.03.2026.
//

#include "../pch.h"

#include "UIEditBookmarks.h"
#include "UILayout.h"

#include <winlamb/textbox.h>
#include "wl_svg_button.h"
#include "wl_svg_static.h"

#include "../../ui/resource.h"

class BookmarkCtrl {
public:
    BookmarkCtrl(UIEditBookmarks* ui, const spBookmark& bm);
    virtual ~BookmarkCtrl();
    virtual void create();
    virtual void layout(UILayout& lo);
    virtual void on_ctrl_edit(HWND changed, WORD msg);
    virtual bool validate(bool* changed);
    UIEditBookmarks* ui;
    spBookmark bm;
    bool expanded;
    bool deleted;

    std::wstring name_text;
    std::wstring star_text;
    std::wstring dock_text;
    std::wstring desc_text;

    wl::svg_button btn_exp;
    wl::svg_button btn_del;

    wl::textbox tb_name;
    wl::textbox tb_star;
    wl::textbox tb_dock;
    wl::textbox tb_desc;
};

UIEditBookmarks::UIEditBookmarks() : UIControl(true) {
    on_command(ID_SAVE, [this](wl::params params) {
        on_bookmarks_save();
        return 0;
    });
}

UIEditBookmarks::~UIEditBookmarks() {
}


void UIEditBookmarks::initialize() {
    btn_save.create(hwnd(), ID_SAVE, toUtf16(_gt("Save bookmarks")).c_str(), {0,0}, {LO_BTN_W,LO_BTN_H}).set_enabled(false);
    clear();
    beginControls();
    auto allBookmarks = Cfg.getBookmarks();
    for (auto& bm : allBookmarks) {
        controls.emplace_back(new BookmarkCtrl(this, bm))->create();
    }
    spBookmark bm(new Bookmark{});
    controls.emplace_back(new BookmarkCtrl(this, bm))->create();
    endControls();
    relayout(true);
}

void UIEditBookmarks::clear() {
    controls.clear();
    nextTryId = 0;
    usedIds = {};
}

void UIEditBookmarks::relayout(bool scroll_to_top) {
    if (scroll_to_top)
        reset_scroll(true);

    RECT rect{};
    GetClientRect(hwnd(), &rect);

    int uiPercent = Cfg.getUiScalePercents();
    int uiDpi = GetDpiForWindow(hwnd());
    UILayout lo(uiDpi, uiPercent, rect);
    if (uiDpi != scaled_to_dpi) {
        scaled_to_dpi = uiDpi;
        loCreateFont(font, uiDpi, uiPercent);
        lo.font = &font;
    }

    panel_width = lo.width;
    panel_height = lo.height;

    lo.wpi = BeginDeferWindowPos(100);
    lo.left = lo.vgap;
    lo.top = lo.vgap - scroll_pos;
    lo.width -= 2*lo.vgap;
    if (lo.font) {
        lo.font->set_on(btn_save);
    }

    int cx = lo.left + lo.width/2;
    lo.wpi = DeferWindowPos(lo.wpi, btn_save.hwnd(), nullptr, cx-lo.btnw, lo.top, 2*lo.btnw, lo.btnh, SWP_NOZORDER);
    lo.top += lo.btnh+lo.vgap;

    for (auto& cc : controls) {
        cc->layout(lo);
    }
    params_height = lo.top + scroll_pos;
    EndDeferWindowPos(lo.wpi);

    reset_scroll(false);
}

void UIEditBookmarks::on_update() {
    bool has_deleted = false;
    for (auto& cc : controls) {
        if (cc->deleted)
            has_deleted = true;
    }
    if (has_deleted) {
        auto* self = const_cast<UIEditBookmarks*>(this);
        self->beginControls();
        std::erase_if(self->controls,[=](auto& bc) { return bc->deleted; });
        self->endControls();
        self->relayout();
    }

    bool add_bookmark = false;
    if (controls.empty()) {
        add_bookmark = true;
    } else {
        auto& bc = controls.back();
        if (!(bc->star_text.empty() || bc->dock_text.empty()))
            add_bookmark = true;
    }
    if (add_bookmark) {
        beginControls();
        spBookmark bm(new Bookmark{});
        controls.emplace_back(new BookmarkCtrl(this, bm))->create();
        endControls();
        relayout();
    }

    std::vector<spBookmark> bookmarks;
    for (auto& cc : controls) {
        if (cc->star_text.empty() || cc->dock_text.empty())
            continue;
        bookmarks.emplace_back(new Bookmark{toUtf8(cc->name_text), toUtf8(cc->star_text), toUtf8(cc->dock_text), toUtf8(cc->desc_text)});
    }
    std::vector<spBookmark> saved_bookmarks = Cfg.getBookmarks();
    bool save = false;
    if (bookmarks.size() != saved_bookmarks.size())
        save = true;
    else {
        for (int i=0; i < bookmarks.size(); i++) {
            auto& bm1 = bookmarks[i];
            auto& bm2 = saved_bookmarks[i];
            if (bm1->name != bm2->name)
                save = true;
            if (bm1->system != bm2->system)
                save = true;
            if (bm1->dock != bm2->dock)
                save = true;
            if (bm1->comment != bm2->comment)
                save = true;
        }
    }
    btn_save.set_enabled(save);
}

void UIEditBookmarks::on_bookmarks_save() {
    std::vector<spBookmark> bookmarks;
    for (auto& cc : controls) {
        if (cc->star_text.empty() || cc->dock_text.empty())
            continue;
        auto& bm = cc->bm;
        const_cast<std::string&>(bm->name) = toUtf8(cc->name_text);
        const_cast<std::string&>(bm->system) = toUtf8(trimTextLine(cc->star_text));
        const_cast<std::string&>(bm->dock) = toUtf8(trimTextLine(cc->dock_text));
        const_cast<std::string&>(bm->comment) = toUtf8(cc->desc_text);
        bookmarks.emplace_back(bm);
    }
    Cfg.setBookmarks(bookmarks);
    btn_save.set_enabled(false);
}

bool UIEditBookmarks::validate() const {
    bool valid = true;
    for (auto& cc : controls) {
        bool cc_changed = false;
        if (!cc->validate(&cc_changed))
            valid = false;
    }
    return valid;
}

void UIEditBookmarks::on_ctrl_edit(int id, WORD msg) {
    HWND changed = GetDlgItem(hwnd(), id);
    for (auto& cc : controls)
        cc->on_ctrl_edit(changed, msg);
}



BookmarkCtrl::BookmarkCtrl(UIEditBookmarks* ui, const spBookmark& bm)
    : ui(ui)
    , bm(std::move(bm))
    , expanded(false)
    , deleted(false)
{
    name_text = toUtf16(bm->name);
    star_text = toUtf16(bm->system);
    dock_text = toUtf16(bm->dock);
    desc_text = toUtf16(bm->comment);
}
BookmarkCtrl::~BookmarkCtrl()
{
    ui->freeCtrl(btn_exp);
    ui->freeCtrl(btn_del);
    ui->freeCtrl(tb_name);
    ui->freeCtrl(tb_star);
    ui->freeCtrl(tb_dock);
    ui->freeCtrl(tb_desc);
}
void BookmarkCtrl::create() {
    btn_exp.create(ui->hwnd(), ui->nextID(), "icon-collapsed", 20, {0,0}, {LO_ICN_S,LO_ICN_S}).style.set_style(true, WS_TABSTOP); // SS_NOTIFY if wl::svg_static

    tb_name.create(ui->hwnd(), ui->nextID(), wl::textbox::type::NORMAL, {0,0}, 200,20);
    ui->font.set_on(tb_name);
    tb_name.style.set_style(TRUE, WS_TABSTOP);
    tb_name.set_text(name_text);
    Edit_SetCueBannerText(tb_name.hwnd(), toUtf16(_gt("Bookmark name")).c_str());

    btn_del.create(ui->hwnd(), ui->nextID(), "icon-del", 20, {180,0}, {LO_ICN_S,LO_ICN_S}).style.set_style(true, WS_TABSTOP); // SS_NOTIFY if wl::svg_static

    tb_star.create(ui->hwnd(), ui->nextID(), wl::textbox::type::NORMAL, {0,20}, 200,20).style.set_style(false, WS_VISIBLE);
    ui->font.set_on(tb_star);
    tb_star.style.set_style(TRUE, WS_TABSTOP);
    tb_star.set_text(star_text);
    Edit_SetCueBannerText(tb_star.hwnd(), toUtf16(_gt("Star system")).c_str());

    tb_dock.create(ui->hwnd(), ui->nextID(), wl::textbox::type::NORMAL, {0,40}, 200,20).style.set_style(false, WS_VISIBLE);
    ui->font.set_on(tb_dock);
    tb_dock.style.set_style(TRUE, WS_TABSTOP);
    tb_dock.set_text(dock_text);
    Edit_SetCueBannerText(tb_dock.hwnd(), toUtf16(_gt("Comment")).c_str());

    tb_desc.create(ui->hwnd(), ui->nextID(), wl::textbox::type::MULTILINE, {0,60}, 200,60).style.set_style(false, WS_VISIBLE);
    ui->font.set_on(tb_desc);
    tb_desc.style.set_style(TRUE, WS_TABSTOP | WS_VSCROLL); // dows not work? ES_AUTOVSCROLL | ES_AUTOHSCROLL
    tb_desc.set_text(desc_text);
    //Edit_SetCueBannerText(tb_desc.hwnd(), toUtf16(_gt("Comment")).c_str()); / not working for multiline editor
}
void BookmarkCtrl::layout(UILayout& lo) {
    if (lo.font) {
        lo.font->set_on(tb_name);
        lo.font->set_on(tb_star);
        lo.font->set_on(tb_dock);
        lo.font->set_on(tb_desc);
    }
    btn_exp.set_icon_size(lo.icsz*0.5);
    btn_del.set_icon_size(lo.icsz);

    int x = lo.left;
    int nw = lo.width-2*lo.vrow;
    lo.wpi = DeferWindowPos(lo.wpi, btn_exp.hwnd(), nullptr, x, lo.top, lo.vrow, lo.vrow, SWP_NOZORDER);
    x += lo.vrow;
    lo.wpi = DeferWindowPos(lo.wpi, tb_name.hwnd(), nullptr, x, lo.top, nw, lo.vrow, SWP_NOZORDER);
    x += nw;
    lo.wpi = DeferWindowPos(lo.wpi, btn_del.hwnd(), nullptr, x, lo.top, lo.vrow, lo.vrow, SWP_NOZORDER);
    lo.top += lo.vrow + lo.vgap;
    if (expanded) {
        lo.wpi = DeferWindowPos(lo.wpi, tb_star.hwnd(), nullptr, lo.left, lo.top, lo.width, lo.vrow, SWP_NOZORDER);
        lo.top += lo.vrow + lo.vgap;
        lo.wpi = DeferWindowPos(lo.wpi, tb_dock.hwnd(), nullptr, lo.left, lo.top, lo.width, lo.vrow, SWP_NOZORDER);
        lo.top += lo.vrow + lo.vgap;
        int h = 2*lo.vrow + lo.vgap;
        lo.wpi = DeferWindowPos(lo.wpi, tb_desc.hwnd(), nullptr, lo.left, lo.top, lo.width, h, SWP_NOZORDER);
        lo.top += h + lo.vgap;
    }
}
void BookmarkCtrl::on_ctrl_edit(HWND changed, WORD msg) {
    if (changed == btn_exp.hwnd() && msg == BN_CLICKED) {
        expanded = !expanded;
        btn_exp.set_icon(expanded ? "icon-expanded" : "icon-collapsed");
        tb_star.style.set_style(expanded, WS_VISIBLE);
        tb_dock.style.set_style(expanded, WS_VISIBLE);
        tb_desc.style.set_style(expanded, WS_VISIBLE);
        ui->relayout();
        return;
    }
    if (changed == btn_del.hwnd() && msg == BN_CLICKED) {
        deleted = true;
        return;
    }
    if (msg == EN_SETFOCUS) {
        if (changed == tb_name.hwnd())
            PostMessage(tb_name.hwnd(), EM_SETSEL, (WPARAM)0, (LPARAM)-1);
        if (changed == tb_star.hwnd())
            PostMessage(tb_star.hwnd(), EM_SETSEL, (WPARAM)0, (LPARAM)-1);
        if (changed == tb_dock.hwnd())
            PostMessage(tb_dock.hwnd(), EM_SETSEL, (WPARAM)0, (LPARAM)-1);
    }
    if (msg == EN_CHANGE) {
        if (changed == tb_name.hwnd())
            name_text = tb_name.get_text();
        if (changed == tb_star.hwnd())
            star_text = tb_star.get_text();
        if (changed == tb_dock.hwnd())
            dock_text = tb_dock.get_text();
        if (changed == tb_desc.hwnd())
            desc_text = tb_desc.get_text();
    }
}
bool BookmarkCtrl::validate(bool* changed) {
    bool valid = true;
    if (name_text.empty() && star_text.empty() && dock_text.empty() && desc_text.empty())
        valid = true;
    else if (star_text.empty())
        valid = false;
    else if (dock_text.empty())
        valid = false;
    if (changed) {
        if (name_text != toUtf16(bm->name))
            *changed = true;
        if (star_text != toUtf16(bm->system))
            *changed = true;
        if (dock_text != toUtf16(bm->dock))
            *changed = true;
        if (desc_text != toUtf16(bm->comment))
            *changed = true;
    }
    return valid;
}
