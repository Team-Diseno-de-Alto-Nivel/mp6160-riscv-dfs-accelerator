#include "cases/test_case.h"

#include <string>
#include <vector>

namespace dfs {
namespace {

Grid make_grid(const std::vector<std::vector<int>>& rows) {
    Grid g;
    g.rows = static_cast<int>(rows.size());
    g.cols = rows.empty() ? 0 : static_cast<int>(rows[0].size());
    for (const auto& row : rows)
        for (int v : row) g.cells.push_back(v);
    return g;
}

Grid make_letters(const std::vector<std::string>& rows) {
    Grid g;
    g.rows = static_cast<int>(rows.size());
    g.cols = rows.empty() ? 0 : static_cast<int>(rows[0].size());
    for (const auto& row : rows)
        for (char ch : row) g.cells.push_back(static_cast<int>(ch));
    return g;
}

TestCase expect(TestCase tc, long value) {
    tc.expected = {value, true};
    return tc;
}

std::vector<TestCase> number_of_islands_cases() {
    std::vector<TestCase> cs;
    const std::vector<std::vector<int>> classic = {
        {1, 1, 0, 0, 0},
        {1, 1, 0, 0, 0},
        {0, 0, 1, 0, 0},
        {0, 0, 0, 1, 1},
    };
    const std::vector<std::vector<int>> checker = {
        {1, 0, 1, 0},
        {0, 1, 0, 1},
        {1, 0, 1, 0},
        {0, 1, 0, 1},
    };
    cs.push_back(expect({"noi_classic_4c", "number_of_islands",
                         {make_grid(classic), {0, 0}, Connectivity::Four, {}}, {}}, 3));
    cs.push_back(expect({"noi_classic_8c", "number_of_islands",
                         {make_grid(classic), {0, 0}, Connectivity::Eight, {}}, {}}, 1));
    cs.push_back(expect({"noi_checker_4c", "number_of_islands",
                         {make_grid(checker), {0, 0}, Connectivity::Four, {}}, {}}, 8));
    cs.push_back(expect({"noi_checker_8c", "number_of_islands",
                         {make_grid(checker), {0, 0}, Connectivity::Eight, {}}, {}}, 1));
    cs.push_back(expect({"noi_all_water", "number_of_islands",
                         {make_grid({{0, 0}, {0, 0}}), {0, 0}, Connectivity::Four, {}}, {}}, 0));
    cs.push_back(expect({"noi_all_land_4c", "number_of_islands",
                         {make_grid({{1, 1, 1}, {1, 1, 1}, {1, 1, 1}}), {0, 0},
                          Connectivity::Four, {}}, {}}, 1));
    return cs;
}

std::vector<TestCase> unique_paths_iii_cases() {
    std::vector<TestCase> cs;
    cs.push_back(expect({"up3_one_obstacle", "unique_paths_iii",
                         {make_grid({{1, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 2, -1}}),
                          {0, 0}, Connectivity::Four, {}}, {}}, 2));
    cs.push_back(expect({"up3_open", "unique_paths_iii",
                         {make_grid({{1, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 2}}),
                          {0, 0}, Connectivity::Four, {}}, {}}, 4));
    cs.push_back(expect({"up3_no_path", "unique_paths_iii",
                         {make_grid({{0, 1}, {2, 0}}), {0, 1}, Connectivity::Four, {}}, {}}, 0));
    return cs;
}

std::vector<TestCase> word_search_ii_cases() {
    std::vector<TestCase> cs;
    cs.push_back(expect({"ws2_classic", "word_search_ii",
                         {make_letters({"oaan", "etae", "ihkr", "iflv"}), {0, 0},
                          Connectivity::Four, {"oath", "pea", "eat", "rain"}}, {}}, 2));
    cs.push_back(expect({"ws2_small", "word_search_ii",
                         {make_letters({"ab", "cd"}), {0, 0}, Connectivity::Four,
                          {"ab", "ac", "abdc", "acdb", "xyz"}}, {}}, 4));
    return cs;
}

std::vector<TestCase> longest_increasing_path_cases() {
    std::vector<TestCase> cs;
    cs.push_back(expect({"lip_desc", "longest_increasing_path",
                         {make_grid({{9, 9, 4}, {6, 6, 8}, {2, 1, 1}}), {0, 0},
                          Connectivity::Four, {}}, {}}, 4));
    cs.push_back(expect({"lip_zigzag", "longest_increasing_path",
                         {make_grid({{3, 4, 5}, {3, 2, 6}, {2, 2, 1}}), {0, 0},
                          Connectivity::Four, {}}, {}}, 4));
    cs.push_back(expect({"lip_single", "longest_increasing_path",
                         {make_grid({{1}}), {0, 0}, Connectivity::Four, {}}, {}}, 1));
    return cs;
}

std::vector<TestCase> pacific_atlantic_cases() {
    std::vector<TestCase> cs;
    const std::vector<std::vector<int>> heights = {
        {1, 2, 2, 3, 5},
        {3, 2, 3, 4, 4},
        {2, 4, 5, 3, 1},
        {6, 7, 1, 4, 5},
        {5, 1, 1, 2, 4},
    };
    cs.push_back(expect({"pa_classic", "pacific_atlantic",
                         {make_grid(heights), {0, 0}, Connectivity::Four, {}}, {}}, 7));
    cs.push_back(expect({"pa_single", "pacific_atlantic",
                         {make_grid({{1}}), {0, 0}, Connectivity::Four, {}}, {}}, 1));
    return cs;
}

void append(std::vector<TestCase>& dst, std::vector<TestCase> src) {
    for (auto& c : src) dst.push_back(std::move(c));
}

}  // namespace

std::vector<TestCase> CaseLoader::load(const std::string& algorithm) const {
    std::vector<TestCase> all;
    if (algorithm.empty() || algorithm == "number_of_islands")
        append(all, number_of_islands_cases());
    if (algorithm.empty() || algorithm == "unique_paths_iii")
        append(all, unique_paths_iii_cases());
    if (algorithm.empty() || algorithm == "word_search_ii")
        append(all, word_search_ii_cases());
    if (algorithm.empty() || algorithm == "longest_increasing_path")
        append(all, longest_increasing_path_cases());
    if (algorithm.empty() || algorithm == "pacific_atlantic")
        append(all, pacific_atlantic_cases());
    return all;
}

}  // namespace dfs
