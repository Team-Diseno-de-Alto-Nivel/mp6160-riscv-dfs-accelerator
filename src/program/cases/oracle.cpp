#include "cases/oracle.h"

#include <algorithm>
#include <cstdint>

namespace dfs {
namespace {

constexpr int kDr[8] = {-1, 1, 0, 0, -1, -1, 1, 1};
constexpr int kDc[8] = {0, 0, -1, 1, -1, 1, -1, 1};

int dir_count(Connectivity connectivity) {
    return connectivity == Connectivity::Eight ? 8 : 4;
}

void reach_from_border(const Grid& grid, Connectivity connectivity, bool pacific,
                       std::vector<char>& reach) {
    const std::size_t n = grid.cells.size();
    reach.assign(n, 0);
    std::vector<std::uint32_t> queue;
    queue.reserve(n);

    for (int r = 0; r < grid.rows; ++r) {
        for (int c = 0; c < grid.cols; ++c) {
            const bool on_border = pacific ? (r == 0 || c == 0)
                                           : (r == grid.rows - 1 || c == grid.cols - 1);
            if (!on_border) continue;
            const std::uint32_t idx = static_cast<std::uint32_t>(grid.index(r, c));
            if (reach[idx]) continue;
            reach[idx] = 1;
            queue.push_back(idx);
        }
    }

    const int dirs = dir_count(connectivity);
    for (std::size_t head = 0; head < queue.size(); ++head) {
        const std::uint32_t idx = queue[head];
        const int r = static_cast<int>(idx) / grid.cols;
        const int c = static_cast<int>(idx) % grid.cols;
        for (int d = 0; d < dirs; ++d) {
            const int nr = r + kDr[d];
            const int nc = c + kDc[d];
            if (!grid.in_bounds(nr, nc)) continue;
            const std::uint32_t nidx = static_cast<std::uint32_t>(grid.index(nr, nc));
            if (reach[nidx]) continue;
            if (grid.at(nr, nc) < grid.at(r, c)) continue;
            reach[nidx] = 1;
            queue.push_back(nidx);
        }
    }
}

bool find_word(const Grid& grid, Connectivity connectivity, const std::string& word,
               Coord cur, std::size_t i, std::vector<char>& used) {
    if (grid.at(cur.row, cur.col) != word[i]) return false;
    if (i + 1 == word.size()) return true;

    used[grid.index(cur.row, cur.col)] = 1;
    const int dirs = dir_count(connectivity);
    for (int d = 0; d < dirs; ++d) {
        const int nr = cur.row + kDr[d];
        const int nc = cur.col + kDc[d];
        if (!grid.in_bounds(nr, nc)) continue;
        if (used[grid.index(nr, nc)]) continue;
        if (find_word(grid, connectivity, word, {nr, nc}, i + 1, used)) {
            used[grid.index(cur.row, cur.col)] = 0;
            return true;
        }
    }
    used[grid.index(cur.row, cur.col)] = 0;
    return false;
}

}  // namespace

long oracle_number_of_islands(const Grid& grid, Connectivity connectivity) {
    const std::size_t n = grid.cells.size();
    if (n == 0) return 0;

    std::vector<char> seen(n, 0);
    std::vector<std::uint32_t> stack;
    const int dirs = dir_count(connectivity);
    long islands = 0;

    for (int r = 0; r < grid.rows; ++r) {
        for (int c = 0; c < grid.cols; ++c) {
            const std::uint32_t src = static_cast<std::uint32_t>(grid.index(r, c));
            if (grid.at(r, c) == 0 || seen[src]) continue;

            ++islands;
            seen[src] = 1;
            stack.push_back(src);
            while (!stack.empty()) {
                const std::uint32_t idx = stack.back();
                stack.pop_back();
                const int cr = static_cast<int>(idx) / grid.cols;
                const int cc = static_cast<int>(idx) % grid.cols;
                for (int d = 0; d < dirs; ++d) {
                    const int nr = cr + kDr[d];
                    const int nc = cc + kDc[d];
                    if (!grid.in_bounds(nr, nc)) continue;
                    const std::uint32_t nidx = static_cast<std::uint32_t>(grid.index(nr, nc));
                    if (seen[nidx] || grid.at(nr, nc) == 0) continue;
                    seen[nidx] = 1;
                    stack.push_back(nidx);
                }
            }
        }
    }
    return islands;
}

long oracle_longest_increasing_path(const Grid& grid, Connectivity connectivity) {
    const std::size_t n = grid.cells.size();
    if (n == 0) return 0;

    std::vector<std::uint32_t> order(n);
    for (std::size_t i = 0; i < n; ++i) order[i] = static_cast<std::uint32_t>(i);
    std::sort(order.begin(), order.end(), [&](std::uint32_t a, std::uint32_t b) {
        return grid.cells[a] > grid.cells[b];
    });

    std::vector<int> best(n, 1);
    const int dirs = dir_count(connectivity);
    long longest = 0;

    for (const std::uint32_t idx : order) {
        const int r = static_cast<int>(idx) / grid.cols;
        const int c = static_cast<int>(idx) % grid.cols;
        for (int d = 0; d < dirs; ++d) {
            const int nr = r + kDr[d];
            const int nc = c + kDc[d];
            if (!grid.in_bounds(nr, nc)) continue;
            if (grid.at(nr, nc) <= grid.at(r, c)) continue;
            best[idx] = std::max(best[idx], 1 + best[grid.index(nr, nc)]);
        }
        longest = std::max(longest, static_cast<long>(best[idx]));
    }
    return longest;
}

long oracle_pacific_atlantic(const Grid& grid, Connectivity connectivity) {
    if (grid.cells.empty()) return 0;

    std::vector<char> pacific;
    std::vector<char> atlantic;
    reach_from_border(grid, connectivity, true, pacific);
    reach_from_border(grid, connectivity, false, atlantic);

    long count = 0;
    for (std::size_t i = 0; i < grid.cells.size(); ++i)
        if (pacific[i] && atlantic[i]) ++count;
    return count;
}

long oracle_word_search_ii(const Grid& grid, const std::vector<std::string>& words,
                           Connectivity connectivity) {
    if (grid.cells.empty()) return 0;

    std::vector<std::string> unique;
    for (const std::string& w : words) {
        if (w.empty()) continue;
        if (std::find(unique.begin(), unique.end(), w) == unique.end()) unique.push_back(w);
    }

    std::vector<char> used(grid.cells.size(), 0);
    long count = 0;
    for (const std::string& word : unique) {
        bool found = false;
        for (int r = 0; r < grid.rows && !found; ++r)
            for (int c = 0; c < grid.cols && !found; ++c)
                if (find_word(grid, connectivity, word, {r, c}, 0, used)) found = true;
        if (found) ++count;
    }
    return count;
}

}  // namespace dfs
