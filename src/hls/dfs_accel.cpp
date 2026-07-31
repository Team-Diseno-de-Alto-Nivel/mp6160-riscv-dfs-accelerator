#include "dfs_accel.h"

namespace {

using dfs_hls::cell_t;
using dfs_hls::kMaxCells;
using dfs_hls::kMaxCols;
using dfs_hls::kMaxRows;
using dfs_hls::kMaxStack;
using dfs_hls::kMaxTrieNodes;
using dfs_hls::kMaxWordLen;
using dfs_hls::kMaxWords;

constexpr std::int8_t kDeltaRow[8] = {-1, 1, 0, 0, -1, -1, 1, 1};
constexpr std::int8_t kDeltaCol[8] = {0, 0, -1, 1, -1, 1, -1, 1};
constexpr std::uint32_t kAlphabet = 26;

struct Coord {
    std::uint8_t row;
    std::uint8_t col;
};

struct Frame {
    Coord cur;
    std::uint8_t next;
    std::uint8_t char_index;
    std::int16_t node;
    std::uint16_t best;
    std::uint32_t walked;
    bool checked;
};

struct Engine {
    cell_t grid[kMaxCells];
    bool visited[kMaxCells];
    bool reachable[kMaxCells];
    std::uint16_t memo[kMaxCells];
    Coord stack[kMaxStack];
    Frame frames[kMaxStack];
    std::int16_t trie_next[kMaxTrieNodes][kAlphabet];
    std::int8_t trie_word[kMaxTrieNodes];
    std::uint8_t words[kMaxWords][kMaxWordLen];
    std::uint8_t word_len[kMaxWords];
    bool word_found[kMaxWords];
    std::uint32_t trie_size;
    std::uint32_t rows;
    std::uint32_t cols;
    std::uint32_t degree;
    std::uint32_t sp;
    std::uint32_t fp;
    std::uint32_t expanded_nodes;
    std::uint32_t visited_cells;
    std::uint32_t peak_stack_depth;

    std::uint32_t cell_count() const { return rows * cols; }
    std::uint32_t index(Coord c) const { return c.row * cols + c.col; }
    cell_t at(Coord c) const { return grid[index(c)]; }
    bool is_visited(Coord c) const { return visited[index(c)]; }

    void mark_visited(Coord c) {
        const std::uint32_t i = index(c);
        if (!visited[i]) {
            visited[i] = true;
            ++visited_cells;
        }
    }

    void unmark_visited(Coord c) { visited[index(c)] = false; }

    void clear_visited() {
        for (std::uint32_t i = 0; i < cell_count(); ++i) visited[i] = false;
    }

    bool neighbor(Coord c, std::uint32_t i, Coord &out) const {
        const int r = static_cast<int>(c.row) + kDeltaRow[i];
        const int col = static_cast<int>(c.col) + kDeltaCol[i];
        if (r < 0 || col < 0 || r >= static_cast<int>(rows) || col >= static_cast<int>(cols))
            return false;
        out.row = static_cast<std::uint8_t>(r);
        out.col = static_cast<std::uint8_t>(col);
        return true;
    }

    void note_depth(std::uint32_t depth) {
        if (depth > peak_stack_depth) peak_stack_depth = depth;
    }

    void push(Coord c) {
        stack[sp++] = c;
        note_depth(sp);
    }

    Coord pop() { return stack[--sp]; }

