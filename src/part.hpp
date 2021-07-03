// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (C) 2019 Gwenhael Goavec-Merou <gwenhael.goavec-merou@trabucayre.com>
 */

#ifndef PART_HPP
#define PART_HPP

#include <map>
#include <string>

typedef struct {
	std::string manufacturer;
	std::string family;
	std::string model;
	int irlength;
} fpga_model;

static std::map <int, fpga_model> fpga_list = {
	{0x0a014c35, {"anlogic", "eagle s20", "EG4S20BG256", 8}},

	{0x0362D093, {"xilinx", "artix a7 35t", "xc7a35", 6}},
	{0x0362c093, {"xilinx", "artix a7 50t",  "xc7a50t", 6}},
	{0x13632093, {"xilinx", "artix a7 75t",  "xc7a75t", 6}},
	{0x13631093, {"xilinx", "artix a7 100t", "xc7a100", 6}},
	{0x13636093, {"xilinx", "artix a7 200t", "xc7a200", 6}},

	{0x43651093, {"xilinx", "kintex7", "xc7k325t", 6}},

	{0x24001093, {"xilinx", "spartan6", "xc6slx9",  6}},
	{0x24002093, {"xilinx", "spartan6", "xc6slx16", 6}},
	{0x24004093, {"xilinx", "spartan6", "xc6slx25", 6}},
	{0x24011093, {"xilinx", "spartan6", "xc6slx100", 6}},
	{0x44008093, {"xilinx", "spartan6", "xc6slx45", 6}},
	{0x03620093, {"xilinx", "spartan7", "xc7s15ftgb196-1", 6}},
	{0x037c4093, {"xilinx", "spartan7", "xc7s25", 6}},
	{0x0362f093, {"xilinx", "spartan7", "xc7s50", 6}},

	{0x23727093, {"xilinx", "zynq",     "xc7z020", 6}},

	{0x020f20dd, {"altera", "cyclone III", "EP3C16", 10}},

	{0x020f30dd, {"altera", "cyclone 10 LP", "10CL025", 10}},

	{0x02b150dd, {"altera", "cyclone V", "5CEA2", 10}},
	{0x02b050dd, {"altera", "cyclone V", "5CEBA4", 10}},
	{0x02d020dd, {"altera", "cyclone V Soc", "5CSEBA6", 10}},
	{0x02d010dd, {"altera", "cyclone V Soc", "5CSEMA4", 10}},

	{0x010F0043, {"lattice", "CrosslinkNX", "LIFCL-17", 8}},
	{0x010F1043, {"lattice", "CrosslinkNX", "LIFCL-40-ES", 8}},
	{0x110F1043, {"lattice", "CrosslinkNX", "LIFCL-40", 8}},

	{0x310F0043, {"lattice", "CertusNX", "LFD2NX-17", 8}},
	{0x310F1043, {"lattice", "CertusNX", "LFD2NX-40", 8}},

	{0x012b9043, {"lattice", "MachXO2",   "LCMXO2-640HC", 8}},
	{0x012ba043, {"lattice", "MachXO2",   "LCMXO2-1200HC", 8}},
	{0x012bd043, {"lattice", "MachXO2",   "LCMXO2-7000HC", 8}},
	{0x012b5043, {"lattice", "MachXO2",   "LCMXO2-7000HE", 8}},

	{0xE12BB043, {"lattice", "MachXO3LF", "LCMX03LF-1300C", 8}},
	{0x612B2043, {"lattice", "MachXO3LF", "LCMX03LF-1300E", 8}},
	{0x612BB043, {"lattice", "MachXO3LF", "LCMX03LF-2100C", 8}},
	{0x612B3043, {"lattice", "MachXO3LF", "LCMX03LF-2100E", 8}},
	{0x612BC043, {"lattice", "MachXO3LF", "LCMX03LF-4300C", 8}},
	{0x612B4043, {"lattice", "MachXO3LF", "LCMX03LF-4300E", 8}},
	{0x612BD043, {"lattice", "MachXO3LF", "LCMX03LF-6900C", 8}},
	{0x612B5043, {"lattice", "MachXO3LF", "LCMX03LF-6900E", 8}},
	{0x612BE043, {"lattice", "MachXO3LF", "LCMX03LF-9400C", 8}},
	{0x612B6043, {"lattice", "MachXO3LF", "LCMX03LF-9400E", 8}},

	{0x212e3043, {"lattice", "MachXO3D", "LCMX03D-9400HC", 8}},

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

	{0x1100581b, {"Gowin", "GW1N", "GW1NR-9", 8}},
	{0x0900281B, {"Gowin", "GW1N", "GW1N-1", 8}},
	{0x0100381B, {"Gowin", "GW1N", "GW1N-4", 8}},
	{0x0300181b, {"Gowin", "GW1NS", "GW1NS-2C", 8}},
};

/* device potentially in JTAG chain but not handled */
typedef struct {
	std::string name;
	int irlength;
} misc_device;

static std::map <int, misc_device> misc_dev_list = {
	{0x4ba00477, {"ARM cortex A9", 4}},
};

#endif
