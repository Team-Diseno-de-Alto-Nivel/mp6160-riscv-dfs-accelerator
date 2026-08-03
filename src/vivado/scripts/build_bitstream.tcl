# Vivado batch flow: generate the on-board bitstream for issue #67 (PYNQ
# bring-up). Deliberately separate from run_impl.tcl (#66), which stops
# before write_bitstream on purpose -- see that script's header for why.
# Run: vivado -mode batch -source src/vivado/scripts/build_bitstream.tcl
# Board: Kria KV260 (xilinx.com:kv260_som:part0:1.4), part xck26-sfvc784-2LV-c.
#
# Frequency: 200 MHz, NOT the 204 MHz reported as Fmax by #66.
# src/vivado/reports/fmax_summary.txt documents why, in a note addressed to
# this issue: the WNS margin at 204 MHz is razor-thin (0.084 ns of a 4.9 ns
# period, ~2%), and this script's own synth/impl run can place/route
# slightly differently and eat that margin. A timing violation doesn't show
# up as a report line on real hardware -- it shows up as silently wrong
# results, which is a much worse failure mode to debug. 200 MHz costs ~2%
# throughput for real margin.
#
# Requires the block design from #64 to already exist: run `make vivado-bd`
# first. This script reconfigures that project's PL0 clock in place, same
# as run_impl.tcl does -- `make vivado-bd` resets it to 250 MHz (its
# documented baseline) if you need to start clean.
#
# Outputs, gitignored (local to whoever's machine has Vivado + the KV260 --
# same rationale as the rest of src/vivado/dfs_system/, see .gitignore):
#   src/vivado/dfs_system/exports/dfs_system.bit
#   src/vivado/dfs_system/exports/dfs_system.hwh
# Versioned summary so the team can see the result without Vivado:
#   src/vivado/reports/bitstream_summary.txt

set script_path [file normalize [info script]]
set script_dir  [file dirname $script_path]
set repo_root   [file normalize [file join $script_dir .. .. ..]]

set project_name "dfs_system"
set project_dir  [file join $repo_root src vivado $project_name]
set xpr          [file join $project_dir "$project_name.xpr"]

if {![file exists $xpr]} {
    puts stderr "build_bitstream.tcl: no project at $xpr"
    puts stderr "build_bitstream.tcl: run 'make vivado-bd' first (issue #64)"
    exit 1
}

set target_freq_mhz 200

open_project $xpr

# --- HDL wrapper for the block design (same guard as run_impl.tcl) ---
set bd_file [get_files -quiet */system.bd]
if {$bd_file eq ""} {
    puts stderr "build_bitstream.tcl: system.bd not found in the project"
    exit 1
}

if {[get_property top [current_fileset]] eq ""} {
    set wrapper [make_wrapper -files $bd_file -top]
    add_files -norecurse $wrapper
    update_compile_order -fileset sources_1
    set_property top system_wrapper [current_fileset]
    update_compile_order -fileset sources_1
}

# --- Reconfigure PL0 to the fixed on-board target ---
# Same mechanism as run_impl.tcl's reconfigure_pl0_clock, duplicated
# instead of shared: that script searches for the achievable frequency,
# this one always targets a fixed, already-known-good one, so the two
# have no logic in common worth factoring out.
open_bd_design $bd_file
set ps [get_bd_cells zynq_ultra_ps_e_0]
set_property -dict [list CONFIG.PSU__CRL_APB__PL0_REF_CTRL__FREQMHZ [expr {int($target_freq_mhz)}]] $ps
validate_bd_design
save_bd_design
reset_target all $bd_file
generate_target all $bd_file
set actual_freq_mhz [get_property CONFIG.PSU__CRL_APB__PL0_REF_CTRL__FREQMHZ $ps]

# --- Synth + implementation through route_design, same as run_impl.tcl ---
reset_run synth_1
launch_runs synth_1 -jobs 4
wait_on_run synth_1
if {[get_property PROGRESS [get_runs synth_1]] ne "100%"} {
    puts stderr "build_bitstream.tcl: synth_1 did not complete (status: [get_property STATUS [get_runs synth_1]])"
    exit 1
}