    Frame &push_frame(Coord c) {
        Frame &f = frames[fp++];
        f.cur = c;
        f.next = 0;
        f.char_index = 0;
        f.node = 0;
        f.best = 1;
        f.walked = 0;
        f.checked = false;
        ++expanded_nodes;
        note_depth(fp);
        return f;
    }
};

Coord make_coord(std::uint32_t r, std::uint32_t c) {
    Coord out;
    out.row = static_cast<std::uint8_t>(r);
    out.col = static_cast<std::uint8_t>(c);
    return out;
}

std::uint32_t run_number_of_islands(Engine &e) {
    e.clear_visited();
    std::uint32_t islands = 0;
    for (std::uint32_t r = 0; r < e.rows; ++r) {
        for (std::uint32_t c = 0; c < e.cols; ++c) {
            const Coord src = make_coord(r, c);
            if (e.at(src) == 0 || e.is_visited(src)) continue;
            ++islands;
            e.sp = 0;
            e.push(src);
            e.mark_visited(src);
            while (e.sp != 0) {
                const Coord cur = e.pop();
                ++e.expanded_nodes;
                for (std::uint32_t i = 0; i < e.degree; ++i) {
                    Coord n;
                    if (!e.neighbor(cur, i, n)) continue;
                    if (e.at(n) != 0 && !e.is_visited(n)) {
                        e.mark_visited(n);
                        e.push(n);
                    }
                }
            }
        }
    }
    return islands;
}

void flood(Engine &e, bool from_pacific) {
    e.clear_visited();
    e.sp = 0;
    for (std::uint32_t r = 0; r < e.rows; ++r) {
        for (std::uint32_t c = 0; c < e.cols; ++c) {
            const bool border = from_pacific ? (r == 0 || c == 0)
                                             : (r + 1 == e.rows || c + 1 == e.cols);
            if (!border) continue;
            const Coord s = make_coord(r, c);
            if (!e.is_visited(s)) {
                e.mark_visited(s);
                e.push(s);
            }
        }
    }
    while (e.sp != 0) {
        const Coord cur = e.pop();
        ++e.expanded_nodes;
        for (std::uint32_t i = 0; i < e.degree; ++i) {
            Coord n;
            if (!e.neighbor(cur, i, n)) continue;
            if (!e.is_visited(n) && e.at(n) >= e.at(cur)) {
                e.mark_visited(n);
                e.push(n);
            }
        }
    }
}

std::uint32_t run_pacific_atlantic(Engine &e) {
    flood(e, true);
    for (std::uint32_t i = 0; i < e.cell_count(); ++i) e.reachable[i] = e.visited[i];
    flood(e, false);
    std::uint32_t count = 0;
    for (std::uint32_t i = 0; i < e.cell_count(); ++i)
        if (e.reachable[i] && e.visited[i]) ++count;
    return count;
}

std::uint32_t run_unique_paths_iii(Engine &e) {
    e.clear_visited();
    std::uint32_t walkable = 0;
    Coord start = make_coord(0, 0);
    for (std::uint32_t r = 0; r < e.rows; ++r) {
        for (std::uint32_t c = 0; c < e.cols; ++c) {
            const Coord p = make_coord(r, c);
            if (e.at(p) != -1) ++walkable;
            if (e.at(p) == 1) start = p;
        }
    }

    std::uint32_t paths = 0;
    e.mark_visited(start);
    e.fp = 0;
    e.push_frame(start).walked = 1;

    while (e.fp != 0) {
        Frame &f = e.frames[e.fp - 1];
        const Coord cur = f.cur;

        if (e.at(cur) == 2) {
            if (f.walked == walkable) ++paths;
            --e.fp;
            if (e.fp != 0) e.unmark_visited(cur);
            continue;
        }

        if (f.next < e.degree) {
            const std::uint32_t i = f.next++;
            Coord n;
            if (!e.neighbor(cur, i, n)) continue;
            if (e.at(n) == -1 || e.is_visited(n)) continue;
            const std::uint32_t walked = f.walked;
            e.mark_visited(n);
            e.push_frame(n).walked = walked + 1;
            continue;
        }

        --e.fp;
        if (e.fp != 0) e.unmark_visited(cur);
    }
    return paths;
}

void propagate_best(Engine &e, std::uint32_t child) {
    if (e.fp == 0) return;
    Frame &parent = e.frames[e.fp - 1];
    if (child + 1 > parent.best) parent.best = static_cast<std::uint16_t>(child + 1);
}

std::uint32_t run_longest_increasing_path(Engine &e, bool use_memo) {
    for (std::uint32_t i = 0; i < e.cell_count(); ++i) e.memo[i] = 0;

    std::uint32_t longest = 0;
    for (std::uint32_t r = 0; r < e.rows; ++r) {
        for (std::uint32_t c = 0; c < e.cols; ++c) {
            e.fp = 0;
            e.push_frame(make_coord(r, c));
            std::uint32_t ret = 0;

            while (e.fp != 0) {
                Frame &f = e.frames[e.fp - 1];
                const Coord cur = f.cur;

                if (!f.checked) {
                    f.checked = true;
                    if (use_memo && e.memo[e.index(cur)] != 0) {
                        ret = e.memo[e.index(cur)];
                        --e.fp;
                        propagate_best(e, ret);
                        continue;
                    }
                }

                if (f.next < e.degree) {
                    const std::uint32_t i = f.next++;
                    Coord n;
                    if (!e.neighbor(cur, i, n)) continue;
                    if (e.at(n) > e.at(cur)) e.push_frame(n);
                    continue;
                }

                ret = f.best;
                if (use_memo) e.memo[e.index(cur)] = static_cast<std::uint16_t>(ret);
                --e.fp;
                propagate_best(e, ret);
            }

            if (ret > longest) longest = ret;
        }
    }
    return longest;
}

void build_trie(Engine &e, std::uint32_t num_words) {
    for (std::uint32_t n = 0; n < kMaxTrieNodes; ++n) {
        for (std::uint32_t k = 0; k < kAlphabet; ++k) e.trie_next[n][k] = -1;
        e.trie_word[n] = -1;
    }
    e.trie_size = 1;
    for (std::uint32_t w = 0; w < num_words; ++w) {
        std::uint32_t node = 0;
        for (std::uint32_t k = 0; k < e.word_len[w]; ++k) {
            const std::uint32_t li = e.words[w][k] - 'a';
            if (li >= kAlphabet) continue;
            if (e.trie_next[node][li] < 0) {
                e.trie_next[node][li] = static_cast<std::int16_t>(e.trie_size);
                ++e.trie_size;
            }
            node = static_cast<std::uint32_t>(e.trie_next[node][li]);
        }
        e.trie_word[node] = static_cast<std::int8_t>(w);
    }
}

bool enter_trie(Engine &e, Coord cur, std::int16_t node, std::uint32_t &count) {
    ++e.expanded_nodes;
    e.note_depth(e.fp + 1);
    const std::uint32_t li = static_cast<std::uint32_t>(e.at(cur) - 'a');
    if (li >= kAlphabet) return false;
    const std::int16_t nxt = e.trie_next[node][li];
    if (nxt < 0) return false;
    const std::int8_t id = e.trie_word[nxt];
    if (id >= 0 && !e.word_found[id]) {
        e.word_found[id] = true;
        ++count;
    }
    e.mark_visited(cur);
    Frame &f = e.frames[e.fp++];
    f.cur = cur;
    f.next = 0;
    f.node = nxt;
    return true;
}

std::uint32_t run_word_search_trie(Engine &e, std::uint32_t num_words) {
    e.clear_visited();
    build_trie(e, num_words);
    for (std::uint32_t w = 0; w < kMaxWords; ++w) e.word_found[w] = false;

    std::uint32_t count = 0;
    for (std::uint32_t r = 0; r < e.rows; ++r) {
        for (std::uint32_t c = 0; c < e.cols; ++c) {
            e.fp = 0;
            if (!enter_trie(e, make_coord(r, c), 0, count)) continue;
            while (e.fp != 0) {
                Frame &f = e.frames[e.fp - 1];
                const Coord cur = f.cur;
                if (f.next < e.degree) {
                    const std::uint32_t i = f.next++;
                    Coord n;
                    if (!e.neighbor(cur, i, n)) continue;
                    if (e.is_visited(n)) continue;
                    enter_trie(e, n, f.node, count);
                    continue;
                }
                e.unmark_visited(cur);
                --e.fp;
            }
        }
    }
    return count;
}

bool enter_match(Engine &e, Coord cur, std::uint8_t char_index, std::uint32_t word,
                 bool &ret) {
    ++e.expanded_nodes;
    e.note_depth(e.fp + 1);
    if (e.at(cur) != static_cast<cell_t>(e.words[word][char_index])) {
        ret = false;
        return false;
    }
    if (char_index + 1u == e.word_len[word]) {
        ret = true;
        return false;
    }
    e.mark_visited(cur);
    Frame &f = e.frames[e.fp++];
    f.cur = cur;
    f.next = 0;
    f.char_index = char_index;
    ret = false;
    return true;
}

bool run_match(Engine &e, Coord start, std::uint32_t word) {
    e.fp = 0;
    bool ret = false;
    if (!enter_match(e, start, 0, word, ret)) return ret;

    while (e.fp != 0) {
        Frame &f = e.frames[e.fp - 1];
        const Coord cur = f.cur;
        if (!ret && f.next < e.degree) {
            const std::uint32_t i = f.next++;
            Coord n;
            if (!e.neighbor(cur, i, n)) continue;
            if (e.is_visited(n)) continue;
            enter_match(e, n, static_cast<std::uint8_t>(f.char_index + 1), word, ret);
            continue;
        }
        e.unmark_visited(cur);
        --e.fp;
    }
    return ret;
}

std::uint32_t run_word_search_no_pruning(Engine &e, std::uint32_t num_words) {
    e.clear_visited();
    std::uint32_t count = 0;
    for (std::uint32_t w = 0; w < num_words; ++w) {
        if (e.word_len[w] == 0) continue;
        bool found = false;
        for (std::uint32_t r = 0; r < e.rows && !found; ++r)
            for (std::uint32_t c = 0; c < e.cols && !found; ++c)
                if (run_match(e, make_coord(r, c), w)) found = true;
        if (found) ++count;
    }
    return count;
}

Engine engine;

}  // namespace

