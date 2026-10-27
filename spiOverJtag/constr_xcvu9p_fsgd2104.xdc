set_property BITSTREAM.GENERAL.COMPRESS TRUE [current_design]
set_property CONFIG_VOLTAGE 1.8 [current_design]
# Table 1-2 from UG570
set_property CFGBVS GND [current_design]

# Primary QSPI flash
# Connection done through the STARTUPE3 block

# Secondary QSPI flash
set_property PACKAGE_PIN AL25     [get_ports "sdi_sec_dq0"];
set_property IOSTANDARD  LVCMOS18 [get_ports "sdi_sec_dq0"];
set_property PACKAGE_PIN AM25     [get_ports "sdo_sec_dq1"];
set_property IOSTANDARD  LVCMOS18 [get_ports "sdo_sec_dq1"];
set_property PACKAGE_PIN AP23     [get_ports "wpn_sec_dq2"];
set_property IOSTANDARD  LVCMOS18 [get_ports "wpn_sec_dq2"];
set_property PACKAGE_PIN AP24     [get_ports "hldn_sec_dq3"];
set_property IOSTANDARD  LVCMOS18 [get_ports "hldn_sec_dq3"];
set_property PACKAGE_PIN BD23     [get_ports "csn_sec"];
set_property IOSTANDARD  LVCMOS18 [get_ports "csn_sec"];
