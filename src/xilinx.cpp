// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (C) 2019 Gwenhael Goavec-Merou <gwenhael.goavec-merou@trabucayre.com>
 */

#include <unistd.h>

#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>

#include "jtag.hpp"
#include "bitparser.hpp"
#include "configBitstreamParser.hpp"
#include "jedParser.hpp"
#include "mcsParser.hpp"
#include "spiFlash.hpp"
#include "rawParser.hpp"

#include "display.hpp"
#include "xilinx.hpp"
#include "part.hpp"
#include "progressBar.hpp"

Xilinx::Xilinx(Jtag *jtag, const std::string &filename,
	const std::string &file_type,
	Device::prog_type_t prg_type,
	const std::string &device_package, bool verify, int8_t verbose):
	Device(jtag, filename, file_type, verify, verbose),
	_device_package(device_package)
{
	if (prg_type == Device::RD_FLASH) {
		_mode = Device::READ_MODE;
	} else if (!_file_extension.empty()) {
		if (_file_extension == "mcs") {
			_mode = Device::SPI_MODE;
		} else if (_file_extension == "bit" || _file_extension == "bin") {
			if (prg_type == Device::WR_SRAM)
				_mode = Device::MEM_MODE;
			else
				_mode = Device::SPI_MODE;
		} else if (_file_extension == "jed") {
			_mode = Device::FLASH_MODE;
		} else {
			_mode = Device::SPI_MODE;
		}
	}

	uint32_t idcode = _jtag->get_target_device_id();
	std::string family = fpga_list[idcode].family;
	if (family.substr(0, 5) == "artix") {
		_fpga_family = ARTIX_FAMILY;
	} else if (family == "spartan7") {
		_fpga_family = SPARTAN7_FAMILY;
	} else if (family == "zynq") {
		_fpga_family = ZYNQ_FAMILY;
	} else if (family == "kintex7") {
		_fpga_family = KINTEX_FAMILY;
	} else if (family == "spartan3") {
		_fpga_family = SPARTAN3_FAMILY;
		if (_mode != Device::MEM_MODE) {
			throw std::runtime_error("Error: Only load to mem is supported");
		}
	} else if (family == "xcf") {
		_fpga_family = XCF_FAMILY;
		if (_mode == Device::MEM_MODE) {
			throw std::runtime_error("Error: Only write or read is supported");
		}
	} else if (family == "spartan6") {
		_fpga_family = SPARTAN6_FAMILY;
	} else if (family == "xc9500xl") {
		_fpga_family = XC95_FAMILY;
		switch (idcode) {
		case 0x09602093:
			_xc95_line_len = 2;
			break;
		case 0x09604093:
			_xc95_line_len = 4;
			break;
		case 0x09608093:
			_xc95_line_len = 8;
			break;
		case 0x09616093:
			_xc95_line_len = 16;
			break;
		}
	} else {
		_fpga_family = UNKNOWN_FAMILY;
	}
}
Xilinx::~Xilinx() {}

#define USER1	0x02
#define CFG_IN   0x05
#define USERCODE   0x08
#define IDCODE     0x09
#define ISC_ENABLE 0x10
#define JPROGRAM 0x0B
#define JSTART   0x0C
#define JSHUTDOWN 0x0D
#define ISC_PROGRAM 0x11
#define ISC_DISABLE 0x16
#define BYPASS   0xff

/* xc95 instructions set */
#define XC95_IDCODE          0xfe
#define XC95_ISC_ERASE       0xed
#define XC95_ISC_ENABLE      0xe9
#define XC95_ISC_DISABLE     0xf0
#define XC95_XSC_BLANK_CHECK 0xe5
#define XC95_ISC_PROGRAM     0xea
#define XC95_ISC_READ        0xee

