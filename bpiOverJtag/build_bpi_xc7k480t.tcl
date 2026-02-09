# Vivado TCL script to build bpiOverJtag for xc7k480tffg1156
# Run with: vivado -mode batch -source build_bpi_xc7k480t.tcl

set part "xc7k480tffg1156-2"
set project_name "bpiOverJtag_xc7k480tffg1156"
set output_dir "./output_bpi_xc7k480t"

# Create output directory
file mkdir $output_dir

# Create in-memory project
create_project -in_memory -part $part

# Add source files
read_verilog bpiOverJtag_core.v
read_verilog xilinx_bpiOverJtag.v

# Add constraints
read_xdc constr_xc7k480t_bpi_ffg1156.xdc

# Synthesize
synth_design -top bpiOverJtag -part $part

# Optimize
opt_design

# Place
place_design

# Route
route_design

# Generate reports
report_utilization -file $output_dir/utilization.rpt
report_timing_summary -file $output_dir/timing.rpt

# Write bitstream
write_bitstream -force $output_dir/$project_name.bit

# Close project
close_project

puts "Bitstream generated: $output_dir/$project_name.bit"
puts ""
puts "To install, run:"
puts "  gzip -c $output_dir/$project_name.bit > bpiOverJtag_xc7k480tffg1156.bit.gz"
