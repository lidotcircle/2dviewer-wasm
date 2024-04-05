#ifndef _DCPARSER_ALGO_HPP_
#define _DCPARSER_ALGO_HPP_

#include <map>
#include <set>
#include <vector>
#include <optional>
#include <algorithm>

template<typename T>
std::map<T,std::set<T>>
transitive_closure(std::map<T,std::set<T>> graph)
{
    std::map<T,std::set<T>> result;
    for (const auto& kv: graph) {
        const auto& key = kv.first;
        result[key].insert(key);

        for (const auto& j: graph[key]) {
            result[key].insert(j);
            for (const auto& k: graph[j]) {
                result[key].insert(k);
            }
        }
    }
    return result;
}

template<typename T>
class SubsetOf
{
private:
    std::set<T> _set;
    std::vector<bool> _vec;
    bool finished;

    bool advance()
    {
        for (size_t i = 0; i < _vec.size(); ++i) {
            if (!_vec[i]) {
                _vec[i] = true;
                return true;
            }
            _vec[i] = false;
        }

        return false;
    }

public:
    SubsetOf(std::set<T> set)
        : _set(std::move(set)), _vec(_set.size(), false),
          finished(false)
    {
        assert(_set.size() == _vec.size());
    }

    std::optional<std::set<T>> operator()()
    {
        if (finished)
            return std::nullopt;

        std::set<T> result;
        for (size_t i = 0; i < _vec.size(); ++i) {
            if (_vec[i]) {
                auto bg = _set.begin();
                std::advance(bg, i);
                result.insert(*bg);
            }
        }

        finished = !this->advance();
        return result;
    }
};

#endif // _DCPARSER_ALGO_HPP_
