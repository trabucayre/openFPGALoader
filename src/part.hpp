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
	/**************************************************************************/
	/*                            Anlogic                                     */
	/**************************************************************************/

	/* Anlogic Eagle */
	{0x04014c35, {"anlogic", "eagle d20", "EG4D20EG176", 8}},
	{0x0a014c35, {"anlogic", "eagle s20", "EG4S20BG256", 8}},

	/* Anlogic Elf2 */
	{0x00004c37, {"anlogic", "elf2",      "EF2M45", 8}},

	/**************************************************************************/
	/*                             Xilinx                                     */
	/**************************************************************************/

	/* Xilinx XCF */
	{0x05044093, {"xilinx", "xcf",      "xcf01s",    8}},
	{0x05045093, {"xilinx", "xcf",      "xcf02s",    8}},
	{0x05046093, {"xilinx", "xcf",      "xcf04s",    8}},

	/* Xilinx XC2 */
	{0x06e59093, {"xilinx", "xc2c",     "xc2c64a",   8}},
	{0x06e5d093, {"xilinx", "xc2c",     "xc2c64a",   8}},
	{0x06e5e093, {"xilinx", "xc2c",     "xc2c64a",   8}},
	{0x06e1c093, {"xilinx", "xc2c",     "xc2c32a",   8}},

	/* Xilinx XC9 */
	{0x09602093, {"xilinx", "xc9500xl", "xc9536xl",  8}},
	{0x09604093, {"xilinx", "xc9500xl", "xc9572xl",  8}},
	{0x09608093, {"xilinx", "xc9500xl", "xc95144xl", 8}},
	{0x09616093, {"xilinx", "xc9500xl", "xc95288xl", 8}},

	/* Xilinx Spartan3 */
	{0x01414093, {"xilinx", "spartan3",  "xc3s200",  6}},
	{0x11c1a093, {"xilinx", "spartan3e", "xc3s250e", 6}},
	{0x01c22093, {"xilinx", "spartan3e", "xc3s500e", 6}},

	/* Xilinx Spartan6 */
	{0x04001093, {"xilinx", "spartan6", "xc6slx9",         6}},
	{0x04002093, {"xilinx", "spartan6", "xc6slx16",        6}},
	{0x04004093, {"xilinx", "spartan6", "xc6slx25",        6}},
	{0x24024093, {"xilinx", "spartan6", "xc6slx25T",       6}},
	{0x04008093, {"xilinx", "spartan6", "xc6slx45",        6}},
	{0x04028093, {"xilinx", "spartan6", "xc6slx45T",       6}},
	{0x04011093, {"xilinx", "spartan6", "xc6slx100",       6}},
	{0x4403d093, {"xilinx", "spartan6", "xc6slx150T",      6}},

	/* Xilinx Spartan7 */
	{0x03622093, {"xilinx", "spartan7", "xc7s6",           6}},
	{0x03620093, {"xilinx", "spartan7", "xc7s15ftgb196-1", 6}},
	{0x037c4093, {"xilinx", "spartan7", "xc7s25",          6}},
	{0x0362f093, {"xilinx", "spartan7", "xc7s50",          6}},
	{0x037c8093, {"xilinx", "spartan7", "xc7s75",          6}},

	/* Xilinx Virtex6 */
	{0x8424a093, {"xilinx", "virtex6", "xc6vlx130t", 10}},

	/* Xilinx 7-Series / Artix7 */
	{0x0362e093, {"xilinx", "artix a7 15t",  "xc7a15",  6}},
	{0x037c2093, {"xilinx", "artix a7 25t",  "xc7a25",  6}},
	{0x0362D093, {"xilinx", "artix a7 35t",  "xc7a35",  6}},
	{0x0362c093, {"xilinx", "artix a7 50t",  "xc7a50t", 6}},
	{0x03632093, {"xilinx", "artix a7 75t",  "xc7a75t", 6}},
	{0x03631093, {"xilinx", "artix a7 100t", "xc7a100", 6}},
	{0x03636093, {"xilinx", "artix a7 200t", "xc7a200", 6}},

	/* Xilinx 7-Series / Kintex7 */
	{0x03647093, {"xilinx", "kintex7", "xc7k70t",  6}},
	{0x0364c093, {"xilinx", "kintex7", "xc7k160t", 6}},
	{0x03651093, {"xilinx", "kintex7", "xc7k325t", 6}},
	{0x03656093, {"xilinx", "kintex7", "xc7k410t", 6}},
	{0x23752093, {"xilinx", "kintex7", "xc7k420t", 6}},
	{0x23751093, {"xilinx", "kintex7", "xc7k480t", 6}},

	/* Xilinx 7-Series / Virtex7 */
	{0x03667093, {"xilinx", "virtex7", "xc7vx330t",  6}},
	{0x33691093, {"xilinx", "virtex7", "xc7vx690t",  6}},

	/* Xilinx 7-Series / Zynq */
	{0x03722093, {"xilinx", "zynq",     "xc7z010", 6}},
	{0x03727093, {"xilinx", "zynq",     "xc7z020", 6}},
	{0x1372c093, {"xilinx", "zynq",     "xc7z030", 6}},
	{0x23731093, {"xilinx", "zynq",     "xc7z045", 6}},
	{0x03736093, {"xilinx", "zynq",     "xc7z100", 6}},

	/* Xilinx Ultrascale / Kintex */
	{0x13823093, {"xilinx", "kintexus", "xcku035", 6}},
	{0x13822093, {"xilinx", "kintexus", "xcku040", 6}},
	{0x13919093, {"xilinx", "kintexus", "xcku060", 6}},
	{0x1390d093, {"xilinx", "kintexus", "xcku115", 6}},

	/* Xilinx Ultrascale / Virtex */
	{0x03842093, {"xilinx", "virtexus", "xcvu095", 6}},

	/* Xilinx Ultrascale+ / Artix */
	{0x04AC2093, {"xilinx", "artixusp", "xcau15p", 6}},
	{0x04A64093, {"xilinx", "artixusp", "xcau25p", 6}},

	/* Xilinx Ultrascale+ / Kintex */
	{0x04a63093, {"xilinx", "kintexusp", "xcku3p", 6}},
	{0x04a62093, {"xilinx", "kintexusp", "xcku5p", 6}},

	/* Xilinx Ultrascale+ / Virtex */
	{0x04b31093, {"xilinx", "virtexusp", "xcvu9p",  18}},
	{0x14b79093, {"xilinx", "virtexusp", "xcvu37p", 18}},

	/* Xilinx Ultrascale+ / ZynqMP */
	/* When powering a zynq ultrascale+ MPSoC, PL Tap and ARM dap
	 * are disabled and only PS tap with a specific IDCODE is seen.
	 * 0x03 must be written into JTAG_CTRL followed by RTI and
	 * a new scan to discover PL TAP and ARM DAP
	 */
	{0x08e22126, {"xilinx", "zynqmp_cfgn",  "xczu2cg/eg", 4}},
	{0x08e20126, {"xilinx", "zynqmp_cfgn",  "xczu3cg/eg", 4}},
	{0x08e42126, {"xilinx", "zynqmp_cfgn",  "xczu4cg/eg/ev", 4}},
	{0x08e40126, {"xilinx", "zynqmp_cfgn",  "xczu5cg/eg/ev", 4}},
	{0x08e72126, {"xilinx", "zynqmp_cfgn",  "xczu6cg/eg", 4}},
	{0x08e60126, {"xilinx", "zynqmp_cfgn",  "xczu7cg/eg/ev", 4}},
	{0x08e70126, {"xilinx", "zynqmp_cfgn",  "xczu9eg", 4}},
	{0x08e80126, {"xilinx", "zynqmp_cfgn", "xczu11eg", 4}},
	{0x08ea0126, {"xilinx", "zynqmp_cfgn", "xczu15eg", 4}},
	{0x08eb2126, {"xilinx", "zynqmp_cfgn", "xczu17eg", 4}},
	{0x08eb0126, {"xilinx", "zynqmp_cfgn", "xczu19eg", 4}},
	{0x08fc2126, {"xilinx", "zynqmp_cfgn", "xczu21dr", 4}},
	{0x08fca126, {"xilinx", "zynqmp_cfgn", "xczu25dr", 4}},
	{0x08fc8126, {"xilinx", "zynqmp_cfgn", "xczu27dr", 4}},
	{0x08fc0126, {"xilinx", "zynqmp_cfgn", "xczu28dr", 4}},
	{0x08fc4126, {"xilinx", "zynqmp_cfgn", "xczu29dr", 4}},
	{0x08fcc126, {"xilinx", "zynqmp_cfgn", "xczu39dr", 4}},
	{0x08ffa126, {"xilinx", "zynqmp_cfgn", "xczu43dr", 4}},
	{0x08ff0126, {"xilinx", "zynqmp_cfgn", "xczu46dr", 4}},
	{0x08ffe126, {"xilinx", "zynqmp_cfgn", "xczu47dr", 4}},
	{0x08ff6126, {"xilinx", "zynqmp_cfgn", "xczu48dr", 4}},
	{0x08ffc126, {"xilinx", "zynqmp_cfgn", "xczu49dr", 4}},

	{0x04711093, {"xilinx", "zynqmp",       "xczu2cg/eg", 6}},
	{0x04710093, {"xilinx", "zynqmp",       "xczu3cg/eg", 6}},
	{0x04721093, {"xilinx", "zynqmp",       "xczu4cg/eg/ev", 6}},
	{0x04720093, {"xilinx", "zynqmp",       "xczu5cg/eg/ev", 6}},
	{0x04739093, {"xilinx", "zynqmp",       "xczu6cg/eg", 6}},
	{0x04730093, {"xilinx", "zynqmp",       "xczu7cg/eg/ev", 6}},
	{0x04738093, {"xilinx", "zynqmp",       "xczu9eg", 6}},
	{0x04740093, {"xilinx", "zynqmp",      "xczu11eg", 6}},
	{0x04750093, {"xilinx", "zynqmp",      "xczu15eg", 6}},
	{0x04759093, {"xilinx", "zynqmp",      "xczu17eg", 6}},
	{0x04758093, {"xilinx", "zynqmp",      "xczu19eg", 6}},
	{0x047e1093, {"xilinx", "zynqmp",      "xczu21dr", 6}},
	{0x047e5093, {"xilinx", "zynqmp",      "xczu25dr", 6}},
	{0x047e4093, {"xilinx", "zynqmp",      "xczu27dr", 6}},
	{0x047e0093, {"xilinx", "zynqmp",      "xczu28dr", 6}},
	{0x047e2093, {"xilinx", "zynqmp",      "xczu29dr", 6}},
	{0x047e6093, {"xilinx", "zynqmp",      "xczu39dr", 6}},
	{0x047fd093, {"xilinx", "zynqmp",      "xczu43dr", 6}},
	{0x047f8093, {"xilinx", "zynqmp",      "xczu46dr", 6}},
	{0x047ff093, {"xilinx", "zynqmp",      "xczu47dr", 6}},
	{0x047fb093, {"xilinx", "zynqmp",      "xczu48dr", 6}},
	{0x047fe093, {"xilinx", "zynqmp",      "xczu49dr", 6}},

	/**************************************************************************/
	/*                             Altera                                     */
	/**************************************************************************/

	/* Altera Max II*/
	{0x020a10dd, {"altera", "max II", "EPM240T100C5N", 10}},

	/* Altera Cyclone II/III/IV/10 LP */
	{0x020b10dd, {"altera", "cyclone II",           "EP2C5",                    10}},
	{0x020f10dd, {"altera", "cyclone III/IV/10 LP", "EP4CE6/EP4CE10",           10}},
	{0x020f20dd, {"altera", "cyclone III/IV/10 LP", "EP3C16/EP4CE15/10CL016",   10}},
	{0x020f70dd, {"altera", "cyclone III/IV/10 LP", "EP3C120/EP4CE115/10CL120", 10}},
	{0x028040dd, {"altera", "cyclone IV GX",        "EP4CGX150",                10}},

	/* Altera Cyclone V */
	{0x02b010dd, {"altera", "cyclone V",     "5CGX*3",                10}},
	{0x02b020dd, {"altera", "cyclone V",     "5CGT*5/5CGX*5",         10}},
	{0x02b030dd, {"altera", "cyclone V",     "5CGT*7/5CGX*7",         10}},
	{0x02b040dd, {"altera", "cyclone V",     "5CGT*9/5CGX*9",         10}},
	{0x02b050dd, {"altera", "cyclone V",     "5CE*A4",                10}},
	{0x02b120dd, {"altera", "cyclone V",     "5CGX*4",                10}},
	{0x02b130dd, {"altera", "cyclone V",     "5CE*A7",                10}},
	{0x02b140dd, {"altera", "cyclone V",     "5CE*A9",                10}},
	{0x02b150dd, {"altera", "cyclone V",     "5CE*A2",                10}},
	{0x02b220dd, {"altera", "cyclone V",     "5CE*A5",                10}},
	{0x02d010dd, {"altera", "cyclone V Soc", "5CSE*A4/5CSX*4",        10}},
	{0x02d020dd, {"altera", "cyclone V Soc", "5CSE*A6/5CSX*6",        10}},
	{0x02d110dd, {"altera", "cyclone V Soc", "5CSE*A2/5CSX*2",        10}},
	{0x02d120dd, {"altera", "cyclone V Soc", "5CSE*A5/5CST*5/5CSX*5", 10}},

	/* Altera Max 10 */
	{0x031820dd, {"altera", "MAX 10", "10M08SAU169C8G",    10}},
	{0x031050dd, {"altera", "MAX 10", "10M50DAF484",       10}},
	{0x0318d0dd, {"altera", "MAX 10", "10M40SCE144C8G",    10}},
	{0x031830dd, {"altera", "MAX 10", "10M16SAU169C8G",    10}},
	{0x031810dd, {"altera", "MAX 10", "10M02SCM153C8G",    10}},

	/* Altera Cyclone 10 */
	{0x020f30dd, {"altera", "cyclone 10 LP", "10CL025", 10}},
	{0x020f50dd, {"altera", "cyclone 10 LP", "10CL055", 10}},

	/* Altera Stratix V */
	{0x029070dd, {"altera", "stratix V GS", "5SGSD5", 10}},

	/**************************************************************************/
	/*                             Efinix                                     */
	/**************************************************************************/

	/* Efinix Trion */
	{0x00000001, {"efinix", "Trion",    "T4/T8",            4}},
	{0x00210a79, {"efinix", "Trion",    "T8QFP144/T13/T20", 4}},
	{0x00220a79, {"efinix", "Trion",    "T55/T85/T120",     4}},
	{0x00240a79, {"efinix", "Trion",    "T20BGA324/T35",    4}},

	/* Efinix Titanium */
	{0x00660a79, {"efinix", "Titanium", "Ti60",             5}},
	{0x00360a79, {"efinix", "Titanium", "Ti60ES",           5}},
	{0x00661a79, {"efinix", "Titanium", "Ti35",             5}},
	{0x00690a79, {"efinix", "Titanium", "Ti180",            5}},

	/**************************************************************************/
	/*                             Lattice                                    */
	/**************************************************************************/

	/* Lattice XP2 */
	{0x0129a043, {"lattice", "XP2", "LFXP2-8E", 8}},

	/* Lattice MachXO2 */
	{0x012b0043, {"lattice", "MachXO2", "LCMXO2-256ZE", 8}},
	{0x012b1043, {"lattice", "MachXO2", "LCMXO2-640ZE", 8}},
	{0x012b2043, {"lattice", "MachXO2", "LCMXO2-1200ZE", 8}},
	{0x012b3043, {"lattice", "MachXO2", "LCMXO2-2000ZE", 8}},
	{0x012b4043, {"lattice", "MachXO2", "LCMXO2-4000ZE", 8}},
	{0x012b5043, {"lattice", "MachXO2", "LCMXO2-7000ZE", 8}},
	{0x012b8043, {"lattice", "MachXO2", "LCMXO2-256HC", 8}},
	{0x012b9043, {"lattice", "MachXO2", "LCMXO2-640HC", 8}},
	{0x012ba043, {"lattice", "MachXO2", "LCMXO2-1200HC", 8}},
	{0x012ba043, {"lattice", "MachXO2", "LCMXO2-640UHC", 8}},
	{0x012bb043, {"lattice", "MachXO2", "LCMXO2-2000HC", 8}},
	{0x012bb043, {"lattice", "MachXO2", "LCMXO2-1200UHC", 8}},
	{0x012bc043, {"lattice", "MachXO2", "LCMXO2-4000HC", 8}},
	{0x012bc043, {"lattice", "MachXO2", "LCMXO2-2000UHC", 8}},
	{0x012bd043, {"lattice", "MachXO2", "LCMXO2-7000HC", 8}},

	/* Lattice MachXO3 */
	{0x412b2043, {"lattice", "MachXO3L", "LCMXO3L-1300E", 8}},
	{0x412b3043, {"lattice", "MachXO3L", "LCMXO3L-2100E", 8}},
	{0x412b4043, {"lattice", "MachXO3L", "LCMXO3L-4300E", 8}},
	{0x412b5043, {"lattice", "MachXO3L", "LCMXO3L-6900E", 8}},
	{0x412b6043, {"lattice", "MachXO3L", "LCMXO3L-9400E", 8}},
	{0x412bb043, {"lattice", "MachXO3L", "LCMXO3L-2100C", 8}},
	{0x412bc043, {"lattice", "MachXO3L", "LCMXO3L-4300C", 8}},
	{0x412bd043, {"lattice", "MachXO3L", "LCMXO3L-6900C", 8}},
	{0x412be043, {"lattice", "MachXO3L", "LCMXO3L-9400C", 8}},
	{0xc12b2043, {"lattice", "MachXO3L", "LCMXO3L-640E", 8}},
	{0xc12b3043, {"lattice", "MachXO3L", "LCMXO3L-1300E", 8}},
	{0xc12b4043, {"lattice", "MachXO3L", "LCMXO3L-2100E", 8}},
	{0xc12bb043, {"lattice", "MachXO3L", "LCMXO3L-1300C", 8}},
	{0xc12bc043, {"lattice", "MachXO3L", "LCMXO3L-2100C", 8}},
	{0xc12bd043, {"lattice", "MachXO3L", "LCMXO3L-4300C", 8}},

	{0x612b2043, {"lattice", "MachXO3LF", "LCMXO3LF-1300E", 8}},
	{0x612b3043, {"lattice", "MachXO3LF", "LCMXO3LF-2100E", 8}},
	{0x612b4043, {"lattice", "MachXO3LF", "LCMXO3LF-4300E", 8}},
	{0x612b5043, {"lattice", "MachXO3LF", "LCMXO3LF-6900E", 8}},
	{0x612b6043, {"lattice", "MachXO3LF", "LCMXO3LF-9400E", 8}},
	{0x612bb043, {"lattice", "MachXO3LF", "LCMXO3LF-2100C", 8}},
	{0x612bc043, {"lattice", "MachXO3LF", "LCMXO3LF-4300C", 8}},
	{0x612bd043, {"lattice", "MachXO3LF", "LCMXO3LF-6900C", 8}},
	{0x612be043, {"lattice", "MachXO3LF", "LCMXO3LF-9400C", 8}},
	{0xe12b2043, {"lattice", "MachXO3LF", "LCMXO3LF-640E", 8}},
	{0xe12b3043, {"lattice", "MachXO3LF", "LCMXO3LF-1300E", 8}},
	{0xe12b4043, {"lattice", "MachXO3LF", "LCMXO3LF-2100E", 8}},
	{0xe12bb043, {"lattice", "MachXO3LF", "LCMXO3LF-1300C", 8}},
	{0xe12bc043, {"lattice", "MachXO3LF", "LCMXO3LF-2100C", 8}},
	{0xe12bd043, {"lattice", "MachXO3LF", "LCMXO3LF-4300C", 8}},

	{0x012e3043, {"lattice", "MachXO3D", "LCMX03D-9400HC", 8}},

	/* Lattice ECP3 */
	{0x01014043, {"lattice", "ECP3", "LFE3-70E",    8}},

	/* Lattice ECP5 */
	{0x21111043, {"lattice", "ECP5", "LFE5U-12",    8}},
	{0x41111043, {"lattice", "ECP5", "LFE5U-25",    8}},
	{0x41112043, {"lattice", "ECP5", "LFE5U-45",    8}},
	{0x41113043, {"lattice", "ECP5", "LFE5U-85",    8}},
	{0x01111043, {"lattice", "ECP5", "LFE5UM-25",   8}},
	{0x01112043, {"lattice", "ECP5", "LFE5UM-45",   8}},
	{0x01113043, {"lattice", "ECP5", "LFE5UM-85",   8}},
	{0x81111043, {"lattice", "ECP5", "LFE5UM5G-25", 8}},
	{0x81112043, {"lattice", "ECP5", "LFE5UM5G-45", 8}},
	{0x81113043, {"lattice", "ECP5", "LFE5UM5G-85", 8}},

	/* Lattice Crosslink-NX */
	{0x010F0043, {"lattice", "CrosslinkNX", "LIFCL-17", 8}},
	{0x010F1043, {"lattice", "CrosslinkNX", "LIFCL-40", 8}},

	/* Lattice Certus-NX */
	{0x310F0043, {"lattice", "CertusNX", "LFD2NX-17", 8}},
	{0x310F1043, {"lattice", "CertusNX", "LFD2NX-40", 8}},

	/* Lattice CertusPro-NX */
	{0x010F4043, {"lattice", "CertusProNX", "LFCPNX-100", 8}},

	/**************************************************************************/
	/*                             Gowin                                      */
	/**************************************************************************/

	/* Gowin GW1 */
	{0x0100481b, {"Gowin", "GW1N",   "GW1N(R)-9C", 8}},
	{0x0100581b, {"Gowin", "GW1N",   "GW1NR-9",    8}},
	{0x0900281B, {"Gowin", "GW1N",   "GW1N-1",     8}},
	{0x0120681b, {"Gowin", "GW1N",   "GW1N-2",     8}},
	{0x0100381B, {"Gowin", "GW1N",   "GW1N-4",     8}},
	{0x0100681b, {"Gowin", "GW1NZ",  "GW1NZ-1",    8}},
	{0x0300181b, {"Gowin", "GW1NS",  "GW1NS-2C",   8}},
	{0x0100981b, {"Gowin", "GW1NSR", "GW1NSR-4C",  8}},

	/* Gowin GW2 */
	{0x0000081b, {"Gowin", "GW2A", "GW2A(R)-18(C)", 8}},
	{0x0000281B, {"Gowin", "GW2A", "GW2A-55",       8}},

	/* Gowin GW5 */
	{0x0001081b, {"Gowin", "GW5AST", "GW5AST-138", 8}},
	{0x0001481b, {"Gowin", "GW5AT",  "GW5AT-60",   8}},
	{0x0001181b, {"Gowin", "GW5AT",  "GW5AT-138",  8}},
	{0x0001281b, {"Gowin", "GW5A",   "GW5A-25",    8}},

	/**************************************************************************/
	/*                           CologneChip                                  */
	/**************************************************************************/

	/* CologneChip GateMate*/
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
	{0x1000563d, {"GD32VF103", 5}},
	{0x790007a3, {"GD32VF103", 5}},
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