void Xilinx::reset()
{
	_jtag->shiftIR(JSHUTDOWN, 6);
	_jtag->shiftIR(JPROGRAM, 6);
	_jtag->set_state(Jtag::RUN_TEST_IDLE);
	_jtag->toggleClk(10000*12);

	_jtag->set_state(Jtag::RUN_TEST_IDLE);
	_jtag->toggleClk(2000);

	_jtag->shiftIR(BYPASS, 6);
	_jtag->set_state(Jtag::RUN_TEST_IDLE);
	_jtag->toggleClk(2000);
}

int Xilinx::idCode()
{
	int id = 0;
	unsigned char tx_data[4]= {0x00, 0x00, 0x00, 0x00};
	unsigned char rx_data[4];
	_jtag->go_test_logic_reset();
	_jtag->shiftIR(IDCODE, 6);
	_jtag->shiftDR(tx_data, rx_data, 32);
	id = ((rx_data[0] & 0x000000ff) |
		((rx_data[1] << 8) & 0x0000ff00) |
		((rx_data[2] << 16) & 0x00ff0000) |
		((rx_data[3] << 24) & 0xff000000));

	/* workaround for XC95 with different
	 * IR length and IDCODE value
	 */
	if (id == 0) {
		_jtag->go_test_logic_reset();
		_jtag->shiftIR(XC95_IDCODE, 8);
		_jtag->shiftDR(tx_data, rx_data, 32);
		id = ((rx_data[0] & 0x000000ff) |
			((rx_data[1] << 8) & 0x0000ff00) |
			((rx_data[2] << 16) & 0x00ff0000) |
			((rx_data[3] << 24) & 0xff000000));
	}

	return id;
}

void Xilinx::program(unsigned int offset)
{
	ConfigBitstreamParser *bit;
	bool reverse = false;

	/* nothing to do */
	if (_mode == Device::NONE_MODE || _mode == Device::READ_MODE)
		return;

	if (_mode == Device::FLASH_MODE && _file_extension == "jed") {
		flow_program();
		return;
	}

	if (_fpga_family == XC95_FAMILY) {
		printError("Only jed file and flash mode supported for XC95 CPLD");
		return;
	}

	if (_mode == Device::MEM_MODE || _fpga_family == XCF_FAMILY)
		reverse = true;

	printInfo("Open file ", false);
	try {
		if (_file_extension == "bit")
			bit = new BitParser(_filename, reverse, _verbose);
		else if (_file_extension == "mcs")
			bit = new McsParser(_filename, reverse, _verbose);
		else
			bit = new RawParser(_filename, reverse);
	} catch (std::exception &e) {
		printError("FAIL");
		return;
	}

	printSuccess("DONE");

	printInfo("Parse file ", false);
	if (bit->parse() == EXIT_FAILURE) {
		printError("FAIL");
		delete bit;
		return;
	} else {
		printSuccess("DONE");
	}

	if (_verbose)
		bit->displayHeader();

	if (_fpga_family == XCF_FAMILY) {
		xcf_program(bit);
		return;
	}

	if (_mode == Device::SPI_MODE) {
		program_spi(bit, offset);
		reset();
	} else {
		program_mem(bit);
	}

	delete bit;
}

bool Xilinx::load_bridge()
{
	if (_device_package.empty()) {
		printError("Can't program SPI flash: missing device-package information");
		return false;
	}

	// DATA_DIR is defined at compile time.
	std::string bitname = DATA_DIR "/openFPGALoader/spiOverJtag_";
	bitname += _device_package + ".bit";

	std::cout << "use: " << bitname << std::endl;

	/* first: load spi over jtag */
	try {
		BitParser bridge(bitname, true, _verbose);
		bridge.parse();
		program_mem(&bridge);
	} catch (std::exception &e) {
		printError(e.what());
		throw std::runtime_error(e.what());
	}
	return true;
}

void Xilinx::program_spi(ConfigBitstreamParser * bit, unsigned int offset)
{
	/* first need to have bridge in RAM */
	if (load_bridge() == false)
		return;

	uint8_t *data = bit->getData();
	int length = bit->getLength() / 8;

	SPIFlash spiFlash(this, (_verbose ? 1 : (_quiet ? -1 : 0)));
	spiFlash.reset();
	spiFlash.read_id();
	spiFlash.read_status_reg();
	spiFlash.erase_and_prog(offset, data, length);

	/* verify write if required */
	if (_verify)
		spiFlash.verify(offset, data, length, 256);
}

