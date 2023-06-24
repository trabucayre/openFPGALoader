set_property BITSTREAM.GENERAL.COMPRESS TRUE [current_design]
set_property CONFIG_VOLTAGE 1.8 [current_design]
# Table 1-2 from UG570
set_property CFGBVS GND [current_design]

# Primary QSPI flash
# Connection done through the STARTUPE3 block
# sdi_dq0  - PACKAGE_PIN AP11     - QSPI0_DQ0                 Bank   0 - D00_MOSI_0
# sdo_dq1  - PACKAGE_PIN AN11     - QSPI0_DQ1                 Bank   0 - D01_DIN_0
# wpn_dq2  - PACKAGE_PIN AM11     - QSPI0_DQ2                 Bank   0 - D02_0
# hldn_dq3 - PACKAGE_PIN AL11     - QSPI0_DQ3                 Bank   0 - D03_0
# csn      - PACKAGE_PIN AJ11     - QSPI0_CS_B                Bank   0 - RDWR_FCS_B_0
# sck      - PACKAGE_PIN AF13     - QSPI_CCLK                 Bank   0 - CCLK_0

# Secondary QSPI flash
set_property PACKAGE_PIN AM19     [get_ports "sdi_sec_dq0"];
set_property IOSTANDARD  LVCMOS18 [get_ports "sdi_sec_dq0"];
set_property PACKAGE_PIN AM18     [get_ports "sdo_sec_dq1"];
set_property IOSTANDARD  LVCMOS18 [get_ports "sdo_sec_dq1"];
set_property PACKAGE_PIN AN20     [get_ports "wpn_sec_dq2"];
set_property IOSTANDARD  LVCMOS18 [get_ports "wpn_sec_dq2"];
set_property PACKAGE_PIN AP20     [get_ports "hldn_sec_dq3"];
set_property IOSTANDARD  LVCMOS18 [get_ports "hldn_sec_dq3"];
set_property PACKAGE_PIN BF16     [get_ports "csn_sec"];
set_property IOSTANDARD  LVCMOS18 [get_ports "csn_sec"];
