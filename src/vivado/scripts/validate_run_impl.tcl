# Cheap CI guard for run_impl.tcl (issue #66). No Vivado license needed:
# runs the real script under plain tclsh, with stub procs standing in for
# the Vivado-only commands. Same approach as validate_build_bd.tcl (#64).
#
# What this checks:
#  - the project-open gate is there (fails clearly without a real project)
#  - the wrapper/synth/impl call order matches what the header promises
#  - impl_1 is launched with -to_step route_design (i.e. it stops before
#    bitstream -- see run_impl.tcl's scope comment)
#  - phase 1 (250 MHz) runs first; when it doesn't close, the script
#    closes the project and `exec`s a FRESH `vivado -mode batch` process
#    (itself, with `-tclargs <freq>`) to confirm the estimated Fmax --
#    that's a deliberate design choice (see run_impl.tcl's header): doing
#    both phases in one Vivado session crashes Vivado for real.
#  - the WNS -> Fmax arithmetic feeding that confirmation is correct
#  - the confirmation sub-run's stdout gets parsed back correctly into the
#    final fmax_summary.txt
#  - it never calls write_bitstream or report_utilization: those are
#    deliberately out of scope for #66 (bitstream is #67, utilization is
#    #65). If you fold either into this script later, delete the matching
#    stub proc below instead of leaving it to fail forever.
#
# What this does NOT cover: since phase 2 is a real separate `vivado`
# process (nothing to stub -- there's no Vivado in CI), this validator
# fakes that sub-run's result instead of actually exercising phase 2's
# code path (reconfigure_pl0_clock / synth_and_implement / measure_wns
# inside the `if {$confirm_freq_mhz ne ""}` branch). That branch reuses
# the same stubbed Vivado commands defined below, so it's not untested
# logic, just not exercised by a run of this file.
#
# run_impl.tcl opens a real Vivado project (from #64, gitignored, only
# exists on a machine with Vivado) via open_project. That call itself is
# stubbed like everything else here, so the placeholder .xpr below only
# needs to exist -- its contents are never read by this validator.
#
# Usage: tclsh src/vivado/scripts/validate_run_impl.tcl

set ::errors [list]
set ::calls [list]
set ::props [dict create]
set ::exit_code {}

# Deliberately unrealistic (negative) so the validator exercises the
# "doesn't close, spawn a confirmation sub-run" branch and its Fmax
# arithmetic, which is the more interesting path to get right. WNS=-0.42
# at 4.0 ns -> first-order estimate ~226.24 MHz -> rounds to 226, which is
# what the confirmation sub-run stub below reports as CLOSING (fake_wns2
# positive), the more interesting success path to exercise.
set ::fake_wns  -0.42
set ::fake_wns2  0.05

proc fail {msg} {
    lappend ::errors $msg
}

proc log_call {name args} {
    lappend ::calls "$name $args"
}

# --- Vivado-only commands, stubbed out ---

proc open_project {args} { log_call open_project {*}$args }
proc close_project {args} { log_call close_project {*}$args }
proc current_fileset args { return "FILESET" }
proc get_files {args} { log_call get_files {*}$args; return "system.bd" }
proc make_wrapper {args} {
    log_call make_wrapper {*}$args
    if {[lsearch -exact $args "-files"] < 0 || [lsearch -exact $args "-top"] < 0} {
        fail "make_wrapper: expected -files ... -top, got '$args'"
    }
    return "system_wrapper.v"
}
proc add_files {args} { log_call add_files {*}$args }
proc update_compile_order {args} { log_call update_compile_order {*}$args }

proc set_property {args} {
    log_call set_property {*}$args
    if {[lindex $args 0] eq "-dict"} {
        foreach {k v} [lindex $args 1] { dict set ::props $k $v }
    } else {
        dict set ::props [lindex $args 0] [lindex $args 1]
    }
}

proc get_property {prop args} {
    switch -- $prop {
        top      { return [dict get $::props top] }
        PROGRESS { return "100%" }
        STATUS   { return "Route Design Complete!" }
        SLACK    { return $::fake_wns }
        default {
            if {[dict exists $::props $prop]} { return [dict get $::props $prop] }
            return ""
        }
    }
}
dict set ::props top ""

proc get_runs {name} { return $name }

proc reset_run {run} { log_call reset_run $run }

proc launch_runs {run args} {
    log_call launch_runs $run {*}$args
    if {$run eq "impl_1" && [lsearch -exact $args "-to_step"] < 0} {
        fail "launch_runs impl_1: missing -to_step (run_impl.tcl must stop before bitstream, see #66 scope)"
    }
}
proc wait_on_run {run} { log_call wait_on_run $run }
proc open_run {run} { log_call open_run $run }
proc close_design {args} { log_call close_design {*}$args }

proc report_timing_summary {args} {
    log_call report_timing_summary {*}$args
    set idx [lsearch -exact $args "-file"]
    if {$idx < 0} {
        fail "report_timing_summary: no -file given"
        return
    }
    set path [lindex $args [expr {$idx + 1}]]
    set f [open $path w]
    puts $f "stub timing report"
    close $f
}

