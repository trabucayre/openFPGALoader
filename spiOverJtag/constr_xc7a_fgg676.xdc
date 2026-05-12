set_property CFGBVS VCCO [current_design]
set_property CONFIG_VOLTAGE 3.3 [current_design]
set_property BITSTREAM.CONFIG.SPI_BUSWIDTH {4} [current_design]
set_property BITSTREAM.GENERAL.COMPRESS TRUE [current_design]

# XC7A*-FGG676 SPI flash uses the dedicated configuration bank pins (UG470/UG475).
# CCLK is driven internally via STARTUPE2.
set_property -dict {PACKAGE_PIN C8  IOSTANDARD LVCMOS33} [get_ports {csn}]
set_property -dict {PACKAGE_PIN B19 IOSTANDARD LVCMOS33} [get_ports {sdi_dq0}]
set_property -dict {PACKAGE_PIN A18 IOSTANDARD LVCMOS33} [get_ports {sdo_dq1}]
set_property -dict {PACKAGE_PIN B18 IOSTANDARD LVCMOS33} [get_ports {wpn_dq2}]
set_property -dict {PACKAGE_PIN A19 IOSTANDARD LVCMOS33} [get_ports {hldn_dq3}]

create_clock -period 33.000 -name jtag_tck -waveform {0.000 16.500} [get_pins {bscane2_inst/DRCK}]
create_clock -period 33.000 -name vers_tck -waveform {0.000 16.500} [get_pins {bscane2_version/DRCK}]
