// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (C) 2019 Gwenhael Goavec-Merou <gwenhael.goavec-merou@trabucayre.com>
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
#include "colognechip.hpp"
#include "device.hpp"
#include "dfu.hpp"
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

#define DEFAULT_FREQ 	6000000

using namespace std;

struct arguments {
	int8_t verbose;
	bool reset, detect, verify;
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
	bool dfu;
	string file_type;
	string fpga_part;
	string probe_firmware;
	int index_chain;
	unsigned int file_size;
	bool external_flash;
	int16_t altsetting;
	uint16_t vid;
	uint16_t pid;
	uint32_t protect_flash;
	bool unprotect_flash;
	string flash_sector;
};

int parse_opt(int argc, char **argv, struct arguments *args, jtag_pins_conf_t *pins_config);

void displaySupported(const struct arguments &args);

int main(int argc, char **argv)
{
	cable_t cable;
	target_board_t *board = NULL;
	jtag_pins_conf_t pins_config = {0, 0, 0, 0};

	/* command line args. */
	struct arguments args = {0, false, false, false, 0, "", "", "-", "", -1,
			0, "-", false, false, false, false, Device::WR_SRAM, false,
			false, false, "", "", "", -1, 0, false, -1, 0, 0, 0, false, ""};
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

	if (args.board[0] != '-') {
		if (board_list.find(args.board) != board_list.end()) {
			board = &(board_list[args.board]);
		} else {
			printError("Error: cannot find board \'" +args.board + "\'");
			return EXIT_FAILURE;
		}
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

		/* Xilinx only: to write flash exact fpga model must be provided */
		if (!board->fpga_part.empty() && !args.fpga_part.empty())
			printInfo("Board default fpga part overridden with " + args.fpga_part);
		else if (!board->fpga_part.empty() && args.fpga_part.empty())
			args.fpga_part = board->fpga_part;

		/* Some boards can override the default clock speed
		 * if args.freq == 0: the `--freq` arg has not been used
		 * => apply board->default_freq with a value of
		 * 0 (no default frequency) or > 0 (board has a default frequency)
		 */
		if (args.freq == 0)
			args.freq = board->default_freq;
	}

	if (args.cable[0] == '-') { /* if no board and no cable */
		if (args.verbose > 0)
			cout << "No cable or board specified: using direct ft2232 interface" << endl;
		args.cable = "ft2232";
	}

	/* if args.freq == 0: no user requirement nor board default
	 * clock speed => set default frequency
	 */
	if (args.freq == 0)
		args.freq = DEFAULT_FREQ;

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

	if (args.vid != 0) {
		printInfo("Cable VID overridden");
		cable.config.vid = args.vid;
	}
	if (args.pid != 0) {
		printInfo("Cable PID overridden");
		cable.config.pid = args.pid;
	}

	/* FLASH direct access */
	if (args.spi || (board && board->mode == COMM_SPI)) {
		FtdiSpi *spi = NULL;
		spi_pins_conf_t pins_config;
		if (board)
			pins_config = board->spi_pins_config;

		try {
			spi = new FtdiSpi(cable.config, pins_config, args.freq, args.verbose > 0);
		} catch (std::exception &e) {
			printError("Error: Failed to claim cable");
			return EXIT_FAILURE;
		}

		int spi_ret = EXIT_SUCCESS;

		if (board && board->manufacturer != "none") {
			Device *target;
			if (board->manufacturer == "efinix") {
				target = new Efinix(spi, args.bit_file, args.file_type,
					board->reset_pin, board->done_pin, board->oe_pin,
					args.verify, args.verbose);
			} else if (board->manufacturer == "lattice") {
				target = new Ice40(spi, args.bit_file, args.file_type,
					board->reset_pin, board->done_pin, args.verify, args.verbose);
			} else if (board->manufacturer == "colognechip") {
				CologneChip target(spi, args.bit_file, args.file_type, args.prg_type,
					board->reset_pin, board->done_pin, DBUS6, board->oe_pin,
					args.verify, args.verbose);
			}
			if (args.prg_type == Device::RD_FLASH) {
				if (args.file_size == 0) {
					printError("Error: 0 size for dump");
				} else {
					target->dumpFlash(args.offset, args.file_size);
				}
			} else if (args.prg_type == Device::WR_FLASH) {
				target->program(args.offset, args.unprotect_flash);
			}
			if (args.unprotect_flash && args.bit_file.empty())
				if (!target->unprotect_flash())
					spi_ret = EXIT_FAILURE;
			if (args.protect_flash)
				if (!target->protect_flash(args.protect_flash))
					spi_ret = EXIT_FAILURE;
		} else {
			RawParser *bit = NULL;
			if (board->reset_pin) {
				spi->gpio_set_output(board->reset_pin, true);
				spi->gpio_clear(board->reset_pin, true);
			}

			SPIFlash flash((SPIInterface *)spi, args.unprotect_flash, args.verbose);
			flash.display_status_reg();

			if (args.prg_type != Device::RD_FLASH &&
					(!args.bit_file.empty() || !args.file_type.empty())) {
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

				try {
					flash.erase_and_prog(args.offset, bit->getData(), bit->getLength()/8);
				} catch (std::exception &e) {
					printError("FAIL: " + string(e.what()));
				}

				if (args.verify)
					flash.verify(args.offset, bit->getData(), bit->getLength() / 8);

				delete bit;
			} else if (args.prg_type == Device::RD_FLASH) {
				flash.dump(args.bit_file, args.offset, args.file_size);
			}

			if (args.unprotect_flash && args.bit_file.empty())
				if (!flash.disable_protection())
					spi_ret = EXIT_FAILURE;
			if (args.protect_flash)
				if (!flash.enable_protection(args.protect_flash))
					spi_ret = EXIT_FAILURE;

			if (board->reset_pin)
				spi->gpio_set(board->reset_pin, true);
		}

		delete spi;

		return spi_ret;
	}

	/* ------------------- */
	/* DFU access          */
	/* ------------------- */
	if (args.dfu || (board && board->mode == COMM_DFU)) {
		/* try to init DFU probe */
		DFU *dfu = NULL;
		uint16_t vid = 0, pid = 0;
		int altsetting = -1;
		if (board) {
			vid = board->vid;
			pid = board->pid;
			altsetting = board->altsetting;
		}
		if (args.altsetting != -1) {
			if (altsetting != -1)
				printInfo("Board altsetting overridden");
			altsetting = args.altsetting;
		}

		if (args.vid != 0) {
			if (vid != 0)
				printInfo("Board VID overridden");
			vid = args.vid;
		}
		if (args.pid != 0) {
			if (pid != 0)
				printInfo("Board PID overridden");
			pid = args.pid;
		}

		try {
			dfu = new DFU(args.bit_file, vid, pid, altsetting, args.verbose);
		} catch (std::exception &e) {
			printError("DFU init failed with: " + string(e.what()));
			return EXIT_FAILURE;
		}
		/* if verbose or detect: display device */
		if (args.verbose > 0 || args.detect)
			dfu->displayDFU();

		/* if detect: stop */
		if (args.detect)
			return EXIT_SUCCESS;

		try {
			dfu->download();
		} catch (std::exception &e) {
			printError("DFU download failed with: " + string(e.what()));
			return EXIT_FAILURE;
		}

		return EXIT_SUCCESS;
	}

	/* jtag base */
	Jtag *jtag;
	try {
		jtag = new Jtag(cable, &pins_config, args.device, args.ftdi_serial,
				args.freq, args.verbose, args.probe_firmware);
	} catch (std::exception &e) {
		printError("JTAG init failed with: " + string(e.what()));
		return EXIT_FAILURE;
	}

	/* chain detection */
	vector<int> listDev = jtag->get_devices_list();
	int found = listDev.size();
	int idcode = -1, index = 0;

	if (args.verbose > 0)
		cout << "found " << std::to_string(found) << " devices" << endl;

	/* in verbose mode or when detect
	 * display full chain with details
	 */
	if (args.verbose > 0 || args.detect) {
		for (int i = 0; i < found; i++) {
			int t = listDev[i];
			printf("index %d:\n", i);
			if (fpga_list.find(t) != fpga_list.end()) {
				printf("\tidcode 0x%x\n\tmanufacturer %s\n\tfamily %s\n\tmodel  %s\n",
				t,
				fpga_list[t].manufacturer.c_str(),
				fpga_list[t].family.c_str(),
				fpga_list[t].model.c_str());
				printf("\tirlength %d\n", fpga_list[t].irlength);
			} else if (misc_dev_list.find(t) != misc_dev_list.end()) {
				printf("\tidcode   0x%x\n\ttype     %s\n\tirlength %d\n",
				t,
				misc_dev_list[t].name.c_str(),
				misc_dev_list[t].irlength);
			}
		}
		if (args.detect == true) {
			delete jtag;
			return EXIT_SUCCESS;
		}
	}

	if (found != 0) {
		if (args.index_chain == -1) {
			for (int i = 0; i < found; i++) {
				if (fpga_list.find(listDev[i]) != fpga_list.end()) {
					index = i;
					if (idcode != -1) {
						printError("Error: more than one FPGA found");
						printError("Use --index-chain to force selection");
						for (int i = 0; i < found; i++)
							printf("0x%08x\n", listDev[i]);
						delete(jtag);
						return EXIT_FAILURE;
					} else {
						idcode = listDev[i];
					}
				}
			}
		} else {
			index = args.index_chain;
			if (index > found || index < 0) {
				printError("wrong index for device in JTAG chain");
				delete(jtag);
				return EXIT_FAILURE;
			}
			idcode = listDev[index];
		}
	} else {
		printError("Error: no device found");
		delete(jtag);
		return EXIT_FAILURE;
	}

	jtag->device_select(index);

	/* check if selected device is supported
	 * mainly used in conjunction with --index-chain
	 */
	if (fpga_list.find(idcode) == fpga_list.end()) {
		cerr << "Error: device " << hex << idcode << " not supported" << endl;
		delete(jtag);
		return EXIT_FAILURE;
	}

	string fab = fpga_list[idcode].manufacturer;

	Device *fpga;
	try {
		if (fab == "xilinx") {
			fpga = new Xilinx(jtag, args.bit_file, args.file_type,
				args.prg_type, args.fpga_part, args.verify, args.verbose);
		} else if (fab == "altera") {
			fpga = new Altera(jtag, args.bit_file, args.file_type,
				args.prg_type, args.fpga_part, args.verify, args.verbose);
		} else if (fab == "anlogic") {
			fpga = new Anlogic(jtag, args.bit_file, args.file_type,
				args.prg_type, args.verify, args.verbose);
		} else if (fab == "efinix") {
			fpga = new Efinix(jtag, args.bit_file, args.file_type,
				/*DBUS4 | DBUS7, DBUS5*/args.board, args.verify, args.verbose);
		} else if (fab == "Gowin") {
			fpga = new Gowin(jtag, args.bit_file, args.file_type,
				args.prg_type, args.external_flash, args.verify, args.verbose);
		} else if (fab == "lattice") {
			fpga = new Lattice(jtag, args.bit_file, args.file_type,
				args.prg_type, args.flash_sector, args.verify, args.verbose);
		} else if (fab == "colognechip") {
			fpga = new CologneChip(jtag, args.bit_file, args.file_type,
				args.prg_type, args.board, args.cable, args.verify, args.verbose);
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

	if ((!args.bit_file.empty() || !args.file_type.empty())
			&& args.prg_type != Device::RD_FLASH) {
		try {
			fpga->program(args.offset, args.unprotect_flash);
		} catch (std::exception &e) {
			printError("Error: Failed to program FPGA: " + string(e.what()));
			delete(fpga);
			delete(jtag);
			return EXIT_FAILURE;
		}
	}

	/* unprotect SPI flash */
	if (args.unprotect_flash && args.bit_file.empty()) {
		fpga->unprotect_flash();
	}

	/* protect SPI flash */
	if (args.protect_flash != 0) {
		fpga->protect_flash(args.protect_flash);
	}

	if (args.prg_type == Device::RD_FLASH) {
		if (args.file_size == 0) {
			printError("Error: 0 size for dump");
		} else {
			fpga->dumpFlash(args.offset, args.file_size);
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
	int8_t verbose_level = -2;
	try {
		cxxopts::Options options(argv[0], "openFPGALoader -- a program to flash FPGA",
			"<gwenhael.goavec-merou@trabucayre.com>");
		options
			.positional_help("BIT_FILE")
			.show_positional_help();

		options
			.add_options()
			("altsetting", "DFU interface altsetting (only for DFU mode)",
				cxxopts::value<int16_t>(args->altsetting))
			("bitstream", "bitstream",
				cxxopts::value<std::string>(args->bit_file))
			("b,board",     "board name, may be used instead of cable",
				cxxopts::value<string>(args->board))
			("c,cable", "jtag interface", cxxopts::value<string>(args->cable))
			("vid", "probe Vendor ID", cxxopts::value<uint16_t>(args->vid))
			("pid", "probe Product ID", cxxopts::value<uint16_t>(args->pid))

			("ftdi-serial", "FTDI chip serial number", cxxopts::value<string>(args->ftdi_serial))
			("ftdi-channel", "FTDI chip channel number (channels 0-3 map to A-D)", cxxopts::value<int>(args->ftdi_channel))
#ifdef USE_UDEV
			("d,device",  "device to use (/dev/ttyUSBx)",
				cxxopts::value<string>(args->device))
#endif
			("detect",      "detect FPGA",
				cxxopts::value<bool>(args->detect))
			("dfu",   "DFU mode", cxxopts::value<bool>(args->dfu))
			("dump-flash",  "Dump flash mode")
			("external-flash",
			 	"select ext flash for device with internal and external storage",
				cxxopts::value<bool>(args->external_flash))
			("file-size",   "provides size in Byte to dump, must be used with dump-flash",
				cxxopts::value<unsigned int>(args->file_size))
			("file-type",   "provides file type instead of let's deduced by using extension",
				cxxopts::value<string>(args->file_type))
			("flash-sector","flash sector (Lattice parts only)", cxxopts::value<string>(args->flash_sector))
			("fpga-part",   "fpga model flavor + package", cxxopts::value<string>(args->fpga_part))
			("freq",        "jtag frequency (Hz)", cxxopts::value<string>(freqo))
			("f,write-flash",
				"write bitstream in flash (default: false)")
			("index-chain",  "device index in JTAG-chain",
				cxxopts::value<int>(args->index_chain))
			("list-boards", "list all supported boards",
				cxxopts::value<bool>(args->list_boards))
			("list-cables", "list all supported cables",
				cxxopts::value<bool>(args->list_cables))
			("list-fpga", "list all supported FPGA",
				cxxopts::value<bool>(args->list_fpga))
			("m,write-sram",
				"write bitstream in SRAM (default: true)")
			("o,offset",  "start offset in EEPROM",
				cxxopts::value<unsigned int>(args->offset))
			("pins", "pin config (only for ft232R) TDI:TDO:TCK:TMS",
				cxxopts::value<vector<string>>(pins))
			("probe-firmware", "firmware for JTAG probe (usbBlasterII)",
				cxxopts::value<string>(args->probe_firmware))
			("protect-flash",   "protect SPI flash area",
				cxxopts::value<uint32_t>(args->protect_flash))
			("quiet", "Produce quiet output (no progress bar)",
				cxxopts::value<bool>(quiet))
			("r,reset",   "reset FPGA after operations",
				cxxopts::value<bool>(args->reset))
			("spi",   "SPI mode (only for FTDI in serial mode)",
				cxxopts::value<bool>(args->spi))
			("unprotect-flash",   "Unprotect flash blocks",
				cxxopts::value<bool>(args->unprotect_flash))
			("v,verbose", "Produce verbose output", cxxopts::value<bool>(verbose))
			("verbose-level", "verbose level -1: quiet, 0: normal, 1:verbose, 2:debug",
				cxxopts::value<int8_t>(verbose_level))
			("h,help", "Give this help list")
			("verify", "Verify write operation (SPI Flash only)",
				cxxopts::value<bool>(args->verify))
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
		if (verbose_level != -2) {
			if ((verbose && verbose_level != 1) ||
					(quiet && verbose_level != -1)) {
				printError("Error: mismatch quiet/verbose and verbose-level\n");
				throw std::exception();
			}

			args->verbose = verbose_level;
		}

		if (result.count("Version")) {
			cout << "openFPGALoader " << VERSION << endl;
			return 1;
		}

		if (result.count("write-flash") && result.count("write-sram") &&
				result.count("dump-flash")) {
			printError("Error: both write to flash and write to ram enabled");
			throw std::exception();
		}

		if (result.count("write-flash"))
			args->prg_type = Device::WR_FLASH;
		else if (result.count("write-sram"))
			args->prg_type = Device::WR_SRAM;
		else if (result.count("dump-flash"))
			args->prg_type = Device::RD_FLASH;
		else if (result.count("external-flash"))
			args->prg_type = Device::WR_FLASH;

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
					pin_num = 1 << std::stoi(pins[i], nullptr, 0);
				} catch (std::exception &e) {
					if (pins_list.find(pins[i]) == pins_list.end()) {
						printError("Invalid pin name");
						throw std::exception();
					}
					pin_num = pins_list[pins[i]];
				}

				if (pin_num > FT232RL_RI || pin_num < FT232RL_TXD) {
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
			!args->protect_flash &&
			!args->unprotect_flash &&
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
			target_board_t c = (*b).second;
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

