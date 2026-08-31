//
// Created by mkizub on 11.08.2025.
//

#include "pch.h"

#include "ShipStats.h"


namespace eddb {

js::value gEDDBFull;
std::unordered_map<std::string,const js::value&> gEDDBShips;
std::unordered_map<std::string,const js::value&> gEDDBModules;
std::unordered_map<std::string,const js::value&> gEDDBBlueprints;
std::unordered_map<std::string,const js::value&> gEDDBEffects;
std::unordered_map<Attr,ShipAttr> gEDDBAttributes;

spShipStats gShipStats;

std::map<std::string,ShipSlotGroup> gCustomSlots = {
        // ship
        {"Hull", ShipSlotGroup::SHIP},
        {"CargoHatch", ShipSlotGroup::SHIP},
        // mandatory components
        {"Armour", ShipSlotGroup::COMPONENT},
        {"PowerPlant", ShipSlotGroup::COMPONENT},
        {"MainEngines", ShipSlotGroup::COMPONENT},
        {"FrameShiftDrive", ShipSlotGroup::COMPONENT},
        {"LifeSupport", ShipSlotGroup::COMPONENT},
        {"PowerDistributor", ShipSlotGroup::COMPONENT},
        {"Radar", ShipSlotGroup::COMPONENT},
        {"FuelTank", ShipSlotGroup::COMPONENT},
        // unused
        {"PlanetaryApproachSuite", ShipSlotGroup::UNUSED},
        {"DataLinkScanner", ShipSlotGroup::UNUSED},
        {"CodexScanner", ShipSlotGroup::UNUSED},
        {"DiscoveryScanner", ShipSlotGroup::UNUSED},
        {"ColonisationSuite", ShipSlotGroup::COMPONENT},
};

spShipStats initShipStats(const std::string &type) {
    auto it = gEDDBShips.find(type);
    if (it == gEDDBShips.end())
        return {};
    return std::make_shared<ShipStats>(type, it->second);
}
void setShipStats(spShipStats shipStats) {
    gShipStats = shipStats;
}
spShipStats getShipStats() {
    return gShipStats;
}
bool shipHasFsdSco() {
    return gShipStats && gShipStats->hasFsdSco();
}

bool loadEDDB() {
    LOG(INFO) << "Loading EDDB";
    gEDDBFull = parseJsonFile(L"eddb.json5");
    if (!gEDDBFull)
        return false;
    for (auto& [key, ship] : gEDDBFull["ship"].as_object()) {
        std::string fdname = toLower(ship["fdname"].as_string());
        gEDDBShips.emplace(fdname, ship);
        for (auto& [mkey, module] : ship["module"].as_object()) {
            fdname = toLower(module["fdname"].as_string());
            gEDDBModules.emplace(fdname, module);
        }
    }
    for (auto& [key, module] : gEDDBFull["module"].as_object()) {
        auto jn = module["fdname"];
        if (jn.is_string())
            gEDDBModules.emplace(toLower(jn.as_string()), module);
    }
    for (auto [bp_name, bp_val] : gEDDBFull["blueprint"].key_value()) {
        gEDDBFull["blueprint"][bp_name]["bpid"] = bp_name;
        auto jn = bp_val["fdname"];
        if (jn.is_string()) {
            gEDDBBlueprints.emplace(toLower(jn.as_string()), bp_val);
        }
    }
    for (auto [_,effect] : gEDDBFull["expeffect"].key_value()) {
        auto jn = effect["fdname"];
        if (jn.is_string())
            gEDDBEffects.emplace(toLower(jn.as_string()), effect);
    }
    for (auto& attr : gEDDBFull["attributes"].as_array()) {
        auto ja = attr.at("attr");
        if (ja.is_string()) {
            auto a = enum_cast<Attr>(ja.as_string());
            if (a.has_value())
                gEDDBAttributes.emplace(a.value(), ShipAttr(a.value(), attr));
        }
    }
    return true;
}

ShipAttr* getShipAttr(Attr attr) {
    auto attr_it = gEDDBAttributes.find(attr);
    if (attr_it != gEDDBAttributes.end())
        return &attr_it->second;
    return nullptr;
}

ShipAttr::ShipAttr(Attr attr, const js::value& jv)
    : attr(attr)
    , jvalue(jv)
{
    id = jv["attr"].as_string();
    assert(enum_name(attr) == id);
    if (jv["hidden"])
        hidden = true;
    if (jv["bad"])
        bad = true;
    if (jv["abbr"])
        abbr = jv["abbr"].as_string();
    if (jv["name"])
        name = jv["name"].as_string();
    if (jv["desc"])
        desc = jv["desc"].as_string();
    modset = bool(jv["modset"]);
    modadd = bool(jv["modadd"]);
    if (jv["unit"].is_string())
        unit = jv["unit"].as_string();
    if (jv["modmod"].is_number())
        modmod = jv["modmod"].as_real();
    //if (jv["scale"].is_number())
    //    scale = jv["scale"].as_number();
    if (jv["min"].is_number())
        min = jv["min"].as_real();
    if (jv["max"].is_number())
        max = jv["max"].as_real();
    if (jv["task_step"].is_number())
        step = jv["task_step"].as_real();

    default_value = DNaN;
    default_attr = nullptr;
    auto dflt = jv["default"];
    if (dflt.is_string()) {
        auto a = enum_cast<Attr>(dflt.as_string());
        if (!a.has_value()) {
            LOG(ERROR) << "Attribute " << dflt << " not found";
        } else {
            default_attr = getShipAttr(a.value());
            LOG_IF(!default_attr,ERROR) << "Attribute " << dflt << " not found";
        }
    }
    else if (dflt.is_number()) {
        default_value = dflt.as_real();
    }
}

ShipSlot::ShipSlot(ShipStats& ship, std::string name, ShipSlotGroup group)
    : ship(ship)
    , name(name)
    , group(group)
{
    if (name == "Hull")
        module = &ship.jship;
}

void ShipSlot::setModule(const std::string& m) {
    moduleName = m;
    auto it = gEDDBModules.find(toLower(m));
    if (it != gEDDBModules.end())
        module = &it->second;
    else
        module = nullptr;
}

double ShipSlot::getBlueprintGradeRollAttrModifier(ShipAttr& attribute) {
    if (!blueprint)
        return DNaN;
    auto& bp = *this->blueprint;
    if (!bp[attribute.id])
        return DNaN;
    auto& attr = attribute.id;
    auto bpgrade = std::min(std::max(blueprintLevel, 1), (int)bp["maxgrade"].as_int());
    auto bproll = std::min(std::max(blueprintQuality, 0.f), 1.f);
    auto himod = bp[attr][bpgrade - 1].as_real();
    double xmod;
    if ((himod < 0) == !attribute.bad) {
        xmod = himod;
    } else {
        auto lomod = ((bpgrade > 1) ? bp[attr][bpgrade - 2].as_real() : ((himod && bp[attr][1]) ? (himod - (bp[attr][1].as_real() - himod)) : 0));
        xmod = lomod + bproll * (himod - lomod);
    }
    if (attribute.modmod.has_value())
        return xmod / attribute.modmod.value();
    return xmod / ((attribute.modadd || attribute.modset) ? 1.0 : 100.0);
}

void ShipSlot::setEngineering(const std::string& bp, int level, float quality, const std::string& exp) {
    if (!module)
        return;
    blueprintName = bp;
    blueprintLevel = 0;
    blueprintQuality = 0;
    effectName = exp;
    {
        auto it = gEDDBBlueprints.find(toLower(bp));
        if (it != gEDDBBlueprints.end()) {
            blueprint = &it->second;
            blueprintLevel = std::min(std::max(level, 1), (int)(*blueprint)["maxgrade"].as_int());
            const float MIN_QUALITY = 0.00025f;
            blueprintQuality = std::min(std::max(quality, MIN_QUALITY), 1.0f);
        } else {
            blueprint = nullptr;
            blueprintName.clear();
            effectName.clear();
        }
    }
    {
        auto it = gEDDBEffects.find(toLower(effectName));
        if (it != gEDDBEffects.end())
            effect = &it->second;
        else
            effect = nullptr;
    }

    auto module_mtype = (*module)["mtype"].as_string_or();
    //if (module_mtype.empty())
    //    LOG(ERROR) << "module_mtype.empty()"; // armour? not interested
    if (blueprint && !module_mtype.empty()) {
        const auto& mtype = gEDDBFull["mtype"][module_mtype].deref();
        //if (!mtype["modifiable"] || !mtype["blueprints"] || mtype["blueprints"].indexOf(blueprint->at("bpid").as_string()) < 0)
        //    return false;
        if (blueprintQuality > 0) {
            attrModifier.clear();
            attrOverride.clear();
            for (auto& a : mtype["modifiable"].as_array_or()) {
                auto& attr_name = a.as_string();
                if (!(*blueprint)[attr_name])
                    continue;
                auto attr_opt = enum_cast<Attr>(attr_name);
                if (!attr_opt.has_value())
                    continue;
                Attr attr = attr_opt.value();
                auto attribute = getShipAttr(attr);
                if (!attribute)
                    continue;
                if (attribute->modset || attribute->modadd || attribute->modmod.has_value() || getBaseAttrValue(attr)) {
                    attrModifier[attr] = getBlueprintGradeRollAttrModifier(*attribute);
                }
            }
        } else {
            if (!attrModifier.empty()) {
                attrOverride.clear();
                for (auto& it : attrModifier)
                    attrOverride[it.first] = true;
            } else {
                attrOverride.clear();
            }
        }
    } else {
        if (blueprintQuality > 0) {
            attrModifier.clear();
            attrOverride.clear();
        } else {
            attrOverride.clear();
            for (auto& it : attrModifier)
                attrOverride[it.first] = true;
        }
    }
}

double ShipSlot::getBaseAttrModifier(Attr attr) {
    auto it = attrModifier.find(attr);
    if (it == attrModifier.end())
        return 0;
    return it->second;
}
double ShipSlot::getRelatedAttrModifier(Attr attr) {
    switch (attr) {
    case Attr::engminmass:
    case Attr::engmaxmass:
        return getEffectiveAttrModifier(Attr::engoptmass);

    case Attr::engminmul:
    case Attr::engmaxmul:
    case Attr::minmulspd:
    case Attr::optmulspd:
    case Attr::maxmulspd:
    case Attr::minmulacc:
    case Attr::optmulacc:
    case Attr::maxmulacc:
    case Attr::minmulrot:
    case Attr::optmulrot:
    case Attr::maxmulrot:
        return getEffectiveAttrModifier(Attr::engoptmul);
    }

    return 0;
}
double ShipSlot::getExperimentalAttrModifier(Attr attr) {
    if (!effect)
        return 0;
    auto attribute = getShipAttr(attr);
    if (!attribute)
        return 0;
    auto& modExp = effect->at(attribute->id);
    if (!modExp.is_number())
        return 0;
    double value = modExp.is_number();
    if (!(attribute->modset || attribute->modadd)) {
        if (attribute->modmod.has_value())
            value /= attribute->modmod.value();
        else
            value /= 100;
    }
    return value;
}

double ShipSlot::getAttrModifierSum(Attr attr, double modifier1, double modifier2) {
    if (std::isnan(modifier1))
        return modifier2;
    if (std::isnan(modifier2))
        return modifier1;
    auto attribute = getShipAttr(attr);
    if (attribute) {
        if (attribute->modset)
            return modifier2;
        if (attribute->modadd)
            return modifier1 + modifier2;
    }
    // modmod and standard
    return ((1 + modifier1) * (1 + modifier2) - 1);
}

double ShipSlot::getEffectiveAttrModifier(Attr attr) {
    // get base, related and experimental modifiers
    auto modBase = getBaseAttrModifier(attr);
    auto modRel = getRelatedAttrModifier(attr);
    auto modExp = getExperimentalAttrModifier(attr);

    // apply these modifiers in reverse; usually it doesn't matter, but for modset we want base to override related which overrides experimental
    return getAttrModifierSum(attr, getAttrModifierSum(attr, modExp, modRel), modBase);

}
double ShipSlot::getAttrValue(Attr attr, double value, double modifier) {
    auto attribute = getShipAttr(attr);
    if (attribute) {
        // fall back on attribute default value
        if (std::isnan(value)) {
            if (attribute->default_attr)
                value = getAttrValue(attribute->default_attr->attr, false);
            else
                value = attribute->default_value;
        }

        // apply modifier?
        if (!std::isnan(modifier) && !std::isnan(value) && (value || attribute->modset || attribute->modadd || attribute->modmod.has_value())) {
            if (attribute->modset)
                value = modifier;
            else if (attribute->modadd)
                value = value + modifier;
            else if (attribute->modmod.has_value())
                value = ((1 + (value / attribute->modmod.value())) * (1 + modifier) - 1) * attribute->modmod.value();
            else
                value = value * (1 + modifier);

            // apply constraints
            if (attribute->step.has_value())
                value = round(value / attribute->step.value()) * attribute->step.value();
            if (attribute->min.has_value())
                value = std::max(value, attribute->min.value());
            if (attribute->max.has_value())
                value = std::min(value, attribute->max.value());
        }
    }
    return value;
}

double ShipSlot::getAttrValue(Attr attr, bool modified) {
    switch (attr) {
    case Attr::engminmul: {
        auto spd = getAttrValue(Attr::minmulspd, modified);
        auto acc = getAttrValue(Attr::minmulacc, modified);
        auto rot = getAttrValue(Attr::minmulrot, modified);
        return (spd + acc + rot) / 3.0;
    }
    case Attr::engoptmul: {
        auto spd = getAttrValue(Attr::optmulspd, modified);
        auto acc = getAttrValue(Attr::optmulacc, modified);
        auto rot = getAttrValue(Attr::optmulrot, modified);
        return (spd + acc + rot) / 3.0;
    }
    case Attr::engmaxmul: {
        auto spd = getAttrValue(Attr::maxmulspd, modified);
        auto acc = getAttrValue(Attr::maxmulacc, modified);
        auto rot = getAttrValue(Attr::maxmulrot, modified);
        return (spd + acc + rot) / 3.0;
    }
    }
    if (!this->module)
        return DNaN;
    if (modified)
        return getModuleAttrValue(*this->module, attr, this->getEffectiveAttrModifier(attr));
    return getModuleAttrValue(*this->module, attr, DNaN);
}

double ShipSlot::getModuleAttrValue(const js::value& module, Attr attr, double modifier) {
    std::string attr_name(enum_name<Attr>(attr));
    js::value value = module.at(attr_name);
    switch (attr) {
    case Attr::engminmul:
        if (module["minmulspd"] || module["minmulacc"] || module["minmulrot"]) {
            value = (getModuleAttrValue(module, Attr::minmulspd) + getModuleAttrValue(module, Attr::minmulacc) + getModuleAttrValue(module, Attr::minmulrot)) / 3.0;
        }
        break;
    case Attr::engoptmul:
        if (module["optmulspd"] || module["optmulacc"] || module["optmulrot"]) {
            value = (getModuleAttrValue(module, Attr::optmulspd) + getModuleAttrValue(module, Attr::optmulacc) + getModuleAttrValue(module, Attr::optmulrot)) / 3.0;
        }
        break;
    case Attr::engmaxmul:
        if (module["maxmulspd"] || module["maxmulacc"] || module["maxmulrot"]) {
            value = (getModuleAttrValue(module, Attr::maxmulspd) + getModuleAttrValue(module, Attr::maxmulacc) + getModuleAttrValue(module, Attr::maxmulrot)) / 3.0;
        }
        break;
    }
    if (!value.is_number() || std::isnan(value.is_number())) {
        auto attribute = getShipAttr(attr);
        if (attribute) {
            if (attribute->default_attr) {
                value = getModuleAttrValue(module, attribute->default_attr->attr);
            } else {
                value = attribute->default_value;
            }
        }
    }
    return getAttrValue(attr, value.as_real(), modifier);
}


void parse_speed_scale(js::value jarr, float arr[3], float dflt) {
    if (!jarr.is_array()) {
        for (int i=0; i < 3; i++)
            arr[i] = dflt;
    } else {
        for (int i = 0; i < 3; i++)
            arr[i] = jarr[i].as_real();
    }
}

ShipStats::ShipStats(const string &type, const js::value &jship)
        : type(type)
        , jship(jship)
{
    stats = {};
    getSlot("Hull");
    getSlot("CargoHatch");
    getSlot("Armour");
    getSlot("PowerPlant");
    getSlot("MainEngines");
    getSlot("FrameShiftDrive");
    getSlot("LifeSupport");
    getSlot("PowerDistributor");
    getSlot("Radar");
    getSlot("FuelTank");
    getSlot("PlanetaryApproachSuite");
    getSlot("DataLinkScanner");
    getSlot("CodexScanner");
    getSlot("DiscoveryScanner");
    getSlot("ColonisationSuite");

    parse_speed_scale(jship["pitch_cruise"], cruise_rot[Axis::Pitch], jship["pitch"].as_real());
    parse_speed_scale(jship["yaw_cruise"], cruise_rot[Axis::Yaw], jship["yaw"].as_real());
    parse_speed_scale(jship["roll_cruise"], cruise_rot[Axis::Roll], jship["roll"].as_real());
    parse_speed_scale(jship["pitch_space"], space_rot[Axis::Pitch], 1);
    parse_speed_scale(jship["yaw_space"], space_rot[Axis::Yaw], 1);
    parse_speed_scale(jship["roll_space"], space_rot[Axis::Roll], 1);
}


void ShipStats::setSlotModule(const js::value &jvalue) {
    const std::string& slotName = jvalue["Slot"].as_string();
    const std::string& moduleName = jvalue["Item"].as_string();
    auto& slot = getSlot(slotName);
    slot.setModule(moduleName);
    auto eng = jvalue["Engineering"];
    if (eng.is_object()) {
        if (eng["BlueprintName"].is_string()) {
            std::string blueprint = eng["BlueprintName"].as_string();
            int level = eng["Level"].as_int();
            float quality = eng["Quality"].as_real();
            std::string effect = eng["ExperimentalEffect"].as_string_or();
            slot.setEngineering(blueprint, level, quality, effect);
        }
    }
}

void ShipStats::updateStat(ShipSlot& slot, Attr attr) {
    double value = slot.getEffectiveAttrValue(attr);
    auto attribute = getShipAttr(attr);
    if (attribute && attribute->unit == "%")
        value /= 100.0;
    stats[int(attr)] = value;
}
void ShipStats::updateStats() {
    auto& slot_hull = getSlot("Hull");
    updateStat(slot_hull, Attr::minthrust);
    updateStat(slot_hull, Attr::topspd);
    updateStat(slot_hull, Attr::fwdacc);
    updateStat(slot_hull, Attr::revacc);
    updateStat(slot_hull, Attr::pitch);
    updateStat(slot_hull, Attr::minpitch);
    updateStat(slot_hull, Attr::yaw);
    updateStat(slot_hull, Attr::minyaw);
    updateStat(slot_hull, Attr::roll);
    updateStat(slot_hull, Attr::minroll);

    auto& slot_eng = getSlot("MainEngines");
    updateStat(slot_eng, Attr::engminmass);
    updateStat(slot_eng, Attr::engoptmass);
    updateStat(slot_eng, Attr::engmaxmass);
    updateStat(slot_eng, Attr::minmulspd);
    updateStat(slot_eng, Attr::optmulspd);
    updateStat(slot_eng, Attr::maxmulspd);
    updateStat(slot_eng, Attr::minmulrot);
    updateStat(slot_eng, Attr::optmulrot);
    updateStat(slot_eng, Attr::maxmulrot);

    auto& slot_fsd = getSlot("FrameShiftDrive");
    hasFsdScoModule = slot_fsd.module && (*slot_fsd.module)["mtype"].as_string_or() == "cfsdo";
}

eddb::ShipSlot& ShipStats::getSlot(const std::string& name) {
    {
        auto it = slots.find(name);
        if (it != slots.end())
            return it->second;
    }
    ShipSlotGroup group = ShipSlotGroup::UNUSED;
    if (gCustomSlots.contains(name)) {
        group = gCustomSlots[name];
    } else {
        if (name.starts_with("Slot")) {
            group = ShipSlotGroup::INTERNAL;
        }
        else if (name.starts_with("Military")) {
            group = ShipSlotGroup::MILITARY;
        }
        else if (name.starts_with("TinyHardpoint")) {
            std::string num = name.substr(13,2);
            int slotnum = std::stoi(num, nullptr, 10);
            if (slotnum <= jship["slots"]["utility"].as_array().size())
                group = ShipSlotGroup::UTILITY;
            else
                group = ShipSlotGroup::HARDPOINT;
        }
        else if (name.contains("Hardpoint")) {
            group = ShipSlotGroup::HARDPOINT;
        }
    }
    auto it = slots.emplace(name, ShipSlot(*this, name, group));
    return it.first->second;
}

double ShipStats::getMassCurveMultiplier(double mass, double minMass, double optMass, double maxMass, double minMul, double optMul, double maxMul) const {
    // https://forums.frontier.co.uk/threads/the-one-formula-to-rule-them-all-the-mechanics-of-shield-and-thruster-mass-curves.300225/
    return (minMul + std::pow(std::min(1.0, (maxMass - mass) / (maxMass - minMass)), std::log((optMul - minMul) / (maxMul - minMul)) / std::log((maxMass - optMass) / (maxMass - minMass))) * (maxMul - minMul));
}
double ShipStats::getMassRotMultiplier() const {
    double totallMass = st::shipStats.unladenMass + st::shipStats.fuelMain + st::shipStats.fuelReservoir + st::shipStats.cargo;
    double minmass = stats[int(Attr::engminmass)];
    double optmass = stats[int(Attr::engoptmass)];
    double maxmass = stats[int(Attr::engmaxmass)];
    double minmulrot = stats[int(Attr::minmulrot)];
    double optmulrot = stats[int(Attr::optmulrot)];
    double maxmulrot = stats[int(Attr::maxmulrot)];
    return getMassCurveMultiplier(totallMass, minmass, optmass, maxmass, minmulrot, optmulrot, maxmulrot);
}
double ShipStats::getMassSpdMultiplier() const {
    double totallMass = st::shipStats.unladenMass + st::shipStats.fuelMain + st::shipStats.fuelReservoir + st::shipStats.cargo;
    double minmass = stats[int(Attr::engminmass)];
    double optmass = stats[int(Attr::engoptmass)];
    double maxmass = stats[int(Attr::engmaxmass)];
    double minmulspd = stats[int(Attr::minmulspd)];
    double optmulspd = stats[int(Attr::optmulspd)];
    double maxmulspd = stats[int(Attr::maxmulspd)];
    return getMassCurveMultiplier(totallMass, minmass, optmass, maxmass, minmulspd, optmulspd, maxmulspd);
}

double speed_scale(int speed_percent, const float values[3]) {
    float spd = 0.01f * std::clamp(speed_percent, 0, 100);
    if (spd >= 0.5f)
        return std::lerp(values[1], values[2], 2*spd-1);
    return std::lerp(values[0], values[1], 2*spd);
}
const double MAX_POWER_DIST = 8.0;

double ShipStats::getRotationScale(Axis::Type at, int speed_percent) const {
    if (st::ship.flags.cruise)
        return speed_scale(speed_percent, cruise_rot[at]) / cruise_rot[at][1];
    return speed_scale(speed_percent, space_rot[at]);
}

double ShipStats::getRotationSpeed(Axis::Type at, int speed_percent) const {
    switch (at) {
    case Axis::Pitch:
        return getPitchSpeed(speed_percent);
    case Axis::Yaw:
        return getYawSpeed(speed_percent);
    case Axis::Roll:
        return getRollSpeed(speed_percent);
    default:
        return 0;
    }
}

double ShipStats::getPitchSpeed(int speed_percent) const {
    if (st::ship.flags.cruise)
        return speed_scale(speed_percent, cruise_rot[Axis::Pitch]);
    double pipsEngMul = st::ship.pips[1] / MAX_POWER_DIST;
    double massRotMul = getMassRotMultiplier();
    double value = stats[int(Attr::pitch)] * pipsEngMul + stats[int(Attr::minpitch)] * (1 - pipsEngMul);
    return value * massRotMul * speed_scale(speed_percent, space_rot[Axis::Pitch]);
}
double ShipStats::getYawSpeed(int speed_percent) const {
    if (st::ship.flags.cruise)
        return speed_scale(speed_percent, cruise_rot[Axis::Yaw]);
    double pipsEngMul = st::ship.pips[1] / MAX_POWER_DIST;
    double massRotMul = getMassRotMultiplier();
    double value = stats[int(Attr::yaw)] * pipsEngMul + stats[int(Attr::minyaw)] * (1 - pipsEngMul);
    return value * massRotMul * speed_scale(speed_percent, space_rot[Axis::Yaw]);
}
double ShipStats::getRollSpeed(int speed_percent) const {
    if (st::ship.flags.cruise)
        return speed_scale(speed_percent, cruise_rot[Axis::Roll]);
    double pipsEngMul = st::ship.pips[1] / MAX_POWER_DIST;
    double massRotMul = getMassRotMultiplier();
    double value = stats[int(Attr::roll)] * pipsEngMul + stats[int(Attr::minroll)] * (1 - pipsEngMul);
    return value * massRotMul * speed_scale(speed_percent, space_rot[Axis::Roll]);
}
double ShipStats::getThrustSpeed() const {
    if (st::ship.flags.cruise)
        return DNaN;
    double pipsEngMul = st::ship.pips[1] / MAX_POWER_DIST;
    double massSpdMul = getMassSpdMultiplier();
    double value = stats[int(Attr::topspd)] * (pipsEngMul + stats[int(Attr::minthrust)] * (1 - pipsEngMul));
    return value * massSpdMul;
}
double ShipStats::getForwardAccel() const {
    if (st::ship.flags.cruise)
        return DNaN;
    double pipsEngMul = st::ship.pips[1] / MAX_POWER_DIST;
    double massSpdMul = getMassSpdMultiplier();
    double value = stats[int(Attr::fwdacc)]; // * (pipsEngMul + stats[int(Attr::minthrust)] * (1 - pipsEngMul));
    return value * massSpdMul;
}
double ShipStats::getReverseAccel() const {
    if (st::ship.flags.cruise)
        return DNaN;
    double pipsEngMul = st::ship.pips[1] / MAX_POWER_DIST;
    double massSpdMul = getMassSpdMultiplier();
    double value = stats[int(Attr::revacc)]; // * (pipsEngMul + stats[int(Attr::minthrust)] * (1 - pipsEngMul));
    return value * massSpdMul;
}


} // namespace eddb
