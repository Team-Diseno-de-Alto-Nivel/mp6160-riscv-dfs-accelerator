#pragma once
// MOCK stand-in for the SystemC DFS accelerator (src/model/modules/dfs_accelerator.h).
//
// This behavioral model lets the RISC-V-side integration run end-to-end over TLM
// today, while the real hardware modules are still skeletons. It exposes the two
// interface surfaces the real accelerator will provide:
//
//   * reg_socket  — the register/memory-map surface driven by the MMIO driver
//                   (src/program/driver/accelerator_driver.h). Autonomous
//                   whole-traversal offload (connected components on Start).
//   * prim_socket — the fine-grained DFS-primitive surface (visited memory +
//                   neighbour generation) driven by the harness Accelerator seam.
//
// TODO(swap-mock-reg): replace reg_socket behaviour with src/model
//   DfsAccelerator + ControlInterface/ResultInterface once HWA-3/HWB-3 land.
// TODO(swap-mock-prim): replace prim_socket behaviour with src/model
//   VisitedMemory (HWC-1) + NeighborGenerator (HWB-1) once they are implemented.

#include <systemc.h>
#include <tlm.h>
#include <tlm_utils/simple_target_socket.h>

#include <algorithm>
#include <cstdint>
#include <vector>

#include "config/memory_map.h"
#include "dfs_types.h"
#include "integration/accel_tlm_protocol.h"

