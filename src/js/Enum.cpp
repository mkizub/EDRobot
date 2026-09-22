//
// Created by mkizub on 21.09.2026.
//

namespace js {

std::unordered_set<std::string, string_hash, std::equal_to<void>>& symbol::getSymbolSet() {
    static std::unordered_set<std::string, string_hash, std::equal_to<void>> gSymbolSet;
    return gSymbolSet;
}

EnumDeclBase::EnumDeclBase(std::string_view nm, IList values)
        : name(nm)
        , maxPredefinedId(uint8_t(values.size()))
{
    if (values.size() > 255)
        throw std::overflow_error(name);

    allValues.reserve(values.size()+1);
    allValues.emplace_back(new EnumVal{0, false, {}, {}, this});
    for (auto value : values) {
        auto spl = split(value.second, '\f');
        js::vector<js::symbol> aliases;
        js::symbol sym(spl[0]);
        if (spl.size() > 1) {
            aliases.reserve(spl.size()-1);
            for (int a=1; a < spl.size(); a++) {
                aliases.push_back(js::symbol(spl[a]));
            }
        }
        allValues.emplace_back(new EnumVal{value.first, true, sym, aliases, this});
    }
    mapById.reserve(allValues.size());
    mapByName.reserve(allValues.size());
    for (auto &v: allValues) {
        mapById.insert({v->id, v.get()});
        mapByName.insert({v->sym.sv(), v.get()});
        for (auto& a : v->aliases)
            mapByName.insert({a.sv(), v.get()});
    }
}

EnumVal* EnumDeclBase::get_ptr(unsigned id) const {
    if (auto it = mapById.find(id); it != mapById.end())
        return it->second;
    return nullptr;
}

EnumVal* EnumDeclBase::get_ptr(std::string_view sv) const {
    if (auto it = mapByName.find(sv); it != mapByName.end())
        return it->second;
    return nullptr;
}

EnumVal* EnumDeclBase::addNewValue(const std::string& str) {
    LOG_ERROR("Error: Added new key \"{}\" into enum '{}'", str, name);
    unsigned new_id = 0;
    for (auto& v : allValues) {
        if (v->id > new_id)
            new_id = v->id;
    }
    new_id += 1;
    if (new_id > 255)
        throw std::overflow_error(name);
    js::symbol sym(str);
    auto& v = allValues.emplace_back(new EnumVal{uint8_t(new_id), false, sym, {}, this});
    mapById.insert({v->id, v.get()});
    mapByName.insert({v->sym.sv(), v.get()});
    return v.get();
}


} // namespace js