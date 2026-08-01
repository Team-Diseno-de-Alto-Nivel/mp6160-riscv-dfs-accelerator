# Vivado batch flow: PS<->PL block design integrating dfs_accel (issue #64).
# Run: vivado -mode batch -source src/vivado/scripts/build_bd.tcl
# Board: Kria KV260 (xilinx.com:kv260_som:part0:1.4), part xck26-sfvc784-2LV-c.
#
# Builds and validates the block design only. No synth_design/write_bitstream
# here: resource utilization is #65, timing closure is #66, on-board bring-up
# is #67.
#
# Every connection below is explicit (no connection/block automation beyond
# the PS board preset), so the topology is readable in this file without
# opening Vivado -- most of the team doesn't have a license (see #63/#95).

set script_dir  [file dirname [file normalize [info script]]]
set repo_root   [file normalize [file join $script_dir .. .. ..]]
set hls_dir     [file join $repo_root src hls]

# dfs_accel becomes a Vivado IP via Vitis HLS's export_design. That step is
# gated on #95 (cosim) on purpose -- see run_hls.tcl -- so this script stops
# here instead of wiring a system around an unverified RTL.
set ip_repo_dir [file join $hls_dir scripts dfs_accel_prj solution1 impl ip]
if {![file exists [file join $ip_repo_dir component.xml]]} {
    puts stderr "build_bd.tcl: no packaged IP at $ip_repo_dir"
    puts stderr "build_bd.tcl: run export_design in Vitis HLS first -- gated on #95 (cosim), not on this issue"
    exit 1
}

set project_name "dfs_system"
set project_dir  [file join $repo_root src vivado $project_name]

create_project -force $project_name $project_dir -part xck26-sfvc784-2LV-c
set_property board_part {xilinx.com:kv260_som:part0:1.4} [current_project]

set_property ip_repo_paths $ip_repo_dir [current_project]
update_ip_catalog

create_bd_design "system"

# --- Processing System (ARM) ---
set ps [create_bd_cell -type ip -vlnv xilinx.com:ip:zynq_ultra_ps_e:3.5 zynq_ultra_ps_e_0]

# Board preset only: clock/DDR/voltage defaults for the KV260. Everything
# past this point is wired by hand, on purpose (see #64 discussion).
apply_bd_automation -rule xilinx.com:bd_rule:zynq_ultra_ps_e \
    -config {apply_board_preset "1"} [get_bd_cells $ps]

# M_AXI_HPM0_LPD: PS as master, drives dfs_accel's s_axi_control (AXI4-Lite).
# S_AXI_HP0_FPD:  PS as slave, receives dfs_accel's 3 m_axi masters (gmem0/1/2).
# Verified against the real zynq_ultra_ps_e_v3_5 IP: PSU__USE__M_AXI_GP2 is
# what exposes M_AXI_HPM0_LPD, PSU__USE__S_AXI_GP2 is what exposes
# S_AXI_HP0_FPD. HPC0 (coherent) is deliberately not used -- nothing here
# needs cache coherency with the APU, and HP is the simpler match for a
# PYNQ-allocated non-cacheable buffer in #67.
#
# PL0 clock: raised to 250 MHz (4 ns) to match the clock #63 synthesized
# dfs_accel against. Leaving it at the board preset's 100 MHz default would
# make #66's timing closure test a frequency nobody verified in HLS.
set_property -dict [list \
    CONFIG.PSU__USE__M_AXI_GP2                    {1}   \
    CONFIG.PSU__USE__S_AXI_GP2                     {1}   \
    CONFIG.PSU__CRL_APB__PL0_REF_CTRL__FREQMHZ     {250} \
] $ps

# --- Reset, synchronized to the PL0 clock ---
set rst [create_bd_cell -type ip -vlnv xilinx.com:ip:proc_sys_reset:5.0 proc_sys_reset_0]
connect_bd_net [get_bd_pins $ps/pl_clk0]    [get_bd_pins $rst/slowest_sync_clk]
connect_bd_net [get_bd_pins $ps/pl_resetn0] [get_bd_pins $rst/ext_reset_in]

# --- Accelerator ---
# Port names (s_axi_control, m_axi_gmem0/1/2) follow Vitis HLS's documented
# packaging convention (<axi_type>_<bundle name from the #pragma HLS
# INTERFACE in dfs_accel.cpp>) but are UNVERIFIED against a real exported
# IP, since export_design hasn't run yet (blocked on #95). Recheck these
# against the actual component.xml the day #95 unblocks export_design.
set accel [create_bd_cell -type ip -vlnv xilinx.com:hls:dfs_accel:1.0 dfs_accel_0]
connect_bd_net [get_bd_pins $ps/pl_clk0] [get_bd_pins $accel/ap_clk]
connect_bd_net [get_bd_pins $rst/peripheral_aresetn] [get_bd_pins $accel/ap_rst_n]

# --- Control path: PS -> dfs_accel s_axi_control ---
# SmartConnect converts AXI4 (PS master) <-> AXI4-Lite (accel slave) on its
# own, so no separate axi_protocol_converter is needed here.
set sc_ctrl [create_bd_cell -type ip -vlnv xilinx.com:ip:smartconnect:1.0 smartconnect_ctrl]
set_property CONFIG.NUM_SI {1} $sc_ctrl
set_property CONFIG.NUM_MI {1} $sc_ctrl
connect_bd_net [get_bd_pins $ps/pl_clk0] [get_bd_pins $sc_ctrl/aclk]
connect_bd_net [get_bd_pins $rst/peripheral_aresetn] [get_bd_pins $sc_ctrl/aresetn]
connect_bd_intf_net [get_bd_intf_pins $ps/M_AXI_HPM0_LPD] [get_bd_intf_pins $sc_ctrl/S00_AXI]
connect_bd_intf_net [get_bd_intf_pins $sc_ctrl/M00_AXI] [get_bd_intf_pins $accel/s_axi_control]

# --- Data path: dfs_accel's 3 m_axi masters -> PS S_AXI_HP0_FPD ---
set sc_data [create_bd_cell -type ip -vlnv xilinx.com:ip:smartconnect:1.0 smartconnect_data]
set_property CONFIG.NUM_SI {3} $sc_data
set_property CONFIG.NUM_MI {1} $sc_data
connect_bd_net [get_bd_pins $ps/pl_clk0] [get_bd_pins $sc_data/aclk]
connect_bd_net [get_bd_pins $rst/peripheral_aresetn] [get_bd_pins $sc_data/aresetn]
connect_bd_intf_net [get_bd_intf_pins $accel/m_axi_gmem0] [get_bd_intf_pins $sc_data/S00_AXI]
connect_bd_intf_net [get_bd_intf_pins $accel/m_axi_gmem1] [get_bd_intf_pins $sc_data/S01_AXI]
connect_bd_intf_net [get_bd_intf_pins $accel/m_axi_gmem2] [get_bd_intf_pins $sc_data/S02_AXI]
connect_bd_intf_net [get_bd_intf_pins $sc_data/M00_AXI] [get_bd_intf_pins $ps/S_AXI_HP0_FPD]

assign_bd_address
validate_bd_design

save_bd_design
puts "build_bd.tcl OK: block design '$project_name/system' validated"
exit 0