void Xilinx::program_mem(ConfigBitstreamParser *bitfile)
{
	if (_file_extension.empty()) return;
	std::cout << "load program" << std::endl;
	unsigned char tx_buf, rx_buf;
	/*            comment                                TDI   TMS TCK
	 * 1: On power-up, place a logic 1 on the TMS,
	 *    and clock the TCK five times. This ensures      X     1   5
	 *    starting in the TLR (Test-Logic-Reset) state.
	 */
	_jtag->go_test_logic_reset();
	/*
	 * 2: Move into the RTI state.                        X     0   1
	 * 3: Move into the SELECT-IR state.                  X     1   2
	 * 4: Enter the SHIFT-IR state.                       X     0   2
	 * 5: Start loading the JPROGRAM instruction,     01011(4)  0   5
	 *    LSB first:
	 * 6: Load the MSB of the JPROGRAM instruction
	 *    when exiting SHIFT-IR, as defined in the        0     1   1
	 *    IEEE standard.
	 * 7: Place a logic 1 on the TMS and clock the
	 *    TCK five times. This ensures starting in        X     1   5
	 *    the TLR (Test-Logic-Reset) state.
	 */
	_jtag->shiftIR(JPROGRAM, 6);
	/* test */
	tx_buf = BYPASS;
	do {
		_jtag->shiftIR(&tx_buf, &rx_buf, 6);
	} while (!(rx_buf &0x01));
	/*
	 * 8: Move into the RTI state.                        X     0   10,000(1)
	 */
	_jtag->set_state(Jtag::RUN_TEST_IDLE);
	_jtag->toggleClk(10000*12);
	/*
	 * 9: Start loading the CFG_IN instruction,
	 *    LSB first:                                    00101   0   5
	 * 10: Load the MSB of CFG_IN instruction when
	 *     exiting SHIFT-IR, as defined in the            0     1   1
	 *     IEEE standard.
	 */
	_jtag->shiftIR(CFG_IN, 6);
	/*
	 * 11: Enter the SELECT-DR state.                     X     1   2
	 */
	_jtag->set_state(Jtag::SELECT_DR_SCAN);
	/*
	 * 13: Shift in the FPGA bitstream. Bitn (MSB)
	 *     is the first bit in the bitstream(2).    bit1...bitn 0  (bits in bitstream)-1
	 * 14: Shift in the last bit of the bitstream.
	 *     Bit0 (LSB) shifts on the transition to       bit0    1   1
	 *     EXIT1-DR.
	 */
	/* GGM: TODO */
	int byte_length = bitfile->getLength() / 8;
	uint8_t *data = bitfile->getData();
	int tx_len, tx_end;
	int burst_len = byte_length / 100;

	ProgressBar progress("Flash SRAM", byte_length, 50, _quiet);

	for (int i=0; i < byte_length; i+=burst_len) {
		if (i + burst_len > byte_length) {
			tx_len = (byte_length - i) * 8;
			/*
			 * 15: Enter UPDATE-DR state.                 X     1   1
			 */
			tx_end = Jtag::UPDATE_DR;
		} else {
			tx_len = burst_len * 8;
	        /*
	         * 12: Enter the SHIFT-DR state.              X     0   2
	         */
			tx_end = Jtag::SHIFT_DR;
		}
		_jtag->shiftDR(data+i, NULL, tx_len, tx_end);
		_jtag->flush();
		progress.display(i);
	}
	progress.done();
	/*
	 * 16: Move into RTI state.                           X     0   1
	 */
	_jtag->set_state(Jtag::RUN_TEST_IDLE);
	/*
	 * 17: Enter the SELECT-IR state.                     X     1   2
	 * 18: Move to the SHIFT-IR state.                    X     0   2
	 * 19: Start loading the JSTART instruction 
	 *     (optional). The JSTART instruction           01100   0   5
	 *     initializes the startup sequence.
	 * 20: Load the last bit of the JSTART instruction.   0     1   1
	 * 21: Move to the UPDATE-IR state.                   X     1   1
	 */
	_jtag->shiftIR(JSTART, 6, Jtag::UPDATE_IR);
	/*
	 * 22: Move to the RTI state and clock the
	 *     startup sequence by applying a minimum         X     0   2000
	 *     of 2000 clock cycles to the TCK.
	 */
	_jtag->set_state(Jtag::RUN_TEST_IDLE);
	_jtag->toggleClk(2000);
	/*
	 * 23: Move to the TLR state. The device is
	 * now functional.                                    X     1   3
	 */
	_jtag->go_test_logic_reset();
}

