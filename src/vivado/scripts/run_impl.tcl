# Vivado batch flow: synthesis + implementation + timing closure for the
# dfs_system block design (issue #66).
# Run: vivado -mode batch -source src/vivado/scripts/run_impl.tcl
# Board: Kria KV260 (xilinx.com:kv260_som:part0:1.4), part xck26-sfvc784-2LV-c.
#
# Two-phase flow, one script, but the confirmation phase runs as a FRESH
# Vivado process (see below for why):
#   1. synth + implement at the 250 MHz target inherited from HLS (#63).
#   2. if that doesn't close, compute the achievable Fmax from the WNS,
#      then re-invoke this very script in a new `vivado -mode batch`
#      process (`-tclargs <freq>`) to reconfigure the PS PL0 clock to that
#      frequency and re-synth + re-implement, CONFIRMING it closes there --
#      not just reporting the first-order estimate. This is what makes the
#      reported Fmax reproducible from the repo alone, instead of living
#      only in a one-off manual run.
#
# Why phase 2 is a separate process: doing `open_run` (to measure phase
# 1's timing) and THEN editing/regenerating the block design in the SAME
# Vivado session crashes Vivado outright ("pure virtual method called",
# an internal object-model bug) -- hit this for real once already. A
# fresh process for the reconfiguration sidesteps it entirely, since it
# never opens an implemented run before touching the BD.
#
# Scope (on purpose): this script only answers #66's question -- what's
# the achievable Fmax. It does NOT call write_bitstream (that belongs to
# #67, on-board bring-up, which is the first thing that actually needs a
# bitstream) and it does NOT capture LUT/FF/BRAM/DSP utilization as a
# deliverable (that's #65). Both would be nearly free to add here later
# since they come from the same place_design/route_design run --
# deliberately left out for now so this script does exactly one thing.
#
# Requires the block design from #64 to already exist: run
# `make vivado-bd` first. Running this script mutates that project's PL0
# clock in place if phase 2 runs; `make vivado-bd` rebuilds it fresh at
# 250 MHz (its documented baseline) if you need to reset that.

set script_path [file normalize [info script]]
set script_dir  [file dirname $script_path]
set repo_root   [file normalize [file join $script_dir .. .. ..]]

set project_name "dfs_system"
set project_dir  [file join $repo_root src vivado $project_name]
set xpr          [file join $project_dir "$project_name.xpr"]

if {![file exists $xpr]} {
    puts stderr "run_impl.tcl: no project at $xpr"
    puts stderr "run_impl.tcl: run 'make vivado-bd' first (issue #64)"
    exit 1
}

# `-tclargs <freq>` marks this invocation as the phase-2 confirmation
# sub-run (see header). Absent -> this is the normal, initial invocation.
set confirm_freq_mhz {}
if {[llength $argv] >= 1} {
    set confirm_freq_mhz [lindex $argv 0]
}

open_project $xpr

# --- HDL wrapper for the block design ---
# system.bd (from #64) has no HDL top by itself -- synth_design needs one.
# Guarded so re-running this script on an already-wrapped project is a no-op
# here instead of erroring on a duplicate wrapper. Applies to both the
# initial and the confirmation sub-run, since each opens its own fresh copy
# of the project.
set bd_file [get_files -quiet */system.bd]
if {$bd_file eq ""} {
    puts stderr "run_impl.tcl: system.bd not found in the project"
    exit 1
}

if {[get_property top [current_fileset]] eq ""} {
    set wrapper [make_wrapper -files $bd_file -top]
    add_files -norecurse $wrapper
    update_compile_order -fileset sources_1
    set_property top system_wrapper [current_fileset]
    update_compile_order -fileset sources_1
}

set reports_dir [file join $repo_root src vivado reports]
file mkdir $reports_dir

# --- Helpers ---

