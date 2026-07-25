#pragma once
// Neighbor Generator (HWB-1): emits the 4/8 neighbours of a cell with boundary
// handling.
//
// valid_in is a request/ack pulse, not a one-shot start — one pulse per
// candidate wanted (including the first), cur_x/cur_y held steady for the
// whole sequence. Each pulse answers with at most one valid_out (or done
// once exhausted). Never advances on its own, so a slower consumer can't
// miss a candidate to an overwrite.
//
// connectivity is 4 or 8 (anything else treated as 4), matching what the
// host writes to kRegParams. Latched at scan start so it can't change
// mid-scan.

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