bool Xilinx::dumpFlash(const std::string &filename,
		uint32_t base_addr, uint32_t len)
{
	if (_fpga_family == XC95_FAMILY || _fpga_family == XCF_FAMILY) {
		std::string buffer;
		if (_fpga_family == XC95_FAMILY) {
			/* enable ISC */
			flow_enable();
			buffer = flow_read();
			/* disable ISC */
			flow_disable();
		} else {
			/* enable ISC */
			xcf_flow_enable(0x34);
			buffer = xcf_read();
			/* disable ISC */
			xcf_flow_disable();
		}
		printInfo("Open dump file ", false);
		FILE *fd = fopen(filename.c_str(), "wb");
		if (!fd) {
			printError("FAIL");
			return false;
		}
		printSuccess("DONE");

		printInfo("Read flash ", false);
		fwrite(buffer.c_str(), sizeof(uint8_t), buffer.size(), fd);

		printSuccess("DONE");

		fclose(fd);

		return true;
	}

	int ret = true;
	/* first need to have bridge in RAM */
	if (load_bridge() == false)
		return false;

	/* prepare SPI access */
	SPIFlash flash(this, _verbose);

	try {
		flash.reset();
		ret = flash.dump(filename, base_addr, len, 256);
	} catch (std::exception &e) {
		printError(e.what());
		ret = false;
	}

	/* reset device */
	reset();

	return ret;
}

/*                                */
/* internal flash (xc95)          */
/* based on ISE xx_1532.bsd files */
/*                                */

void Xilinx::flow_enable()
{
	uint8_t xfer_buf = 0x15;
	_jtag->shiftIR(XC95_ISC_ENABLE, 8);
	_jtag->shiftDR(&xfer_buf, NULL, 6);
	_jtag->toggleClk(1);
}

void Xilinx::flow_disable()
{
	_jtag->shiftIR(XC95_ISC_DISABLE, 8);
	usleep(100);
	_jtag->shiftIR(BYPASS, 8);
	_jtag->toggleClk(1);
}

bool Xilinx::flow_erase()
{
	uint8_t xfer_buf[3] = {0x03, 0x00, 0x00};

	printInfo("Erase flash ", false);

	_jtag->shiftIR(XC95_ISC_ERASE, 8);
	_jtag->shiftDR(xfer_buf, NULL, 18);
	_jtag->toggleClk(2000000);
	_jtag->shiftDR(NULL, xfer_buf, 18);
	if ((xfer_buf[0] & 0x03) != 0x01) {
		printError("FAIL");
		return false;
	}

	if (_verify) {
		xfer_buf[0] = 0x03;
		xfer_buf[1] = xfer_buf[2] = 0x00;

		_jtag->shiftIR(XC95_XSC_BLANK_CHECK, 8);
		_jtag->shiftDR(xfer_buf, NULL, 18);
		_jtag->toggleClk(500);
		_jtag->shiftDR(NULL, xfer_buf, 18);
		if ((xfer_buf[0] & 0x03) != 0x01) {
			printError("FAIL");
			return false;
		}
	}
	printSuccess("DONE");

	return true;
}