# Reconfigures the PS PL0 clock (the one build_bd.tcl drives dfs_accel's
# ap_clk from) to a new frequency and regenerates everything downstream of
# that change. Returns the frequency Vivado actually resolved to -- the PS
# clock generator only hits discrete PLL-divided frequencies, so a
# requested value can get snapped to something slightly different.
proc reconfigure_pl0_clock {bd_file freq_mhz} {
    open_bd_design $bd_file
    set ps [get_bd_cells zynq_ultra_ps_e_0]
    set_property -dict [list CONFIG.PSU__CRL_APB__PL0_REF_CTRL__FREQMHZ $freq_mhz] $ps
    validate_bd_design
    save_bd_design
    reset_target all $bd_file
    generate_target all $bd_file
    # No export_ip_user_files (only syncs per-IP *simulation* sources, not
    # needed for synth/impl) and no close_bd_design (querying
    # current_bd_design right after generate_target is what actually
    # crashed real Vivado -- "pure virtual method called" -- three times in
    # a row; most likely a stale/dangling design handle after
    # generate_target's internal IP regeneration). Leaving the BD design
    # open doesn't block launch_runs, which works off the project's run
    # definitions, not off whatever design happens to be open in memory.
    return [get_property CONFIG.PSU__CRL_APB__PL0_REF_CTRL__FREQMHZ $ps]
}

# reset_run before each launch: if a previous batch session died mid-run
# (killed process, crashed host -- happened once already), Vivado leaves
# that run's on-disk state marked "running" and refuses to relaunch it
# ("Run 'synth_1' is already running and cannot be relaunched"), even
# though nothing is actually running. reset_run clears that stale state;
# it's a no-op on a run that hasn't started yet, so this is safe to do
# unconditionally on every invocation of this script.
proc synth_and_implement {label} {
    reset_run synth_1
    launch_runs synth_1 -jobs 4
    wait_on_run synth_1
    if {[get_property PROGRESS [get_runs synth_1]] ne "100%"} {
        puts stderr "run_impl.tcl: synth_1 did not complete ($label) (status: [get_property STATUS [get_runs synth_1]])"
        exit 1
    }

    reset_run impl_1
    launch_runs impl_1 -to_step route_design -jobs 4
    wait_on_run impl_1
    if {[get_property PROGRESS [get_runs impl_1]] ne "100%"} {
        puts stderr "run_impl.tcl: impl_1 did not reach route_design ($label) (status: [get_property STATUS [get_runs impl_1]])"
        exit 1
    }
}

# Worst setup slack across the whole design, queried directly instead of
# parsed out of the text report -- report_timing_summary's layout isn't
# meant to be scraped.
proc measure_wns {report_path} {
    catch {close_design}
    open_run impl_1
    report_timing_summary -delay_type min_max -file $report_path
    set wns [get_property SLACK [get_timing_paths -max_paths 1 -nworst 1 -setup]]
    close_design
    return $wns
}

# --- Confirmation sub-run (phase 2, invoked as a fresh process) ---
if {$confirm_freq_mhz ne ""} {
    set actual_freq_mhz [reconfigure_pl0_clock $bd_file $confirm_freq_mhz]
    synth_and_implement "$actual_freq_mhz MHz"
    set retry_rpt_name "timing_summary_${actual_freq_mhz}mhz.rpt"
    set wns2 [measure_wns [file join $reports_dir $retry_rpt_name]]
    # Parsed back out of captured stdout by the parent invocation below --
    # keep this exact "key=value" shape if you touch it.
    puts "RUN_IMPL_CONFIRM_RESULT freq=$actual_freq_mhz wns=$wns2 report=$retry_rpt_name"
    exit 0
}

# --- Phase 1: the 250 MHz target inherited from HLS (#63) ---
# build_bd.tcl configures PL0 to this by default -- but force it here too
# instead of trusting that, so this script gives the same answer no matter
# what frequency the project happens to be sitting at (e.g. left over from
# a previous confirmation run, or an aborted one -- happened for real: a
# crashed run saved the .bd mid-reconfiguration, leaving PL0 at 204 MHz,
# and this script silently measured that instead of 250 until this fix).

set target_period_ns 4.0
set target_freq_mhz  250.0

# CONFIG.PSU__CRL_APB__PL0_REF_CTRL__FREQMHZ as an integer, not "250.0" --
# passing it as a float crashed Vivado for real, twice, always at the same
# point (inside generate_target/export_ip_user_files, well after the bad
# property was set -- the failure is delayed, which is what made this look
# like session-reuse instability at first). The confirmation sub-run never
# hit this because round() already returns an integer.
reconfigure_pl0_clock $bd_file [expr {int($target_freq_mhz)}]
synth_and_implement "$target_freq_mhz MHz"
set wns [measure_wns [file join $reports_dir "timing_summary.rpt"]]