namespace dfs::integration {

SC_MODULE(MockAcceleratorModel) {
    tlm_utils::simple_target_socket<MockAcceleratorModel> reg_socket;
    tlm_utils::simple_target_socket<MockAcceleratorModel> prim_socket;

    SC_CTOR(MockAcceleratorModel) : reg_socket("reg_socket"), prim_socket("prim_socket") {
        reg_socket.register_b_transport(this, &MockAcceleratorModel::reg_b_transport);
        prim_socket.register_b_transport(this, &MockAcceleratorModel::prim_b_transport);
    }

    void reg_b_transport(tlm::tlm_generic_payload& trans, sc_time& delay) {
        const std::uint64_t addr = trans.get_address();
        auto* data = reinterpret_cast<std::uint32_t*>(trans.get_data_ptr());
        delay += sc_time(10, SC_NS);

        if (trans.get_command() == tlm::TLM_WRITE_COMMAND) {
            reg_write(addr, *data);
        } else {
            *data = reg_read(addr);
        }
        trans.set_response_status(tlm::TLM_OK_RESPONSE);
    }

    void prim_b_transport(tlm::tlm_generic_payload& trans, sc_time& delay) {
        auto* msg = reinterpret_cast<PrimMessage*>(trans.get_data_ptr());
        delay += sc_time(10, SC_NS);
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

    // ── Register / memory-map surface (coarse whole-traversal offload) ──────────
    std::uint32_t control_ = 0;
    std::uint32_t status_ = 0;
    std::uint32_t rows_ = 0;
    std::uint32_t cols_ = 0;
    std::uint32_t start_x_ = 0;
    std::uint32_t start_y_ = 0;
    std::uint32_t params_ = 0;
    std::uint32_t result_ = 0;
    std::uint32_t visited_count_ = 0;
    std::uint32_t peak_stack_ = 0;
    std::vector<std::uint32_t> grid_cells_;

    // ── Primitive surface (fine-grained visited memory + neighbour gen) ─────────
    const Grid* prim_grid_ = nullptr;
    Connectivity prim_conn_ = Connectivity::Four;
    std::vector<char> prim_visited_;

    void reg_write(std::uint64_t addr, std::uint32_t value) {
        switch (addr) {
            case memmap::kRegControl:
                control_ = value;
                if (value & memmap::kControlStart) run_traversal();
                return;
            case memmap::kRegRows: rows_ = value; return;
            case memmap::kRegCols: cols_ = value; return;
            case memmap::kRegStartX: start_x_ = value; return;
            case memmap::kRegStartY: start_y_ = value; return;
            case memmap::kRegParams: params_ = value; return;
            default: break;
        }
        if (addr >= memmap::kGridBase) {
            const std::size_t i = (addr - memmap::kGridBase) / sizeof(std::uint32_t);
            if (i >= grid_cells_.size()) grid_cells_.resize(i + 1, 0);
            grid_cells_[i] = value;
        }
    }

    std::uint32_t reg_read(std::uint64_t addr) const {
        switch (addr) {
            case memmap::kRegControl: return control_;
            case memmap::kRegStatus: return status_;
            case memmap::kRegResult: return result_;
            case memmap::kRegVisited: return visited_count_;
            case memmap::kRegPeakStack: return peak_stack_;
            default: return 0;
        }
    }

    // Behavioural connected-components traversal (the "one algorithm" of INT-1).
    void run_traversal() {
        status_ = memmap::kStatusBusy;
        const int rows = static_cast<int>(rows_);
        const int cols = static_cast<int>(cols_);
        const int dirs = params_ == 8 ? 8 : 4;

        std::vector<char> visited(static_cast<std::size_t>(rows) * cols, 0);
        auto cell = [&](int r, int c) -> std::uint32_t {
            return grid_cells_[static_cast<std::size_t>(r) * cols + c];
        };

        std::uint32_t islands = 0;
        std::uint32_t total_visited = 0;
        std::uint32_t peak = 0;
        for (int r = 0; r < rows; ++r) {
            for (int c = 0; c < cols; ++c) {
                if (cell(r, c) == 0 || visited[r * cols + c]) continue;
                ++islands;
                std::vector<Coord> stack{{r, c}};
                visited[r * cols + c] = 1;
                ++total_visited;
                while (!stack.empty()) {
                    if (stack.size() > peak) peak = static_cast<std::uint32_t>(stack.size());
                    const Coord cur = stack.back();
                    stack.pop_back();
                    for (int k = 0; k < dirs; ++k) {
                        const int nr = cur.row + kDr[k];
                        const int nc = cur.col + kDc[k];
                        if (nr < 0 || nr >= rows || nc < 0 || nc >= cols) continue;
                        if (cell(nr, nc) == 0 || visited[nr * cols + nc]) continue;
                        visited[nr * cols + nc] = 1;
                        ++total_visited;
                        stack.push_back({nr, nc});
                    }
                }
            }
        }

        result_ = islands;
        visited_count_ = total_visited;
        peak_stack_ = peak;
        status_ = memmap::kStatusDone;
    }

    // ── Primitive surface behaviour ─────────────────────────────────────────────
    void run_primitive(PrimMessage & msg) {
        switch (msg.op) {
            case PrimOp::LoadGrid:
                prim_grid_ = msg.grid;
                prim_conn_ = msg.connectivity;
                prim_visited_.assign(
                    static_cast<std::size_t>(prim_grid_->rows) * prim_grid_->cols, 0);
                return;
            case PrimOp::ResetVisited:
                std::fill(prim_visited_.begin(), prim_visited_.end(), 0);
                return;
            case PrimOp::IsVisited:
                msg.visited_out = prim_visited_[index(msg.cell)] != 0;
                return;
            case PrimOp::MarkVisited: {
                const int i = index(msg.cell);
                msg.newly_marked_out = prim_visited_[i] == 0;
                prim_visited_[i] = 1;
                return;
            }
            case PrimOp::UnmarkVisited:
                prim_visited_[index(msg.cell)] = 0;
                return;
            case PrimOp::Neighbors: {
                const int count = (prim_conn_ == Connectivity::Eight) ? 8 : 4;
                int n = 0;
                for (int k = 0; k < count; ++k) {
                    const int nr = msg.cell.row + kDr[k];
                    const int nc = msg.cell.col + kDc[k];
                    if (prim_grid_->in_bounds(nr, nc)) msg.neighbors_out[n++] = {nr, nc};
                }
                msg.neighbor_count_out = n;
                return;
            }
        }
    }

    int index(Coord c) const { return prim_grid_->index(c.row, c.col); }
};

}  // namespace dfs::integration
