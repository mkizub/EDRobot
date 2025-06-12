//
// Created by mkizub on 11.06.2025.
//

#pragma once

#ifndef EDROBOT_FUZZYMATCH_H
#define EDROBOT_FUZZYMATCH_H


class FuzzyMatch {
public:
    FuzzyMatch();

    double ratio(const std::wstring& source, const std::wstring& target) const;

private:
    double distance(const std::wstring& source, const std::wstring& target) const;
    double delete_cost(wchar_t ch = '\0') const;
    double insert_cost(wchar_t ch = '\0') const;
    double replace_cost(wchar_t ch1, wchar_t ch2) const;

    struct Cost {
        Cost() : org(L'\0'), alt(L'\0'), cost(1) {}
        Cost(wchar_t o, float c) : org(o), alt(L'\0'), cost(c) {}
        Cost(wchar_t o, wchar_t a, float c) : org(o), alt(a), cost(c) {}
        wchar_t org;
        wchar_t alt;
        float cost;
    };

    std::vector<Cost> delete_cost_table;
    std::vector<Cost> insert_cost_table;
    std::vector<Cost> replace_cost_table;
};


#endif //EDROBOT_FUZZYMATCH_H
