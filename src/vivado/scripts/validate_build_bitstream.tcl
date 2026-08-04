# Cheap CI guard for build_bitstream.tcl (issue #67). No Vivado license
# needed: runs the real script under plain tclsh, with stub procs standing
# in for the Vivado-only commands. Same approach as validate_run_impl.tcl
# (#66) and validate_build_bd.tcl (#64).
#
# What this checks:
#  - the project-open gate is there (fails clearly without a real project)
#  - PL0 gets reconfigured to 200 MHz (an integer, not "200.0" -- run_impl.tcl
#    already hit real Vivado crashes passing a float here) before synth
#  - synth_1 launches (and is waited on) before impl_1
#  - impl_1 is launched with -to_step write_bitstream, i.e. it goes further
#    than run_impl.tcl's -to_step route_design -- that's the whole point of
#    this script existing separately (see its header)
#  - write_bitstream and the .hwh copy both happen
#  - bitstream_summary.txt reflects the WNS the stub reports, for both the
#    closes and doesn't-close cases
#
# What this does NOT cover: exercising a real Vivado bitstream write, or
# anything about the KV260/PYNQ side of #67 -- those need real hardware.
#
# Usage: tclsh src/vivado/scripts/validate_build_bitstream.tcl

set ::errors [list]
set ::calls [list]
set ::props [dict create]
set ::exit_code {}

# Overridable by the second CLI arg (see bottom): default exercises the
# expected-good path (200 MHz confirmed closing), matching what #66 already
# demonstrated at 204 MHz with less margin.
set ::fake_wns 1.2
if {[llength $argv] >= 1} {
    set ::fake_wns [lindex $argv 0]
}

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
        STATUS   { return "write_bitstream Complete!" }
        SLACK    { return $::fake_wns }
        CONFIG.PSU__CRL_APB__PL0_REF_CTRL__FREQMHZ {
            return [dict get $::props CONFIG.PSU__CRL_APB__PL0_REF_CTRL__FREQMHZ]
        }
        default {
            if {[dict exists $::props $prop]} { return [dict get $::props $prop] }
            return ""
        }
    }
}
dict set ::props top ""
dict set ::props CONFIG.PSU__CRL_APB__PL0_REF_CTRL__FREQMHZ ""

proc get_runs {name} { return $name }
proc reset_run {run} { log_call reset_run $run }
proc launch_runs {run args} { log_call launch_runs $run {*}$args }
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

proc open_bd_design {args} { log_call open_bd_design {*}$args }
proc get_bd_cells {args} { log_call get_bd_cells {*}$args; return "PS_CELL" }
proc validate_bd_design {args} { log_call validate_bd_design {*}$args }
proc save_bd_design {args} { log_call save_bd_design {*}$args }
proc reset_target {args} { log_call reset_target {*}$args }
proc generate_target {args} { log_call generate_target {*}$args }

# Shadows Tcl's built-in `file` command so `file copy` (used by
# build_bitstream.tcl to stage the .bit and .hwh into exports/) doesn't
# require a real source file to exist -- everything else (mkdir/exists/
# delete/join/dirname/normalize/...) falls through to the real command
# unchanged.
rename ::file ::_real_file
proc file {sub args} {
    if {$sub eq "copy"} {
        log_call file_copy {*}$args
        set dst [lindex $args end]
        _real_file mkdir [_real_file dirname $dst]
        set f [open $dst w]
        puts $f "stub hwh"
        close $f
        return
    }
    return [uplevel 1 [list ::_real_file $sub {*}$args]]
}

# report_utilization is deliberately out of scope for #67 too (that's #65) --
# make sure this script never calls it.
proc report_utilization {args} {
    fail "build_bitstream.tcl calls report_utilization -- out of scope for #67 (that's #65)"
}

rename ::exit ::_real_exit
# Unlike validate_run_impl.tcl's stub, this one must actually unwind the
# script: build_bitstream.tcl calls exit 1 mid-script (after the
# route_design WNS check, before write_bitstream) and expects nothing past
# it to run -- exactly what real Vivado's exit does by terminating the
# process. A stub that only records the code and returns would let
# execution fall through into the write_bitstream/copy code that a real
# exit 1 would have skipped.
proc exit {{code 0}} {
    set ::exit_code $code
    return -code error -errorcode {VALIDATOR_EXIT} "validator-exit $code"
}

# --- Placeholder project so build_bitstream.tcl gets past its open-gate ---

set script [file join [file dirname [file normalize [info script]]] build_bitstream.tcl]
if {![_real_file exists $script]} {
    puts stderr "validate_build_bitstream.tcl: build_bitstream.tcl not found at $script"
    _real_exit 1
}

set repo_root  [file normalize [file join [file dirname $script] .. .. ..]]
set project_dir [file join $repo_root src vivado dfs_system]
set xpr         [file join $project_dir "dfs_system.xpr"]
set created_dir [expr {![_real_file exists $project_dir]}]

# build_bitstream.tcl (closing-WNS path) creates project_dir/exports/ as a
# side effect -- track whether it pre-existed so it can be cleaned up
# afterward even when project_dir itself is real and must be left alone
# (had_xpr true). Missing this left stub .bit/.hwh files sitting inside a
# real Vivado project on a real run of this validator.
set exports_dir [file join $project_dir "exports"]
set had_exports_dir [_real_file exists $exports_dir]

