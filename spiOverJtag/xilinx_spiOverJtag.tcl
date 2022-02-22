set parts [lindex $argv 0]

set project_name "spiOverJtag"

set build_path tmp_${parts}
file delete -force $build_path

# Project creation
set grade [dict create \
	xc7a35tcpg236  -1 \
	xc7a35tcsg324  -1 \
	xc7a35tftg256  -1 \
	xc7a50tcpg236  -2 \
	xc7a75tfgg484  -2 \
	xc7a100tfgg484 -2 \
	xc7a200tsbg484 -1 \
	xc7k325tffg676 -1 \
	xc7k325tffg900 -2 \
	xc7s50csga324  -1 \
	]

set pkg_name [dict create \
	xc7a35tcpg236  xc7a_cpg236 \
	xc7a35tcsg324  xc7a_csg324 \
	xc7a35tftg256  xc7a_ftg256 \
	xc7a50tcpg236  xc7a_cpg236 \
	xc7a75tfgg484  xc7a_fgg484 \
	xc7a100tfgg484 xc7a_fgg484 \
	xc7a200tsbg484 xc7a_sbg484 \
	xc7a200tfbg484 xc7a_fbg484 \
	xc7k325tffg676 xc7k_ffg676 \
	xc7k325tffg900 xc7k_ffg900 \
	xc7s50csga324  xc7s_csga324 \
	]

set curr_grade [dict get $grade $parts]
set curr_pins  [dict get $pkg_name $parts]

create_project $project_name $build_path -part ${parts}${curr_grade}

add_files -norecurse xilinx_spiOverJtag.vhd
add_files -norecurse -fileset constrs_1 constr_${curr_pins}.xdc

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
