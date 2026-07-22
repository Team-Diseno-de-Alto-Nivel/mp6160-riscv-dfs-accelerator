#pragma once

#include <memory>
#include <vector>

#include "dfs_types.h"
#include "harness/instrumentation.h"

namespace dfs {

enum class Mode { Off, On };

class Accelerator {
  public:
    Accelerator(Mode mode, Counters& counters) : mode_(mode), counters_(counters) {}
    virtual ~Accelerator() = default;

    Mode mode() const { return mode_; }
    Counters& counters() { return counters_; }
    void set_connectivity(Connectivity c) { connectivity_ = c; }

    virtual void load_grid(const Grid& grid) = 0;
    virtual void reset_visited() = 0;
    virtual bool is_visited(Coord c) const = 0;
    virtual void mark_visited(Coord c) = 0;
    virtual void unmark_visited(Coord c) = 0;
    virtual std::vector<Coord> neighbors(Coord c) const = 0;

  protected:
    Mode mode_;
    Counters& counters_;
    Connectivity connectivity_ = Connectivity::Four;
    const Grid* grid_ = nullptr;
};

std::unique_ptr<Accelerator> make_accelerator(Mode mode, Counters& counters);

}  // namespace dfs
