#include "algorithms/word_search_ii/word_search_ii.h"

#include <array>
#include <functional>
#include <string>
#include <vector>

#include "harness/instrumentation.h"

namespace dfs {
namespace {

struct TrieNode {
    std::array<int, 26> next;
    int word_id = -1;
    TrieNode() { next.fill(-1); }
};

long search_no_pruning(const Grid& g, const std::vector<std::string>& words,
                       Accelerator& accel) {
    accel.reset_visited();
    Counters& counters = accel.counters();
    long count = 0;

    std::function<bool(Coord, const std::string&, std::size_t, std::size_t)> match =
        [&](Coord cur, const std::string& word, std::size_t i, std::size_t depth) -> bool {
        ++counters.expanded_nodes;
        if (depth > counters.peak_stack_depth) counters.peak_stack_depth = depth;
        if (g.at(cur.row, cur.col) != word[i]) return false;
        if (i + 1 == word.size()) return true;
        accel.mark_visited(cur);
        bool ok = false;
        for (const Coord n : accel.neighbors(cur)) {
            if (!accel.is_visited(n) && match(n, word, i + 1, depth + 1)) {
                ok = true;
                break;
            }
        }
        accel.unmark_visited(cur);
        return ok;
    };

    for (const std::string& word : words) {
        if (word.empty()) continue;
        bool found = false;
        for (int r = 0; r < g.rows && !found; ++r)
            for (int c = 0; c < g.cols && !found; ++c)
                if (match({r, c}, word, 0, 1)) found = true;
        if (found) ++count;
    }
    return count;
}

long search_trie(const Grid& g, const std::vector<std::string>& words, Accelerator& accel) {
    accel.reset_visited();
    Counters& counters = accel.counters();

    std::vector<TrieNode> trie(1);
    for (std::size_t w = 0; w < words.size(); ++w) {
        int node = 0;
        for (char ch : words[w]) {
            const int li = ch - 'a';
            if (trie[node].next[li] == -1) {
                trie[node].next[li] = static_cast<int>(trie.size());
                trie.emplace_back();
            }
            node = trie[node].next[li];
        }
        trie[node].word_id = static_cast<int>(w);
    }

    std::vector<bool> found(words.size(), false);
    long count = 0;

    std::function<void(Coord, int, std::size_t)> dfs = [&](Coord cur, int node,
                                                           std::size_t depth) {
        ++counters.expanded_nodes;
        if (depth > counters.peak_stack_depth) counters.peak_stack_depth = depth;

        const int li = g.at(cur.row, cur.col) - 'a';
        const int nxt = trie[node].next[li];
        if (nxt == -1) return;

        if (trie[nxt].word_id >= 0 && !found[trie[nxt].word_id]) {
            found[trie[nxt].word_id] = true;
            ++count;
        }

        accel.mark_visited(cur);
        for (const Coord n : accel.neighbors(cur)) {
            if (!accel.is_visited(n)) dfs(n, nxt, depth + 1);
        }
        accel.unmark_visited(cur);
    };

    for (int r = 0; r < g.rows; ++r)
        for (int c = 0; c < g.cols; ++c) dfs({r, c}, 0, 1);

    return count;
}

}  // namespace

long word_search_ii(const Grid& board, const std::vector<std::string>& words,
                    Accelerator& accel, bool use_pruning) {
    return use_pruning ? search_trie(board, words, accel)
                       : search_no_pruning(board, words, accel);
}

}  // namespace dfs
