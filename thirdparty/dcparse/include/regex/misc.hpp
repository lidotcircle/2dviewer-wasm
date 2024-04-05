#ifndef _DC_PARSER_REGEX_MISC_HPP_
#define _DC_PARSER_REGEX_MISC_HPP_

#include <algorithm>
#include <vector>
#include <set>
#include <assert.h>


template<typename T>
std::vector<std::pair<T,T>> merge_sorted_range(const std::vector<std::pair<T,T>>& ranges) {
    std::vector<std::pair<T,T>> result;
    for (auto const& range : ranges) {
        if (result.empty() || result.back().second+1 < range.first) {
            result.push_back(range);
        } else {
            result.back().second = std::max(result.back().second, range.second);
        }
    }
    return result;
}

template<typename ValueT>
std::vector<std::pair<ValueT,ValueT>>
split_ranges_to_units(std::vector<std::pair<ValueT,ValueT>> ranges)
{
    std::sort(ranges.begin(), ranges.end());
    auto merged = merge_sorted_range(ranges);
    std::set<std::pair<ValueT,ValueT>> merged_set(merged.begin(), merged.end());
    std::set<ValueT> low_set;
    std::set<ValueT> high_set;
    for (auto& r: ranges) {
        low_set.insert(r.first);
        high_set.insert(r.second);
    }

    for (auto low: low_set) {
        auto lb = std::lower_bound(merged_set.begin(), merged_set.end(), low,
                                   [](auto const& r, ValueT l) { return r.second < l; });
        assert(lb != merged_set.end());

        if (lb->first > low) continue;

        std::vector<std::pair<ValueT,ValueT>> mv;
        if (low > lb->first) mv.push_back(std::make_pair(lb->first, low - 1));
        mv.push_back(std::make_pair(low, lb->second));

        merged_set.erase(lb);
        merged_set.insert(mv.begin(), mv.end());
    }

    for (auto high: high_set) {
        auto lb = std::lower_bound(merged_set.begin(), merged_set.end(), high,
                                   [](auto const& r, ValueT l) { return r.second < l; });
        assert(lb != merged_set.end());

        if (lb->first > high) continue;

        std::vector<std::pair<ValueT,ValueT>> mv;
        mv.push_back(std::make_pair(lb->first, high));
        if (high < lb->second) mv.push_back(std::make_pair(high + 1, lb->second));

        merged_set.erase(lb);
        merged_set.insert(mv.begin(), mv.end());
    }

    return std::vector<std::pair<ValueT,ValueT>>(merged_set.begin(), merged_set.end());
}

std::vector<std::set<size_t>>
transitive_closure(std::vector<std::set<size_t>> graph);

#endif // _DC_PARSER_REGEX_MISC_HPP_
