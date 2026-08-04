#include "algorithms/dfs_algorithm.h"

#include "algorithms/longest_increasing_path/longest_increasing_path.h"
#include "algorithms/number_of_islands/number_of_islands.h"
#include "algorithms/pacific_atlantic/pacific_atlantic.h"
#include "algorithms/unique_paths_iii/unique_paths_iii.h"
#include "algorithms/word_search_ii/word_search_ii.h"

namespace dfs {

std::vector<AlgoEntry> make_algorithms() {
    return {
        {"number_of_islands", "number_of_islands", Tier::Large,
         [](const Problem& p, Accelerator& a) { return number_of_islands(p.grid, a); }},
        {"unique_paths_iii", "unique_paths_iii", Tier::Legacy,
         [](const Problem& p, Accelerator& a) { return unique_paths_iii(p.grid, a); }},
        {"word_search_ii", "word_search_ii", Tier::Medium,
         [](const Problem& p, Accelerator& a) {
             return word_search_ii(p.grid, p.words, a, true);
         }},
        {"word_search_ii_nopruning", "word_search_ii", Tier::Legacy,
         [](const Problem& p, Accelerator& a) {
             return word_search_ii(p.grid, p.words, a, false);
         }},
        {"longest_increasing_path", "longest_increasing_path", Tier::Large,
         [](const Problem& p, Accelerator& a) {
             return longest_increasing_path(p.grid, a, true);
         }},
        {"longest_increasing_path_nomemo", "longest_increasing_path", Tier::Legacy,
         [](const Problem& p, Accelerator& a) {
             return longest_increasing_path(p.grid, a, false);
         }},
        {"pacific_atlantic", "pacific_atlantic", Tier::Large,
         [](const Problem& p, Accelerator& a) { return pacific_atlantic(p.grid, a); }},
    };
}

}  // namespace dfs