bool Xilinx::flow_program()
{
	uint8_t wr_buf[16+2];  // largest section length
	uint8_t rd_buf[16+3];
	JedParser *jed;
	printInfo("Open file ", false);

	jed = new JedParser(_filename, _verbose);
	if (jed->parse() == EXIT_FAILURE) {
		printError("FAIL");
		return false;
	}
	printSuccess("DONE");

	/* limit JTAG clock frequency to 1MHz */
	if (_jtag->getClkFreq() > 1e6)
		_jtag->setClkFreq(1e6);

	/* enable ISC */
	flow_enable();

	/* erase internal flash */
	if (!flow_erase())
		return false;

	/* xc95 internal flash is written by sector
	 * for each one them 15 jed sections are used
	 */
	size_t nb_section = jed->nb_section() / (15);

	ProgressBar progress("Write Flash", nb_section, 50, _quiet);

	for (size_t i = 0; i < nb_section; i++) {
		uint16_t addr2 = i * 32;
		for (int ii = 0; ii < 15; ii++) {
			uint8_t mode = (ii == 14) ? 0x3 : 0x1;
			int id = i * 15 + ii;

			memcpy(wr_buf, jed->data_for_section(id)[0].c_str(),
					_xc95_line_len);
			wr_buf[_xc95_line_len] = (uint8_t) addr2&0xff;
			wr_buf[_xc95_line_len+ 1 ] = (uint8_t)((addr2 >> 8) & 0xff);

			_jtag->shiftIR(XC95_ISC_PROGRAM, 8);
			_jtag->shiftDR(&mode, NULL, 2, Jtag::SHIFT_DR);
			_jtag->shiftDR(wr_buf, NULL, 8 * (_xc95_line_len + 2));

			if (ii == 14)
				_jtag->toggleClk(20000);
			else
				_jtag->toggleClk(1);


			if (ii == 14) {
				mode = 0x00;
				for (int loop_try = 0; loop_try < 32; loop_try++) {
					_jtag->shiftIR(XC95_ISC_PROGRAM, 8);
					_jtag->shiftDR(&mode, NULL, 2, Jtag::SHIFT_DR);
					_jtag->shiftDR(wr_buf, NULL, 8 * (_xc95_line_len + 2));
					_jtag->toggleClk(50000);
					_jtag->shiftDR(NULL, rd_buf, 8 * (_xc95_line_len + 2) + 2);
					if ((rd_buf[0] & 0x03) == 0x01)
						break;
				}

				if ((rd_buf[0] & 0x03) != 0x01) {
					progress.fail();
					return false;
				}
			}
			addr2 += ((ii+1) % 0x05) ? 1 : 4;
		}
		progress.display(i);
	}
	progress.done();

	/* TODO: verify */
	if (_verify) {
		std::string flash = flow_read();
		int flash_pos = 0;
		ProgressBar progress2("Verify Flash", nb_section, 50, _quiet);
		for (size_t section = 0; section < 108; section++) {
			for (size_t subsection = 0; subsection < 15; subsection++) {
				int id = section * 15 + subsection;
				std::string content = jed->data_for_section(id)[0];
				for (int col = 0; col < _xc95_line_len; col++, flash_pos++) {
					if ((uint8_t)content[col] != (uint8_t)flash[flash_pos]) {
						char error[256];
						progress2.fail();
						snprintf(error, sizeof(error),
								"Error: wrong value: read %02x instead of %02x",
								(uint8_t)flash[flash_pos], (uint8_t)content[col]);
						printError(error);
						flow_disable();
						return false;
					}
				}
			}
		}
		progress2.done();
	}

	/* disable ISC */
	flow_disable();

	return true;
}

