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
#include <winlamb/dialog_modal.h>

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
    const char* icon_name();
    UIEditSystem* ui;
    gal::spEntity entity;
    std::string entity_name;
    std::vector<short> pathBodyID;
    int indent {};
    int icon_scale = 150; // percents
    bool deleted {};

    wl::svg_static icon_site;
    wl::label lbl_name;
};

const int DLG_ENTRY_SAVE   = 100;
const int DLG_ENTRY_DELETE = 101;
class EntityDialog : public wl::dialog_modal {
public:
    EntityDialog(EntityCtrl* ctrl);
    void initialize();
    void relayout();
    void on_save();

    EntityCtrl* ctrl;
    std::vector<std::pair<TypeNav,std::string>> typeNavEntries;
    std::vector<EntityCtrl*> parentEntries;
    wl::font font;
    wl::label lbl_name;
    wl::textbox txt_name;
    wl::label lbl_type;
    wl::combobox cbx_type;
    wl::label lbl_parent;
    wl::combobox cbx_parent;
    wl::textbox txt_info;
    wl::svg_button btn_save;
    wl::svg_button btn_del;
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
    HWND changedWnd = GetDlgItem(hwnd(), id);
    for (auto& cc : controls)
        cc->on_ctrl_edit(changedWnd, msg);
    bool has_deleted = false;
    for (auto it = controls.begin(); it != controls.end(); ) {
        auto c = (*it).get();
        if (c->deleted) {
            currStarSystem->removeEntity(c->entity);
            it = controls.erase(it);
            has_deleted = true;
        } else {
            ++it;
        }
    }
    if (has_deleted) {
        sort_controls();
        relayout();
    }
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
    entity_name = entity->name;
    if (!entity->nloc.empty())
        entity_name = entity->nloc;
    else
        entity_name = entity->name;
    if (auto ss = ui->currStarSystem.get()) {
        if (entity->type == TypeNav::Planet) {
            if (entity_name.starts_with(ss->systemName))
                entity_name = entity_name.substr(ss->systemName.size() + 1);
        }
    }
    if (entity_name.empty())
        entity_name = std::format("{} ({})", enum_name<TypeNav>(entity->type), entity->bodyId);
}

EntityCtrl::~EntityCtrl() {
    ui->freeCtrl(icon_site);
    ui->freeCtrl(lbl_name);
}

void EntityCtrl::create() {
    icon_site.create(ui->hwnd(), ui->nextID(), icon_name(), 2*LO_ICN_S, {0, 0}, {48, 48});
    icon_site.style.set_style(true, SS_NOTIFY);

    lbl_name.create(ui->hwnd(), ui->nextID(), toUtf16(entity_name).c_str(), {50, 0}, {200, 48});
    lbl_name.style.set_style(true, SS_WORDELLIPSIS|SS_NOTIFY);
    ui->font.set_on(lbl_name);
}

