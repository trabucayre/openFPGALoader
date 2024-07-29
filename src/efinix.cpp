// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (C) 2020 Gwenhael Goavec-Merou <gwenhael.goavec-merou@trabucayre.com>
 */

#include "efinix.hpp"

#include <string.h>
#include <unistd.h>

#include <iostream>
#include <stdexcept>
#include <string>

#include "common.hpp"
#include "device.hpp"
#include "display.hpp"
#include "efinixHexParser.hpp"
#include "ftdiJtagMPSSE.hpp"
#include "ftdispi.hpp"
#include "jtag.hpp"
#include "part.hpp"
#include "progressBar.hpp"
#include "rawParser.hpp"
#if defined (_WIN64) || defined (_WIN32)
#include "pathHelper.hpp"
#endif
#include "spiFlash.hpp"

Efinix::Efinix(FtdiSpi* spi, const std::string &filename,
			const std::string &file_type,
			uint16_t rst_pin, uint16_t done_pin,
			uint16_t oe_pin,
			bool verify, int8_t verbose):
	Device(NULL, filename, file_type, verify, verbose), _ftdi_jtag(NULL),
		_rst_pin(rst_pin), _done_pin(done_pin), _cs_pin(0), _oe_pin(oe_pin),
		_fpga_family(UNKNOWN_FAMILY), _irlen(0), _device_package(""),
		_spiOverJtagPath("")
{
	_spi = spi;
	init_common(Device::WR_FLASH);
}

Efinix::Efinix(Jtag* jtag, const std::string &filename,
			const std::string &file_type, Device::prog_type_t prg_type,
			const std::string &board_name, const std::string &device_package,
			const std::string &spiOverJtagPath, bool verify, int8_t verbose):
	Device(jtag, filename, file_type, verify, verbose),
	SPIInterface(filename, verbose, 256, false, false, false),
	_spi(NULL), _rst_pin(0), _done_pin(0), _cs_pin(0),
	_oe_pin(0), _fpga_family(UNKNOWN_FAMILY), _irlen(0),
	_device_package(device_package), _spiOverJtagPath(spiOverJtagPath)
{
	_ftdi_jtag = reinterpret_cast<FtdiJtagMPSSE *>(jtag->get_ll_class());

	/* detect FPGA type (Trion or Titanium) */

	const uint32_t idcode = _jtag->get_target_device_id();
	const std::string family = fpga_list[idcode].family;
	if (family == "Titanium") {
		if (_file_extension == "hex" && prg_type == Device::WR_SRAM) {
			throw std::runtime_error("Error: loading (RAM) hex file is not "
							   "allowed for Titanium devices");
		} else if (_file_extension == "bit" && prg_type == Device::WR_FLASH) {
			throw std::runtime_error("Error: writing bit (FLASH) file is not "
							   "allowed for Titanium devices");
		}
		_fpga_family = TITANIUM_FAMILY;
	} else if (family == "Trion") {
		_fpga_family = TRION_FAMILY;
	} else {
		throw std::runtime_error("Error: unknown family " + family);
	}
	/* get irlen value from model */
	_irlen = fpga_list[idcode].irlength;

	/* WA: before using JTAG, device must restart with cs low
	 *     but cs and rst for xyloni are connected to interfaceA (ie SPI)
	 *     TODO: some boards have cs, reset and done in both interface
	 */

	/* 1: need to find SPI board definition based on JTAG board def */
	std::string spi_board_name = "";
	if (board_name == "xyloni_jtag") {
		spi_board_name = "xyloni_spi";
	} else if (board_name == "trion_t120_bga576_jtag") {
		spi_board_name = "trion_t120_bga576";
	} else if (board_name == "titanium_ti60_f225_jtag") {
		spi_board_name = "titanium_ti60_f225";
	} else {
		init_common(prg_type);
		return;
	}

	/* 2: retrieve spi board */
	const target_board_t *spi_board = &(board_list[spi_board_name]);

	/* 3: SPI cable */
	cable_t spi_cable = (cable_list[spi_board->cable_name]);
	spi_cable.bus_addr    = _ftdi_jtag->bus_addr();
	spi_cable.device_addr = _ftdi_jtag->device_addr();

	/* 4: get pinout (cs, oe, rst) */
	_cs_pin = spi_board->spi_pins_config.cs_pin;
	_rst_pin = spi_board->reset_pin;
	_oe_pin = spi_board->oe_pin;
	_done_pin = spi_board->done_pin;

	/* 5: open SPI interface */
	_spi = new FtdiSpi(spi_cable, spi_board->spi_pins_config,
			jtag->getClkFreq(), verbose);

	/* 6: configure pins direction and default state */
	init_common(prg_type);
}