void dfs_accel(const dfs_hls::cell_t *grid_in,
               const std::uint8_t *words_in,
               std::uint32_t *result_out,
               std::uint32_t rows,
               std::uint32_t cols,
               std::uint32_t mode,
               std::uint32_t flags,
               std::uint32_t num_words) {
#pragma HLS INTERFACE m_axi port = grid_in offset = slave bundle = gmem0 depth = 4096
#pragma HLS INTERFACE m_axi port = words_in offset = slave bundle = gmem1 depth = 256
#pragma HLS INTERFACE m_axi port = result_out offset = slave bundle = gmem2 depth = 4
#pragma HLS INTERFACE s_axilite port = grid_in bundle = control
#pragma HLS INTERFACE s_axilite port = words_in bundle = control
#pragma HLS INTERFACE s_axilite port = result_out bundle = control
#pragma HLS INTERFACE s_axilite port = rows bundle = control
#pragma HLS INTERFACE s_axilite port = cols bundle = control
#pragma HLS INTERFACE s_axilite port = mode bundle = control
#pragma HLS INTERFACE s_axilite port = flags bundle = control
#pragma HLS INTERFACE s_axilite port = num_words bundle = control
#pragma HLS INTERFACE s_axilite port = return bundle = control

    Engine &e = engine;
    e.expanded_nodes = 0;
    e.visited_cells = 0;
    e.peak_stack_depth = 0;
    e.sp = 0;
    e.fp = 0;

    if (rows == 0 || cols == 0 || rows > kMaxRows || cols > kMaxCols) {
        result_out[dfs_hls::kSlotValue] = 0;
        result_out[dfs_hls::kSlotExpandedNodes] = 0;
        result_out[dfs_hls::kSlotVisitedCells] = 0;
        result_out[dfs_hls::kSlotPeakStackDepth] = 0;
        return;
    }

    e.rows = rows;
    e.cols = cols;
    e.degree = (flags & dfs_hls::kFlagEightConnected) ? 8u : 4u;

    for (std::uint32_t i = 0; i < rows * cols; ++i) e.grid[i] = grid_in[i];

    const std::uint32_t words = num_words > kMaxWords ? kMaxWords : num_words;
    for (std::uint32_t w = 0; w < kMaxWords; ++w) {
        e.word_len[w] = 0;
        for (std::uint32_t k = 0; k < kMaxWordLen; ++k) e.words[w][k] = 0;
    }
    for (std::uint32_t w = 0; w < words; ++w) {
        std::uint32_t len = 0;
        for (std::uint32_t k = 0; k < kMaxWordLen; ++k) {
            const std::uint8_t ch = words_in[w * kMaxWordLen + k];
            e.words[w][k] = ch;
            if (ch != 0) ++len;
        }
        e.word_len[w] = static_cast<std::uint8_t>(len);
    }

    std::uint32_t value = 0;
    switch (mode) {
        case dfs_hls::kNumberOfIslands:
            value = run_number_of_islands(e);
            break;
        case dfs_hls::kUniquePathsIii:
            value = run_unique_paths_iii(e);
            break;
        case dfs_hls::kWordSearchIi:
            value = (flags & dfs_hls::kFlagPrune) ? run_word_search_trie(e, words)
                                                  : run_word_search_no_pruning(e, words);
            break;
        case dfs_hls::kLongestIncreasingPath:
            value = run_longest_increasing_path(e, (flags & dfs_hls::kFlagMemo) != 0);
            break;
        case dfs_hls::kPacificAtlantic:
            value = run_pacific_atlantic(e);
            break;
        default:
            value = 0;
            break;
    }

    result_out[dfs_hls::kSlotValue] = value;
    result_out[dfs_hls::kSlotExpandedNodes] = e.expanded_nodes;
    result_out[dfs_hls::kSlotVisitedCells] = e.visited_cells;
    result_out[dfs_hls::kSlotPeakStackDepth] = e.peak_stack_depth;
}
