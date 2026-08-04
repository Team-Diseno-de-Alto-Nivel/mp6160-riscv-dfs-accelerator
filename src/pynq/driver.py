"""PYNQ driver for the DFS accelerator on the KV260 (issue #67).

STATUS: currently blocked on this lab's KV260 board -- PYNQ 3.x needs
`pyxrt` (XRT's Python bindings), which isn't available here (missing from
apt, not just unconfigured). The path that actually works today is
validate_cynq.cpp (C++, via CYNQ: https://github.com/ECASLab/cynq) -- see
README.md. This file is kept ready to use as-is if `pyxrt` ever becomes
available; the register-level logic below is identical to
validate_cynq.cpp's.

Talks to the REAL AXI register map that Vitis HLS generated for dfs_accel
(see src/hls/dfs_accel.cpp's #pragma HLS INTERFACE, and the auto-generated
xdfs_accel_hw.h under src/vivado/dfs_system/.../drivers/ once exported) --
this is NOT the same memory map as src/program/driver/accelerator_driver.h,
which is the TLM/SystemC simulation model's address layout (a different
target entirely, see that file's own header comment).

register_map field names below (rows, cols, mode, flags, num_words,
grid_in, words_in, result_out, CTRL.AP_START/AP_DONE) follow the standard
Vitis HLS AXI-Lite control-bundle naming convention that PYNQ's Overlay
auto-parses from the .hwh shipped next to the .bit. Only actually
confirmable once loaded on real hardware -- if a name doesn't match,
`ip.register_map` (printed) lists the real ones.

Only runs where `pynq` is installed, i.e. on the KV260 itself.
"""

from __future__ import annotations

import numpy as np
import pynq

# dfs_hls::kResultWords (src/hls/dfs_accel.h) -- result_out is always
# exactly this many uint32 words: value, expanded_nodes, visited_cells,
# peak_stack_depth (dfs_hls::ResultSlot), regardless of the case.
RESULT_WORDS = 4


class DfsAccelDriver:
    """Thin wrapper around the dfs_accel IP: one call in, one result out.

    Buffers are allocated fresh per run_case() call (sized exactly to that
    case's grid/words) rather than reused from a fixed max-size pool --
    simpler to reason about for the 21-case correctness check this exists
    for (#67), at the cost of a re-allocation per case that would matter
    for a throughput benchmark (that's #91, not this).
    """

    def __init__(self, overlay: "pynq.Overlay", ip_name: str = "dfs_accel_0"):
        self.ip = getattr(overlay, ip_name)

    def run_case(self, case: dict) -> dict:
        """Runs one case (a dict as loaded from src/pynq/cases.json) on the
        accelerator and returns the same fields the HLS cosim testbench
        (src/hls/tb/dfs_accel_tb.cpp) compares against software:
        value, expanded_nodes, visited_cells, peak_stack_depth.
        """
        grid = np.array(case["grid"], dtype=np.int8)
        words = np.array(case["words"], dtype=np.uint8)

        grid_buf = pynq.allocate(grid.shape, dtype=np.int8)
        words_buf = pynq.allocate(words.shape, dtype=np.uint8)
        result_buf = pynq.allocate((RESULT_WORDS,), dtype=np.uint32)
        try:
            grid_buf[:] = grid
            words_buf[:] = words
            # Buffers live in CPU cache until flushed -- without this the PL
            # can read stale (or garbage) DDR contents instead of what was
            # just written.
            grid_buf.flush()
            words_buf.flush()

            reg = self.ip.register_map
            # Pointer registers take the buffer's PHYSICAL address, not its
            # contents -- the accelerator reads/writes DDR itself over its
            # own AXI-master ports (m_axi gmem0/1/2), it doesn't receive
            # the data through these registers.
            reg.grid_in = grid_buf.physical_address
            reg.words_in = words_buf.physical_address
            reg.result_out = result_buf.physical_address
            reg.rows = case["rows"]
            reg.cols = case["cols"]
            reg.mode = case["mode"]
            reg.flags = case["flags"]
            reg.num_words = case["num_words"]

            reg.CTRL.AP_START = 1
            while not reg.CTRL.AP_DONE:
                pass

            # Mirror image of flush(): pull the PL's writes back into CPU-
            # visible memory before reading.
            result_buf.invalidate()
            return {
                "value": int(result_buf[0]),
                "expanded_nodes": int(result_buf[1]),
                "visited_cells": int(result_buf[2]),
                "peak_stack_depth": int(result_buf[3]),
            }
        finally:
            # Buffers come from a limited CMA memory pool -- release
            # explicitly rather than waiting on Python's GC, since this
            # runs once per case across 21 cases in the same session.
            grid_buf.freebuffer()
            words_buf.freebuffer()
            result_buf.freebuffer()
