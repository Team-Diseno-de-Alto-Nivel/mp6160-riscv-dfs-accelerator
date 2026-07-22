#pragma once
// Visited Memory (HWC-1): per-cell visited state, cleared at run start. Some
// algorithms need more than one instance (e.g. Pacific Atlantic).

#include <systemc.h>

#include "config/accelerator_config.h"

namespace dfs {

SC_MODULE(VisitedMemory) {
    sc_in<bool> clk;
    sc_in<bool> clear;

    sc_in<bool> we;
    sc_in<sc_uint<32>> addr;
    sc_in<bool> wdata;   // mark visited
    sc_out<bool> rdata;  // is-visited

    SC_CTOR(VisitedMemory) {}  // TODO(HWC-1): register access()

    void access();  // TODO(HWC-1)

  private:
    bool visited_[config::kMaxCells] = {};
};

}  // namespace dfs
