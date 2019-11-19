/*
 * Copyright (C) 2019 Gwenhael Goavec-Merou <gwenhael.goavec-merou@trabucayre.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */
#include <argp.h>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string.h>
#include <unistd.h>
#include <vector>

#include "altera.hpp"
#include "board.hpp"
#include "cable.hpp"
#include "device.hpp"
#include "lattice.hpp"
#include "ftdijtag.hpp"
#include "part.hpp"
#include "xilinx.hpp"

using namespace std;

struct arguments {
	bool verbose, reset;
	unsigned int offset;
	string bit_file;
	string device;
	string cable;
	string board;
};

const char *argp_program_version = "cycloader 1.0";
const char *argp_program_bug_address = "<gwenhael.goavec-merou@trabucayre.com>";
static char doc[] = "cycloader -- a program to flash cyclone10 LP FPGA";
static char args_doc[] = "BIT_FILE";
static error_t parse_opt(int key, char *arg, struct argp_state *state);
static struct argp_option options[] = {
	{"cable",   'c', "CABLE", 0, "jtag interface"},
	{"board",   'b', "BOARD", 0, "board name, may be used instead of cable"},
	{"device",  'd', "DEVICE", 0, "device to use (/dev/ttyUSBx)"},
	{"offset",  'o', "OFFSET", 0, "start offset in EEPROM"},
	{"verbose", 'v', 0, 0, "Produce verbose output"},
	{"reset",   'r', 0, 0, "reset FPGA after operations"},
	{0}
};
static struct argp argp = { options, parse_opt, args_doc, doc };

int main(int argc, char **argv)
{
	FTDIpp_MPSSE::mpsse_bit_config cable;

	/* command line args. */
	struct arguments args = {false, false, 0, "", "-", "-", "-"};
	/* parse arguments */
	argp_parse(&argp, argc, argv, 0, 0, &args);

	/* if a board name is specified try to use this to determine cable */
	if (args.board[0] != '-' && board_list.find(args.board) != board_list.end()) {
		auto t = cable_list.find(board_list[args.board]);
		if (t == cable_list.end()) {
			cerr << "Error: interface "<< board_list[args.board];
			cerr << " for board " << args.board << " is not supported" << endl;
			return 1;
		}
		args.cable = (*t).first;
	} else if (args.cable[0] == '-') { /* if no board and no cable */
		cout << "No cable or board specified: using direct ft2232 interface" << endl;
		args.cable = "ft2232";
	}

	auto select_cable = cable_list.find(args.cable);
	if (select_cable == cable_list.end()) {
		cerr << "error : " << args.cable << " not found" << endl;
		return EXIT_FAILURE;
	}
	cable = select_cable->second;

	/* jtag base */
	FtdiJtag *jtag;
	if (args.device == "-")
		jtag = new FtdiJtag(cable, 1, 6000000);
	else
		jtag = new FtdiJtag(cable, args.device, 1, 6000000);

	/* chain detection */
	vector<int> listDev;
	int found = jtag->detectChain(listDev, 5);

	cout << "found " << std::to_string(found) << " devices" << endl;
	if (found > 1) {
		cerr << "Error: currently only one device is supported" << endl;
		return EXIT_FAILURE;
	} else if (found < 1) {
		cerr << "Error: no device found" << endl;
		return EXIT_FAILURE;
	}

	int idcode = listDev[0];

	if (fpga_list.find(idcode) == fpga_list.end()) {
		cerr << "Error: device not supported" << endl;
		return 1;
	} else {
		printf("idcode 0x%x\nfunder %s\nmodel  %s\nfamily %s\n",
			idcode,
			fpga_list[idcode].funder.c_str(),
			fpga_list[idcode].model.c_str(),
			fpga_list[idcode].family.c_str());
	}
	string fab = fpga_list[idcode].funder;

	Device *fpga;
	if (fab == "xilinx") {
		fpga = new Xilinx(jtag, args.bit_file);
	} else if (fab == "altera") {
		fpga = new Altera(jtag, args.bit_file);
	} else {
		fpga = new Lattice(jtag, args.bit_file);
	}

	fpga->program(args.offset);

	if (args.reset)
		fpga->reset();

	delete(fpga);

}

/* arguments parser */

static error_t parse_opt(int key, char *arg, struct argp_state *state)
{
	struct arguments *arguments = (struct arguments *)state->input;

	switch (key) {
	case 'r':
		arguments->reset = true;
		break;
	case 'd':
		printf("device\n");
		arguments->device = arg;
		break;
	case 'v':
		arguments->verbose = true;
		break;
	case 'o':
		arguments->offset = strtoul(arg, NULL, 16);
		break;
	case 'c':
		arguments->cable = arg;
		break;
	case 'b':
		arguments->board = arg;
		break;
	case ARGP_KEY_ARG:
		arguments->bit_file = arg;
		break;
	case ARGP_KEY_END:
		break;
	default:
		return ARGP_ERR_UNKNOWN;
	}
	return 0;
}

