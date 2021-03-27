#ifndef PART_HPP
#define PART_HPP

#include <map>
#include <string>

typedef struct {
	std::string manufacturer;
	std::string family;
	std::string model;
} fpga_model;

static std::map <int, fpga_model> fpga_list = {
	{0x0a014c35, {"anlogic", "eagle s20", "EG4S20BG256"}},

	{0x0362D093, {"xilinx", "artix a7 35t", "xc7a35"}},
	{0x0362c093, {"xilinx", "artix a7 50t",  "xc7a50t"}},
	{0x13631093, {"xilinx", "artix a7 100t", "xc7a100"}},
	{0x13636093, {"xilinx", "artix a7 200t", "xc7a200"}},

	{0x44008093, {"xilinx", "spartan6", "xc6slx45"}},
	{0x03620093, {"xilinx", "spartan7", "xc7s15ftgb196-1"}},
	{0x037c4093, {"xilinx", "spartan7", "xc7s25"}},
	{0x0362f093, {"xilinx", "spartan7", "xc7s50"}},

	{0x020f20dd, {"altera", "cyclone III", "EP3C16"}},

	{0x020f30dd, {"altera", "cyclone 10 LP", "10CL025"}},

	{0x02b050dd, {"altera", "cyclone V", "5CEBA4"}},
	{0x02b150dd, {"altera", "cyclone V", "5CEA2"}},

	{0x010F0043, {"lattice", "CrosslinkNX", "LIFCL-17"}},
	{0x010F1043, {"lattice", "CrosslinkNX", "LIFCL-40-ES"}},
	{0x110F1043, {"lattice", "CrosslinkNX", "LIFCL-40"}},

	{0x310F0043, {"lattice", "CertusNX", "LFD2NX-17"}},
	{0x310F1043, {"lattice", "CertusNX", "LFD2NX-40"}},

	{0x012b9043, {"lattice", "MachXO2",   "LCMXO2-640HC"}},
	{0x012bd043, {"lattice", "MachXO2",   "LCMXO2-7000HC"}},
	{0x012b5043, {"lattice", "MachXO2",   "LCMXO2-7000HE"}},

	{0xE12BB043, {"lattice", "MachXO3LF", "LCMX03LF-1300C"}},
	{0x612B2043, {"lattice", "MachXO3LF", "LCMX03LF-1300E"}},
	{0x612BB043, {"lattice", "MachXO3LF", "LCMX03LF-2100C"}},
	{0x612B3043, {"lattice", "MachXO3LF", "LCMX03LF-2100E"}},
	{0x612BC043, {"lattice", "MachXO3LF", "LCMX03LF-4300C"}},
	{0x612B4043, {"lattice", "MachXO3LF", "LCMX03LF-4300E"}},
	{0x612BD043, {"lattice", "MachXO3LF", "LCMX03LF-6900C"}},
	{0x612B5043, {"lattice", "MachXO3LF", "LCMX03LF-6900E"}},
	{0x612BE043, {"lattice", "MachXO3LF", "LCMX03LF-9400C"}},
	{0x612B6043, {"lattice", "MachXO3LF", "LCMX03LF-9400E"}},

	{0x212e3043, {"lattice", "MachXO3D", "LCMX03D-9400HC"}},

	{0x21111043, {"lattice", "ECP5", "LFE5U-12"}},
	{0x41111043, {"lattice", "ECP5", "LFE5U-25"}},
	{0x41112043, {"lattice", "ECP5", "LFE5U-45"}},
	{0x41113043, {"lattice", "ECP5", "LFE5U-85"}},
	{0x01111043, {"lattice", "ECP5", "LFE5UM-25"}},
	{0x01112043, {"lattice", "ECP5", "LFE5UM-45"}},
	{0x01113043, {"lattice", "ECP5", "LFE5UM-85"}},
	{0x81111043, {"lattice", "ECP5", "LFE5UM5G-25"}},
	{0x81112043, {"lattice", "ECP5", "LFE5UM5G-45"}},
	{0x81113043, {"lattice", "ECP5", "LFE5UM5G-85"}},

	{0x0129a043, {"lattice", "XP2", "LFXP2-8E"}},

	{0x1100581b, {"Gowin", "GW1N", "GW1NR-9"}},
	{0x0900281B, {"Gowin", "GW1N", "GW1N-1"}},
	{0x0100381B, {"Gowin", "GW1N", "GW1N-4"}},
	{0x0300181b, {"Gowin", "GW1NS", "GW1NS-2C"}},
};

#endif
