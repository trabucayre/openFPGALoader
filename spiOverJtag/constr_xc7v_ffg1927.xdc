set_property BITSTREAM.GENERAL.COMPRESS TRUE [current_design]
set_property CONFIG_VOLTAGE 1.8 [current_design]
# Table 1-2 from UG570
set_property CFGBVS GND [current_design]
set_property BITSTREAM.CONFIG.CONFIGRATE 33 [current_design]

set_property -dict {PACKAGE_PIN BA29 IOSTANDARD LVCMOS18} [get_ports {csn}]
set_property -dict {PACKAGE_PIN BC27 IOSTANDARD LVCMOS18} [get_ports {sdi_dq0}]
set_property -dict {PACKAGE_PIN BD27 IOSTANDARD LVCMOS18} [get_ports {sdo_dq1}]
set_property -dict {PACKAGE_PIN BD29 IOSTANDARD LVCMOS18} [get_ports {wpn_dq2}]
set_property -dict {PACKAGE_PIN BD30 IOSTANDARD LVCMOS18} [get_ports {hldn_dq3}]