void Efinix::init_common(const Device::prog_type_t &prg_type)
{
	if (_spi) {
		_spi->gpio_set_input(_done_pin);
		_spi->gpio_set_output(_rst_pin | _oe_pin);
	}

	switch (prg_type) {
		case Device::WR_FLASH:
			_mode = (_jtag) ? Device::FLASH_MODE : Device::SPI_MODE;
			break;
		case Device::WR_SRAM:
			if (!_jtag) {
				throw std::runtime_error("Efinix: SRAM load requires jtag");
			}
			_mode = MEM_MODE;
			break;
		default:
			_mode = NONE_MODE;
	}
}

Efinix::~Efinix()
{
	if (_jtag && _spi)
		delete _spi;
}

void Efinix::reset()
{
	if (!_spi) {
		printError("jtag: reset not supported");
		return;
	}
	uint32_t timeout = 1000;
	_spi->gpio_clear(_rst_pin | _oe_pin);
	usleep(1000);
	_spi->gpio_set(_rst_pin | _oe_pin);

	printInfo("Reset ", false);
	do {
		timeout--;
		usleep(12000);
	} while (((_spi->gpio_get(true) & _done_pin) == 0) && timeout > 0);
	if (timeout == 0)
		printError("FAIL");
	else
		printSuccess("DONE");
}

void Efinix::program(unsigned int offset, bool unprotect_flash)
{
	if (_file_extension.empty())
		return;
	if (_mode == Device::NONE_MODE)
		return;

	ConfigBitstreamParser *bit;
	try {
		if (_file_extension == "hex" || _file_extension == "bit") {
			bit = new EfinixHexParser(_filename);
		} else {
			if (offset == 0 && _spi) {
				printError("Error: can't write raw data at the beginning of the flash");
				throw std::exception();
			}
			bit = new RawParser(_filename, false);
		}
	} catch (std::exception &e) {
		printError("FAIL: " + std::string(e.what()));
		return;
	}

	printInfo("Parse file ", false);
	if (bit->parse() == EXIT_SUCCESS) {
		printSuccess("DONE");
	} else {
		printError("FAIL");
		delete bit;
		return;
	}

	const uint8_t *data = bit->getData();
	const int length = bit->getLength() / 8;

	if (_verbose)
		bit->displayHeader();

	switch (_mode) {
		case MEM_MODE:
			programJTAG(data, length);
			break;
		case FLASH_MODE:
			if (_jtag)
				SPIInterface::write(offset, const_cast<uint8_t *>(data),
					length, unprotect_flash);
			else
				programSPI(offset, data, length, unprotect_flash);
			break;
		default:
			return;
	}

	delete bit;
}

bool Efinix::dumpFlash(uint32_t base_addr, uint32_t len)
{
	if (!_spi) {
		printError("jtag: dumpFlash not supported");
		return false;
	}

	uint32_t timeout = 1000;
	_spi->gpio_clear(_rst_pin);

	/* prepare SPI access */
	printInfo("Read Flash ", false);
	try {
		SPIFlash flash(reinterpret_cast<SPIInterface *>(_spi), false, _verbose);
		flash.reset();
		flash.power_up();
		flash.dump(_filename, base_addr, len);
	} catch (std::exception &e) {
		printError("Fail");
		printError(std::string(e.what()));
		return false;
	}

	/* release SPI access */
	_spi->gpio_set(_rst_pin | _oe_pin);
	usleep(12000);

	printInfo("Wait for CDONE ", false);
	do {
		timeout--;
		usleep(12000);
	} while (((_spi->gpio_get(true) & _done_pin) == 0) && timeout > 0);
	if (timeout == 0)
		printError("FAIL");
	else
		printSuccess("DONE");

	return false;
}

