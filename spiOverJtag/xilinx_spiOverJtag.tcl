set model [lindex $argv 0]

set project_name "spiOverJtag_${model}"

set build_path tmp
file delete -force $build_path

# Project creation
create_project $project_name $build_path -part xc7a35ticsg324-1L

add_files -norecurse xilinx_spiOverJtag.vhd
add_files -norecurse -fileset constrs_1 constr.xdc

set_property VERILOG_DEFINE {TOOL_VIVADO} [current_fileset]

# set the current synth run
current_run -synthesis [get_runs synth_1]
reset_run synth_1

set obj [get_runs impl_1]
set_property AUTO_INCREMENTAL_CHECKPOINT 1 [get_runs impl_1]

set_property "needs_refresh" "1" $obj

# set the current impl run
current_run -implementation [get_runs impl_1]

puts "INFO: Project created: $project_name"

launch_runs synth_1 -jobs 4
wait_on_run synth_1
## do implementation
launch_runs impl_1 -jobs 4
wait_on_run impl_1
## make bit file
launch_runs impl_1 -jobs 4 -to_step write_bitstream
wait_on_run impl_1
exit
