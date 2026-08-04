#include <string>
#include <vector>

#include "cases/generators.h"
#include "cases/oracle.h"
#include "cases/test_case.h"

namespace dfs {
namespace {

struct SizeStep {
    int side;
    Tier tier;
};

const SizeStep kLadder[] = {
    {64, Tier::Small},
    {256, Tier::Medium},
    {512, Tier::Large},
    {1024, Tier::Large},
    {2048, Tier::Large},
};

const std::uint64_t kSeeds[] = {1, 2, 3, 4, 5};

const char* const kWords[] = {"ab", "abc", "abcd", "dcba", "aaaa",
                              "abab", "dddd", "acdb", "bbbb", "cccc"};

std::vector<std::string> word_list() {
    std::vector<std::string> out;
    for (const char* w : kWords) out.push_back(w);
    return out;
}

std::string tag(const std::string& prefix, int side, std::uint64_t seed) {
    return prefix + "_" + std::to_string(side) + "_s" + std::to_string(seed);
}

CaseSpec make_spec(const std::string& name, const std::string& algorithm, Tier tier,
                   const GridSpec& grid, Connectivity connectivity) {
    CaseSpec s;
    s.name = name;
    s.algorithm = algorithm;
    s.tier = tier;
    s.connectivity = connectivity;
    s.generated = true;
    s.spec = grid;
    return s;
}

CaseSpec with_expected(CaseSpec s, long value) {
    s.expected = {value, true};
    return s;
}

void number_of_islands_specs(Tier max_tier, std::vector<CaseSpec>& out) {
    for (const SizeStep& step : kLadder) {
        if (step.tier > max_tier) continue;
        const int n = step.side;

        for (const std::uint64_t seed : kSeeds) {
            GridSpec g;
            g.topology = Topology::UniformRandom;
            g.rows = n;
            g.cols = n;
            g.density = 45;
            g.seed = seed;
            out.push_back(make_spec(tag("noi_rand", n, seed), "number_of_islands", step.tier,
                                    g, Connectivity::Four));
        }

        GridSpec checker;
        checker.topology = Topology::Checkerboard;
        checker.rows = n;
        checker.cols = n;
        out.push_back(with_expected(
            make_spec("noi_checker_" + std::to_string(n) + "_4c", "number_of_islands",
                      step.tier, checker, Connectivity::Four),
            (static_cast<long>(n) * n + 1) / 2));
        out.push_back(with_expected(
            make_spec("noi_checker_" + std::to_string(n) + "_8c", "number_of_islands",
                      step.tier, checker, Connectivity::Eight),
            1));

        GridSpec land;
        land.topology = Topology::AllLand;
        land.rows = n;
        land.cols = n;
        out.push_back(with_expected(
            make_spec("noi_land_" + std::to_string(n), "number_of_islands", step.tier, land,
                      Connectivity::Four),
            1));

        GridSpec snake;
        snake.topology = Topology::Serpentine;
        snake.rows = n;
        snake.cols = n;
        out.push_back(with_expected(
            make_spec("noi_snake_" + std::to_string(n), "number_of_islands", step.tier, snake,
                      Connectivity::Four),
            1));

        GridSpec stripes;
        stripes.topology = Topology::DiagonalStripes;
        stripes.rows = n;
        stripes.cols = n;
        out.push_back(make_spec("noi_stripes_" + std::to_string(n), "number_of_islands",
                                step.tier, stripes, Connectivity::Four));
    }
}

void pacific_atlantic_specs(Tier max_tier, std::vector<CaseSpec>& out) {
    for (const SizeStep& step : kLadder) {
        if (step.tier > max_tier) continue;
        for (const std::uint64_t seed : kSeeds) {
            GridSpec g;
            g.topology = Topology::RandomHeights;
            g.rows = step.side;
            g.cols = step.side;
            g.levels = 64;
            g.seed = seed;
            out.push_back(make_spec(tag("pa_rand", step.side, seed), "pacific_atlantic",
                                    step.tier, g, Connectivity::Four));
        }
    }
}

void longest_increasing_path_specs(Tier max_tier, std::vector<CaseSpec>& out) {
    for (const SizeStep& step : kLadder) {
        if (step.tier > max_tier) continue;
        const int n = step.side;

        for (const std::uint64_t seed : kSeeds) {
            GridSpec g;
            g.topology = Topology::RandomHeights;
            g.rows = n;
            g.cols = n;
            g.levels = 1024;
            g.seed = seed;
            out.push_back(make_spec(tag("lip_rand", n, seed), "longest_increasing_path",
                                    step.tier, g, Connectivity::Four));
        }

        GridSpec grad;
        grad.topology = Topology::Gradient;
        grad.rows = n;
        grad.cols = n;
        out.push_back(with_expected(
            make_spec("lip_grad_" + std::to_string(n), "longest_increasing_path", step.tier,
                      grad, Connectivity::Four),
            static_cast<long>(n) + n - 1));
    }
}

void word_search_ii_specs(Tier max_tier, std::vector<CaseSpec>& out) {
    for (const SizeStep& step : kLadder) {
        if (step.tier > max_tier || step.tier > Tier::Medium) continue;
        for (const std::uint64_t seed : kSeeds) {
            GridSpec g;
            g.topology = Topology::Letters;
            g.rows = step.side;
            g.cols = step.side;
            g.alphabet = 4;
            g.seed = seed;
            CaseSpec s = make_spec(tag("ws2_rand", step.side, seed), "word_search_ii",
                                   step.tier, g, Connectivity::Four);
            s.words = word_list();
            out.push_back(s);
        }
    }
}

}  // namespace

std::vector<CaseSpec> synthetic_specs(const std::string& algorithm, Tier max_tier) {
    std::vector<CaseSpec> out;
    if (max_tier == Tier::Legacy) return out;

    if (algorithm.empty() || algorithm == "number_of_islands")
        number_of_islands_specs(max_tier, out);
    if (algorithm.empty() || algorithm == "pacific_atlantic")
        pacific_atlantic_specs(max_tier, out);
    if (algorithm.empty() || algorithm == "longest_increasing_path")
        longest_increasing_path_specs(max_tier, out);
    if (algorithm.empty() || algorithm == "word_search_ii")
        word_search_ii_specs(max_tier, out);
    return out;
}

TestCase materialize(const CaseSpec& spec) {
    TestCase tc;
    tc.name = spec.name;
    tc.algorithm = spec.algorithm;
    tc.input.grid = spec.generated ? generate_grid(spec.spec) : spec.grid;
    tc.input.start = spec.start;
    tc.input.connectivity = spec.connectivity;
    tc.input.words = spec.words;
    tc.expected = spec.expected;

    if (tc.expected.valid) return tc;

    long value = 0;
    if (spec.algorithm == "number_of_islands")
        value = oracle_number_of_islands(tc.input.grid, tc.input.connectivity);
    else if (spec.algorithm == "longest_increasing_path")
        value = oracle_longest_increasing_path(tc.input.grid, tc.input.connectivity);
    else if (spec.algorithm == "pacific_atlantic")
        value = oracle_pacific_atlantic(tc.input.grid, tc.input.connectivity);
    else if (spec.algorithm == "word_search_ii")
        value = oracle_word_search_ii(tc.input.grid, tc.input.words, tc.input.connectivity);
    else
        return tc;

    tc.expected = {value, true};
    return tc;
}

}  // namespace dfs
