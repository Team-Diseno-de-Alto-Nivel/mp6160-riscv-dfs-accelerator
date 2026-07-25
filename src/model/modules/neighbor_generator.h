#pragma once
// Neighbor Generator (HWB-1): emits the 4/8 neighbours of a cell with boundary
// handling.
//
// Protocol (see the peer-contract note atop dfs_controller.h): valid_in is a
// request/ack pulse, not a one-shot "start" — the caller must pulse it once
// per candidate it wants, including the first, holding cur_x/cur_y steady
// for the whole sequence. Each pulse answers with at most one valid_out
// pulse (nbr_x/nbr_y valid that same cycle); once every direction has been
// produced or rejected as out-of-bounds, a further pulse raises done instead
// (held until the next request for a *new* node resets it). This module
// never advances on its own — it always waits for the next request — so a
// slower consumer can never see a candidate overwritten before reading it.
//
// connectivity selects 4 vs 8 neighbours: any value other than 8 is treated
// as 4. It's exactly what the host writes to kRegParams (see
// dfs_accelerator.h), matching src/program/dfs_types.h's Connectivity enum
// values (Four=4, Eight=8). It's latched alongside cur_x/cur_y when a scan
// (re)starts, so it can't change mid-scan even if the host writes a new
// value while a traversal is still running.

#include <cstdint>

#include <systemc.h>

#include "utils/types.h"

namespace dfs {

SC_MODULE(NeighborGenerator) {
    sc_in<bool> clk;
    sc_in<bool> rst_n;
    sc_in<bool> valid_in;
    sc_in<sc_uint<32>> cur_x;
    sc_in<sc_uint<32>> cur_y;
    sc_in<sc_uint<32>> rows;
    sc_in<sc_uint<32>> cols;
    sc_in<sc_uint<32>> connectivity;

    sc_out<bool> valid_out;
    sc_out<bool> done;
    sc_out<sc_uint<32>> nbr_x;
    sc_out<sc_uint<32>> nbr_y;

    SC_CTOR(NeighborGenerator) {
        SC_METHOD(generate);
        sensitive << clk.pos();
        dont_initialize();
    }

    void generate() {
        if (!rst_n.read()) {
            active_ = false;
            next_dir_ = 0;
            num_dirs_ = 4;
            valid_out.write(false);
            done.write(false);
            nbr_x.write(0);
            nbr_y.write(0);
            return;
        }

        valid_out.write(false);  // single-cycle pulse unless re-asserted below

        if (!valid_in.read()) return;  // no request this cycle: hold state

        if (!active_) {
            // First request for a new node: (re)start the scan.
            active_ = true;
            next_dir_ = 0;
            base_x_ = cur_x.read();
            base_y_ = cur_y.read();
            num_dirs_ = (connectivity.read() == 8) ? 8 : 4;
        }
        done.write(false);
        emit_next();
    }

  private:
    // Orthogonal (x-1,y), (x+1,y), (x,y-1), (x,y+1) plus, for 8-connectivity,
    // the four diagonals.
    static constexpr int kDx[8] = {-1, 1, 0, 0, -1, -1, 1, 1};
    static constexpr int kDy[8] = {0, 0, -1, 1, -1, 1, -1, 1};

    void emit_next() {
        while (next_dir_ < num_dirs_) {
            const int dir = next_dir_++;
            const std::int64_t nx = static_cast<std::int64_t>(base_x_) + kDx[dir];
            const std::int64_t ny = static_cast<std::int64_t>(base_y_) + kDy[dir];
            if (nx < 0 || ny < 0 || nx >= static_cast<std::int64_t>(cols.read()) ||
                ny >= static_cast<std::int64_t>(rows.read())) {
                continue;  // out of bounds: skip within this same request, no extra cycle
            }
            nbr_x.write(static_cast<std::uint32_t>(nx));
            nbr_y.write(static_cast<std::uint32_t>(ny));
            valid_out.write(true);
            return;
        }
        active_ = false;
        done.write(true);
    }

    bool active_ = false;
    int next_dir_ = 0;
    int num_dirs_ = 4;
    sc_uint<32> base_x_ = 0;
    sc_uint<32> base_y_ = 0;
};

}  // namespace dfs
