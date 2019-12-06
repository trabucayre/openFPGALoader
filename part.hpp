#ifndef PART_HPP
#define PART_HPP

#include <map>
#include <string>

typedef struct {
	std::string funder;
	std::string model;
	std::string family;
} fpga_model;

static std::map <int, fpga_model> fpga_list = {
	{0x0362D093, {"xilinx", "artix a7 35t", "xc7a35"}},
	{0x020f30dd, {"altera", "cyclone 10 LP", "10CL025"}},
	{0x612bd043, {"lattice", "MachXO3LF", "LCMX03LF-6900C"}},
	{0x1100581b, {"Gowin", "GW1NR-9", "GW1N"}},
};

#endif
