//
// Created by mkizub on 06.04.2026.
//

#include "../pch.h"

#include "UIEditSystem.h"
#include "UILayout.h"
#include "UIManager.h"
#include "UIMainDialog.h"
#include "../Galaxy.h"

#include "../../ui/resource.h"

class EntityCtrl {
public:
    EntityCtrl(UIEditSystem* ui, gal::spEntity& entity);
    virtual ~EntityCtrl();
    virtual void create();
    virtual void layout(UILayout& lo);
    virtual void on_ctrl_edit(HWND changed, WORD msg);
    virtual bool validate(bool* changed);
    virtual js::value value();
    void update_path();
    UIEditSystem* ui;
    gal::spEntity entity;
    std::vector<short> pathBodyID;
    int indent {};

    wl::svg_static icon_site;
    wl::label lbl_name;
};

bool compareBodyIDbyPath(const std::vector<short>& p1, const std::vector<short>& p2) {
    for (int i=0; i < 100; i++) {
        if (p1.size() <= i && p2.size() <= i)
            return false;
        if (p1.size() <= i)
            return true;
        if (p2.size() <= i)
            return false;
        auto b1 = p1[i];
        auto b2 = p2[i];
        if (b1 != b2)
            return b1 < b2;
    }
    return false;
}

UIEditSystem::UIEditSystem() : UIControl(true) {
    on_command(IDC_COMBO_TEMPLATES, [this](wl::params p) {
        if (HIWORD(p.wParam) == CBN_SELCHANGE) {
            auto nm = cb_system.get_entry_text(cb_system.get_selected_index());
            auto ss = gal::getStarSystem(toUtf8(nm));
            on_system_selected(ss);
        }
        return 0;
    });
    on_command(ID_RUN, [this](wl::params params) {
        on_system_import();
        return 0;
    });
    on_command(ID_SAVE, [this](wl::params params) {
        on_system_save();
        return 0;
    });
    on_command(ID_DELETE, [this](wl::params params) {
        on_system_delete();
        return 0;
    });
}

UIEditSystem::~UIEditSystem() {
}

void UIEditSystem::initialize() {
    SetDialogDpiChangeBehavior(hwnd(), DDC_DISABLE_ALL, DDC_DISABLE_ALL);

    RECT rect{};
    GetClientRect(hwnd(), &rect);
    int uiDpi = GetDpiForWindow(hwnd());
    int uiPercent = Cfg.getUiScalePercents();

    UILayout lo(uiDpi, uiPercent, rect);
    loCreateFont(font, uiDpi, uiPercent);

    lo.left += lo.border;
    lo.top += lo.border;
    lo.width -= 2*lo.border;
    int x = lo.left;
    int y = lo.top;
    int cb_w = lo.width - 3*(lo.btnh+lo.xgap);
    cb_system.create(hwnd(), IDC_COMBO_TEMPLATES, {x,y}, cb_w, wl::starbox::sort::UNSORTED);
    x += cb_w + lo.xgap;
    btn_import.create(hwnd(), ID_RUN, "task-run", lo.icsz, {x,y}, {lo.btnh,lo.btnh});
    x += cb_w + lo.xgap;
    btn_save.create(hwnd(), ID_SAVE, "icon-save", lo.icsz, {x,y}, {lo.btnh,lo.btnh}).set_enabled(false);
    x += cb_w + lo.xgap;
    btn_del.create(hwnd(), ID_DELETE, "icon-del", lo.icsz, {x,y}, {lo.btnh,lo.btnh}).set_enabled(false);

    init_systems_list("");
    relayout();
}

void UIEditSystem::clear() {
    controls.clear();
    nextTryId = 0;
    usedIds = {};
}

void UIEditSystem::init_systems_list(std::string select) {
    cb_system.remove_all();
    starSystems.clear();
    if (gal::getCurrentStarSystem())
        starSystems.push_back(gal::getCurrentStarSystem()->systemName);
    starSystems.push_back("Sol");
    int select_index = -1;
    for (int i=0; i < starSystems.size(); i++) {
        auto& name = starSystems[i];
        cb_system.add({toUtf16(name).c_str()});
        if (select_index < 0 && !select.empty() && select == name)
            select_index = i;
    }
    if (select_index >= 0)
        cb_system.select(select_index);
}

void UIEditSystem::on_system_import() {
    std::wstring wname = cb_system.get_text();
    if (wname.empty())
        return;
    auto ss = gal::getStarSystem(toUtf8(wname));
    on_system_selected(ss);
    return;
}