std::string Xilinx::flow_read()
{
	uint8_t mode;
	std::string buffer;
	uint8_t wr_buf[16+2];  // largest section length
	uint8_t rd_buf[16+2];
	memset(wr_buf, 0xff, 16);

	/* limit JTAG clock frequency to 1MHz */
	if (_jtag->getClkFreq() > 1e6)
		_jtag->setClkFreq(1e6);

	ProgressBar progress("Read Flash", 108, 50, _quiet);

	for (size_t section = 0; section < 108; section++) {
		uint16_t addr2 = section * 32;
		for (int subsection = 0; subsection < 15; subsection++) {
			wr_buf[_xc95_line_len    ] = (uint8_t)((addr2     ) & 0xff);
			wr_buf[_xc95_line_len + 1] = (uint8_t)((addr2 >> 8) & 0xff);

			mode = 3;
			_jtag->shiftIR(XC95_ISC_READ, 8);
			_jtag->shiftDR(&mode, NULL, 2, Jtag::SHIFT_DR);
			_jtag->shiftDR(wr_buf, NULL, 8 * (_xc95_line_len + 2));

			_jtag->toggleClk(1);

			mode = 0;
			_jtag->shiftDR(&mode, NULL, 2, Jtag::SHIFT_DR);
			_jtag->shiftDR(NULL, rd_buf, 8 * (_xc95_line_len + 2));
			for (int pos = 0; pos < _xc95_line_len; pos++)
				buffer += rd_buf[pos];
			addr2 += ((subsection+1) % 0x05) ? 1 : 4;
		}
		progress.display(section);
	}
	progress.done();

	return buffer;
}

/*               */
/*   XCF Prom    */
/*               */

#define XCF_FVFY3          0xE2
#define XCF_ISCTESTSTATUS  0xE3
#define XCF_ISC_ENABLE     0xE8
#define XCF_ISC_PROGRAM    0xEA
#define XCF_ISC_ADDR_SHIFT 0xEB
#define XCF_ISC_ERASE      0xEC
#define XCF_ISC_DATA_SHIFT 0xED
#define XCF_ISC_READ       0xeF
#define XCF_ISC_DISABLE    0xF0

void Xilinx::xcf_flow_enable(uint8_t mode)
{
	_jtag->shiftIR(XCF_ISC_ENABLE, 8);
	_jtag->shiftDR(&mode, NULL, 6);
	_jtag->toggleClk(1);
}

void Xilinx::xcf_flow_disable()
{
	_jtag->shiftIR(XCF_ISC_DISABLE, 8);
	usleep(110000);
	_jtag->shiftIR(BYPASS, 8);
	_jtag->toggleClk(1);
}

bool Xilinx::xcf_flow_erase()
{
	uint8_t xfer_buf[2] = {0x01, 0x00};

	printInfo("Erase flash ", false);
	xcf_flow_enable();

	_jtag->shiftIR(XCF_ISC_ADDR_SHIFT, 8);
	_jtag->shiftDR(xfer_buf, NULL, 16);
	_jtag->toggleClk(1);

	_jtag->shiftIR(XCF_ISC_ERASE, 8);
	usleep(500000);

	int i;
	for (i = 0; i < 32; i++) {
		_jtag->shiftIR(XCF_ISCTESTSTATUS, 8);
		usleep(500000);
		_jtag->shiftDR(NULL, xfer_buf, 8);
		if ((xfer_buf[0] & 0x04))
			break;
	}

	if (i == 32) {
		printError("FAIL");
		return false;
	}

	printSuccess("DONE");

	xcf_flow_disable();

	return true;
}

