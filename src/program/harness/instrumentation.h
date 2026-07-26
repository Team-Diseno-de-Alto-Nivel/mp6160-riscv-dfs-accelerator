#pragma once

#include <cstdint>
#include <vector>

#include "dfs_types.h"

namespace dfs {

struct Counters
{
    uint64_t pop_cycles = 0;
    uint64_t visited_cycles = 0;
    uint64_t mark_cycles = 0;
    uint64_t neighbor_cycles = 0;
    uint64_t push_cycles = 0;
    uint64_t expanded_nodes = 0;
    uint64_t visited_cells = 0;
    uint64_t peak_stack_depth = 0;
};

struct CostModel {
    double clock_period_ns = 10.0;
    double cycles_per_op = 1.0;

    double latency_ns(const Counters& c) const {
        return total_cycles(c)*clock_period_ns;
    }

    uint64_t total_cycles(const Counters& c) const
    {
    return
        c.pop_cycles +
        c.visited_cycles +
        c.mark_cycles +
        c.neighbor_cycles +
        c.push_cycles;
    }
};

class CountedStack {
  public:
    explicit CountedStack(Counters& counters) : counters_(counters) {}

    void push(Coord c) {
        data_.push_back(c);
        ++counters_.push_cycles;
        if (data_.size() > counters_.peak_stack_depth) {
            counters_.peak_stack_depth = data_.size();
        }
    }

    Coord pop() {
        Coord c = data_.back();
        data_.pop_back();
        ++counters_.pop_cycles;
        return c;
    }

    bool empty() const { return data_.empty(); }
    std::size_t size() const { return data_.size(); }

  private:
    Counters& counters_;
    std::vector<Coord> data_;
};

}  // namespace dfs