_real_file mkdir $project_dir
set had_xpr [_real_file exists $xpr]
if {!$had_xpr} {
    set f [open $xpr w]
    puts $f "<!-- placeholder for validate_build_bitstream.tcl, safe to delete -->"
    close $f
}

set reports_dir [file join $repo_root src vivado reports]
set had_reports_dir [_real_file exists $reports_dir]
set reports_backup [file join $repo_root src vivado .reports_validate_backup]
if {$had_reports_dir} {
    _real_file delete -force $reports_backup
    _real_file copy $reports_dir $reports_backup
}

proc run_checks {} {
    global reports_dir

    set all_calls [join $::calls "\n"]

    foreach needle {open_project get_files make_wrapper add_files\
                     launch_runs\ synth_1 launch_runs\ impl_1\
                     report_timing_summary get_timing_paths} {
        if {![string match "*$needle*" $all_calls]} {
            fail "expected a call matching '$needle', never happened"
        }
    }

    set idx_synth [lsearch -glob $::calls "launch_runs synth_1*"]
    set idx_route [lsearch -glob $::calls "launch_runs impl_1*-to_step route_design*"]
    if {$idx_synth < 0 || $idx_route < 0 || $idx_synth > $idx_route} {
        fail "expected launch_runs synth_1 to happen before launch_runs impl_1 -to_step route_design"
    }

    set idx_force_200 [lsearch -glob $::calls "set_property -dict*PSU__CRL_APB__PL0_REF_CTRL__FREQMHZ 200*"]
    if {$idx_force_200 < 0} {
        fail "expected PL0 to be reconfigured to 200 MHz"
    } elseif {$idx_force_200 > $idx_synth} {
        fail "expected the PL0 200 MHz reconfiguration before launch_runs synth_1, not after"
    } elseif {[string match "*FREQMHZ 200.0*" [lindex $::calls $idx_force_200]]} {
        fail "PL0 reconfiguration must pass an integer (200), not a float (200.0)"
    }

    # WNS is checked (via open_run/report_timing_summary/get_timing_paths)
    # before any decision to spend more time on write_bitstream.
    set idx_open_run [lsearch -glob $::calls "open_run impl_1*"]
    if {$idx_open_run < 0 || $idx_open_run < $idx_route} {
        fail "expected open_run impl_1 after route_design, to measure WNS"
    }

    set idx_bitstream_step [lsearch -glob $::calls "launch_runs impl_1*-to_step write_bitstream*"]
    set copy_count [llength [lsearch -all -glob $::calls "file_copy*"]]

    if {$::fake_wns >= 0} {
        if {$idx_bitstream_step < 0 || $idx_bitstream_step < $idx_open_run} {
            fail "expected launch_runs impl_1 -to_step write_bitstream after the WNS check, for a closing WNS=$::fake_wns"
        }
        if {$copy_count != 2} {
            fail "expected exactly 2 file copies (.bit + .hwh) for a closing WNS=$::fake_wns, got $copy_count"
        }
        if {$::exit_code ne "0"} {
            fail "build_bitstream.tcl exited with code '$::exit_code' instead of 0 for a closing WNS=$::fake_wns"
        }
    } else {
        if {$idx_bitstream_step >= 0} {
            fail "build_bitstream.tcl called launch_runs -to_step write_bitstream despite a non-closing WNS=$::fake_wns -- should bail out after the route_design timing check instead"
        }
        if {$copy_count != 0} {
            fail "expected no bitstream/.hwh files copied for a non-closing WNS=$::fake_wns, got $copy_count copies"
        }
        if {$::exit_code eq "0"} {
            fail "expected a non-zero exit when timing does not close at 200 MHz"
        }
    }

    set summary_path [file join $reports_dir "bitstream_summary.txt"]
    if {![_real_file exists $summary_path]} {
        fail "bitstream_summary.txt was not written to $reports_dir"
    } else {
        set f [open $summary_path r]
        set content [read $f]
        close $f
        if {$::fake_wns >= 0} {
            if {![string match "*CLOSES at 200 MHz*" $content]} {
                fail "bitstream_summary.txt: expected a CLOSES result for WNS=$::fake_wns, got:\n$content"
            }
        } else {
            if {![string match "*DOES NOT close*" $content]} {
                fail "bitstream_summary.txt: expected a DOES NOT close result for WNS=$::fake_wns, got:\n$content"
            }
        }
    }
}

try {
    if {[catch {source $script} err erropts]} {
        if {[dict get $erropts -errorcode] ne "VALIDATOR_EXIT"} {
            fail "unexpected Tcl error while sourcing build_bitstream.tcl: $err"
        }
    }
    run_checks
} finally {
    if {!$had_xpr} { _real_file delete $xpr }
    if {!$had_exports_dir} { _real_file delete -force $exports_dir }
    if {$created_dir} { _real_file delete -force $project_dir }
    if {$had_reports_dir} {
        _real_file delete -force $reports_dir
        _real_file rename $reports_backup $reports_dir
    } else {
        _real_file delete -force $reports_dir
    }
}

if {[llength $::errors] > 0} {
    foreach e $::errors { puts stderr "FAIL: $e" }
    puts stderr "[llength $::errors] problem(s) found in build_bitstream.tcl"
    _real_exit 1
}

puts "build_bitstream.tcl OK: reconfigures PL0 to 200 MHz (int), synths, implements through write_bitstream, copies .hwh, writes bitstream_summary.txt correctly for WNS=$::fake_wns"
_real_exit 0
