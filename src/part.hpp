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
	{0x0362D093, {"xilinx", "artix a7 35t", "xc7a35"}},
	{0x13631093, {"xilinx", "artix a7 100t", "xc7a100"}},
	{0x03620093, {"xilinx", "spartan7", "xc7s15ftgb196-1"}},
	{0x020f30dd, {"altera", "cyclone 10 LP", "10CL025"}},
	{0x612bd043, {"lattice", "MachXO3LF", "LCMX03LF-6900C"}},
	{0x41111043, {"lattice", "ECP5", "LFE5U-25F-6BG256C"}},
	{0x81113043, {"lattice", "ECP5-5G", "LFE5UM5G-85F-8BG381"}},
	{0x1100581b, {"Gowin", "GW1N", "GW1NR-9"}},
	{0x0900281B, {"Gowin", "GW1N", "GW1N-1"}},
	{0x0100381B, {"Gowin", "GW1N", "GW1N-4"}},
};

#endif
