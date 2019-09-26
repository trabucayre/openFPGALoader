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

#include "board.hpp"
#include "cable.hpp"
#include "device.hpp"
#include "part.hpp"
#include "epcq.hpp"
#include "ftdipp_mpsse.hpp"
#include "ftdijtag.hpp"
#include "svf_jtag.hpp"
#include "bitparser.hpp"
#include "xilinx.hpp"

#define BIT_FOR_FLASH "/usr/local/share/cycloader_prog/test_sfl.svf"

enum {
	BIT_FORMAT = 1,
	SVF_FORMAT,
	RPD_FORMAT
} _file_format;

using namespace std;


struct arguments {
	bool verbose, display, reset;
	unsigned int offset;
	string bit_file;
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
	{"display", 'd', 0, 0, "display FPGA and EEPROM model"},
	{"offset",  'o', "OFFSET", 0, "start offset in EEPROM"},
	{"verbose", 'v', 0, 0, "Produce verbose output"},
	{"reset",   'r', 0, 0, "reset FPGA after operations"},
	{0}
};
static struct argp argp = { options, parse_opt, args_doc, doc };

void reset_fpga(FtdiJtag *jtag)
{
	/* PULSE_NCONFIG */
	unsigned char tx_buff[4] = {0x01, 0x00};
	tx_buff[0] = 0x01;
	tx_buff[1] = 0x00;
	jtag->set_state(FtdiJtag::TEST_LOGIC_RESET);
	jtag->shiftIR(tx_buff, NULL, 10);
	jtag->toggleClk(1);
	jtag->set_state(FtdiJtag::TEST_LOGIC_RESET);
}

int main(int argc, char **argv)
{
	FTDIpp_MPSSE::mpsse_bit_config cable;
	bool prog_mem = false, prog_eeprom = false;
	/* EEPROM must have reverse order
	 * other flash (softcore binary) must have direct order
	 */
	bool reverse_order=true; 
	short file_format = 0;

	/* command line args. */
	struct arguments args = {false, false, false, 0, "-", "-", "-"};
	/* parse arguments */
	argp_parse(&argp, argc, argv, 0, 0, &args);

	/* if a bit_file is provided check type using file extension */
	if (args.bit_file[0] != '-') {
		string file_extension = args.bit_file.substr(args.bit_file.find_last_of(".") + 1);
		if(file_extension == "svf") {
			/* svf file */
			prog_mem = true;
			file_format = SVF_FORMAT;
		} else if(file_extension == "rpd") {
			/* rdp file */
			prog_eeprom = true;
			args.reset = true; // FPGA must reload eeprom content
			file_format = RPD_FORMAT;
		} else if (file_extension == "bit") {
			prog_mem = true;
			file_format = BIT_FORMAT;
		} else {
			if (args.offset == 0) {
				cout << args.bit_file << " not FPGA bit make no sense to flash at beginning" << endl;
				return EXIT_FAILURE;
			}
			reverse_order = false;
		}
	}

	/* To have access to eeprom, an svf file must be send to memory
	 * so we must reset FPGA if no other bitfile is sent.
	 */
	if (args.display && !prog_mem)
		args.reset = true;

	printf("%s svf:%s eeprom:%s\n", args.bit_file.c_str(), (prog_mem)?"true":"false",
		(prog_eeprom)?"true":"false");

	if (args.reset && prog_mem) {
		cerr << "Error: using both flash to RAM and reset make no sense" << endl;
		return EXIT_FAILURE;
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
		cout << "No cable or board specified: using direct ft2232 interface" << endl;
		args.cable = "ft2232";
	}

	auto select_cable = cable_list.find(args.cable);
	if (select_cable == cable_list.end()) {
		cerr << "error : " << args.cable << " not found" << endl;
		return EXIT_FAILURE;
	}
	cable = select_cable->second;
	printf("%x %x\n", cable.pid, cable.vid);

	/* jtag base */
	FtdiJtag *jtag = new FtdiJtag(cable, 1, 6000000);
	/* SVF logic */
	SVF_jtag *svf = new SVF_jtag(jtag);
	/* epcq */
	EPCQ epcq(cable.vid, cable.pid, 2, 6000000);

	/* chain detection:
	 * GGM TODO: must be done before
	 */
	vector<int> listDev;
	int found = jtag->detectChain(listDev, 5);
	int idcode = listDev[0];
	printf("found %d devices\n", found);
	if (found > 1) {
		cerr << "currently only one device is supported" << endl;
		return EXIT_FAILURE;
	}
	printf("%x\n", idcode);
	if (fpga_list.find(idcode) == fpga_list.end()) {
		cout << "Error: device not supported" << endl;
		return 1;
	} else {
		printf("idcode 0x%x\nfunder %s\nmodel  %s\nfamily %s\n",
			idcode,
			fpga_list[idcode].funder.c_str(),
			fpga_list[idcode].model.c_str(),
			fpga_list[idcode].family.c_str());
	}
	string fab = fpga_list[idcode].funder;

	/* display fpga and eeprom */
	if (args.display) {
		vector<int> listDev;
		int found = jtag->detectChain(listDev, 5);
		printf("found %d devices\n", found);
		for (unsigned int z=0; z < listDev.size(); z++)
			printf("%x\n", listDev[z]);
		printf("\n\n");

		svf->parse(BIT_FOR_FLASH);
		printf("%x\n", epcq.detect());
	}

	if (prog_mem) {
		if (file_format == SVF_FORMAT) {
			svf->parse(args.bit_file);
		} else {
			printf("todo\n");
			Xilinx xil(jtag, Device::MEM_MODE, args.bit_file);
			printf("%x\n", xil.idCode());
			xil.program();
			//xil.reset();
		}
	} else if (prog_eeprom) {
		svf->parse(BIT_FOR_FLASH);
		epcq.program(args.offset, args.bit_file, reverse_order);
	}
	if (args.reset) {
		if (fab == "xilinx") {
			Xilinx xil(jtag, Device::NONE_MODE, "");
			xil.reset();
		} else {
			reset_fpga(jtag);
		}
	}
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
		arguments->display = true;
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