proc get_timing_paths {args} {
    log_call get_timing_paths {*}$args
    if {[lsearch -exact $args "-setup"] < 0} {
        fail "get_timing_paths: expected -setup (worst setup slack), got '$args'"
    }
    return "TIMING_PATH"
}

# --- PL0 clock reconfiguration (phase 2's own process only -- see the
# module docstring on why this validator doesn't exercise that branch) ---
proc open_bd_design {args} { log_call open_bd_design {*}$args }
proc get_bd_cells {args} { log_call get_bd_cells {*}$args; return "PS_CELL" }
proc current_bd_design {} { return "system" }
proc close_bd_design {args} { log_call close_bd_design {*}$args }
proc validate_bd_design {args} { log_call validate_bd_design {*}$args }
proc save_bd_design {args} { log_call save_bd_design {*}$args }
proc reset_target {args} { log_call reset_target {*}$args }
proc generate_target {args} { log_call generate_target {*}$args }
proc export_ip_user_files {args} { log_call export_ip_user_files {*}$args }

# The confirmation sub-run: stubbed as a fake child process instead of a
# real `vivado`, since none exists in CI. Fabricates the exact stdout shape
# run_impl.tcl's parent branch expects to parse back
# (RUN_IMPL_CONFIRM_RESULT ...), and drops the report file the real child
# would have written, so the parent's file-existence checks still mean
# something.
proc exec {args} {
    log_call exec {*}$args
    global reports_dir

    if {[lsearch -exact $args "-tclargs"] < 0} {
        fail "exec: expected -tclargs <freq> in the confirmation sub-run command, got '$args'"
    }
    if {[lsearch -exact $args "-mode"] < 0 || [lsearch -exact $args "batch"] < 0} {
        fail "exec: expected 'vivado -mode batch ...', got '$args'"
    }

    set retry_rpt [file join $reports_dir "timing_summary_226mhz.rpt"]
    set rf [open $retry_rpt w]
    puts $rf "stub confirmation timing report"
    close $rf

    return "RUN_IMPL_CONFIRM_RESULT freq=226 wns=$::fake_wns2 report=timing_summary_226mhz.rpt"
}

# Scope guards: run_impl.tcl must never call these (see header comment).
proc write_bitstream {args} {
    fail "run_impl.tcl calls write_bitstream -- out of scope for #66 (that's #67)"
}
proc report_utilization {args} {
    fail "run_impl.tcl calls report_utilization -- out of scope for #66 (that's #65)"
}

# build_bd.tcl-style exit trap, so a real `exit 0`/`exit 1` in run_impl.tcl
# returns here instead of killing tclsh.
rename ::exit ::_real_exit
proc exit {{code 0}} { set ::exit_code $code }

# --- Placeholder project so run_impl.tcl gets past its open-gate ---

set script [file join [file dirname [file normalize [info script]]] run_impl.tcl]
if {![file exists $script]} {
    puts stderr "validate_run_impl.tcl: run_impl.tcl not found at $script"
    _real_exit 1
}

set repo_root  [file normalize [file join [file dirname $script] .. .. ..]]
set project_dir [file join $repo_root src vivado dfs_system]
set xpr         [file join $project_dir "dfs_system.xpr"]
set created_dir [expr {![file exists $project_dir]}]

file mkdir $project_dir
set had_xpr [file exists $xpr]
if {!$had_xpr} {
    set f [open $xpr w]
    puts $f "<!-- placeholder for validate_run_impl.tcl, safe to delete -->"
    close $f
}

set reports_dir [file join $repo_root src vivado reports]
set had_reports_dir [file exists $reports_dir]

# run_impl.tcl writes real reports under fixed names (fmax_summary.txt,
# timing_summary.rpt, timing_summary_<N>mhz.rpt) -- if a real Vivado run
# already populated this directory, the stub run below would silently
# overwrite those real reports with stub content. Snapshot the whole
# directory first and restore it verbatim in the finally block, instead of
# just deleting it when !$had_reports_dir (that part alone isn't enough
# once real reports already exist). Learned this the hard way: an earlier
# version of this validator did exactly that and clobbered a real
# timing_summary.rpt.
set reports_backup [file join $repo_root src vivado .reports_validate_backup]
if {$had_reports_dir} {
    file delete -force $reports_backup
    file copy $reports_dir $reports_backup
}

# --- Argv for the "no confirmation requested" (parent) invocation ---
set argv [list]

# --- Checks ---
# Called from inside the try block below, before the finally cleanup
# restores the report files run_impl.tcl just wrote.