set fmax_rpt [file join $reports_dir "fmax_summary.txt"]
set f [open $fmax_rpt w]
puts $f "dfs_system timing closure (issue #66)"
puts $f "Target: $target_freq_mhz MHz ($target_period_ns ns)"
puts $f "Worst setup slack (WNS) at target: $wns ns"

if {$wns >= 0} {
    puts $f "Result: timing CLOSES at $target_freq_mhz MHz."
    close $f
    puts "run_impl.tcl OK: closes at $target_freq_mhz MHz (WNS $wns ns)"
    puts "run_impl.tcl: reports written to $reports_dir"
    exit 0
}

# --- Doesn't close: compute the estimate, then hand off to a fresh
# process to confirm it (see header for why it can't happen right here). ---

set achievable_period_ns [expr {$target_period_ns - $wns}]
set achievable_freq_mhz  [expr {1000.0 / $achievable_period_ns}]
set retry_freq_mhz [expr {round($achievable_freq_mhz)}]

puts $f "Result: timing DOES NOT close at $target_freq_mhz MHz."
puts $f "First-order estimate: [format %.2f $achievable_freq_mhz] MHz ([format %.3f $achievable_period_ns] ns)"
puts $f ""
puts $f "--- Confirmed by re-implementation at the estimated frequency ---"

puts "run_impl.tcl: $target_freq_mhz MHz does not close (WNS $wns ns)"
puts "run_impl.tcl: spawning a fresh Vivado process to confirm $retry_freq_mhz MHz"

# The project must be closed before the sub-process can open it -- Vivado
# locks a project against concurrent opens from two processes.
close_project

if {[catch {
    exec vivado -mode batch -source $script_path -tclargs $retry_freq_mhz 2>@1
} confirm_output]} {
    puts stderr "run_impl.tcl: confirmation sub-run failed:"
    puts stderr $confirm_output
    puts $f "Result: confirmation re-run FAILED -- see console output."
    close $f
    exit 1
}

set actual_freq_mhz {}
set wns2 {}
set retry_rpt_name {}
foreach line [split $confirm_output "\n"] {
    if {[string match "RUN_IMPL_CONFIRM_RESULT *" $line]} {
        foreach kv [lrange [split $line] 1 end] {
            set eq [string first "=" $kv]
            set k [string range $kv 0 [expr {$eq - 1}]]
            set v [string range $kv [expr {$eq + 1}] end]
            switch -- $k {
                freq   { set actual_freq_mhz $v }
                wns    { set wns2 $v }
                report { set retry_rpt_name $v }
            }
        }
    }
}
if {$actual_freq_mhz eq ""} {
    puts stderr "run_impl.tcl: could not find RUN_IMPL_CONFIRM_RESULT in the sub-run's output"
    puts stderr $confirm_output
    puts $f "Result: could not parse the confirmation sub-run's output."
    close $f
    exit 1
}

puts $f "PL0 reconfigured to $actual_freq_mhz MHz, full synth_1 + impl_1"
puts $f "(route_design) re-run, not just re-timed."
puts $f "Worst setup slack (WNS) at $actual_freq_mhz MHz: $wns2 ns"

if {$wns2 >= 0} {
    puts $f "Result: timing CLOSES at $actual_freq_mhz MHz."
    puts $f ""
    puts $f "Fmax reported for issue #66: $actual_freq_mhz MHz."
    puts "run_impl.tcl: CONFIRMED -- closes at $actual_freq_mhz MHz (WNS $wns2 ns)"
} else {
    puts $f "Result: STILL VIOLATES at $actual_freq_mhz MHz."
    puts $f "The first-order estimate was optimistic -- lower the target further and re-run."
    puts "run_impl.tcl: STILL VIOLATING at $actual_freq_mhz MHz (WNS $wns2 ns) -- lower further and re-run"
}
puts $f "Full report: $retry_rpt_name"
close $f

puts "run_impl.tcl: reports written to $reports_dir"
exit 0