bool Efinix::programSPI(unsigned int offset, const uint8_t *data,
		const int length, const bool unprotect_flash)
{
	bool ret = true;
	_spi->gpio_clear(_rst_pin | _oe_pin);

	SPIFlash flash(reinterpret_cast<SPIInterface *>(_spi), unprotect_flash,
			_verbose);
	flash.reset();
	flash.power_up();

	printf("%02x\n", flash.read_status_reg());
	flash.read_id();
	if (0 != flash.erase_and_prog(offset, const_cast<uint8_t *>(data), length))
		ret = false;

	/* verify write if required */
	if (_verify)
		ret = flash.verify(offset, data, length);

	reset();
	return ret;
}

#define SAMPLE_PRELOAD 0x02
#define EXTEST         0x00
#define BYPASS         0x0f
#define IDCODE         0x03
#define PROGRAM        0x04
#define ENTERUSER      0x07
#define USER1          0x08

bool Efinix::programJTAG(const uint8_t *data, const int length)
{
	int xfer_len = 512;
	Jtag::tapState_t tx_end;
	uint8_t tx[512];

	if (_fpga_family == TITANIUM_FAMILY)
		_jtag->set_state(Jtag::RUN_TEST_IDLE);

	if(_spi) {
		/* trion has to be reseted with cs low */
		_spi->gpio_clear(_oe_pin | _cs_pin | _rst_pin);
		usleep(30000);
		_spi->gpio_set(_rst_pin);  // assert RST
		usleep(50000);
		_spi->gpio_set(_oe_pin | _rst_pin);  // release OE
		usleep(50000);
	}

	if (_fpga_family == TITANIUM_FAMILY)
		_jtag->set_state(Jtag::TEST_LOGIC_RESET);
	/* force run_test_idle state */
	_jtag->set_state(Jtag::RUN_TEST_IDLE);
	usleep(100000);

	/* send PROGRAM state and stay in SHIFT_DR until
	 * full configuration data has been sent
	 */
	_jtag->shiftIR(PROGRAM, _irlen, Jtag::EXIT1_IR);
	_jtag->shiftIR(PROGRAM, _irlen, Jtag::EXIT1_IR);  // T20 fix

	ProgressBar progress("Load SRAM", length, 50, _quiet);

	for (int i = 0; i < length; i+=xfer_len) {
		if (i + xfer_len > length) {  // last packet
			xfer_len = (length - i);
			tx_end = Jtag::EXIT1_DR;
		} else {
			tx_end = Jtag::SHIFT_DR;
		}
		for (int pos = 0; pos < xfer_len; pos++)
			tx[pos] = EfinixHexParser::reverseByte(data[i+pos]);

		_jtag->shiftDR(tx, NULL, xfer_len*8, tx_end);
		progress.display(i);
	}

	progress.done();

	usleep(10000);

	_jtag->shiftIR(ENTERUSER, _irlen, Jtag::EXIT1_IR);

	memset(tx, 0, 512);
	_jtag->shiftDR(tx, NULL, 100);
	_jtag->shiftIR(IDCODE, _irlen);
	uint8_t idc[4];
	_jtag->shiftDR(NULL, idc, 4);
	printf("%02x%02x%02x%02x\n",
			idc[0], idc[1], idc[2], idc[3]);
	return true;
}

bool Efinix::post_flash_access()
{
	if (_skip_reset)
		printInfo("Skip resetting device");
	else
		reset();
	return true;
}

