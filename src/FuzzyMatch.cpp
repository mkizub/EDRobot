//
// Created by mkizub on 11.06.2025.
//

#include "pch.h"

#include "FuzzyMatch.h"

FuzzyMatch::FuzzyMatch() {
    delete_cost_table = {{'.',0.4},{',',0.5},{'-',0.6},{'`',0.5},{'\'',0.5},{'\"',0.7}};
    insert_cost_table = delete_cost_table;
    replace_cost_table = {
            {L'A',L'А',0.1},
            {L'a',L'а',0.1},
            {L'B',L'В',0.1},
            {L'E',L'Е',0.1},
            {L'e',L'е',0.1},
            {L'E',L'Ё',0.4},
            {L'е',L'ё',0.4},
            {L'З',L'3',0.3},
            {L'И',L'Й',0.4},
            {L'и',L'й',0.4},
            {L'K',L'К',0.1},
            {L'k',L'к',0.3},
            {L'M',L'М',0.1},
            {L'm',L'м',0.3},
            {L'H',L'Н',0.1},
            {L'O',L'О',0.1},
            {L'o',L'о',0.1},
            {L'O',L'0',0.2},
            {L'О',L'0',0.2},
            {L'P',L'Р',0.1},
            {L'p',L'р',0.1},
            {L'C',L'С',0.1},
            {L'c',L'с',0.1},
            {L'T',L'Т',0.1},
            {L'Y',L'У',0.3},
            {L'y',L'у',0.2},
            {L'X',L'Х',0.1},
            {L'x',L'х',0.1},
            {L'Ъ',L'ь',0.3},
            {L'ъ',L'ь',0.3},
            {L'Щ',L'Ш',0.3},
            {L'щ',L'ш',0.3},
            {L'i',L'l',0.3},
            {L'i',L'j',0.3},
            {L'O',L'Q',0.3},
    };
}

double FuzzyMatch::ratio(const std::wstring &source, const std::wstring &target) const {
    double dist, dist_max;
    if (source.size() <= target.size()) {
        dist = distance(source, target);
        dist_max = double(target.size());
    } else {
        dist = distance(target, source);
        dist_max = double(source.size());
    }
    return (1 - dist / dist_max) * 100;
}

double FuzzyMatch::distance(const std::wstring &source, const std::wstring &target) const {
    const size_t min_size = source.size();
    const size_t max_size = target.size();
    std::vector<double> lev_dist(min_size + 1);

    lev_dist[0] = 0;
    for (size_t i=1; i <= min_size; ++i) {
        lev_dist[i] = lev_dist[i-1] + delete_cost();
    }

    for (size_t j=1; j <= max_size; ++j) {
        double previous_diagonal = lev_dist[0];
        lev_dist[0] += insert_cost();

        for (size_t i=1; i <= min_size; ++i) {
            double previous_diagonal_save = lev_dist[i];
            if (source[i-1] == target[j-1]) {
                lev_dist[i] = previous_diagonal;
            } else {
                double del_cost = delete_cost(source[i]);
                double ins_cost = insert_cost(source[i]);
                double repl_cost = replace_cost(source[i], target[j]);
                lev_dist[i] = std::min(std::min(lev_dist[i-1] + del_cost, lev_dist[i] + ins_cost), previous_diagonal + repl_cost);
            }
            previous_diagonal = previous_diagonal_save;
        }
    }

    return lev_dist[min_size];
}

double FuzzyMatch::delete_cost(wchar_t ch) const {
    for (auto it=delete_cost_table.begin(); it != delete_cost_table.end(); ++it)
        if (ch == it->org)
            return it->cost;
    return 1;
}

double FuzzyMatch::insert_cost(wchar_t ch) const {
    for (auto it=insert_cost_table.begin(); it != insert_cost_table.end(); ++it)
        if (ch == it->org)
            return it->cost;
    return 1;
}

double FuzzyMatch::replace_cost(wchar_t ch1, wchar_t ch2) const {
    for (auto it=replace_cost_table.begin(); it != replace_cost_table.end(); ++it)
        if (ch1 == it->org && ch2 == it->alt || ch1 == it->alt && ch2 == it->org)
            return it->cost;
    return 1;
}
