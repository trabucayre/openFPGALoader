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
#include "cxxopts.hpp"
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <string.h>
#include <unistd.h>
#include <vector>

#include "altera.hpp"
#include "anlogic.hpp"
#include "board.hpp"
#include "cable.hpp"
#include "device.hpp"
#include "display.hpp"
#include "efinix.hpp"
#include "ftdispi.hpp"
#include "gowin.hpp"
#include "ice40.hpp"
#include "lattice.hpp"
#include "jtag.hpp"
#include "part.hpp"
#include "spiFlash.hpp"
#include "rawParser.hpp"
#include "xilinx.hpp"

using namespace std;

struct arguments {
	int8_t verbose;
	bool reset, detect;
	unsigned int offset;
	string bit_file;
	string device;
	string cable;
	string ftdi_serial;
	int ftdi_channel;
	uint32_t freq;
	string board;
	bool pin_config;
	bool list_cables;
	bool list_boards;
	bool list_fpga;
	Device::prog_type_t prg_type;
	bool is_list_command;
	bool spi;
	string file_type;
};

int parse_opt(int argc, char **argv, struct arguments *args, jtag_pins_conf_t *pins_config);

void displaySupported(const struct arguments &args);

int main(int argc, char **argv)
{
	cable_t cable;
	target_cable_t *board = NULL;
	jtag_pins_conf_t pins_config = {0, 0, 0, 0};

	/* command line args. */
	struct arguments args = {0, false, false, 0, "", "", "-", "", -1, 6000000, "-",
			false, false, false, false, Device::WR_SRAM, false, false, ""};
	/* parse arguments */
	try {
		if (parse_opt(argc, argv, &args, &pins_config))
			return EXIT_SUCCESS;
	} catch (std::exception &e) {
		printError("Error in parse arg step");
		return EXIT_FAILURE;
	}

	if (args.is_list_command) {
		displaySupported(args);
		return EXIT_SUCCESS;
	}

	if (args.prg_type == Device::WR_SRAM)
		cout << "write to ram" << endl;
	if (args.prg_type == Device::WR_FLASH)
		cout << "write to flash" << endl;

	if (args.board[0] != '-' && board_list.find(args.board) != board_list.end()) {
		board = &(board_list[args.board]);
	}

	/* if a board name is specified try to use this to determine cable */
	if (board) {
		/* set pins config (only when user has not already provided
		 * configuration
		 */
		if (!args.pin_config) {
			pins_config.tdi_pin = board->jtag_pins_config.tdi_pin;
			pins_config.tdo_pin = board->jtag_pins_config.tdo_pin;
			pins_config.tms_pin = board->jtag_pins_config.tms_pin;
			pins_config.tck_pin = board->jtag_pins_config.tck_pin;
		}
		/* search for cable */
		auto t = cable_list.find(board->cable_name);
		if (t == cable_list.end()) {
			cout << "Board " << args.board << " has not default cable" << endl;
		} else {
			if (args.cable[0] == '-') { // no use selection
				args.cable = (*t).first; // use board default cable
			} else {
				cout << "Board default cable overridden with " << args.cable << endl;
			}
		}
	}

	if (args.cable[0] == '-') { /* if no board and no cable */
		if (args.verbose > 0)
			cout << "No cable or board specified: using direct ft2232 interface" << endl;
		args.cable = "ft2232";
	}

	auto select_cable = cable_list.find(args.cable);
	if (select_cable == cable_list.end()) {
		printError("error : " + args.cable + " not found");
		return EXIT_FAILURE;
	}
	cable = select_cable->second;

	if (args.ftdi_channel != -1) {
		if (cable.type != MODE_FTDI_SERIAL && cable.type != MODE_FTDI_BITBANG){
			printError("Error: FTDI channel param is for FTDI cables.");
			return EXIT_FAILURE;
		}

		int mapping[] = {INTERFACE_A, INTERFACE_B, INTERFACE_C, INTERFACE_D};
		cable.config.interface = mapping[args.ftdi_channel];
	}

	if (!args.ftdi_serial.empty()) {
		if (cable.type != MODE_FTDI_SERIAL && cable.type != MODE_FTDI_BITBANG){
			printError("Error: FTDI serial param is for FTDI cables.");
			return EXIT_FAILURE;
		}
	}

	/* FLASH direct access */
	if (args.spi || (board && board->mode == COMM_SPI)) {
		FtdiSpi *spi = NULL;
		spi_pins_conf_t pins_config = board->spi_pins_config;

		try {
			spi = new FtdiSpi(cable.config, pins_config, args.freq, args.verbose > 0);
		} catch (std::exception &e) {
			printError("Error: Failed to claim cable");
			return EXIT_FAILURE;
		}

		if (board->manufacturer == "efinix") {
			Efinix target(spi, args.bit_file, args.file_type,
				board->reset_pin, board->done_pin, args.verbose);
			target.program(args.offset);
		} else if (board->manufacturer == "lattice") {
			Ice40 target(spi, args.bit_file, args.file_type,
				board->reset_pin, board->done_pin, args.verbose);
			target.program(args.offset);
		} else {
			RawParser *bit = NULL;
			if (board->reset_pin) {
				spi->gpio_set_output(board->reset_pin, true);
				spi->gpio_clear(board->reset_pin, true);
			}

			SPIFlash flash((SPIInterface *)spi, args.verbose);
			flash.power_up();
			flash.reset();
			flash.read_id();

			if (!args.bit_file.empty() || !args.file_type.empty()) {
				printInfo("Open file " + args.bit_file + " ", false);
				try {
					bit = new RawParser(args.bit_file, false);
					printSuccess("DONE");
				} catch (std::exception &e) {
					printError("FAIL");
					delete spi;
					return EXIT_FAILURE;
				}

				printInfo("Parse file ", false);
				if (bit->parse() == EXIT_FAILURE) {
					printError("FAIL");
					delete spi;
					return EXIT_FAILURE;
				} else {
					printSuccess("DONE");
				}

				flash.erase_and_prog(args.offset, bit->getData(), bit->getLength()/8);

				delete bit;
			}
			if (board->reset_pin)
				spi->gpio_set(board->reset_pin, true);
		}

		delete spi;

		return EXIT_SUCCESS;
	}

	/* jtag base */
	Jtag *jtag;
	try {
		jtag = new Jtag(cable, &pins_config, args.device, args.ftdi_serial, args.freq, false);
	} catch (std::exception &e) {
		printError("Error: Failed to claim cable");
		return EXIT_FAILURE;
	}

	/* chain detection */
	vector<int> listDev;
	int found = jtag->detectChain(listDev, 5);

	if (args.verbose > 0)
		cout << "found " << std::to_string(found) << " devices" << endl;
	if (found > 1) {
		printError("Error: currently only one device is supported");
		delete(jtag);
		return EXIT_FAILURE;
	} else if (found < 1) {
		printError("Error: no device found");
		delete(jtag);
		return EXIT_FAILURE;
	}

	int idcode = listDev[0];

	if (fpga_list.find(idcode) == fpga_list.end()) {
		cerr << "Error: device " << hex << idcode << " not supported" << endl;
		delete(jtag);
		return EXIT_FAILURE;
	} else if (args.verbose > 0 || args.detect) {
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
	try {
		if (fab == "xilinx") {
			fpga = new Xilinx(jtag, args.bit_file, args.file_type,
				args.prg_type, args.verbose);
		} else if (fab == "altera") {
			fpga = new Altera(jtag, args.bit_file, args.file_type,
				args.verbose);
		} else if (fab == "anlogic") {
			fpga = new Anlogic(jtag, args.bit_file, args.file_type,
				args.prg_type, args.verbose);
		} else if (fab == "Gowin") {
			fpga = new Gowin(jtag, args.bit_file, args.file_type,
				args.prg_type, args.verbose);
		} else if (fab == "lattice") {
			fpga = new Lattice(jtag, args.bit_file, args.file_type,
				args.prg_type, args.verbose);
		} else {
			printError("Error: manufacturer " + fab + " not supported");
			delete(jtag);
			return EXIT_FAILURE;
		}
	} catch (std::exception &e) {
		printError("Error: Failed to claim FPGA device: " + string(e.what()));
		delete(jtag);
		return EXIT_FAILURE;
	}

	if (!args.bit_file.empty() || !args.file_type.empty()) {
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

// parse double from string in engineering notation
// can deal with postfixes k and m, add more when required
static int parse_eng(string arg, double *dst) {
	try {
		size_t end;
		double base = stod(arg, &end);
		if (end == arg.size()) {
			*dst = base;
			return 0;
		} else if (end == (arg.size() - 1)) {
			switch (arg.back()) {
			case 'k': case 'K':
				*dst = (uint32_t)(1e3 * base);
				return 0;
			case 'm': case 'M':
				*dst = (uint32_t)(1e6 * base);
				return 0;
			default:
				return EINVAL;
			}
		} else {
			return EINVAL;
		}
	} catch (...) {
		cerr << "error : speed: invalid format" << endl;
		return EINVAL;
	}
}

/* arguments parser */
int parse_opt(int argc, char **argv, struct arguments *args, jtag_pins_conf_t *pins_config)
{

	string freqo;
	vector<string> pins;
	bool verbose, quiet;
	try {
		cxxopts::Options options(argv[0], "openFPGALoader -- a program to flash FPGA",
			"<gwenhael.goavec-merou@trabucayre.com>");
		options
			.positional_help("BIT_FILE")
			.show_positional_help();

		options
			.add_options()
			("bitstream", "bitstream",
				cxxopts::value<std::string>(args->bit_file))
			("b,board",     "board name, may be used instead of cable",
				cxxopts::value<string>(args->board))
			("c,cable", "jtag interface", cxxopts::value<string>(args->cable))
			("ftdi-serial", "FTDI chip serial number", cxxopts::value<string>(args->ftdi_serial))
			("ftdi-channel", "FTDI chip channel number (channels 0-3 map to A-D)", cxxopts::value<int>(args->ftdi_channel))
#ifdef USE_UDEV
			("d,device",  "device to use (/dev/ttyUSBx)",
				cxxopts::value<string>(args->device))
#endif
			("detect",      "detect FPGA",
				cxxopts::value<bool>(args->detect))
			("file-type",   "provides file type instead of let's deduced by using extension",
				cxxopts::value<string>(args->file_type))
			("freq",        "jtag frequency (Hz)", cxxopts::value<string>(freqo))
			("f,write-flash",
				"write bitstream in flash (default: false, only for Gowin and ECP5 devices)")
			("list-boards", "list all supported boards",
				cxxopts::value<bool>(args->list_boards))
			("list-cables", "list all supported cables",
				cxxopts::value<bool>(args->list_cables))
			("list-fpga", "list all supported FPGA",
				cxxopts::value<bool>(args->list_fpga))
			("m,write-sram",
				"write bitstream in SRAM (default: true, only for Gowin and ECP5 devices)")
			("o,offset",  "start offset in EEPROM",
				cxxopts::value<unsigned int>(args->offset))
			("pins", "pin config (only for ft232R and fx2) TDI:TDO:TCK:TMS",
				cxxopts::value<vector<string>>(pins))
			("quiet", "Produce quiet output (no progress bar)",
				cxxopts::value<bool>(quiet))
			("r,reset",   "reset FPGA after operations",
				cxxopts::value<bool>(args->reset))
			("spi",   "SPI mode (only for FTDI in serial mode)",
				cxxopts::value<bool>(args->spi))
			("v,verbose", "Produce verbose output", cxxopts::value<bool>(verbose))
			("h,help", "Give this help list")
			("V,Version", "Print program version");

		options.parse_positional({"bitstream"});
		auto result = options.parse(argc, argv);

		if (result.count("help")) {
			cout << options.help() << endl;
			return 1;
		}

		if (verbose && quiet) {
			printError("Error: can't select quiet and verbose mode in same time");
			throw std::exception();
		}
		if (verbose)
			args->verbose = 1;
		if (quiet)
			args->verbose = -1;

		if (result.count("Version")) {
			cout << "openFPGALoader " << VERSION << endl;
			return 1;
		}

		if (result.count("write-flash") && result.count("write-sram")) {
			printError("Error: both write to flash and write to ram enabled");
			throw std::exception();
		}

		if (result.count("write-flash"))
			args->prg_type = Device::WR_FLASH;
		else if (result.count("write-sram"))
			args->prg_type = Device::WR_SRAM;

		if (result.count("freq")) {
			double freq;
			if (parse_eng(freqo, &freq)) {
				printError("Error: invalid format for --freq");
				throw std::exception();
			}
			if (freq < 1) {
				printError("Error: --freq must be positive");
				throw std::exception();
			}
			args->freq = static_cast<uint32_t>(freq);
		}

		if (result.count("ftdi-channel")) {
			if (args->ftdi_channel < 0 || args->ftdi_channel > 3) {
				printError("Error: valid FTDI channels are 0-3.");
				throw std::exception();
			}
		}

		if (result.count("pins")) {
			if (pins.size() != 4) {
				printError("Error: pin_config need 4 pins");
				throw std::exception();
			}

			static std::map <std::string, int> pins_list = {
				{"TXD", FT232RL_TXD},
				{"RXD", FT232RL_RXD},
				{"RTS", FT232RL_RTS},
				{"CTS", FT232RL_CTS},
				{"DTR", FT232RL_DTR},
				{"DSR", FT232RL_DSR},
				{"DCD", FT232RL_DCD},
				{"RI" , FT232RL_RI }};


			for (int i = 0; i < 4; i++) {
				int pin_num;
				try {
					pin_num = std::stoi(pins[i], nullptr, 16);
				} catch (std::exception &e) {
					if (pins_list.find(pins[i]) == pins_list.end()) {
						printError("Invalid pin name");
						throw std::exception();
					}
					pin_num = pins_list[pins[i]];
				}

				if ((pin_num > 7 || pin_num < 0) &&
					(pin_num > 0xD7 || pin_num < 0xD0) &&
					(pin_num > 0xC7 || pin_num < 0xC0) &&
					(pin_num > 0xB7 || pin_num < 0xB0) &&
					(pin_num > 0xA7 || pin_num < 0xA0)) {
					printError("Invalid pin ID");
					throw std::exception();
				}

				switch (i) {
					case 0:
						pins_config->tdi_pin = pin_num;
						break;
					case 1:
						pins_config->tdo_pin = pin_num;
						break;
					case 2:
						pins_config->tck_pin = pin_num;
						break;
					case 3:
						pins_config->tms_pin = pin_num;
						break;
				}
			}
			args->pin_config = true;
		}

		if (args->list_cables || args->list_boards || args->list_fpga)
			args->is_list_command = true;

		if (args->bit_file.empty() &&
			args->file_type.empty() &&
			!args->is_list_command &&
			!args->detect &&
			!args->reset) {
			printError("Error: bitfile not specified");
			cout << options.help() << endl;
			throw std::exception();
		}

	} catch (const cxxopts::OptionException& e) {
		cerr << "Error parsing options: " << e.what() << endl;
		throw std::exception();
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

