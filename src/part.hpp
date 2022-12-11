// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (C) 2019 Gwenhael Goavec-Merou <gwenhael.goavec-merou@trabucayre.com>
 */

#ifndef PART_HPP
#define PART_HPP

#include <cstdint>
#include <map>
#include <string>

typedef struct {
	std::string manufacturer;
	std::string family;
	std::string model;
	int irlength;
} fpga_model;

/* Highest nibble (version) must always be set to 0 */
static std::map <uint32_t, fpga_model> fpga_list = {
	{0x0a014c35, {"anlogic", "eagle s20", "EG4S20BG256", 8}},
	{0x00004c37, {"anlogic", "elf2", "EF2M45", 8}},

	{0x0362D093, {"xilinx", "artix a7 35t", "xc7a35", 6}},
	{0x0362c093, {"xilinx", "artix a7 50t",  "xc7a50t", 6}},
	{0x03632093, {"xilinx", "artix a7 75t",  "xc7a75t", 6}},
	{0x03631093, {"xilinx", "artix a7 100t", "xc7a100", 6}},
	{0x03636093, {"xilinx", "artix a7 200t", "xc7a200", 6}},

	{0x0364c093, {"xilinx", "kintex7", "xc7k160t", 6}},
	{0x03651093, {"xilinx", "kintex7", "xc7k325t", 6}},
	{0x03656093, {"xilinx", "kintex7", "xc7k410t", 6}},
	{0x23752093, {"xilinx", "kintex7", "xc7k420t", 6}},

	{0x13822093, {"xilinx", "kintexus", "xcku040", 6}},
	{0x13823093, {"xilinx", "kintexus", "xcku035", 6}},

	{0x01414093, {"xilinx", "spartan3", "xc3s200",  6}},
	{0x11c1a093, {"xilinx", "spartan3e", "xc3s250e", 6}},
	{0x01c22093, {"xilinx", "spartan3e", "xc3s500e", 6}},

	{0x04001093, {"xilinx", "spartan6", "xc6slx9",  6}},
	{0x04002093, {"xilinx", "spartan6", "xc6slx16", 6}},
	{0x04004093, {"xilinx", "spartan6", "xc6slx25", 6}},
	{0x04008093, {"xilinx", "spartan6", "xc6slx45", 6}},
	{0x04011093, {"xilinx", "spartan6", "xc6slx100", 6}},
	{0x4403d093, {"xilinx", "spartan6", "xc6slx150T", 6}},
	{0x03620093, {"xilinx", "spartan7", "xc7s15ftgb196-1", 6}},
	{0x037c4093, {"xilinx", "spartan7", "xc7s25", 6}},
	{0x0362f093, {"xilinx", "spartan7", "xc7s50", 6}},

	{0x06e1c093, {"xilinx", "xc2c",     "xc2c32a",   8}},
	{0x09602093, {"xilinx", "xc9500xl", "xc9536xl",  8}},
	{0x09604093, {"xilinx", "xc9500xl", "xc9572xl",  8}},
	{0x09608093, {"xilinx", "xc9500xl", "xc95144xl", 8}},
	{0x09616093, {"xilinx", "xc9500xl", "xc95288xl", 8}},

	{0x05044093, {"xilinx", "xcf",      "xcf01s",    8}},
	{0x05045093, {"xilinx", "xcf",      "xcf02s",    8}},
	{0x05046093, {"xilinx", "xcf",      "xcf04s",    8}},

	{0x03722093, {"xilinx", "zynq",     "xc7z010", 6}},
	{0x03727093, {"xilinx", "zynq",     "xc7z020", 6}},
	{0x23731093, {"xilinx", "zynq",     "xc7z045", 6}},

	{0x04b31093, {"xilinx", "virtexusp", "xcvu9p", 6}},

	/* When powering a zynq ultrascale+ MPSoC, PL Tap and ARM dap
	 * are disabled and only PS tap with a specific IDCODE is seen.
	 * 0x03 must be written into JTAG_CTRL followed by RTI and
	 * a new scan to discover PL TAP and ARM DAP
	 */
	{0x08e22126, {"xilinx", "zynqmp_cfgn", "xczu2cg", 4}},
	{0x08e70126, {"xilinx", "zynqmp_cfgn", "xczu9eg", 4}},
	{0x08e80126, {"xilinx", "zynqmp_cfgn","xczu11eg", 4}},
	{0x08e60126, {"xilinx", "zynqmp_cfgn", "xczu7ev", 4}},

	{0x04711093, {"xilinx", "zynqmp",      "xczu2cg", 6}},
	{0x04738093, {"xilinx", "zynqmp",      "xczu9eg", 6}},
	{0x04740093, {"xilinx", "zynqmp",     "xczu11eg", 6}},
	{0x04730093, {"xilinx", "zynqmp",      "xczu7ev", 6}},

	{0x020f20dd, {"altera", "cyclone III/IV", "EP3C16/EP4CE15", 10}},
	{0x020f70dd, {"altera", "cyclone IV", "EP4CE115", 10}},

	{0x020f30dd, {"altera", "cyclone 10 LP", "10CL025", 10}},

	{0x02b140dd, {"altera", "cyclone V", "5CEBA9", 10}},
	{0x02b150dd, {"altera", "cyclone V", "5CEA2", 10}},
	{0x02b050dd, {"altera", "cyclone V", "5CEBA4", 10}},
	{0x02b220dd, {"altera", "cyclone V", "5CEFA5", 10}},
	{0x02d020dd, {"altera", "cyclone V Soc", "5CSEBA6", 10}},
	{0x02d010dd, {"altera", "cyclone V Soc", "5CSEMA4", 10}},
	{0x02d120dd, {"altera", "cyclone V Soc", "5CSEMA5", 10}},

	{0x00000001, {"efinix", "Trion",    "T4/T8",            4}},
	{0x00210a79, {"efinix", "Trion",    "T8QFP144/T13/T20", 4}},
	{0x00220a79, {"efinix", "Trion",    "T55/T85/T120",     4}},
	{0x00240a79, {"efinix", "Trion",    "T20BGA324/T35",    4}},
	{0x00660a79, {"efinix", "Titanium", "Ti60",             4}},
	{0x00360a79, {"efinix", "Titanium", "Ti60ES",           4}},
	{0x00661a79, {"efinix", "Titanium", "Ti35",             4}},

	{0x010F0043, {"lattice", "CrosslinkNX", "LIFCL-17", 8}},
	{0x010F1043, {"lattice", "CrosslinkNX", "LIFCL-40", 8}},

	{0x010F0043, {"lattice", "CertusNX", "LFD2NX-17", 8}},
	{0x010F1043, {"lattice", "CertusNX", "LFD2NX-40", 8}},

	{0x012b9043, {"lattice", "MachXO2",   "LCMXO2-640HC", 8}},
	{0x012ba043, {"lattice", "MachXO2",   "LCMXO2-1200HC", 8}},
	{0x012bd043, {"lattice", "MachXO2",   "LCMXO2-7000HC", 8}},
	{0x012b5043, {"lattice", "MachXO2",   "LCMXO2-7000HE", 8}},

	{0x012BB043, {"lattice", "MachXO3LF", "LCMX03LF-1300C", 8}},
	{0x012B2043, {"lattice", "MachXO3LF", "LCMX03LF-1300E", 8}},
	{0x012BB043, {"lattice", "MachXO3LF", "LCMX03LF-2100C", 8}},
	{0x012B3043, {"lattice", "MachXO3LF", "LCMX03LF-2100E", 8}},
	{0x012BC043, {"lattice", "MachXO3LF", "LCMX03LF-4300C", 8}},
	{0x012B4043, {"lattice", "MachXO3LF", "LCMX03LF-4300E", 8}},
	{0x012BD043, {"lattice", "MachXO3LF", "LCMX03LF-6900C", 8}},
	{0x012B5043, {"lattice", "MachXO3LF", "LCMX03LF-6900E", 8}},
	{0x012BE043, {"lattice", "MachXO3LF", "LCMX03LF-9400C", 8}},
	{0x012B6043, {"lattice", "MachXO3LF", "LCMX03LF-9400E", 8}},

	{0x012e3043, {"lattice", "MachXO3D", "LCMX03D-9400HC", 8}},

	{0x21111043, {"lattice", "ECP5", "LFE5U-12", 8}},
	{0x41111043, {"lattice", "ECP5", "LFE5U-25", 8}},
	{0x41112043, {"lattice", "ECP5", "LFE5U-45", 8}},
	{0x41113043, {"lattice", "ECP5", "LFE5U-85", 8}},
	{0x01111043, {"lattice", "ECP5", "LFE5UM-25", 8}},
	{0x01112043, {"lattice", "ECP5", "LFE5UM-45", 8}},
	{0x01113043, {"lattice", "ECP5", "LFE5UM-85", 8}},
	{0x81111043, {"lattice", "ECP5", "LFE5UM5G-25", 8}},
	{0x81112043, {"lattice", "ECP5", "LFE5UM5G-45", 8}},
	{0x81113043, {"lattice", "ECP5", "LFE5UM5G-85", 8}},

	{0x0129a043, {"lattice", "XP2", "LFXP2-8E", 8}},

	{0x0100481b, {"Gowin", "GW1N", "GW1N(R)-9C", 8}},
	{0x0100581b, {"Gowin", "GW1N", "GW1NR-9", 8}},
	{0x0900281B, {"Gowin", "GW1N", "GW1N-1", 8}},
	{0x0120681b, {"Gowin", "GW1N", "GW1N-2", 8}},
	{0x0100381B, {"Gowin", "GW1N", "GW1N-4", 8}},
	{0x0100681b, {"Gowin", "GW1NZ", "GW1NZ-1", 8}},
	{0x0300181b, {"Gowin", "GW1NS", "GW1NS-2C", 8}},
	{0x0100981b, {"Gowin", "GW1NSR", "GW1NSR-4C", 8}},
	{0x0000081b, {"Gowin", "GW2A", "GW2A(R)-18(C)", 8}},

	/* keep highest nibble to prevent confusion with Efinix T4/T8 IDCODE */
	{0x20000001, {"colognechip", "GateMate Series", "GM1Ax", 6}},
};

/* device potentially in JTAG chain but not handled */
typedef struct {
	std::string name;
	int irlength;
} misc_device;

static std::map <uint32_t, misc_device> misc_dev_list = {
	{0x4ba00477, {"ARM cortex A9",         4}},
	{0x5ba00477, {"ARM cortex A53",        4}},
	{0xfffffffe, {"ZynqMP dummy device",   12}},
};

/* list of JTAG manufacturer ID */
static std::map <uint16_t, std::string> list_manufacturer = {
	{0x000, "CologneChip or efinix trion T4/T8"},
	{0x021, "lattice"},
	{0x049, "Xilinx"},
	{0x06e, "altera"},
	{0x093, "Xilinx"},  //  ZynqMP not configured
	{0x40d, "Gowin"},
	{0x53c, "efinix"},
	{0x61a, "anlogic"},
	{0x61b, "anlogic"},  // yes two manufacturer id for anlogic
};

#define IDCODE2MANUFACTURERID(_idcode) ((_idcode >>  1) & 0x7ff)
#define IDCODE2PART(_idcode)           ((_idcode >> 21) & 0x07f)
#define IDCODE2VERS(_idcode)           ((_idcode >> 28) & 0x00f)

#endif
