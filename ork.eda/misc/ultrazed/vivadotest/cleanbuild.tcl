puts "what up yo"
set outputDir ./.build
file mkdir $outputDir
read_verilog [ glob ./src/*.v ]
read_xdc ./src/ultrazed.xdc
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
