set_property BITSTREAM.GENERAL.COMPRESS TRUE [current_design]
set_property BITSTREAM.CONFIG.CONFIGRATE 16 [current_design]

set_property CONFIG_VOLTAGE 1.8 [current_design]
set_property CFGBVS GND [current_design]

set_property CONFIG_MODE SPIx4 [current_design]
set_property BITSTREAM.CONFIG.SPI_32BIT_ADDR NO [current_design]
set_property BITSTREAM.CONFIG.SPI_BUSWIDTH 4 [current_design]
set_property BITSTREAM.CONFIG.M1PIN PULLNONE [current_design]
set_property BITSTREAM.CONFIG.M2PIN PULLNONE [current_design]
set_property BITSTREAM.CONFIG.M0PIN PULLNONE [current_design]

set_property BITSTREAM.CONFIG.USR_ACCESS TIMESTAMP [current_design]
set_property BITSTREAM.CONFIG.UNUSEDPIN PULLDOWN [current_design]
set_property BITSTREAM.CONFIG.OVERTEMPPOWERDOWN ENABLE [current_design]

set_property -dict {PACKAGE_PIN L15 IOSTANDARD SSTL135_R} [get_ports csn]
set_property -dict {PACKAGE_PIN K16 IOSTANDARD SSTL135_R} [get_ports sdi_dq0]
set_property -dict {PACKAGE_PIN L17 IOSTANDARD SSTL135_R} [get_ports sdo_dq1]
set_property -dict {PACKAGE_PIN J15 IOSTANDARD SSTL135_R} [get_ports wpn_dq2]
set_property -dict {PACKAGE_PIN J16 IOSTANDARD SSTL135_R} [get_ports hldn_dq3]

