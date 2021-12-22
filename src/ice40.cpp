// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (C) 2020 Gwenhael Goavec-Merou <gwenhael.goavec-merou@trabucayre.com>
 */

#include "ice40.hpp"

#include <string.h>
#include <unistd.h>

#include <iostream>
#include <string>

#include "display.hpp"
#include "ftdispi.hpp"
#include "device.hpp"
#include "progressBar.hpp"
#include "rawParser.hpp"
#include "spiFlash.hpp"

Ice40::Ice40(FtdiSpi* spi, const std::string &filename,
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

Ice40::~Ice40()
{}

void Ice40::reset()
{
	uint32_t timeout = 1000;
	_spi->gpio_clear(_rst_pin);
	usleep(1000);
	_spi->gpio_set(_rst_pin);
	printInfo("Reset ", false);
	usleep(12000);
	do {
		timeout--;
		usleep(12000);
	} while (((_spi->gpio_get(true) & _done_pin) == 0) && timeout > 0);
	if (timeout == 0)
		printError("FAIL");
	else
		printSuccess("DONE");
}

void Ice40::program(unsigned int offset, bool unprotect_flash)
{
	uint32_t timeout = 1000;

	if (_file_extension.empty())
		return;

	RawParser bit(_filename, false);

	printInfo("Parse file ", false);
	if (bit.parse() == EXIT_SUCCESS) {
		printSuccess("DONE");
	} else {
		printError("FAIL");
		return;
	}

	uint8_t *data = bit.getData();
	int length = bit.getLength() / 8;

	_spi->gpio_clear(_rst_pin);

	SPIFlash flash(reinterpret_cast<SPIInterface *>(_spi), unprotect_flash,
			_quiet);
	flash.reset();
	flash.power_up();

	printf("%02x\n", flash.read_status_reg());
	flash.read_id();
	flash.erase_and_prog(offset, data, length);

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

bool Ice40::dumpFlash(uint32_t base_addr, uint32_t len)
{
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

bool Ice40::protect_flash(uint32_t len)
{
	/* SPI access */
	prepare_flash_access();
	/* acess */
	try {
		SPIFlash flash(reinterpret_cast<SPIInterface *>(_spi), false, _verbose);
		/* configure flash protection */
		if (flash.enable_protection(len) == -1)
			return false;
	} catch (std::exception &e) {
		printError("Fail");
		printError(std::string(e.what()));
		return false;
	}

	/* reload */
	return post_flash_access();
}

bool Ice40::unprotect_flash()
{
	/* SPI access */
	prepare_flash_access();
	/* acess */
	try {
		SPIFlash flash(reinterpret_cast<SPIInterface *>(_spi), false, _verbose);
		/* configure flash protection */
		if (flash.disable_protection() == -1)
			return false;
	} catch (std::exception &e) {
		printError("Fail");
		printError(std::string(e.what()));
		return false;
	}

	/* reload */
	return post_flash_access();
}

bool Ice40::prepare_flash_access()
{
	/* SPI access: shutdown ICE40 */
	_spi->gpio_clear(_rst_pin);
	usleep(1000);
	return true;
}

bool Ice40::post_flash_access()
{
	reset();
	return ((_spi->gpio_get(true) & _done_pin) == 0) ? false : true;
}
