# 
# Project automation script for spiOverJtag_xc6
# 
# Created for ISE version 14.7
# 

set myProject "xilinx_spiOverJtag_xc6"
set myScript "xilinx_spiOverJtag_xc6.tcl"

puts "\n$myScript: Rebuilding ($myProject)...\n"

if { [file exists "${myProject}.xise" ] } {
   project open $myProject
} else {
   project new $myProject

   project set family "Spartan6"
   project set device "xc6slx100"
   project set package "fgg484"
   project set speed "-2"
   project set top_level_module_type "HDL"
   project set synthesis_tool "XST (VHDL/Verilog)"
   project set simulator "ISim (VHDL/Verilog)"
   project set "Preferred Language" "VHDL"
   project set "Enable Message Filtering" "false"
   
   project set "VHDL Source Analysis Standard" "VHDL-200X"
   project set "Enable Internal Done Pipe" "true" -process "Generate Programming File"
   
   xfile add "constr_xc6s_fgg484.ucf"
   xfile add "xilinx_spiOverJtag_xc6.vhd"
   
   project set top "bhv" "xilinx_spiOverJtag"
}

if { ! [ process run "Implement Design" ] } {
   return false;
}
if { ! [ process run "Generate Programming File" ] } {
   return false;
}

puts "Run completed successfully."
project close
