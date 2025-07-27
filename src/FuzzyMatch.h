//
// Created by mkizub on 11.06.2025.
//

#pragma once

#ifndef EDROBOT_FUZZYMATCH_H
#define EDROBOT_FUZZYMATCH_H

// sample font Segoe UI Symbol
//const wchar_t ICON_CHAR_NAV_STAR             = u'\u2726'; // ✦
//const wchar_t ICON_CHAR_NAV_BEACON           = u'\u2604'; // ☄
//const wchar_t ICON_CHAR_NAV_TOURIST_BEACON   = u'\u2615'; // ☕
//const wchar_t ICON_CHAR_NAV_BODY             = u'\u26BE'; // ⚾
//const wchar_t ICON_CHAR_NAV_LAND             = u'\u26BD'; // ⚽
//const wchar_t ICON_CHAR_NAV_BELT             = u'\u26EC'; // ⛬
//const wchar_t ICON_CHAR_NAV_ORBIS            = u'\u2707'; // ✇
//const wchar_t ICON_CHAR_NAV_CORIOLIS         = u'\u26CB'; // ⛋
//const wchar_t ICON_CHAR_NAV_MINER_BASE       = u'\u2B56'; // ⭖
//const wchar_t ICON_CHAR_NAV_OUTPOST          = u'\u29F0'; // ⧰
//const wchar_t ICON_CHAR_NAV_INSTALLATION     = u'\u29D6'; // ⧖
//const wchar_t ICON_CHAR_NAV_PORT             = u'\u26EB'; // ⛫
//const wchar_t ICON_CHAR_NAV_FACTORY          = u'\u2617'; // ☗
//const wchar_t ICON_CHAR_NAV_SETTLEMENT       = u'\u2616'; // ☖
//const wchar_t ICON_CHAR_NAV_CARRIER          = u'\u2708'; // ✈
//const wchar_t ICON_CHAR_NAV_STATION_MEGASHIP = u'\u267B'; // ♻
//const wchar_t ICON_CHAR_NAV_MEGASHIP         = u'\u2672'; // ♲
//const wchar_t ICON_CHAR_NAV_ENGINEER         = u'\u23E3'; // ⏣
//const wchar_t ICON_CHAR_NAV_SIGNAL           = u'\u2BD0'; // ⯐
//const wchar_t ICON_CHAR_NAV_WAR_ZONE         = u'\u2316'; // ⌖
//const wchar_t ICON_CHAR_NAV_RES_SITE         = u'\u26CF'; // ⛏
//const wchar_t ICON_CHAR_NAV_SYSTEM           = u'\u2600'; // ☀
//const wchar_t ICON_CHAR_NAV_LOCATION         = u'\u2207'; // ∇
//const wchar_t ICON_CHAR_NAV_SHIELD1          = u'\u25C7'; // ◇
//const wchar_t ICON_CHAR_NAV_SHIELD2          = u'\u2B16'; // ⬖
//const wchar_t ICON_CHAR_NAV_SHIELD3          = u'\u25C6'; // ◆

class FuzzyMatch {
public:
    FuzzyMatch() = default;

    double ratio(const std::wstring& source, const std::wstring& target) const;

    std::wstring toOCR(const std::wstring& source);
    wchar_t toOCR(wchar_t ch);

private:
    double distance(const std::wstring& source, const std::wstring& target) const;
    double delete_cost(wchar_t ch = '\0') const;
    double insert_cost(wchar_t ch = '\0') const;
    double replace_cost(wchar_t ch1, wchar_t ch2) const;

    struct Cost {
        Cost() : org(L'\0'), alt(L'\0'), cost(1) {}
        Cost(wchar_t o, float c) : org(o), alt(L'\0'), cost(c) {}
        Cost(wchar_t o, wchar_t a, float c) : org(o), alt(a), cost(c) {}
        Cost(const Cost& other) = default;
        const wchar_t org;
        const wchar_t alt;
        const float cost;
    };

    static const std::vector<Cost> delete_cost_table;
    static const std::vector<Cost> insert_cost_table;
    static const std::vector<Cost> replace_cost_table;
};


#endif //EDROBOT_FUZZYMATCH_H
