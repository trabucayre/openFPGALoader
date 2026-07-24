set_property BITSTREAM.GENERAL.COMPRESS TRUE [current_design]
set_property CONFIG_VOLTAGE 1.8 [current_design]
# Table 1-2 from UG570
set_property CFGBVS GND [current_design]

# Primary QSPI flash
# Connection done through the STARTUPE3 block
# sdi_dq0  - PACKAGE_PIN AK12     - QSPI0_DQ0                 Bank   0 - D00_MOSI_0
# sdo_dq1  - PACKAGE_PIN AJ12     - QSPI0_DQ1                 Bank   0 - D01_DIN_0
# wpn_dq2  - PACKAGE_PIN AL12     - QSPI0_DQ2                 Bank   0 - D02_0
# hldn_dq3 - PACKAGE_PIN AL12     - QSPI0_DQ3                 Bank   0 - D03_0
# csn      - PACKAGE_PIN AG12     - QSPI0_CS_B                Bank   0 - RDWR_FCS_B_0
# sck      - PACKAGE_PIN AG13     - QSPI_CCLK                 Bank   0 - CCLK_0
