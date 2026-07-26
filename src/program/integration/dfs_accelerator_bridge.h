#pragma once
// Bridges the real hardware model (src/model) onto the two TLM surfaces the
// integration harness expects:
//
//   reg_socket  — whole-traversal register/MMIO, forwarded straight to
//                 DfsAccelerator's own socket, unchanged.
//   prim_socket — fine-grained primitives (is_visited/mark_visited/
//                 neighbours) for the four algorithms that skip
//                 DfsController entirely — see dfs_controller.h.
//
// prim_socket skips the cycle-accurate FSM/TLM decode and reaches straight
// into GridMemory/VisitedMemory's backdoor accessors, one call per
// primitive with no clock stepping — that's how the primitive interface
// itself works (see TlmAccelerator::send()).
//
// This file includes headers from both src/model and src/program, which
// each define a different dfs::Coord — that's why the model side is called
// CellCoord (see utils/types.h). Don't reintroduce the collision.

#include <systemc.h>
#include <tlm.h>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/simple_target_socket.h>

#include <cstddef>
#include <cstdint>

#include "integration/accel_tlm_protocol.h"
#include "modules/dfs_accelerator.h"

namespace dfs::integration {

SC_MODULE(RealAcceleratorBridge) {
    tlm_utils::simple_target_socket<RealAcceleratorBridge> reg_socket;
    tlm_utils::simple_target_socket<RealAcceleratorBridge> prim_socket;

    dfs::DfsAccelerator accel;
    sc_core::sc_clock clk;
    sc_core::sc_signal<bool> rst_n;
    tlm_utils::simple_initiator_socket<RealAcceleratorBridge> accel_init;

    SC_CTOR(RealAcceleratorBridge)
        : reg_socket("reg_socket"),
          prim_socket("prim_socket"),
          accel("accel"),
          clk("clk", sc_core::sc_time(10, sc_core::SC_NS)),
          accel_init("accel_init") {
        accel.clk(clk);
        accel.rst_n(rst_n);
        // Released once at elaboration and never toggled again — per-run
        // state gets cleared via LoadGrid/ResetVisited and DfsController's
        // own self-clearing Start, not via this pin.
        rst_n.write(true);
        accel_init.bind(accel.socket);

        reg_socket.register_b_transport(this, &RealAcceleratorBridge::reg_b_transport);
        prim_socket.register_b_transport(this, &RealAcceleratorBridge::prim_b_transport);
    }

    void reg_b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay) {
        accel_init->b_transport(trans, delay);
    }

    void prim_b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay) {
        auto* msg = reinterpret_cast<PrimMessage*>(trans.get_data_ptr());
        delay += sc_core::sc_time(10, sc_core::SC_NS);
        if (msg == nullptr) {
            trans.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
            return;
        }
        run_primitive(*msg);
        trans.set_response_status(tlm::TLM_OK_RESPONSE);
    }

  private:
    static constexpr int kDr[8] = {-1, 1, 0, 0, -1, -1, 1, 1};
    static constexpr int kDc[8] = {0, 0, -1, 1, -1, 1, -1, 1};

    const Grid* grid_ = nullptr;
    Connectivity conn_ = Connectivity::Four;

    std::uint32_t index(Coord c) const {
        return static_cast<std::uint32_t>(grid_->index(c.row, c.col));
    }

    void run_primitive(PrimMessage& msg) {
        switch (msg.op) {
            case PrimOp::LoadGrid:
                grid_ = msg.grid;
                conn_ = msg.connectivity;
                for (std::size_t i = 0; i < grid_->cells.size(); ++i) {
                    accel.engine.grid_mem.load_cell(static_cast<std::uint32_t>(i), grid_->cells[i]);
                }
                accel.engine.visited_mem.clear_all();
                return;
            case PrimOp::ResetVisited:
                accel.engine.visited_mem.clear_all();
                return;
            case PrimOp::IsVisited:
                msg.visited_out = accel.engine.visited_mem.peek(index(msg.cell));
                return;
            case PrimOp::MarkVisited: {
                const std::uint32_t i = index(msg.cell);
                msg.newly_marked_out = !accel.engine.visited_mem.peek(i);
                accel.engine.visited_mem.poke(i, true);
                return;
            }
            case PrimOp::UnmarkVisited:
                accel.engine.visited_mem.poke(index(msg.cell), false);
                return;
            case PrimOp::Neighbors: {
                const int count = (conn_ == Connectivity::Eight) ? 8 : 4;
                int n = 0;
                for (int k = 0; k < count; ++k) {
                    const int nr = msg.cell.row + kDr[k];
                    const int nc = msg.cell.col + kDc[k];
                    if (grid_->in_bounds(nr, nc)) msg.neighbors_out[n++] = {nr, nc};
                }
                msg.neighbor_count_out = n;
                return;
            }
        }
    }
};

}  // namespace dfs::integration
