/*
 * Copyright (C) 2020 Gwenhael Goavec-Merou <gwenhael.goavec-merou@trabucayre.com>
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
	do {
		timeout--;
		usleep(12000);
	} while (((_spi->gpio_get(true) & _done_pin) == 0) || timeout > 0);
	if (timeout == 0)
		printError("FAIL");
	else
		printSuccess("DONE");
}

void Ice40::program(unsigned int offset)
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

	SPIFlash flash(reinterpret_cast<SPIInterface *>(_spi), _quiet);
	flash.reset();
	flash.power_up();

	printf("%02x\n", flash.read_status_reg());
	flash.read_id();
	flash.erase_and_prog(offset, data, length);

	if (_verify) {
		printInfo("Verifying write");
		std::string verify_data;
		verify_data.resize(length);
		printInfo("Read flash ", false);
		if (0 != flash.read(offset, (uint8_t*)&verify_data[0], length)) {
			printError("FAIL");
			return;
		} else {
			printSuccess("DONE");
		}

		ProgressBar progress("Check", length, 50, _quiet);
		for (int i = 0; i < length; i++) {
			if ((uint8_t)verify_data[i] != data[i]) {
				progress.fail();
				printError("Verification failed at " +
						std::to_string(offset + i));
				return;
			}
			progress.display(i);
		}
		progress.done();
	}

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

bool Ice40::dumpFlash(const std::string &filename,
		uint32_t base_addr, uint32_t len)
{
	uint32_t timeout = 1000;
	_spi->gpio_clear(_rst_pin);

	std::string data;
	data.resize(len);

	/* prepare SPI access */
	printInfo("Read Flash ", false);
	try {
		SPIFlash flash(reinterpret_cast<SPIInterface *>(_spi), _verbose);
		flash.reset();
		flash.power_up();
		flash.read_id();
		flash.read_status_reg();
		flash.read(base_addr, (uint8_t*)&data[0], len);
	} catch (std::exception &e) {
		printError("Fail");
		printError(std::string(e.what()));
		return false;
	}

	FILE *fd = fopen(filename.c_str(), "wb");
	if (!fd) {
		printError("Fail");
		return false;
	}

	fwrite(data.c_str(), sizeof(uint8_t), len, fd);
	fclose(fd);

	printSuccess("Done");

	/* prepare SPI access */

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
