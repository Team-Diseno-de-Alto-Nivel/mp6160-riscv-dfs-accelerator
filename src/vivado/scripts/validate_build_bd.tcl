# Cheap CI guard for build_bd.tcl (issue #64). No Vivado license needed:
# runs the real script under plain tclsh, with stub procs standing in for
# the Vivado-only commands. The stubs assert what Vivado would actually care
# about (part, board_part, PS ports, wiring, no premature synth/bitstream)
# without touching Vivado itself.
#
# build_bd.tcl refuses to run past its IP gate until #95 exports the real
# dfs_accel IP. To validate everything past that gate anyway, this script
# drops a placeholder component.xml at the expected path before sourcing
# build_bd.tcl, and always removes it afterwards.
#
# Usage: tclsh src/vivado/scripts/validate_build_bd.tcl

set ::errors [list]
set ::set_props [dict create]
set ::cell_vlnvs [list]
set ::connections [list]
set ::automation_calls 0
set ::saw_validate 0
set ::saw_save 0
set ::exit_code {}

proc fail {msg} {
    lappend ::errors $msg
}

# --- Vivado-only commands, stubbed out ---

proc create_project {args} {
    set idx [lsearch -exact $args "-part"]
    if {$idx < 0} {
        fail "create_project: no -part given"
        return
    }
    set expected "xck26-sfvc784-2LV-c"
    set part [lindex $args [expr {$idx + 1}]]
    if {$part ne $expected} {
        fail "create_project: expected part '$expected', got '$part'"
    }
}

proc current_project args { return "PROJECT" }

proc set_property {args} {
    if {[lindex $args 0] eq "-dict"} {
        foreach {k v} [lindex $args 1] { dict set ::set_props $k $v }
    } else {
        dict set ::set_props [lindex $args 0] [lindex $args 1]
    }
}

proc update_ip_catalog args {}
proc create_bd_design args {}

proc create_bd_cell {args} {
    set idx [lsearch -exact $args "-vlnv"]
    if {$idx < 0} {
        fail "create_bd_cell: no -vlnv given"
        return "cell"
    }
    set vlnv [lindex $args [expr {$idx + 1}]]
    set name [lindex $args end]
    lappend ::cell_vlnvs $vlnv
    return $name
}

proc apply_bd_automation {args} {
    incr ::automation_calls
    set idx [lsearch -exact $args "-rule"]
    set rule [expr {$idx >= 0 ? [lindex $args [expr {$idx + 1}]] : ""}]
    if {$rule ne "xilinx.com:bd_rule:zynq_ultra_ps_e"} {
        fail "apply_bd_automation: expected only the PS board-preset rule, got '$rule' -- wiring must be explicit (see #64)"
    }
}

proc get_bd_cells {args} { return [join $args ""] }
proc get_bd_pins {args} { return [join $args ""] }
proc get_bd_intf_pins {args} { return [join $args ""] }

proc connect_bd_net {args} { lappend ::connections [join $args " -> "] }
proc connect_bd_intf_net {args} { lappend ::connections [join $args " -> "] }

proc assign_bd_address args {}
proc validate_bd_design args { set ::saw_validate 1 }
proc save_bd_design args { set ::saw_save 1 }

# If someone adds these later, they belong to #65 (synth/utilization) or
# #66 (bitstream after timing closure), not to #64's block design.
proc synth_design args { fail "build_bd.tcl calls synth_design -- that's #65, out of scope here" }
proc write_bitstream args { fail "build_bd.tcl calls write_bitstream -- that's #66, out of scope here" }

# build_bd.tcl ends with a bare `exit <code>`. Renaming the real builtin and
# stubbing `exit` lets that call return here instead of killing tclsh
# before we get to check what happened.
rename ::exit ::_real_exit
proc exit {{code 0}} { set ::exit_code $code }

# --- Placeholder IP so build_bd.tcl gets past its #95 gate ---

set script [file join [file dirname [file normalize [info script]]] build_bd.tcl]
if {![file exists $script]} {
    puts stderr "validate_build_bd.tcl: build_bd.tcl not found at $script"
    _real_exit 1
}

set repo_root   [file normalize [file join [file dirname $script] .. .. ..]]
set ip_repo_dir [file join $repo_root src hls scripts dfs_accel_prj solution1 impl ip]
set placeholder [file join $ip_repo_dir component.xml]
set created_dir [expr {![file exists $ip_repo_dir]}]

file mkdir $ip_repo_dir
set had_placeholder [file exists $placeholder]
if {!$had_placeholder} {
    set f [open $placeholder w]
    puts $f "<!-- placeholder for validate_build_bd.tcl, safe to delete -->"
    close $f
}

try {
    source $script
} finally {
    if {!$had_placeholder} { file delete $placeholder }
    if {$created_dir} { file delete -force $ip_repo_dir }
}

# --- Checks ---

if {[dict get $::set_props board_part] ne "xilinx.com:kv260_som:part0:1.4"} {
    fail "board_part: expected xilinx.com:kv260_som:part0:1.4, got [dict get $::set_props board_part]"
}
if {[dict get $::set_props CONFIG.PSU__USE__M_AXI_GP2] ne "1"} {
    fail "PS: CONFIG.PSU__USE__M_AXI_GP2 must be 1 (exposes M_AXI_HPM0_LPD, the control-path master)"
}
if {[dict get $::set_props CONFIG.PSU__USE__S_AXI_GP2] ne "1"} {
    fail "PS: CONFIG.PSU__USE__S_AXI_GP2 must be 1 (exposes S_AXI_HP0_FPD, the data-path slave)"
}
if {[dict get $::set_props CONFIG.PSU__CRL_APB__PL0_REF_CTRL__FREQMHZ] ne "250"} {
    fail "PS: PL0 clock must be 250 (4 ns), to match what #63 synthesized dfs_accel against"
}

if {[lsearch -exact $::cell_vlnvs "xilinx.com:hls:dfs_accel:1.0"] < 0} {
    fail "dfs_accel IP is never instantiated"
}
if {[lsearch -exact $::cell_vlnvs "xilinx.com:ip:zynq_ultra_ps_e:3.5"] < 0} {
    fail "PS IP is never instantiated"
}

if {$::automation_calls != 1} {
    fail "expected exactly 1 apply_bd_automation call (PS board preset only), got $::automation_calls"
}

set all_connections [join $::connections "\n"]
foreach needle {M_AXI_HPM0_LPD s_axi_control m_axi_gmem0 m_axi_gmem1 m_axi_gmem2 S_AXI_HP0_FPD ap_clk ap_rst_n} {
    if {![string match "*$needle*" $all_connections]} {
        fail "no connection involving '$needle' found -- wiring looks incomplete"
    }
}

if {!$::saw_validate} {
    fail "build_bd.tcl never calls validate_bd_design"
}
if {!$::saw_save} {
    fail "build_bd.tcl never calls save_bd_design"
}
if {$::exit_code ne "0"} {
    fail "build_bd.tcl exited with code '$::exit_code' instead of 0 under a placeholder IP"
}

if {[llength $::errors] > 0} {
    foreach e $::errors { puts stderr "FAIL: $e" }
    puts stderr "[llength $::errors] problem(s) found in build_bd.tcl"
    _real_exit 1
}

puts "build_bd.tcl OK: part/board_part/PS ports/clock match, wiring present, no premature synth/bitstream"
_real_exit 0