void UIEditSystem::on_system_save() {
    return;
}

void UIEditSystem::on_system_delete() {
    return;
}

std::unique_ptr<EntityCtrl> UIEditSystem::create_ctrl(gal::spEntity& entity) {
    return std::make_unique<EntityCtrl>(this, entity);
}

EntityCtrl* UIEditSystem::find_ctrl(int bodyId) {
    if (bodyId < 0)
        return nullptr;
    for (auto& ctrl : controls) {
        auto entity = ctrl->entity.get();
        if (entity->bodyId == bodyId)
            return ctrl.get();
    }
    return nullptr;
}

void UIEditSystem::sort_controls() {
    for (auto& ctrl : controls)
        ctrl->update_path();
    std::stable_sort(controls.begin(), controls.end(), [](const std::unique_ptr<EntityCtrl>& c1, const std::unique_ptr<EntityCtrl>& c2)->bool {
        return compareBodyIDbyPath(c1->pathBodyID, c2->pathBodyID);
    });
    std::vector<gal::Entity*> parents;
    parents.reserve(16);
    for (auto& ctrl : controls) {
        auto entity = ctrl->entity.get();
        if (entity->parentBodyId >= 0) {
            while (!parents.empty() && parents.back()->bodyId != entity->parentBodyId)
                parents.pop_back();
        } else {
            parents.clear();
        }
        ctrl->indent = (int)parents.size();
        if (entity->bodyId >= 0)
            parents.push_back(entity);
    }
}

void UIEditSystem::on_ctrl_edit(int id, WORD msg) {
    HWND changed = GetDlgItem(hwnd(), id);
    for (auto& cc : controls)
        cc->on_ctrl_edit(changed, msg);
}

bool UIEditSystem::validate() const {
    changed = false;
    valid = true;
    for (auto& cc : controls) {
        if (!cc->validate(&changed))
            valid = false;
    }
    return valid;
}

void UIEditSystem::relayout(bool scroll_to_top) {
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
        font.set_on(cb_system);
    }
    btn_import.set_icon_size(lo.icsz);
    btn_save.set_icon_size(lo.icsz);
    btn_del.set_icon_size(lo.icsz);

    panel_width = lo.width;
    panel_height = lo.height;
    lo.left += lo.xgap;
    lo.width -= 2*lo.xgap;
    lo.top += lo.border - scroll_pos;

    lo.wpi = BeginDeferWindowPos(100);

    int x = lo.left;
    int y = lo.top;
    int cb_w = lo.width - 3*(lo.btnh+lo.xgap);
    lo.wpi = DeferWindowPos(lo.wpi, cb_system.hwnd(), nullptr, x, y, cb_w, lo.height, SWP_NOZORDER);
    x += cb_w + lo.xgap;
    lo.wpi = DeferWindowPos(lo.wpi, btn_import.hwnd(), nullptr, x, y, lo.btnh, lo.btnh, SWP_NOZORDER);
    x += lo.btnh + lo.xgap;
    lo.wpi = DeferWindowPos(lo.wpi, btn_save.hwnd(), nullptr, x, y, lo.btnh, lo.btnh, SWP_NOZORDER);
    x += lo.btnh + lo.xgap;
    lo.wpi = DeferWindowPos(lo.wpi, btn_del.hwnd(), nullptr, x, y, lo.btnh, lo.btnh, SWP_NOZORDER);

    lo.top += lo.btnh + lo.vgap;

    for (auto& cc : controls)
        cc->layout(lo);

    params_height = lo.top + scroll_pos + 5*lo.vrow;

    EndDeferWindowPos(lo.wpi);

    reset_scroll(false);
}

void UIEditSystem::on_system_selected(gal::spStarSystem starSystem) {
    clear();
    currStarSystem = starSystem;
    if (!currStarSystem)
        return;

    beginControls();
    for (auto& b : currStarSystem->bodies) {
        controls.push_back(create_ctrl(b));
        controls.back()->create();
    }
    for (auto& b : currStarSystem->stations) {
        controls.push_back(create_ctrl(b));
        controls.back()->create();
    }
    sort_controls();
    endControls();
    relayout(true);

    validate();
}



EntityCtrl::EntityCtrl(UIEditSystem *ui, gal::spEntity &entity)
    : ui(ui)
    , entity(entity)
{
}

EntityCtrl::~EntityCtrl() {
    ui->freeCtrl(icon_site);
    ui->freeCtrl(lbl_name);
}