reset_run impl_1
launch_runs impl_1 -to_step route_design -jobs 4
wait_on_run impl_1
if {[get_property PROGRESS [get_runs impl_1]] ne "100%"} {
    puts stderr "build_bitstream.tcl: impl_1 did not reach route_design (status: [get_property STATUS [get_runs impl_1]])"
    exit 1
}

set reports_dir [file join $repo_root src vivado reports]
file mkdir $reports_dir

# Worst setup slack, queried directly rather than parsed out of the text
# report -- same approach as run_impl.tcl's measure_wns. Checked BEFORE
# spending the extra time on write_bitstream: no point generating a
# bitstream from a build that doesn't meet timing.
open_run impl_1
set report_path [file join $reports_dir "timing_summary_bitstream.rpt"]
report_timing_summary -delay_type min_max -file $report_path
set wns [get_property SLACK [get_timing_paths -max_paths 1 -nworst 1 -setup]]
close_design

set summary_path [file join $reports_dir "bitstream_summary.txt"]

if {$wns < 0} {
    set f [open $summary_path w]
    puts $f "dfs_system on-board bitstream (issue #67)"
    puts $f "Target: $actual_freq_mhz MHz (fixed, per #66's fmax_summary.txt margin note)"
    puts $f "Worst setup slack (WNS) at $actual_freq_mhz MHz: $wns ns"
    puts $f "Result: timing DOES NOT close at $actual_freq_mhz MHz -- bitstream NOT written."
    puts $f "This should not happen at 200 MHz given #66's confirmed 204 MHz closure --"
    puts $f "investigate before using this build on-board."
    puts $f ""
    puts $f "Full timing report: timing_summary_bitstream.rpt"
    close $f
    puts stderr "build_bitstream.tcl: WARNING -- timing does not close at $actual_freq_mhz MHz, see $summary_path"
    exit 1
}

# Timing closes -- finish the run through write_bitstream. Resumes the
# same impl_1 run past route_design rather than relaunching it.
launch_runs impl_1 -to_step write_bitstream -jobs 4
wait_on_run impl_1
if {[get_property PROGRESS [get_runs impl_1]] ne "100%"} {
    puts stderr "build_bitstream.tcl: impl_1 did not reach write_bitstream (status: [get_property STATUS [get_runs impl_1]])"
    exit 1
}

set exports_dir [file join $project_dir "exports"]
file mkdir $exports_dir

# launch_runs -to_step write_bitstream drops the .bit under the run's own
# directory, named after the top-level wrapper -- copy it out to a stable,
# PYNQ-friendly name/location instead of depending on that internal path.
set bit_src [file join $project_dir "$project_name.runs" "impl_1" "system_wrapper.bit"]
set bit_path [file join $exports_dir "$project_name.bit"]
file copy -force $bit_src $bit_path

# PYNQ's Overlay() loads a .bit next to a .hwh with the same basename --
# copy the hw handoff generate_target already produced alongside the bit.
set hwh_src [file join $project_dir "$project_name.gen" "sources_1" "bd" "system" "hw_handoff" "system.hwh"]
set hwh_path [file join $exports_dir "$project_name.hwh"]
file copy -force $hwh_src $hwh_path

set f [open $summary_path w]
puts $f "dfs_system on-board bitstream (issue #67)"
puts $f "Target: $actual_freq_mhz MHz (fixed, per #66's fmax_summary.txt margin note)"
puts $f "Worst setup slack (WNS) at $actual_freq_mhz MHz: $wns ns"
puts $f "Result: timing CLOSES at $actual_freq_mhz MHz -- bitstream written."
puts $f ""
puts $f "Outputs (local only, not versioned -- see .gitignore):"
puts $f "  $bit_path"
puts $f "  $hwh_path"
puts $f "Full timing report: timing_summary_bitstream.rpt"
close $f

puts "build_bitstream.tcl OK: closes at $actual_freq_mhz MHz (WNS $wns ns)"
puts "build_bitstream.tcl: bitstream at $bit_path"
puts "build_bitstream.tcl: summary written to $summary_path"
exit 0