bool Efinix::prepare_flash_access()
{
	if (_skip_load_bridge) {
		printInfo("Skip loading bridge for spiOverjtag");
		return true;
	}

	std::string bitname;
	if (!_spiOverJtagPath.empty()) {
		bitname = _spiOverJtagPath;
	} else {
		if (_device_package.empty()) {
			printError("Can't program SPI flash: missing device-package information");
			return false;
		}

		bitname = get_shell_env_var("OPENFPGALOADER_SOJ_DIR",
			DATA_DIR "/openFPGALoader");
		bitname += "/spiOverJtag_efinix_" + _device_package + ".bit.gz";
	}

#if defined (_WIN64) || defined (_WIN32)
	/* Convert relative path embedded at compile time to an absolute path */
	bitname = PathHelper::absolutePath(bitname);
#endif

	std::cout << "use: " << bitname << std::endl;

	/* first: load spi over jtag */
	try {
		EfinixHexParser bridge(bitname);
		bridge.parse();
		const uint8_t *data = bridge.getData();
		const int length = bridge.getLength() / 8;
		programJTAG(data, length);
	} catch (std::exception &e) {
		printError(e.what());
		return false;
	}

	return true;
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
int Efinix::spi_put(uint8_t cmd,
			const uint8_t *tx, uint8_t *rx, uint32_t len)
{
	int kXferLen = len + 1 + ((rx == NULL) ? 0 : 1);
	uint8_t jtx[kXferLen];
	jtx[0] = EfinixHexParser::reverseByte(cmd);
	uint8_t jrx[kXferLen];
	if (tx != NULL) {
		for (uint32_t i=0; i < len; i++)
			jtx[i+1] = EfinixHexParser::reverseByte(tx[i]);
	}
	/* addr BSCAN user1 */
	_jtag->shiftIR(USER1, _irlen);
	/* send first already stored cmd,
	 * in the same time store each byte
	 * to next
	 */
	_jtag->shiftDR(jtx, (rx == NULL)? NULL: jrx, 8*kXferLen);

	if (rx != NULL) {
		for (uint32_t i=0; i < len; i++)
			rx[i] = EfinixHexParser::reverseByte(jrx[i+1] >> 1) | (jrx[i+2] & 0x01);
	}
	return 0;
}

int Efinix::spi_put(const uint8_t *tx, uint8_t *rx, uint32_t len)
{
	int kXferLen = len + ((rx == NULL) ? 0 : 1);
	uint8_t jtx[kXferLen];
	uint8_t jrx[kXferLen];
	if (tx != NULL) {
		for (uint32_t i=0; i < len; i++)
			jtx[i] = EfinixHexParser::reverseByte(tx[i]);
	}
	/* addr BSCAN user1 */
	_jtag->shiftIR(USER1, _irlen);
	/* send first already stored cmd,
	 * in the same time store each byte
	 * to next
	 */
	_jtag->shiftDR(jtx, (rx == NULL)? NULL: jrx, 8*kXferLen);

	if (rx != NULL) {
		for (uint32_t i=0; i < len; i++)
			rx[i] = EfinixHexParser::reverseByte(jrx[i] >> 1) | (jrx[i+1] & 0x01);
	}
	return 0;
}

int Efinix::spi_wait(uint8_t cmd, uint8_t mask, uint8_t cond,
			uint32_t timeout, bool verbose)
{
	uint8_t rx[2], dummy[2], tmp;
	memset(dummy, 0xff, sizeof(dummy));
	uint8_t tx = EfinixHexParser::reverseByte(cmd);
	uint32_t count = 0;

	_jtag->shiftIR(USER1, _irlen, Jtag::UPDATE_IR);
	_jtag->shiftDR(&tx, NULL, 8, Jtag::SHIFT_DR);

	do {
		_jtag->shiftDR(dummy, rx, 8*2, Jtag::SHIFT_DR);
		tmp = (EfinixHexParser::reverseByte(rx[0] >> 1)) | (0x01 & rx[1]);
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
	}
	return 0;
}
