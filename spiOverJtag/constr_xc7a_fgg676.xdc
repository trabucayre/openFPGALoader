set_property CFGBVS VCCO [current_design]
set_property CONFIG_VOLTAGE 3.3 [current_design]
set_property BITSTREAM.CONFIG.SPI_BUSWIDTH {4} [current_design]
set_property BITSTREAM.GENERAL.COMPRESS TRUE [current_design]

# XC7A75T/100T/200T-FGG676 SPI flash pins on QMTech XC7A100T-2FGG676I core board.
# Verified against QMTECH_XC7A75T_100T_200T-CORE-BOARD-V01-20210109.pdf schematic
# (https://github.com/ChinaQMTECH/QMTECH_XC7A75T-100T-200T_Core_Board).
# These are Bank 14 dual-purpose user IO pins (D00..D03, FCS_B) wired to the
# on-board N25Q064A SPI flash (JEDEC 0x20BA17). CCLK is driven internally via
# the STARTUPE2 primitive instantiated in xilinx_spiOverJtag.v.
set_property -dict {PACKAGE_PIN P18 IOSTANDARD LVCMOS33} [get_ports {csn}]
set_property -dict {PACKAGE_PIN R14 IOSTANDARD LVCMOS33} [get_ports {sdi_dq0}]
set_property -dict {PACKAGE_PIN R15 IOSTANDARD LVCMOS33} [get_ports {sdo_dq1}]
set_property -dict {PACKAGE_PIN P14 IOSTANDARD LVCMOS33} [get_ports {wpn_dq2}]
set_property -dict {PACKAGE_PIN N14 IOSTANDARD LVCMOS33} [get_ports {hldn_dq3}]

create_clock -period 33.000 -name jtag_tck -waveform {0.000 16.500} [get_pins {bscane2_inst/DRCK}]
create_clock -period 33.000 -name vers_tck -waveform {0.000 16.500} [get_pins {bscane2_version/DRCK}]
