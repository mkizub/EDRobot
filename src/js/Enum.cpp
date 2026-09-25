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
        , allValues {}
        , allValueNames {}
{
    if (values.size() > 255)
        throw std::overflow_error(name);

    for (auto [id, names] : values) {
        auto spl = split(names, '\f');
        js::symbol sym(spl[0]);
        auto& v = allValues[id];
        const_cast<uint8_t&>(v.id) = id;
        const_cast<bool&>(v.predefined) = true;
        const_cast<js::symbol&>(v.sym) = js::symbol(spl[0]);
        const_cast<EnumDeclBase*&>(v.ED) = this;
        if (spl.size() > 1) {
            auto& aliases = const_cast<js::vector<js::symbol>&>(v.aliases);
            aliases.reserve(spl.size()-1);
            for (int a=1; a < spl.size(); a++) {
                aliases.push_back(js::symbol(spl[a]));
            }
        }
        allValueNames[id] = sym.sv().data();
    }
    mapById.reserve(values.size());
    mapByName.reserve(values.size());
    for (auto &v: allValues) {
        if (!v.id)
            continue;
        mapById.insert({v.id, &v});
        mapByName.insert({v.sym.sv(), &v});
        for (auto& a : v.aliases)
            mapByName.insert({a.sv(), &v});
    }
}

EnumVal* EnumDeclBase::addNewValue(const std::string& str) {
    LOG_ERROR("Error: Added new key \"{}\" into enum '{}'", str, name);
    unsigned new_id = 0;
    for (auto& v : allValues) {
        if (v.id > new_id)
            new_id = v.id;
    }
    new_id += 1;
    if (new_id > 255)
        throw std::overflow_error(name);
    auto& v = allValues[new_id];
    const_cast<uint8_t&>(v.id) = new_id;
    const_cast<bool&>(v.predefined) = true;
    const_cast<js::symbol&>(v.sym) = js::symbol(str);
    const_cast<EnumDeclBase*&>(v.ED) = this;
    allValueNames[new_id] = v.sym.sv().data();
    mapById.insert({v.id, &v});
    mapByName.insert({v.sym.sv(), &v});
    return &v;
}


} // namespace js