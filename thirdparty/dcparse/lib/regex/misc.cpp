#include "regex/misc.hpp"
using namespace std;


std::vector<std::set<size_t>>
transitive_closure(std::vector<std::set<size_t>> graph)
{
    const auto s = graph.size();
    for (size_t i = 0; i < s; ++i) {
        graph[i].insert(i);

        for (auto& k: graph[i]) {
            for (auto& jset: graph) {
                if (jset.count(i)) {
                    jset.insert(k);
                }
            }
        }
    }
    return graph;
}
