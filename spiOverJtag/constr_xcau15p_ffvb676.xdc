set_property BITSTREAM.GENERAL.COMPRESS TRUE [current_design]
set_property CONFIG_VOLTAGE 1.8 [current_design]
# Table 1-2 from UG570
set_property CFGBVS GND [current_design]

set_property BITSTREAM.CONFIG.SPI_BUSWIDTH 4 [current_design]
set_property CONFIG_MODE SPIx4 [current_design]

# Primary QSPI flash
# Connection done through the STARTUPE3 block