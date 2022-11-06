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
			Device::prog_type_t prg_type,
			uint16_t rst_pin, uint16_t done_pin,
			bool verify, int8_t verbose):
	Device(NULL, filename, file_type, verify, verbose), _rst_pin(rst_pin),
		_done_pin(done_pin)
{
	_spi = spi;
	_spi->gpio_set_input(_done_pin);
	_spi->gpio_set_output(_rst_pin);

	if (prg_type == Device::WR_FLASH)
		_mode = Device::SPI_MODE;
	else
		_mode = Device::MEM_MODE;
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

/* cf. TN1248 (iCE40 Programming and Configuration)
 * Appendix A. SPI Slave Configuration Procedure
 */
bool Ice40::program_cram(uint8_t *data, uint32_t length)
{
	uint32_t timeout = 1000;

	/* configure SPI */
	_spi->setMode(3); // IDLE high, write on falling
	_spi->setCSmode(FtdiSpi::SPI_CS_MANUAL);

	/* reset device */
	_spi->clearCs();
	_spi->gpio_clear(_rst_pin);
	usleep(100); // 200 ns ...
	_spi->gpio_set(_rst_pin);
	usleep(2000); // 800 -> 1200 us + guard

	/* load configuration data MSB first
	 */
	ProgressBar progress("Loading to CRAM", length, 50, _verbose);
	uint8_t *ptr = data;
	int size = 0;
	for (uint32_t addr = 0; addr < length; addr += size, ptr+=size) {
		size = (addr + 256 > length)?(length-addr) : 256;
		if (_spi->spi_put(ptr, NULL, size) == -1)
			return -1;
		progress.display(addr);
	}
	progress.done();

	/* send 48 to 100 dummy bits */
	uint8_t dummy[12];
	_spi->spi_put(dummy, NULL, 12);

	/* wait CDONE */
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

	_spi->setCs();

	return true;
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

	if (_mode == Device::MEM_MODE) {
		program_cram(data, length);
		return;
	}

	_spi->gpio_clear(_rst_pin);

	SPIFlash flash(reinterpret_cast<SPIInterface *>(_spi), unprotect_flash,
			_quiet);

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

bool Ice40::bulk_erase_flash()
{
	/* SPI access */
	prepare_flash_access();
	/* acess */
	try {
		SPIFlash flash(reinterpret_cast<SPIInterface *>(_spi), false, _verbose);
		/* bulk erase flash */
		if (flash.bulk_erase() == -1)
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
