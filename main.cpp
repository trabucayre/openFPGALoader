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
#include <iomanip>
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
#include "display.hpp"
#include "gowin.hpp"
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
	bool list_cables;
	bool list_boards;
	bool list_fpga;
};

#define LIST_CABLE	1
#define LIST_BOARD 	2
#define LIST_FPGA	3

const char *argp_program_version = "openFPGALoader 1.0";
const char *argp_program_bug_address = "<gwenhael.goavec-merou@trabucayre.com>";
static char doc[] = "openFPGALoader -- a program to flash FPGA";
static char args_doc[] = "BIT_FILE";
static error_t parse_opt(int key, char *arg, struct argp_state *state);
static struct argp_option options[] = {
	{"cable",   'c', "CABLE", 0, "jtag interface"},
	{"list-cables", LIST_CABLE, 0, 0, "list all supported cables"},
	{"board",   'b', "BOARD", 0, "board name, may be used instead of cable"},
	{"list-boards", LIST_BOARD, 0, 0, "list all supported boards"},
	{"device",  'd', "DEVICE", 0, "device to use (/dev/ttyUSBx)"},
	{"list-fpga", LIST_FPGA, 0, 0, "list all supported FPGA"},
	{"offset",  'o', "OFFSET", 0, "start offset in EEPROM"},
	{"verbose", 'v', 0, 0, "Produce verbose output"},
	{"reset",   'r', 0, 0, "reset FPGA after operations"},
	{0}
};

static struct argp argp = { options, parse_opt, args_doc, doc };
void displaySupported(const struct arguments &args);

int main(int argc, char **argv)
{
	FTDIpp_MPSSE::mpsse_bit_config cable;

	/* command line args. */
	struct arguments args = {false, false, 0, "", "-", "-", "-",
			false, false, false};
	/* parse arguments */
	argp_parse(&argp, argc, argv, 0, 0, &args);

	if (args.list_boards == true || args.list_cables == true || args.list_fpga) {
		displaySupported(args);
		return EXIT_SUCCESS;
	}

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
		if (args.verbose)
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
		jtag = new FtdiJtag(cable, 1, 6000000, args.verbose);
	else
		jtag = new FtdiJtag(cable, args.device, 1, 6000000, args.verbose);

	/* chain detection */
	vector<int> listDev;
	int found = jtag->detectChain(listDev, 5);

	if (args.verbose)
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
		cerr << "Error: device " << hex << idcode << " not supported" << endl;
		return 1;
	} else if (args.verbose) {
		printf("idcode 0x%x\nmanufacturer %s\nmodel  %s\nfamily %s\n",
			idcode,
			fpga_list[idcode].manufacturer.c_str(),
			fpga_list[idcode].model.c_str(),
			fpga_list[idcode].family.c_str());
	}
	string fab = fpga_list[idcode].manufacturer;

	Device *fpga;
	if (fab == "xilinx") {
		fpga = new Xilinx(jtag, args.bit_file, args.verbose);
	} else if (fab == "altera") {
		fpga = new Altera(jtag, args.bit_file, args.verbose);
	} else if (fab == "gowin") {
		fpga = new Gowin(jtag, args.bit_file, args.verbose);
	} else {
		fpga = new Lattice(jtag, args.bit_file, args.verbose);
	}

	fpga->program(args.offset);

	if (args.reset)
		fpga->reset();

	delete(fpga);
	delete(jtag);
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
	case LIST_CABLE:
		arguments->list_cables = true;
		break;
	case LIST_BOARD:
		arguments->list_boards = true;
		break;
	case LIST_FPGA:
		arguments->list_fpga = true;
		break;
	default:
		return ARGP_ERR_UNKNOWN;
	}
	return 0;
}

/* display list of cables, boards and devices supported */
void displaySupported(const struct arguments &args)
{
	if (args.list_cables == true) {
		stringstream t;
		t << setw(15) << left << "cable name:" << "vid:pid";
		printSuccess(t.str());
		for (auto b = cable_list.begin(); b != cable_list.end(); b++) {
			FTDIpp_MPSSE::mpsse_bit_config c = (*b).second;
			stringstream ss;
			ss << setw(15) << left << (*b).first;
			ss << "0x" << hex << c.vid << ":" << c.pid;
			printInfo(ss.str());
		}
		cout << endl;
	}

	if (args.list_boards) {
		stringstream t;
		t << setw(15) << left << "board name:" << "cable_name";
		printSuccess(t.str());
		for (auto b = board_list.begin(); b != board_list.end(); b++) {
			stringstream ss;
			ss << setw(15) << left << (*b).first << " " << (*b).second;
			printInfo(ss.str());
		}
		cout << endl;
	}

	if (args.list_fpga) {
		stringstream t;
		t << setw(12) << left << "IDCode" << setw(14) << "manufacturer";
		t << setw(15) << "family" << setw(20) << "model";
		printSuccess(t.str());
		for (auto b = fpga_list.begin(); b != fpga_list.end(); b++) {
			fpga_model fpga = (*b).second;
			stringstream ss, idCode;
			idCode << "0x" << hex << (*b).first;
			ss << setw(12) << left << idCode.str();
			ss << setw(14) << fpga.manufacturer << setw(15) << fpga.family;
			ss << setw(20) << fpga.model;
			printInfo(ss.str());
		}
		cout << endl;
	}
}
