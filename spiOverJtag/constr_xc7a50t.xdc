set_property CFGBVS VCCO [current_design]
set_property CONFIG_VOLTAGE 3.3 [current_design]
set_property BITSTREAM.CONFIG.SPI_BUSWIDTH {4} [current_design]
set_property BITSTREAM.GENERAL.COMPRESS TRUE [current_design]
set_property -dict {PACKAGE_PIN K19 IOSTANDARD LVCMOS33} [get_ports {csn}];
set_property -dict {PACKAGE_PIN D18 IOSTANDARD LVCMOS33} [get_ports {sdi_dq0}];
set_property -dict {PACKAGE_PIN D19 IOSTANDARD LVCMOS33} [get_ports {sdo_dq1}];
set_property -dict {PACKAGE_PIN G18 IOSTANDARD LVCMOS33} [get_ports {wpn_dq2}];
set_property -dict {PACKAGE_PIN F18 IOSTANDARD LVCMOS33} [get_ports {hldn_dq3}];

