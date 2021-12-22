// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (C) 2020 Gwenhael Goavec-Merou <gwenhael.goavec-merou@trabucayre.com>
 */

#include "efinix.hpp"

#include <string.h>
#include <unistd.h>

#include <iostream>
#include <string>

#include "display.hpp"
#include "efinixHexParser.hpp"
#include "ftdispi.hpp"
#include "device.hpp"
#include "ftdiJtagMPSSE.hpp"
#include "jtag.hpp"
#include "progressBar.hpp"
#include "rawParser.hpp"
#include "spiFlash.hpp"

Efinix::Efinix(FtdiSpi* spi, const std::string &filename,
			const std::string &file_type,
			uint16_t rst_pin, uint16_t done_pin,
			uint16_t oe_pin,
			bool verify, int8_t verbose):
	Device(NULL, filename, file_type, verify, verbose), _ftdi_jtag(NULL),
		_rst_pin(rst_pin), _done_pin(done_pin), _cs_pin(0), _oe_pin(oe_pin)
{
	_spi = spi;
	_spi->gpio_set_input(_done_pin);
	_spi->gpio_set_output(_rst_pin | _oe_pin);
}

Efinix::Efinix(Jtag* jtag, const std::string &filename,
			const std::string &file_type,
			const std::string &board_name,
			bool verify, int8_t verbose):
	Device(jtag, filename, file_type, verify, verbose),
	_spi(NULL), _rst_pin(0), _done_pin(0), _cs_pin(0),
	_oe_pin(0)
{
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
		throw std::runtime_error("Error: unknown board name");
	}

	/* 2: retrieve spi board */
	target_board_t *spi_board = &(board_list[spi_board_name]);

	/* 3: SPI cable */
	cable_t *spi_cable = &(cable_list[spi_board->cable_name]);

	/* 4: get pinout (cs, oe, rst) */
	_cs_pin = spi_board->spi_pins_config.cs_pin;
	_rst_pin = spi_board->reset_pin;
	_oe_pin = spi_board->oe_pin;

	/* 5: open SPI interface */
	_spi = new FtdiSpi(spi_cable->config, spi_board->spi_pins_config,
			jtag->getClkFreq(), verbose > 0);
	_ftdi_jtag = reinterpret_cast<FtdiJtagMPSSE *>(jtag);

	/* 6: configure pins direction and default state */
	_spi->gpio_set_output(_oe_pin | _rst_pin | _cs_pin);
}

Efinix::~Efinix()
{}

void Efinix::reset()
{
	if (_ftdi_jtag)  // not supported
		return;
	uint32_t timeout = 1000;
	_spi->gpio_clear(_rst_pin | _oe_pin);
	usleep(1000);
	_spi->gpio_set(_rst_pin | _oe_pin);
	printInfo("Reset ", false);
	do {
		timeout--;
		usleep(12000);
	} while (((_spi->gpio_get(true) & _done_pin) == 0) || timeout > 0);
	if (timeout == 0)
		printError("FAIL");
	else
		printSuccess("DONE");
}

void Efinix::program(unsigned int offset, bool unprotect_flash)
{
	if (_file_extension.empty())
		return;

	ConfigBitstreamParser *bit;
	try {
		if (_file_extension == "hex") {
			bit = new EfinixHexParser(_filename, _verbose);
		} else {
			if (offset == 0) {
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

	unsigned char *data = bit->getData();
	int length = bit->getLength() / 8;

	if (_verbose)
		bit->displayHeader();

	if (_ftdi_jtag)
		programJTAG(data, length);
	else
		programSPI(offset, data, length, unprotect_flash);
}

bool Efinix::dumpFlash(const std::string &filename,
		uint32_t base_addr, uint32_t len)
{
	uint32_t timeout = 1000;
	_spi->gpio_clear(_rst_pin);

	/* prepare SPI access */
	printInfo("Read Flash ", false);
	try {
		SPIFlash flash(reinterpret_cast<SPIInterface *>(_spi), false, _verbose);
		flash.reset();
		flash.power_up();
		flash.dump(filename, base_addr, len);
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

void Efinix::programSPI(unsigned int offset, uint8_t *data, int length,
		bool unprotect_flash)
{
	uint32_t timeout = 1000;

	_spi->gpio_clear(_rst_pin | _oe_pin);

	SPIFlash flash(reinterpret_cast<SPIInterface *>(_spi), unprotect_flash,
			_verbose);
	flash.reset();
	flash.power_up();

	printf("%02x\n", flash.read_status_reg());
	flash.read_id();
	flash.erase_and_prog(offset, data, length);

	/* verify write if required */
	if (_verify)
		flash.verify(offset, data, length);

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
}

#define SAMPLE_PRELOAD 0x02
#define EXTEST         0x00
#define BYPASS         0x0f
#define IDCODE         0x03
#define PROGRAM        0x04
#define ENTERUSER      0x07
#define IRLENGTH       4

void Efinix::programJTAG(uint8_t *data, int length)
{
	int xfer_len = 512, tx_end;
	uint8_t tx[512];

	/* trion has to be reseted with cs low */
	_spi->gpio_clear(_oe_pin | _cs_pin | _rst_pin);
	usleep(30000);
	_spi->gpio_set(_rst_pin);  // assert RST
	usleep(50000);
	_spi->gpio_set(_oe_pin | _rst_pin);  // release OE
	usleep(50000);

	/* force run_test_idle state */
	_jtag->set_state(Jtag::RUN_TEST_IDLE);
	usleep(100000);

	/* send PROGRAM state and stay in SHIFT_DR until
	 * full configuration data has been sent
	 */
	_jtag->shiftIR(PROGRAM, IRLENGTH, Jtag::EXIT1_IR);
	_jtag->shiftIR(PROGRAM, IRLENGTH, Jtag::EXIT1_IR);  // T20 fix

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

	_jtag->shiftIR(ENTERUSER, IRLENGTH, Jtag::EXIT1_IR);

	memset(tx, 0, 512);
	_jtag->shiftDR(tx, NULL, 100);
	_jtag->shiftIR(IDCODE, IRLENGTH);
}