void EntityCtrl::layout(UILayout &lo) {
    if (lo.font) {
        lo.font->set_on(lbl_name);
    }
    icon_site.set_icon_size(lo.vrow * icon_scale / 100);

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
    if (msg == STN_CLICKED && (changed == icon_site.hwnd() || changed == lbl_name.hwnd())) {
        EntityDialog dlg(this);
        dlg.show(ui);
        ui->validate();
    }
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

const char* EntityCtrl::icon_name() {
    icon_scale = 150;
    switch (entity->type) {
    case TypeNav::Other:
    case TypeNav::Error:
    case TypeNav::NotExplored:
    case TypeNav::Signal:
    case TypeNav::WarZone:
    case TypeNav::ResSite:
    case TypeNav::StarSystem:       break;
    case TypeNav::Body:             break;
    case TypeNav::Barycenter:                           return "body-barycenter";
    case TypeNav::Ring:             icon_scale = 200;   return "body-barycenter";
    case TypeNav::AsteroidCluster:                      return "body-asteroid-cluster";
    case TypeNav::Star:             icon_scale = 200;   return "body-star";
    case TypeNav::Planet:           icon_scale = 200;   return "body-planet";
    case TypeNav::SpaceThing:
    case TypeNav::NavBeacon:
    case TypeNav::TouristBeacon:
    case TypeNav::SpaceStation:     break;
    case TypeNav::Orbis:            icon_scale = 200;   return "site-orbis";
    case TypeNav::Ocellus:          icon_scale = 200;   return "site-ocellus";
    case TypeNav::Dodec:            icon_scale = 200;   return "site-dodec";
    case TypeNav::Coriolis:         icon_scale = 200;   return "site-coriolis";
    case TypeNav::AsteroidBase:     icon_scale = 200;   return "site-asteroid-base";
    case TypeNav::SpaceOutpost:                         return "site-outpost";
    case TypeNav::SpaceInstallation:                    return "site-space-installation";
    case TypeNav::SpaceConstrDepot:                     return "site-construction";
    case TypeNav::Megaship:                             return "site-megaship";
    case TypeNav::StationMegaShip:                      return "site-megaship";
    case TypeNav::StrongholdCarrier:icon_scale = 200;   return "site-megaship";
    case TypeNav::FleetCarrier:                         return "site-carrier";
    case TypeNav::SquadronCarrier:  icon_scale = 200;   return "site-carrier";
    case TypeNav::ColonisationShip: icon_scale = 200;   return "site-colonization-ship";
    case TypeNav::PlanetaryThing:   break;
    case TypeNav::PlanetaryPort:    icon_scale = 200;   return "site-port";
    case TypeNav::EngineerPort:     icon_scale = 200;   return "site-engineer";
    case TypeNav::Settlement:                           return "site-settlement";
    case TypeNav::PlanetaryInstallation:                return "site-planetary-installation";
    case TypeNav::PlanetaryConstrDepot:                 return "site-construction";
    }
    return "body-unknown";
}

EntityDialog::EntityDialog(EntityCtrl *ctrl)
    : ctrl(ctrl)
{
    setup.dialogId = IDD_EDIT_SYSTEM_ENTRY;
    on_message(WM_INITDIALOG, [this](wl::params p) -> INT_PTR {
        initialize();
        return 0;
    });
    on_message(WM_DPICHANGED, [this](wl::params params) {
        relayout();
        return 0;
    });
    on_message(WM_SIZE, [this](wl::params params) {
        relayout();
        return 0;
    });
    on_command(ID_SAVE, [this](wl::params params) {
        on_save();
        return 0;
    });
    on_command(ID_DELETE, [this](wl::params params) {
        this->ctrl->deleted = true;
        EndDialog(hwnd(), IDOK);
        return 0;
    });

    typeNavEntries = {
            {TypeNav::Other, _gt("TypeNav\4Other")},
            {TypeNav::Barycenter, _gt("TypeNav\4Barycenter")},
            {TypeNav::Ring, _gt("TypeNav\4Ring")},
            {TypeNav::AsteroidCluster, _gt("TypeNav\4Asteroid Cluster")},
            {TypeNav::Star, _gt("TypeNav\4Star")},
            {TypeNav::Planet, _gt("TypeNav\4Planet")},
            {TypeNav::NavBeacon, _gt("TypeNav\4Navigation Beacon")},
            {TypeNav::TouristBeacon, _gt("TypeNav\4Tourist Beacon")},
            {TypeNav::Orbis, _gt("TypeNav\4Orbis")},
            {TypeNav::Ocellus, _gt("TypeNav\4Ocellus")},
            {TypeNav::Dodec, _gt("TypeNav\4Dodec")},
            {TypeNav::Coriolis, _gt("TypeNav\4Coriolis")},
            {TypeNav::AsteroidBase, _gt("TypeNav\4Asteroid Base")},
            {TypeNav::SpaceOutpost, _gt("TypeNav\4Space Outpost")},
            {TypeNav::SpaceInstallation, _gt("TypeNav\4Space Installation")},
            {TypeNav::SpaceConstrDepot, _gt("TypeNav\4Space Construction Depot")},
            {TypeNav::Megaship, _gt("TypeNav\4Megaship")},
            {TypeNav::StationMegaShip, _gt("TypeNav\4Station Megaship")},
            {TypeNav::FleetCarrier, _gt("TypeNav\4Fleet Carrier")},
            {TypeNav::SquadronCarrier, _gt("TypeNav\4Squadron Carrier")},
            {TypeNav::ColonisationShip, _gt("TypeNav\4Colonisation Ship")},
            {TypeNav::PlanetaryPort, _gt("TypeNav\4Planetary Port")},
            {TypeNav::EngineerPort, _gt("TypeNav\4Engineer Port")},
            {TypeNav::Settlement, _gt("TypeNav\4Settlement")},
            {TypeNav::PlanetaryInstallation, _gt("TypeNav\4Planetary Installation")},
            {TypeNav::PlanetaryConstrDepot, _gt("TypeNav\4Planetary Construction Depot")},
    };
    for (auto& c : ctrl->ui->controls) {
        parentEntries.emplace_back(c.get());
    }
}

void EntityDialog::initialize() {
    //SetDialogDpiChangeBehavior(hwnd(), DDC_DISABLE_CONTROL_RELAYOUT, DDC_DISABLE_CONTROL_RELAYOUT);

    lbl_name.assign(hwnd(), IDC_STATIC_NAME);
    txt_name.assign(hwnd(), IDC_NAME);

    lbl_type.assign(hwnd(), IDC_STATIC_TYPE);
    cbx_type.assign(hwnd(), IDC_COMBO_TYPE);

    lbl_parent.assign(hwnd(), IDC_STATIC_PARENT);
    cbx_parent.assign(hwnd(), IDC_COMBO_PARENT);

    txt_info.assign(hwnd(), IDC_EDIT_INFO);

    btn_save.create(hwnd(), ID_SAVE, "icon-save", LO_ICN_S, {0, 0}, {24,24});
    btn_del.create(hwnd(), ID_DELETE, "icon-del", LO_ICN_S, {30, 0}, {24,24});

    auto& entity = ctrl->entity;
    std::string name = entity->name;
    if (!entity->nloc.empty())
        name = entity->nloc;
    else
        name = entity->name;
    if (auto ss = ctrl->ui->currStarSystem.get()) {
        if (entity->type == TypeNav::Planet) {
            if (name.starts_with(ss->systemName))
                name = name.substr(ss->systemName.size() + 1);
        }
    }
    std::wstring wname = toUtf16(name);
    txt_name.set_text(wname.c_str());

    int select_index = -1;
    int index = 0;
    for (auto type : typeNavEntries) {
        const char* nm = gettext(type.second.c_str());
        if (type.second == nm)
            nm = type.second.data() + 1 + type.second.find('\4');
        cbx_type.add({toUtf16(nm).c_str()});
        if (type.first == ctrl->entity->type)
            select_index = index;
        index += 1;
    }
    cbx_type.select(select_index);

    select_index = -1;
    index = 0;
    for (auto c : parentEntries) {
        cbx_parent.add({toUtf16(c->entity_name).c_str()});
        if (c->entity->bodyId >= 0 && c->entity->bodyId == ctrl->entity->parentBodyId)
            select_index = index;
        index += 1;
    }
    cbx_parent.select(select_index);

    relayout();
}

void EntityDialog::relayout() {
    UILayout lo(hwnd());
    loCreateFont(font, lo.uiDpi, lo.uiPercent);

    lo.wpi = BeginDeferWindowPos(20);

    int x0 = lo.left + lo.border;
    int x1 = x0 + lo.txt20w;
    int y = lo.top + lo.border;
    int w0 = lo.txt20w;
    int w1 = lo.width - w0 - 2*lo.border;

    font.set_on(lbl_name);
    font.set_on(txt_name);
    font.set_on(lbl_type);
    font.set_on(cbx_type);
    font.set_on(lbl_parent);
    font.set_on(cbx_parent);
    font.set_on(txt_info);
    btn_save.set_icon_size(lo.icsz);
    btn_del.set_icon_size(lo.icsz);

    lo.wpi = DeferWindowPos(lo.wpi, lbl_name.hwnd(), NULL, x0, y, w0, lo.vrow, SWP_NOZORDER);
    lo.wpi = DeferWindowPos(lo.wpi, txt_name.hwnd(), NULL, x1, y, w1, lo.vrow, SWP_NOZORDER);
    y += lo.vrow + lo.xgap;

    lo.wpi = DeferWindowPos(lo.wpi, lbl_type.hwnd(), NULL, x0, y, w0, lo.vrow, SWP_NOZORDER);
    lo.wpi = DeferWindowPos(lo.wpi, cbx_type.hwnd(), NULL, x1, y, w1, lo.vrow, SWP_NOZORDER);
    y += lo.vrow + lo.xgap;

    lo.wpi = DeferWindowPos(lo.wpi, lbl_parent.hwnd(), NULL, x0, y, w0, lo.vrow, SWP_NOZORDER);
    lo.wpi = DeferWindowPos(lo.wpi, cbx_parent.hwnd(), NULL, x1, y, w1, lo.vrow, SWP_NOZORDER);
    y += lo.vrow + lo.xgap;

    int x = lo.left + lo.border;
    int info_h = lo.height - 2*lo.border - 4*(lo.vrow + lo.xgap);
    lo.wpi = DeferWindowPos(lo.wpi, txt_info.hwnd(), NULL, x, y, w0+w1, info_h, SWP_NOZORDER);

    y = lo.top + lo.height - lo.border - lo.vrow;
    lo.wpi = DeferWindowPos(lo.wpi, btn_save.hwnd(), NULL, x, y, lo.vrow, lo.vrow, SWP_NOZORDER);
    x = lo.left + lo.width - lo.border - lo.vrow;
    lo.wpi = DeferWindowPos(lo.wpi, btn_del.hwnd(), NULL, x, y, lo.vrow, lo.vrow, SWP_NOZORDER);

    EndDeferWindowPos(lo.wpi);
}

void EntityDialog::on_save() {
    TypeNav tp = TypeNav::Other;
    auto t_idx = cbx_type.get_selected_index();
    if (t_idx >= 0 && t_idx < typeNavEntries.size())
        tp = typeNavEntries[t_idx].first;
    if (tp != ctrl->entity->type) {
        ctrl->entity->type = tp;
        ctrl->ui->currStarSystem->saved = false;
        ctrl->icon_site.set_icon(ctrl->icon_name());
        UILayout lo(ctrl->ui->hwnd());
        ctrl->icon_site.set_icon_size(lo.vrow * ctrl->icon_scale / 100);
    }

    short parentBodyId = -1;
    auto p_idx = cbx_parent.get_selected_index();
    if (p_idx >= 0 && p_idx < parentEntries.size())
        parentBodyId = parentEntries[t_idx]->entity->bodyId;
    if (parentBodyId != ctrl->entity->parentBodyId) {
        ctrl->entity->parentBodyId = parentBodyId;
        ctrl->ui->currStarSystem->saved = false;
        ctrl->ui->sort_controls();
        ctrl->ui->relayout();
    }

    EndDialog(hwnd(), IDOK);
}
