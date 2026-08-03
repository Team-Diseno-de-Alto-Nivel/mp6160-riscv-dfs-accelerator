# Captures real, post-route resource utilization (LUT/FF/BRAM/DSP) for
# `dfs_system`, closing #65. This replaces Vitis HLS's pre-Vivado *estimates*
# (already recorded from #90) with actual numbers from the implemented design.
#
# Vivado batch flow: resource utilization report for the dfs_system block
# design, off whatever impl_1 run is already routed on disk (issue #65).
# Run: vivado -mode batch -source src/vivado/scripts/report_utilization.tcl
#
# Deliberately just this one thing, same spirit as run_impl.tcl (#66) and
# build_bd.tcl (#64): does NOT run synth/impl itself, does NOT touch the
# PL0 clock. Utilization comes for free off the routed netlist regardless
# of which clock target produced it -- run `make vivado-bd` + `make
# vivado-impl` first (see run_impl.tcl's header: it leaves the project at
# whichever frequency its own confirmation run last closed at, currently
# 204 MHz per fmax_summary.txt -- 200 MHz once #67 re-targets it for the
# actual bitstream. Either is fine here: LUT/FF/BRAM/DSP counts don't
# meaningfully shift between 200-250 MHz retargets of the same RTL).

set script_dir  [file dirname [file normalize [info script]]]
set repo_root   [file normalize [file join $script_dir .. .. ..]]

set project_name "dfs_system"
set project_dir  [file join $repo_root src vivado $project_name]
set xpr          [file join $project_dir "$project_name.xpr"]
set reports_dir  [file join $repo_root src vivado reports]

if {![file exists $xpr]} {
    puts stderr "report_utilization.tcl: no project at $xpr"
    puts stderr "report_utilization.tcl: run 'make vivado-bd' first (issue #64)"
    exit 1
}

open_project $xpr

if {[get_property PROGRESS [get_runs impl_1]] ne "100%"} {
    puts stderr "report_utilization.tcl: impl_1 hasn't completed route_design"
    puts stderr "report_utilization.tcl: run 'make vivado-impl' first (issue #66)"
    exit 1
}

open_run impl_1

# Record which clock target this utilization corresponds to, since #66's
# script can leave the project at different frequencies across runs.
set ps        [get_bd_cells -quiet zynq_ultra_ps_e_0]
set clock_mhz "unknown"
if {$ps ne ""} {
    set clock_mhz [get_property CONFIG.PSU__CRL_APB__PL0_REF_CTRL__FREQMHZ $ps]
}

file mkdir $reports_dir
set util_rpt [file join $reports_dir "utilization.rpt"]

report_utilization -file $util_rpt

# Prepend the clock context -- report_utilization's own header doesn't
# include it, and it matters for reproducing/interpreting the numbers.
set body [open $util_rpt r]
set body_text [read $body]
close $body
set out [open $util_rpt w]
puts $out "# dfs_system post-route utilization (issue #65)"
puts $out "# Captured off impl_1 at PL0 = $clock_mhz MHz"
puts $out "#"
puts $out $body_text
close $out

close_design

puts "report_utilization.tcl OK: $util_rpt (PL0 = $clock_mhz MHz)"
exit 0
