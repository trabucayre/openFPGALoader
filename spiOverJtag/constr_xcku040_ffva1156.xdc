set_property BITSTREAM.GENERAL.COMPRESS TRUE [current_design]
set_property CONFIG_VOLTAGE 1.8 [current_design]
# Table 1-5 from UG917
set_property CFGBVS GND [current_design]

# Primary QSPI flash
# Connection done through the STARTUPE3 block
# sdi_dq0  - PACKAGE_PIN AC7      - QSPI0_IO0                 Bank   0 - D00_MOSI_0
# sdo_dq1  - PACKAGE_PIN AB7      - QSPI0_IO1                 Bank   0 - D01_DIN_0
# wpn_dq2  - PACKAGE_PIN AA7      - QSPI0_IO2                 Bank   0 - D02_0
# hldn_dq3 - PACKAGE_PIN Y7       - QSPI0_IO3                 Bank   0 - D03_0
# csn      - PACKAGE_PIN U7       - QSPI0_CSB                 Bank   0 - RDWR_FCS_B_0
# sck      - PACKAGE_PIN AA9      - FPGA_CCLK                 Bank   0 - CCLK_0

# Secondary QSPI flash
set_property PACKAGE_PIN M20       [get_ports "sdi_sec_dq0"] ;# Bank  65 VCCO - VCC1V8   - IO_L22P_T3U_N6_DBC_AD0P_D04_65
set_property IOSTANDARD  LVCMOS18  [get_ports "sdi_sec_dq0"] ;# Bank  65 VCCO - VCC1V8   - IO_L22P_T3U_N6_DBC_AD0P_D04_65
set_property PACKAGE_PIN L20       [get_ports "sdo_sec_dq1"] ;# Bank  65 VCCO - VCC1V8   - IO_L22N_T3U_N7_DBC_AD0N_D05_65
set_property IOSTANDARD  LVCMOS18  [get_ports "sdo_sec_dq1"] ;# Bank  65 VCCO - VCC1V8   - IO_L22N_T3U_N7_DBC_AD0N_D05_65
set_property PACKAGE_PIN R21       [get_ports "wpn_sec_dq2"] ;# Bank  65 VCCO - VCC1V8   - IO_L21P_T3L_N4_AD8P_D06_65
set_property IOSTANDARD  LVCMOS18  [get_ports "wpn_sec_dq2"] ;# Bank  65 VCCO - VCC1V8   - IO_L21P_T3L_N4_AD8P_D06_65
set_property PACKAGE_PIN R22       [get_ports "hldn_sec_dq3"] ;# Bank  65 VCCO - VCC1V8   - IO_L21N_T3L_N5_AD8N_D07_65
set_property IOSTANDARD  LVCMOS18  [get_ports "hldn_sec_dq3"] ;# Bank  65 VCCO - VCC1V8   - IO_L21N_T3L_N5_AD8N_D07_65
set_property PACKAGE_PIN G26       [get_ports "csn_sec"] ;# Bank  65 VCCO - VCC1V8   - IO_L2N_T0L_N3_FWE_FCS2_B_65
set_property IOSTANDARD  LVCMOS18  [get_ports "csn_sec"] ;# Bank  65 VCCO - VCC1V8   - IO_L2N_T0L_N3_FWE_FCS2_B_65
