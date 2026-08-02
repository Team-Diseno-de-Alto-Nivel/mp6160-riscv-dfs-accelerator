# Cheap CI guard for run_hls.tcl (issues #63, #95). No Vitis license needed:
# runs the real script under plain tclsh, with stub procs standing in for
# the Vitis-only commands. The stubs assert what Vitis would actually care
# about (paths, part, clock) without touching Vitis itself.
#
# Usage: tclsh src/hls/scripts/validate_run_hls.tcl

set ::errors [list]
set ::saw_csynth 0
set ::saw_cosim 0
set ::saw_export 0

proc fail {msg} {
    lappend ::errors $msg
}

proc check_path_args {cmd args} {
    foreach a $args {
        if {[string index $a 0] eq "/" && ![file exists $a]} {
            fail "$cmd: path does not exist: $a"
        }
    }
}

# --- Vitis-only commands, stubbed out ---

proc open_project args {}
proc set_top args {}
proc open_solution args {}
proc set_clock_uncertainty args {}
proc config_interface args {}
proc csim_design args {}

proc add_files args {
    check_path_args add_files {*}$args
}

proc set_part {part} {
    set expected "xck26-sfvc784-2LV-c"
    if {$part ne $expected} {
        fail "set_part: expected '$expected', got '$part'"
    }
}

proc create_clock args {
    set idx [lsearch -exact $args "-period"]
    if {$idx < 0} {
        fail "create_clock: no -period given"
        return
    }
    set period [lindex $args [expr {$idx + 1}]]
    if {$period ne "4"} {
        fail "create_clock: expected -period 4, got -period $period"
    }
}

proc csynth_design args { set ::saw_csynth 1 }
proc cosim_design  args { set ::saw_cosim 1 }
proc export_design args { set ::saw_export 1 }

# run_hls.tcl ends with a bare `exit`. Renaming the real builtin and
# stubbing `exit` lets that call return here instead of killing tclsh
# before we get to check what happened.
rename ::exit ::_real_exit
proc exit {{code 0}} {}

# --- Run the real script ---

set script [file join [file dirname [file normalize [info script]]] run_hls.tcl]
if {![file exists $script]} {
    puts stderr "validate_run_hls.tcl: run_hls.tcl not found at $script"
    _real_exit 1
}

source $script

if {!$::saw_csynth} {
    fail "run_hls.tcl never calls csynth_design"
}
if {!$::saw_cosim} {
    fail "run_hls.tcl never calls cosim_design -- required by #95"
}
if {!$::saw_export} {
    fail "run_hls.tcl never calls export_design -- #95 passed, #64 needs the IP now"
}

if {[llength $::errors] > 0} {
    foreach e $::errors { puts stderr "FAIL: $e" }
    puts stderr "[llength $::errors] problem(s) found in run_hls.tcl"
    _real_exit 1
}

puts "run_hls.tcl OK: paths resolve, part/clock match, csynth_design and cosim_design present, no export"
_real_exit 0
