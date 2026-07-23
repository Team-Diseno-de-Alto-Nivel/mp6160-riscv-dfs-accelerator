#pragma once
// TlmAccelerator implements the harness Accelerator seam
// (src/program/harness/accelerator.h) by mapping every DFS primitive
// (is_visited / mark_visited / neighbours / ...) onto a TLM 2.0 transaction against
// the accelerator's primitive surface. This is the fine-grained ON path used by
// INT-1 per-algorithm integration (#38-#41): the five algorithms run unchanged,
// their DFS primitives now travelling over TLM to the (mock) model.

#include <cstdint>
#include <vector>

#include "harness/accelerator.h"
#include "integration/accel_tlm_protocol.h"
#include "integration/tlm_bus.h"

namespace dfs::integration {

class TlmAccelerator final : public Accelerator {
  public:
    TlmAccelerator(Mode mode, Counters& counters, TransportFn transport)
        : Accelerator(mode, counters), transport_(std::move(transport)) {}

    void load_grid(const Grid& grid) override {
        grid_ = &grid;
        PrimMessage msg;
        msg.op = PrimOp::LoadGrid;
        msg.grid = &grid;
        msg.connectivity = connectivity_;
        send(msg);
    }

    void reset_visited() override {
        PrimMessage msg;
        msg.op = PrimOp::ResetVisited;
        send(msg);
    }

    bool is_visited(Coord c) const override {
        ++counters_.ops;
        PrimMessage msg;
        msg.op = PrimOp::IsVisited;
        msg.cell = c;
        send(msg);
        return msg.visited_out;
    }

    void mark_visited(Coord c) override {
        ++counters_.ops;
        PrimMessage msg;
        msg.op = PrimOp::MarkVisited;
        msg.cell = c;
        send(msg);
        if (msg.newly_marked_out) ++counters_.visited_cells;
    }

    void unmark_visited(Coord c) override {
        ++counters_.ops;
        PrimMessage msg;
        msg.op = PrimOp::UnmarkVisited;
        msg.cell = c;
        send(msg);
    }

    std::vector<Coord> neighbors(Coord c) const override {
        ++counters_.ops;
        PrimMessage msg;
        msg.op = PrimOp::Neighbors;
        msg.cell = c;
        send(msg);
        return std::vector<Coord>(msg.neighbors_out, msg.neighbors_out + msg.neighbor_count_out);
    }

  private:
    void send(PrimMessage& msg) const {
        tlm::tlm_generic_payload trans;
        sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
        trans.set_command(tlm::TLM_IGNORE_COMMAND);
        trans.set_address(0);
        trans.set_data_ptr(reinterpret_cast<unsigned char*>(&msg));
        trans.set_data_length(sizeof(PrimMessage));
        trans.set_streaming_width(sizeof(PrimMessage));
        trans.set_byte_enable_ptr(nullptr);
        trans.set_dmi_allowed(false);
        trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        transport_(trans, delay);
    }

    TransportFn transport_;
};

}  // namespace dfs::integration
