#pragma once
// Grid Memory (HWC-1): read-only input grid, loaded by the host.

#include <systemc.h>

#include "config/accelerator_config.h"
#include "utils/types.h"

namespace dfs {

SC_MODULE(GridMemory) {
    sc_in<bool> clk;

    // Write port (host data-loading).
    sc_in<bool> we;
    sc_in<sc_uint<32>> waddr;
    sc_in<sc_int<32>> wdata;

    // Read port (traversal).
    sc_in<sc_uint<32>> raddr;
    sc_out<sc_int<32>> rdata;

    SC_CTOR(GridMemory) {}  // TODO(HWC-1): register access()

    void access();  // TODO(HWC-1)

  private:
    CellValue cells_[config::kMaxCells] = {};
};

}  // namespace dfs