proc run_checks {} {
global reports_dir

set all_calls [join $::calls "\n"]

foreach needle {open_project get_files make_wrapper add_files launch_runs\ synth_1 launch_runs\ impl_1 open_run report_timing_summary get_timing_paths close_project exec} {
    if {![string match "*$needle*" $all_calls]} {
        fail "expected a call matching '$needle', never happened"
    }
}

# Order: synth_1 must launch (and be waited on) before impl_1.
set idx_synth [lsearch -glob $::calls "launch_runs synth_1*"]
set idx_impl  [lsearch -glob $::calls "launch_runs impl_1*"]
if {$idx_synth < 0 || $idx_impl < 0 || $idx_synth > $idx_impl} {
    fail "expected launch_runs synth_1 to happen before launch_runs impl_1"
}

set idx_reset_synth [lsearch -exact $::calls "reset_run synth_1"]
if {$idx_reset_synth < 0 || $idx_reset_synth > $idx_synth} {
    fail "expected reset_run synth_1 before launch_runs synth_1"
}
set idx_reset_impl [lsearch -exact $::calls "reset_run impl_1"]
if {$idx_reset_impl < 0 || $idx_reset_impl > $idx_impl} {
    fail "expected reset_run impl_1 before launch_runs impl_1"
}

# Phase 1 must explicitly force PL0 to 250 before synthesizing, instead of
# trusting whatever the project already happens to be configured for (a
# crashed prior run once left it silently at a different frequency). Must
# be the plain integer "250", not "250.0" -- passing a float string here
# crashed real Vivado, twice, always inside generate_target/
# export_ip_user_files well after this property was set.
set idx_force_250 [lsearch -glob $::calls "set_property -dict*PSU__CRL_APB__PL0_REF_CTRL__FREQMHZ 250*"]
if {$idx_force_250 < 0} {
    fail "expected phase 1 to explicitly reconfigure PL0 to 250 before synthesizing"
} elseif {$idx_force_250 > $idx_synth} {
    fail "expected the PL0 250 MHz reconfiguration before launch_runs synth_1, not after"
} elseif {[string match "*FREQMHZ 250.0*" [lindex $::calls $idx_force_250]]} {
    fail "PL0 reconfiguration must pass an integer (250), not a float (250.0) -- this is what crashed real Vivado"
}

# The project must be closed before exec-ing the confirmation sub-run
# (Vivado locks a project against concurrent opens), and exec must not
# happen until phase 1 actually finished and decided not to close.
set idx_close_project [lsearch -glob $::calls "close_project*"]
set idx_exec [lsearch -glob $::calls "exec vivado*"]
if {$idx_close_project < 0 || $idx_exec < 0 || $idx_close_project > $idx_exec} {
    fail "expected close_project before exec vivado (confirmation sub-run)"
}
if {$idx_exec < $idx_impl} {
    fail "expected exec vivado to happen after phase 1's launch_runs impl_1"
}

# Fmax arithmetic: WNS=-0.42 at 4.0 ns -> achievable period 4.42 ns -> ~226.24 MHz.
set fmax_file [file join $reports_dir "fmax_summary.txt"]
if {![file exists $fmax_file]} {
    fail "fmax_summary.txt was not written to $reports_dir"
} else {
    set f [open $fmax_file r]
    set content [read $f]
    close $f
    if {![string match "*DOES NOT close*" $content]} {
        fail "fmax_summary.txt: expected a 'DOES NOT close' result for WNS=$::fake_wns"
    }
    if {![string match "*226.24 MHz*" $content]} {
        fail "fmax_summary.txt: expected achievable Fmax ~226.24 MHz for WNS=$::fake_wns, got:\n$content"
    }
    if {![string match "*CLOSES at 226 MHz*" $content]} {
        fail "fmax_summary.txt: expected the confirmation sub-run's CLOSES result to make it into the final report, got:\n$content"
    }
    if {![string match "*Fmax reported for issue #66: 226 MHz*" $content]} {
        fail "fmax_summary.txt: expected the final reported Fmax line for 226 MHz, got:\n$content"
    }
}

if {![file exists [file join $reports_dir "timing_summary.rpt"]]} {
    fail "timing_summary.rpt (phase 1) was not written to $reports_dir"
}
if {![file exists [file join $reports_dir "timing_summary_226mhz.rpt"]]} {
    fail "timing_summary_226mhz.rpt (confirmation) was not written to $reports_dir"
}

if {$::exit_code ne "0"} {
    fail "run_impl.tcl exited with code '$::exit_code' instead of 0 under a placeholder project"
}

}
;# end of run_checks -- deliberately does not exit itself, so the try/finally
;# below always gets to clean up the placeholder project and report files
;# regardless of pass/fail.

try {
    source $script
    run_checks
} finally {
    if {!$had_xpr} { file delete $xpr }
    if {$created_dir} { file delete -force $project_dir }
    if {$had_reports_dir} {
        file delete -force $reports_dir
        file rename $reports_backup $reports_dir
    } else {
        file delete -force $reports_dir
    }
}

if {[llength $::errors] > 0} {
    foreach e $::errors { puts stderr "FAIL: $e" }
    puts stderr "[llength $::errors] problem(s) found in run_impl.tcl"
    _real_exit 1
}

puts "run_impl.tcl OK: phase 1 (250 MHz) correct, stops before bitstream, closes project + execs a fresh process to confirm the Fmax estimate, WNS->Fmax arithmetic correct, stays out of #65/#67 scope"
_real_exit 0