bool Xilinx::xcf_program(ConfigBitstreamParser *bitfile)
{
	uint8_t tx_buf[4096 / 8];
	uint16_t pkt_len =
		((_jtag->get_target_device_id() == 0x05044093) ? 2048 : 4096) / 8;
	uint8_t *data = bitfile->getData();
	uint32_t data_len = bitfile->getLength() / 8;
	uint32_t xfer_len, offset = 0;
	uint32_t addr = 0;
	int xfer_end;

	/* limit JTAG clock frequency to 15MHz */
	if (_jtag->getClkFreq() > 15e6)
		_jtag->setClkFreq(15e6);

	if (!xcf_flow_erase()) {
		printError("flow erase failed");
		return false;
	}

	xcf_flow_enable();

	int blk_id = 0;

	ProgressBar progress("Write PROM", (data_len / pkt_len), 50, _quiet);

	while (data_len > 0) {
		if (data_len < pkt_len) {
			xfer_len = data_len;
			xfer_end = Jtag::SHIFT_DR;
		} else {
			xfer_len = pkt_len;
			xfer_end = Jtag::RUN_TEST_IDLE;
		}

		/* send data to PROM */
		_jtag->shiftIR(XCF_ISC_DATA_SHIFT, 8);
		_jtag->shiftDR(data+offset, NULL, xfer_len * 8, xfer_end);
		if (xfer_len != pkt_len) {
			uint32_t res = pkt_len - xfer_len;
			memset(tx_buf, 0xff, res);
			_jtag->shiftDR(tx_buf, NULL, res * 8);
		}

		_jtag->toggleClk(1);

		/* send address */
		tx_buf[0] = (addr >> 0) & 0x00ff;
		tx_buf[1] = (addr >> 8) & 0x00ff;
		_jtag->shiftIR(XCF_ISC_ADDR_SHIFT, 8);
		_jtag->shiftDR(tx_buf, NULL, 16);
		_jtag->toggleClk(1);

		/* send program instruction */
		_jtag->shiftIR(XCF_ISC_PROGRAM, 8);
		usleep((addr == 0) ? 14000: 500);

		/* wait until bit 3 != 1 */
		int i;
		for (i = 0; i < 29; i++) {
			_jtag->shiftIR(XCF_ISCTESTSTATUS, 8);
			usleep(500);
			_jtag->shiftDR(NULL, tx_buf, 8);
			if ((tx_buf[0] & 0x04))
				break;
		}

		if (i == 29) {
			progress.fail();
			return false;
		}

		blk_id++;
		offset += xfer_len;
		addr += 32;
		data_len -= xfer_len;
		progress.display(blk_id);
	}
	progress.done();

	/* program done */
	_jtag->shiftIR(BYPASS, 8);
	_jtag->toggleClk(1);

	if (_verify) {
		std::string flash = xcf_read();
		uint32_t file_size = bitfile->getLength() / 8;
		uint32_t prom_size = (uint32_t)flash.size();

		uint32_t nb_bytes = (file_size > prom_size) ? prom_size : file_size;
		ProgressBar progress2("Verify Flash", nb_bytes, 50, _quiet);

		for (uint32_t pos = 0; pos < nb_bytes; pos++) {
			if (data[pos] != (uint8_t)flash[pos]) {
				progress2.fail();
				char error[64];
				snprintf(error, sizeof(error),
						"Error: wrong value: read %02x instead of %02x",
						(uint8_t)flash[pos], (uint8_t)data[pos]);
				printError(error);
				xcf_flow_disable();
				return false;
			}
			progress.display(pos);
		}
		progress2.done();
	}

	_jtag->go_test_logic_reset();

	xcf_flow_disable();

	return true;
}

std::string Xilinx::xcf_read()
{
	uint32_t addr = 0;
	uint8_t rx_buf[4096 / 8];
	uint16_t pkt_len =
		((_jtag->get_target_device_id() == 0x05044093) ? 2048 : 4096) / 8;
	uint16_t nb_section =
		((_jtag->get_target_device_id() == 0x05046093) ? 1024 : 512);

	std::string buffer;

	/* limit JTAG clock frequency to 15MHz */
	if (_jtag->getClkFreq() > 15e6)
		_jtag->setClkFreq(15e6);

	ProgressBar progress("Read PROM", nb_section, 50, _quiet);

	for (size_t section = 0; section < nb_section; section++) {
		/* send address */
		rx_buf[0] = (addr >> 0) & 0x00ff;
		rx_buf[1] = (addr >> 8) & 0x00ff;
		_jtag->shiftIR(XCF_ISC_ADDR_SHIFT, 8);
		_jtag->shiftDR(rx_buf, NULL, 16);
		_jtag->toggleClk(1);

		/* send data to PROM */
		_jtag->shiftIR(XCF_ISC_READ, 8);
		usleep(50);
		_jtag->shiftDR(NULL, rx_buf, pkt_len * 8);

		for (int i = 0; i < pkt_len; i++)
			buffer += rx_buf[i];

		progress.display(section);
		addr += 32;
	}
	progress.done();

	return buffer;
}

