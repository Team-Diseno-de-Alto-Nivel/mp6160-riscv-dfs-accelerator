#pragma once

#include <cstdint>
#include <vector>

#include "dfs_types.h"

namespace dfs {

struct Counters {
    std::uint64_t ops = 0;
    std::uint64_t expanded_nodes = 0;
    std::uint64_t visited_cells = 0;
    std::uint64_t peak_stack_depth = 0;
};

struct CostModel {
    double clock_period_ns = 10.0;
    double cycles_per_op = 1.0;

    double latency_ns(const Counters& c) const {
        return static_cast<double>(c.ops) * cycles_per_op * clock_period_ns;
    }
};

class CountedStack {
  public:
    explicit CountedStack(Counters& counters) : counters_(counters) {}

    void push(Coord c) {
        data_.push_back(c);
        ++counters_.ops;
        if (data_.size() > counters_.peak_stack_depth) {
            counters_.peak_stack_depth = data_.size();
        }
    }

    Coord pop() {
        Coord c = data_.back();
        data_.pop_back();
        ++counters_.ops;
        return c;
    }

    bool empty() const { return data_.empty(); }
    std::size_t size() const { return data_.size(); }

  private:
    Counters& counters_;
    std::vector<Coord> data_;
};

}  // namespace dfs
