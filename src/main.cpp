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
#include "jtag.hpp"
#include "part.hpp"
#include "xilinx.hpp"

using namespace std;

struct arguments {
	bool verbose, reset, detect;
	unsigned int offset;
	string bit_file;
	string device;
	string cable;
	string speed;
	string board;
	bool list_cables;
	bool list_boards;
	bool list_fpga;
	bool write_flash;
	bool write_sram;
	bool is_list_command;
};

#define LIST_CABLE	1
#define LIST_BOARD	2
#define LIST_FPGA	3
#define DETECT		4

const char *argp_program_version = "openFPGALoader 1.0";
const char *argp_program_bug_address = "<gwenhael.goavec-merou@trabucayre.com>";
static char doc[] = "openFPGALoader -- a program to flash FPGA";
static char args_doc[] = "BIT_FILE";
static error_t parse_opt(int key, char *arg, struct argp_state *state);
static struct argp_option options[] = {
	{"cable",   'c', "CABLE", 0, "jtag interface"},
	{"speed",   's', "SPEED", 0, "jtag frequency (Hz)"},
	{"list-cables", LIST_CABLE, 0, 0, "list all supported cables"},
	{"board",   'b', "BOARD", 0, "board name, may be used instead of cable"},
	{"list-boards", LIST_BOARD, 0, 0, "list all supported boards"},
#ifdef USE_UDEV
	{"device",  'd', "DEVICE", 0, "device to use (/dev/ttyUSBx)"},
#endif
	{"list-fpga", LIST_FPGA, 0, 0, "list all supported FPGA"},
	{"detect", DETECT, 0, 0, "detect FPGA"},
	{"write-flash", 'f', 0, 0,
			"write bitstream in flash (default: false, only for Gowin and ECP5 devices)"},
	{"write-sram", 'm', 0, 0,
			"write bitstream in SRAM (default: true, only for Gowin and ECP5 devices)"},
	{"offset",  'o', "OFFSET", 0, "start offset in EEPROM"},
	{"verbose", 'v', 0, 0, "Produce verbose output"},
	{"reset",   'r', 0, 0, "reset FPGA after operations"},
	{0}
};

static struct argp argp = { options, parse_opt, args_doc, doc };
void displaySupported(const struct arguments &args);

int main(int argc, char **argv)
{
	cable_t cable;
	jtag_pins_conf_t *pins_config = NULL;

	/* command line args. */
	struct arguments args = {false, false, false, 0, "", "-", "-", "6M", "-",
			false, false, false, false, true, false};
	/* parse arguments */
	argp_parse(&argp, argc, argv, 0, 0, &args);

	if (args.is_list_command) {
		displaySupported(args);
		return EXIT_SUCCESS;
	}

	/* if a board name is specified try to use this to determine cable */
	if (args.board[0] != '-' && board_list.find(args.board) != board_list.end()) {
		/* set pins config */
		pins_config = &board_list[args.board].pins_config;
		/* search for cable */
		auto t = cable_list.find(board_list[args.board].cable_name);
		if (t == cable_list.end()) {
			args.cable = "-";
			cout << "Board " << args.board << " has not default cable" << endl;
		} else
			args.cable = (*t).first;
	}

	if (args.cable[0] == '-') { /* if no board and no cable */
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

	uint32_t speed;
	try {
		size_t end;
		float speed_base = stof(args.speed, &end);
		if (end == args.speed.size()) {
			speed = (uint32_t)speed_base;
		} else if (end == (args.speed.size() - 1)) {
			switch (args.speed.back()) {
			case 'k': case 'K':
				speed = (uint32_t)(1e3 * speed_base);
				break;
			case 'm': case 'M':
				speed = (uint32_t)(1e6 * speed_base);
				break;
			default:
				cerr << "error : speed: invaild postfix \"" << args.speed.back() << "\"" << endl;
				return EXIT_FAILURE;
			}
		} else {
			cerr << "error : speed: invaild postfix \"" << args.speed.substr(end) << "\"" << endl;
			return EXIT_FAILURE;
		}
	} catch (...) {
		cerr << "error : speed: invaild format" << endl;
		return EXIT_FAILURE;
	}

	/* jtag base */
	Jtag *jtag;
	if (args.device == "-")
		jtag = new Jtag(cable, pins_config, speed, false);
	else
		jtag = new Jtag(cable, pins_config, args.device, speed, false);

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
	} else if (args.verbose || args.detect) {
		printf("idcode 0x%x\nmanufacturer %s\nmodel  %s\nfamily %s\n",
			idcode,
			fpga_list[idcode].manufacturer.c_str(),
			fpga_list[idcode].model.c_str(),
			fpga_list[idcode].family.c_str());
	}

	if (args.detect == true) {
		delete jtag;
		return EXIT_SUCCESS;
	}

	string fab = fpga_list[idcode].manufacturer;

	Device *fpga;
	if (fab == "xilinx") {
		fpga = new Xilinx(jtag, args.bit_file, args.verbose);
	} else if (fab == "altera") {
		fpga = new Altera(jtag, args.bit_file, args.verbose);
	} else if (fab == "Gowin") {
		fpga = new Gowin(jtag, args.bit_file, args.write_flash, args.write_sram,
			args.verbose);
	} else if (fab == "lattice") {
		fpga = new Lattice(jtag, args.bit_file, args.write_flash, args.write_sram,
			args.verbose);
	} else {
		cerr << "Error: manufacturer " << fab << " not supported" << endl;
		delete(jtag);
		return EXIT_FAILURE;
	}

	if (!args.bit_file.empty()) {
		try {
			fpga->program(args.offset);
		} catch (std::exception &e) {
			delete(fpga);
			delete(jtag);
			return EXIT_FAILURE;
		}
	}

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
	case 'f':
		arguments->write_flash = true;
		arguments->write_sram = false;
		break;
	case 'm':
		arguments->write_sram = true;
		break;
	case 'r':
		arguments->reset = true;
		break;
#ifdef USE_UDEV
	case 'd':
		arguments->device = arg;
		break;
#endif
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
	case 's':
		arguments->speed = arg;
		break;
	case ARGP_KEY_ARG:
		arguments->bit_file = arg;
		break;
	case ARGP_KEY_END:
		if (arguments->bit_file.empty() &&
			!arguments->is_list_command &&
			!arguments->detect &&
			!arguments->reset) {
			cout << "Error: bitfile not specified" << endl;
			argp_usage(state);
		}
		break;
	case LIST_CABLE:
		arguments->list_cables = true;
		arguments->is_list_command = true;
		break;
	case LIST_BOARD:
		arguments->list_boards = true;
		arguments->is_list_command = true;
		break;
	case LIST_FPGA:
		arguments->list_fpga = true;
		arguments->is_list_command = true;
		break;
	case DETECT:
		arguments->detect = true;
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
			FTDIpp_MPSSE::mpsse_bit_config c = (*b).second.config;
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
			target_cable_t c = (*b).second;
			ss << setw(15) << left << (*b).first << " " << c.cable_name;
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