/*               */
/* SPI interface */
/*               */

/*
 * jtag : jtag interface
 * cmd  : opcode for SPI flash
 * tx   : buffer to send
 * rx   : buffer to fill
 * len  : number of byte to send/receive (cmd not comprise)
 *        so to send only a cmd set len to 0 (or omit this param)
 */
int Xilinx::spi_put(uint8_t cmd,
			uint8_t *tx, uint8_t *rx, uint32_t len)
{
	int xfer_len = len + 1 + ((rx == NULL) ? 0 : 1);
	uint8_t jtx[xfer_len];
	jtx[0] = McsParser::reverseByte(cmd);
	/* uint8_t jtx[xfer_len] = {McsParser::reverseByte(cmd)}; */
	uint8_t jrx[xfer_len];
	if (tx != NULL) {
		for (uint32_t i=0; i < len; i++)
			jtx[i+1] = McsParser::reverseByte(tx[i]);
	}
	/* addr BSCAN user1 */
	_jtag->shiftIR(USER1, 6);
	/* send first already stored cmd,
	 * in the same time store each byte
	 * to next
	 */
	_jtag->shiftDR(jtx, (rx == NULL)? NULL: jrx, 8*xfer_len);

	if (rx != NULL) {
		for (uint32_t i=0; i < len; i++)
			rx[i] = McsParser::reverseByte(jrx[i+1] >> 1) | (jrx[i+2] & 0x01);
	}
	return 0;
}

int Xilinx::spi_put(uint8_t *tx, uint8_t *rx, uint32_t len)
{
	int xfer_len = len + ((rx == NULL) ? 0 : 1);
	uint8_t jtx[xfer_len];
	uint8_t jrx[xfer_len];
	if (tx != NULL) {
		for (uint32_t i=0; i < len; i++)
			jtx[i] = McsParser::reverseByte(tx[i]);
	}
	/* addr BSCAN user1 */
	_jtag->shiftIR(USER1, 6);
	/* send first already stored cmd,
	 * in the same time store each byte
	 * to next
	 */
	_jtag->shiftDR(jtx, (rx == NULL)? NULL: jrx, 8*xfer_len);

	if (rx != NULL) {
		for (uint32_t i=0; i < len; i++)
			rx[i] = McsParser::reverseByte(jrx[i] >> 1) | (jrx[i+1] & 0x01);
	}
	return 0;
}

int Xilinx::spi_wait(uint8_t cmd, uint8_t mask, uint8_t cond,
			uint32_t timeout, bool verbose)
{
	uint8_t rx[2];
	uint8_t dummy[2];
	uint8_t tmp;
	uint8_t tx = McsParser::reverseByte(cmd);
	uint32_t count = 0;

	_jtag->shiftIR(USER1, 6, Jtag::UPDATE_IR);
	_jtag->shiftDR(&tx, NULL, 8, Jtag::SHIFT_DR);

	do {
		_jtag->shiftDR(dummy, rx, 8*2, Jtag::SHIFT_DR);
		tmp = (McsParser::reverseByte(rx[0]>>1)) | (0x01 & rx[1]);
		count++;
		if (count == timeout){
			printf("timeout: %x %x %x\n", tmp, rx[0], rx[1]);
			break;
		}
		if (verbose) {
			printf("%x %x %x %u\n", tmp, mask, cond, count);
		}
	} while ((tmp & mask) != cond);
	_jtag->shiftDR(dummy, rx, 8*2, Jtag::EXIT1_DR);
	_jtag->go_test_logic_reset();

	if (count == timeout) {
		printf("%x\n", tmp);
		std::cout << "wait: Error" << std::endl;
		return -ETIME;
	} else {
		return 0;
	}
}
