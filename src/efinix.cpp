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
#include "progressBar.hpp"
#include "rawParser.hpp"
#include "spiFlash.hpp"

Efinix::Efinix(FtdiSpi* spi, const std::string &filename,
			const std::string &file_type,
			uint16_t rst_pin, uint16_t done_pin,
			bool verify, int8_t verbose):
	Device(NULL, filename, file_type, verify, verbose), _rst_pin(rst_pin),
		_done_pin(done_pin)
{
	_spi = spi;
	_spi->gpio_set_input(_done_pin);
	_spi->gpio_set_output(_rst_pin);
}

Efinix::~Efinix()
{}

void Efinix::reset()
{
	uint32_t timeout = 1000;
	_spi->gpio_clear(_rst_pin);
	usleep(1000);
	_spi->gpio_set(_rst_pin);
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

void Efinix::program(unsigned int offset)
{
	uint32_t timeout = 1000;

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

	_spi->gpio_clear(_rst_pin);

	SPIFlash flash(reinterpret_cast<SPIInterface *>(_spi), _verbose);
	flash.reset();
	flash.power_up();

	printf("%02x\n", flash.read_status_reg());
	flash.read_id();
	flash.erase_and_prog(offset, data, length);

	/* verify write if required */
	if (_verify)
		flash.verify(offset, data, length);

	_spi->gpio_set(_rst_pin);
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

bool Efinix::dumpFlash(const std::string &filename,
		uint32_t base_addr, uint32_t len)
{
	uint32_t timeout = 1000;
	_spi->gpio_clear(_rst_pin);

	/* prepare SPI access */
	printInfo("Read Flash ", false);
	try {
		SPIFlash flash(reinterpret_cast<SPIInterface *>(_spi), _verbose);
		flash.reset();
		flash.power_up();
		flash.dump(filename, base_addr, len);
	} catch (std::exception &e) {
		printError("Fail");
		printError(std::string(e.what()));
		return false;
	}

	/* release SPI access */
	_spi->gpio_set(_rst_pin);
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