void EntityCtrl::create() {
    std::string site_type;
    switch (entity->type) {
    case TypeNav::Other:
    case TypeNav::Error:
    case TypeNav::NotExplored:
    case TypeNav::Signal:
    case TypeNav::WarZone:
    case TypeNav::ResSite:
    case TypeNav::StarSystem:       break;
    case TypeNav::Body:             break;
    case TypeNav::Barycenter:       site_type = "body-barycenter"; break;
    case TypeNav::Ring:
    case TypeNav::AsteroidCluster:  break;
    case TypeNav::Star:             site_type = "body-star"; break;
    case TypeNav::Planet:           site_type = "body-planet"; break;
    case TypeNav::SpaceThing:
    case TypeNav::NavBeacon:
    case TypeNav::TouristBeacon:    break;
    case TypeNav::SpaceStation:
    case TypeNav::Orbis:            site_type = "site-orbis"; break;
    case TypeNav::Ocellus:          site_type = "site-ocellus"; break;
    case TypeNav::Dodec:            site_type = "site-dodec"; break;
    case TypeNav::Coriolis:         site_type = "site-coriolis"; break;
    case TypeNav::AsteroidBase:
    case TypeNav::SpaceOutpost:
    case TypeNav::SpaceInstallation:break;
    case TypeNav::SpaceConstrDepot: site_type = "site-construction"; break;
    case TypeNav::Megaship:
    case TypeNav::StationMegaShip:
    case TypeNav::FleetCarrier:
    case TypeNav::SquadronCarrier:
    case TypeNav::StrongholdCarrier:break;
    case TypeNav::ColonisationShip: site_type = "site-construction"; break;
    case TypeNav::PlanetaryThing:
    case TypeNav::PlanetaryStation:
    case TypeNav::PlanetaryPort:
    case TypeNav::EngineerPort:
    case TypeNav::Settlement:
    case TypeNav::PlanetaryInstallation: break;
    case TypeNav::PlanetaryConstrDepot: site_type = "site-construction"; break;
    }
    if (site_type.empty())
        site_type = "body-unknown";
    icon_site.create(ui->hwnd(), ui->nextID(), site_type, 2*LO_ICN_S, {0, 0}, {48, 48});


    std::string name = entity->name;
    if (!entity->nloc.empty())
        name = entity->nloc;
    else
        name = entity->name;
    if (auto ss = ui->currStarSystem.get()) {
        if (entity->type == TypeNav::Planet) {
            if (name.starts_with(ss->systemName))
                name = name.substr(ss->systemName.size() + 1);
        }
    }
    std::wstring wname = toUtf16(name);
    lbl_name.create(ui->hwnd(), ui->nextID(), wname.c_str(), {50, 0}, {200, 48});
    lbl_name.style.set_style(TRUE, SS_WORDELLIPSIS|SS_NOTIFY);
    ui->font.set_on(lbl_name);
}

void EntityCtrl::layout(UILayout &lo) {
    if (lo.font) {
        lo.font->set_on(lbl_name);
    }
    icon_site.set_icon_size(2*lo.vrow);

    int offs = 2 * indent * lo.hgap;
    int h = 2*lo.vrow + lo.vgap;
    int x = lo.left + offs;
    int y = lo.top;
    int w_icn = h;
    int w_lbl = lo.width - offs - w_icn - lo.hgap;
    lo.wpi = DeferWindowPos(lo.wpi, icon_site.hwnd(), nullptr, x, y, w_icn, h, SWP_NOZORDER);
    x += w_icn + lo.hgap;
    y += (h-lo.vrow)/2;
    lo.wpi = DeferWindowPos(lo.wpi, lbl_name.hwnd(), nullptr, x, y, w_lbl, lo.vrow, SWP_NOZORDER);

    lo.top += h + lo.vgap;
}

void EntityCtrl::on_ctrl_edit(HWND changed, WORD msg) {

}

bool EntityCtrl::validate(bool *changed) {
    return false;
}

js::value EntityCtrl::value() {
    return js::value();
}

void EntityCtrl::update_path() {
    EntityCtrl* pc = ui->find_ctrl(entity->parentBodyId);
    if (!pc)
        this->pathBodyID.clear();
    if (pc) {
        pc->update_path();
        this->pathBodyID.reserve(pc->pathBodyID.size()+1);
        this->pathBodyID = pc->pathBodyID;
    }
    this->pathBodyID.push_back(entity->bodyId);
}