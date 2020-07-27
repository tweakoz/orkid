puts "what up yo"
set outputDir ./.build
file mkdir $outputDir
read_verilog src/top.v

#####################################
# Workflow improvement TODO
#####################################
# 1. autogenerate build.tcl (this file) from python infrastructure
# 2. have vivado.genIP() build a constraint list dynamically
# 3. constraints order:
#  correct order (according to Xilinx pdf ug903)
## Timing Assertions Section
#   Primary clocks
#   Virtual clocks
#   Generated clocks
#   Clock Groups
#   Bus Skew constraints
#   Input and output delay constraints
## Timing Exceptions Section
#   False Paths
#   Max Delay / Min Delay
#   Multicycle Paths
#   Case Analysis
#   Disable Timing
## Physical Constraints Section
#   located anywhere in the file, preferably before or after the timing constraints
#   or stored in a separate constraint file
#####################################

#####################################
# vivadotest
#####################################
read_verilog [glob .gen/gth/synth/*.v]
read_verilog [glob .gen/gth/hdl/*.v]
read_xdc src/ultrazed.xdc
#####################################
# SDI-GT
#####################################
read_verilog [glob .gen/sdigt/*.v]
read_verilog [glob .gen/sdigt/hdl/drp_ctrl/*.v]
read_verilog [glob .gen/sdigt/hdl/gt_common/*.v]
read_vhdl [glob .gen/sdigt/hdl/nidru/*.vhd]
#read_verilog [glob .gen/sdigt/ip_0/hdl/*.v]
read_verilog [glob .gen/sdigt/ip_0/synth/uhdsdi*.v]
read_xdc [glob .gen/sdigt/*.xdc]
read_xdc [glob .gen/sdigt/ip_0/synth/*.xdc]
#####################################
# SDI-TX
#####################################
read_verilog [glob .gen/sditx/synth/*.v]
read_verilog [glob .gen/sditx/bd_0/hdl/*.v]
read_verilog [glob .gen/sditx/bd_0/synth/*.v]
read_verilog [glob .gen/sditx/bd_0/ip/ip_0/synth/*.v]
read_verilog [glob .gen/sditx/bd_0/ip/ip_1/synth/*.v]
read_verilog [glob .gen/sditx/bd_0/ip/ip_0/hdl/*.v]
read_vhdl [glob .gen/sditx/bd_0/ip/ip_0/hdl/*.vhd]
# ahem: so much for consistency, Xilinx..
read_verilog [glob .gen/sditx/bd_0/ip/ip_1/hdl/verilog/*.v]
read_xdc [glob .gen/sditx/bd_0/ip/ip_0/*.xdc]
#####################################
#
#####################################
read_verilog .gen/systemclocks/systemclocks.v
read_verilog .gen/systemclocks/systemclocks_clk_wiz.v
read_xdc .gen/systemclocks/systemclocks.xdc
#####################################
#
#####################################
synth_design -top uzedtest -part xczu7ev-fbvb900-1-e -verbose > $outputDir/synth_design.log
write_checkpoint -force $outputDir/post_synth.dcp
opt_design -directive Explore -debug_log > $outputDir/opt_design.log
#power_opt_design
place_design
phys_opt_design -directive AggressiveExplore > $outputDir/physopt_design.log
route_design
write_checkpoint -force $outputDir/post_route.dcp
report_clocks -file $outputDir/clock_summary.txt
report_clock_interaction -file $outputDir/clockinteractions.txt
report_clock_networks -file $outputDir/clocknetworks.txt
report_timing_summary -file $outputDir/timing_summary.txt
report_utilization -file $outputDir/utilization_summary.txt
report_power -file $outputDir/power_summary.txt
write_verilog -force $outputDir/impl_netlist.v
write_bitstream -force $outputDir/uzedtest.bit
