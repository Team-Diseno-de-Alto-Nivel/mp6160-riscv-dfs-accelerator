#pragma once
// DFS Accelerator top-level (HWA-1): single TLM 2.0 target socket, decodes host
// transactions against the memory map, and wires the internal modules together.

#include <systemc.h>
#include <tlm.h>
#include <tlm_utils/simple_target_socket.h>

#include "config/tlm_protocol.h"
#include "modules/dfs_controller.h"
#include "modules/grid_memory.h"
#include "modules/neighbor_generator.h"
#include "modules/result_interface.h"
#include "modules/stack_manager.h"
#include "modules/visited_memory.h"
#include "utils/types.h"

namespace dfs {

SC_MODULE(DfsAccelerator) {
    tlm_utils::simple_target_socket<DfsAccelerator> socket;

    sc_in<bool> clk;
    sc_in<bool> rst_n;

    DfsController controller;
    NeighborGenerator neighbor_gen;
    StackManager stack_mgr;
    GridMemory grid_mem;
    VisitedMemory visited_mem;
    ResultInterface result_if;

    SC_CTOR(DfsAccelerator)
        : socket("socket"),
          controller("controller"),
          neighbor_gen("neighbor_gen"),
          stack_mgr("stack_mgr"),
          grid_mem("grid_mem"),
          visited_mem("visited_mem"),
          result_if("result_if") {
        // TODO(HWA-1): socket.register_b_transport(this, &DfsAccelerator::b_transport)
        //              and interconnect the modules via internal sc_signals.
    }

    void b_transport(tlm::tlm_generic_payload& trans, sc_time& delay);  // TODO(HWA-1)

  private:
    bool enabled_ = false;  // ON/OFF toggle (HWA-3)
};

}  // namespace dfs
