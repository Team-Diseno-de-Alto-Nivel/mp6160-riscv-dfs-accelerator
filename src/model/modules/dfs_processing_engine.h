#pragma once
// DFS Processing Engine (HWA-2b): per-node datapath, grouped into its own
// hierarchical module so it can be targeted as a separate synthesis region
// from the control FSM (DfsController/HWA-2, #22) once the model moves to
// Vitis HLS (#62). Resolves issue #44.
//
// This module makes NO changes to DfsController's FSM or to any of its
// port contracts (see the peer-contract note atop dfs_controller.h) — it
// only relocates NeighborGenerator (HWB-1), StackManager (HWB-2), and
// GridMemory/VisitedMemory (HWC-1) one level down as public sub-modules.
// DfsAccelerator (HWA-1) wires DfsController's ports to these sub-modules
// exactly as it did before #44 — only the path changes (e.g.
// engine.neighbor_gen.valid_in(...) instead of neighbor_gen.valid_in(...)) —
// and still reaches GridMemory's backdoor load_cell()/peek() and
// VisitedMemory's backdoor peek()/poke()/clear_all() (used by the
// fine-grained primitive TLM bridge for the other four algorithms) through
// engine.grid_mem / engine.visited_mem.
//
// clk/rst_n fan-out to the sub-modules that need them happens inside this
// module's constructor, so DfsAccelerator only has to bind them once, to
// `engine`, instead of once per sub-module. GridMemory and VisitedMemory
// have no rst_n port (see their own files), so only clk is wired for those
// two here — unchanged from how DfsAccelerator wired them directly before.
// clear (StackManager/VisitedMemory) and every data/control port stay
// unbound here, wired by DfsAccelerator exactly as before, just one level
// deeper.

#include <systemc.h>

#include "modules/grid_memory.h"
#include "modules/neighbor_generator.h"
#include "modules/stack_manager.h"
#include "modules/visited_memory.h"

namespace dfs {

SC_MODULE(DfsProcessingEngine) {
    sc_in<bool> clk;
    sc_in<bool> rst_n;

    NeighborGenerator neighbor_gen;
    StackManager stack_mgr;
    GridMemory grid_mem;
    VisitedMemory visited_mem;

    SC_CTOR(DfsProcessingEngine)
        : neighbor_gen("neighbor_gen"),
          stack_mgr("stack_mgr"),
          grid_mem("grid_mem"),
          visited_mem("visited_mem") {
        neighbor_gen.clk(clk);
        neighbor_gen.rst_n(rst_n);
        stack_mgr.clk(clk);
        stack_mgr.rst_n(rst_n);
        grid_mem.clk(clk);
        visited_mem.clk(clk);
    }
};

}  // namespace dfs
