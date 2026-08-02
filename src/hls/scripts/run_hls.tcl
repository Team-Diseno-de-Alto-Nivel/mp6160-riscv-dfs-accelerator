# Vitis HLS batch flow for dfs_accel (issue #63).
# Run: vitis_hls -f src/hls/scripts/run_hls.tcl
# Board: Kria KV260, part xck26-sfvc784-2LV-c, 250 MHz (4 ns).


# Paths from this script's own location, so it works from anywhere.
set script_dir  [file dirname [file normalize [info script]]]
set repo_root   [file normalize [file join $script_dir .. .. ..]]
set hls_dir     [file join $repo_root src hls]
set program_dir [file join $repo_root src program]

open_project -reset dfs_accel_prj
set_top dfs_accel


add_files [file join $hls_dir dfs_accel.cpp] \
    -cflags "-std=c++14 -I$hls_dir"

# Testbench + the src/program files it depends on.
# Same list as HLS_TB_SOURCES in the root Makefile (hls-host target).
set tb_sources [list \
    [file join $hls_dir tb dfs_accel_tb.cpp] \
    [file join $program_dir cases datasets.cpp] \
    [file join $program_dir harness accelerator.cpp] \
    [file join $program_dir algorithms dfs_algorithm.cpp] \
    [file join $program_dir algorithms number_of_islands number_of_islands.cpp] \
    [file join $program_dir algorithms unique_paths_iii unique_paths_iii.cpp] \
    [file join $program_dir algorithms word_search_ii word_search_ii.cpp] \
    [file join $program_dir algorithms longest_increasing_path longest_increasing_path.cpp] \
    [file join $program_dir algorithms pacific_atlantic pacific_atlantic.cpp] \
]
foreach src $tb_sources {
    add_files -tb $src -cflags "-std=c++14 -I$hls_dir -I$program_dir"
}

open_solution -reset "solution1" -flow_target vivado
set_part {xck26-sfvc784-2LV-c}

create_clock -period 4 -name default
;# 4 ns = 250 MHz


set_clock_uncertainty 0.5

config_interface -m_axi_addr64=0


csim_design

# Generates the RTL and the csynth.rpt that issue #90 reads.
csynth_design

# Runs the RTL (not the g++-compiled C++) against the same 21-case
# testbench used by csim, to catch divergences csim can't see
# (uninitialized BRAM, loop bounds, memory aliasing). Issue #95.
# #95 passed 21/21 (see vitis_hls.log, 2026-08-01). The gate below is
# cleared: export the RTL as a Vivado IP so #64's block design can consume
# a real component.xml instead of the validate script's placeholder.
cosim_design

export_design -rtl verilog -format ip_catalog

exit
