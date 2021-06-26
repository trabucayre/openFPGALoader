// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (C) 2019 Gwenhael Goavec-Merou <gwenhael.goavec-merou@trabucayre.com>
 */

#include <iostream>
#include <stdexcept>
#include <string>

#include "jtag.hpp"
#include "bitparser.hpp"
#include "configBitstreamParser.hpp"
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
	std::string device_package, bool verify, int8_t verbose):
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
		} else {
			_mode = Device::SPI_MODE;
		}
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
#define ISC_DISABLE 0x16
#define BYPASS   0x3f

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
	unsigned char tx_data[4]= {0x00, 0x00, 0x00, 0x00};
	unsigned char rx_data[4];
	_jtag->go_test_logic_reset();
	_jtag->shiftIR(IDCODE, 6);
	_jtag->shiftDR(tx_data, rx_data, 32);
	return ((rx_data[0] & 0x000000ff) |
		((rx_data[1] << 8) & 0x0000ff00) |
		((rx_data[2] << 16) & 0x00ff0000) |
		((rx_data[3] << 24) & 0xff000000));
}

void Xilinx::program(unsigned int offset)
{
	ConfigBitstreamParser *bit;
	bool reverse = false;

	/* nothing to do */
	if (_mode == Device::NONE_MODE || _mode == Device::READ_MODE)
		return;

	if (_mode == Device::MEM_MODE)
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
	if (_verify) {
		std::string verify_data;
		verify_data.resize(256);

		ProgressBar progress("Verifying write", length, 50, _quiet);
		int rd_length = 256;
		for (int i = 0; i < length; i+=rd_length) {
			if (rd_length + i > length)
				rd_length = length - i;
			if (0 != spiFlash.read(offset + i, (uint8_t*)&verify_data[0],
						rd_length)) {
				progress.fail();
				printError("Failed to read flash");
				return;
			}

			for (int ii = 0; ii < rd_length; ii++) {
				if ((uint8_t)verify_data[ii] != data[i + ii]) {
					progress.fail();
					printError("Verification failed at " +
							std::to_string(offset + i + ii));
					return;
				}
			}
			progress.display(i);
		}
		progress.done();
	}
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
	/* first need to have bridge in RAM */
	if (load_bridge() == false)
		return false;

	/* prepare SPI access */
	SPIFlash flash(this, _verbose);
	flash.reset();
	flash.read_id();
	flash.read_status_reg();

	FILE *fd = fopen(filename.c_str(), "wb");
	if (!fd) {
		printError("Open dump file failed\n");
		return false;
	}

	uint32_t rd_length = 256;
	std::string data;
	data.resize(rd_length);

	ProgressBar progress("Dump flash", len, 50, _quiet);

	for (uint32_t i = 0; i < len; i+=rd_length) {
		if (rd_length + i > len)
			rd_length = len - i;
		if (0 != flash.read(base_addr + i, (uint8_t*)&data[0],
					rd_length)) {
			progress.fail();
			printError("Failed to read flash");
			return false;
		}

		fwrite(data.c_str(), sizeof(uint8_t), rd_length, fd);

		progress.display(i);
	}
	progress.done();
	fclose(fd);

	/* reset device */
	reset();

	return false;
}

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
