// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (C) 2021 Gwenhael Goavec-Merou <gwenhael.goavec-merou@trabucayre.com>
 */

#include <iostream>
#include <vector>

#include "display.hpp"
#include "spiInterface.hpp"
#include "spiFlash.hpp"

SPIInterface::SPIInterface():_spif_verbose(0), _spif_rd_burst(0),
	_spif_verify(false)
{}

SPIInterface::SPIInterface(const std::string &filename, uint8_t verbose,
		uint32_t rd_burst, bool verify):
	_spif_verbose(verbose), _spif_rd_burst(rd_burst),
	_spif_verify(verify), _spif_filename(filename)
{}

/* spiFlash generic acces */
bool SPIInterface::protect_flash(uint32_t len)
{
	bool ret = true;
	printInfo("protect_flash: ", false);

	/* move device to spi access */
	if (!prepare_flash_access()) {
		printError("Fail");
		return false;
	}

	/* spi flash access */
	try {
		SPIFlash flash(this, false, _spif_verbose);

		/* configure flash protection */
		ret = flash.enable_protection(len) != 0;
		if (ret != 0)
			printError("Fail");
		else
			printSuccess("Done");
	} catch (std::exception &e) {
		printError("Fail");
		printError(e.what());
		ret = false;
	}

	/* reload bitstream */
	return post_flash_access() && ret;
}

bool SPIInterface::unprotect_flash()
{
	bool ret = true;
	printInfo("unprotect_flash: ", false);

	/* move device to spi access */
	if (!prepare_flash_access()) {
		printError("Fail");
		return false;
	}

	/* spi flash access */
	try {
		SPIFlash flash(this, false, _spif_verbose);

		/* configure flash protection */
		ret = flash.disable_protection() != 0;
		if (!ret)
			printError("Fail " + std::to_string(ret));
		else
			printSuccess("Done");
	} catch (std::exception &e) {
		printError("Fail");
		printError(e.what());
		ret = false;
	}

	/* reload bitstream */
	return post_flash_access() && ret;
}

bool SPIInterface::write(uint32_t offset, uint8_t *data, uint32_t len,
		bool unprotect_flash)
{
	bool ret = true;
	if (!prepare_flash_access())
		return false;

	/* test SPI */
	try {
		SPIFlash flash(this, unprotect_flash, _spif_verbose);
		flash.read_status_reg();
		if (flash.erase_and_prog(offset, data, len) == -1)
			ret = false;
		if (_spif_verify && ret)
			ret = flash.verify(offset, data, len, _spif_rd_burst);
	} catch (std::exception &e) {
		printError(e.what());
		ret = false;
	}

	bool ret2 = post_flash_access();
	return ret && ret2;
}

bool SPIInterface::dump(uint32_t base_addr, uint32_t len)
{
	bool ret = true;
	/* enable SPI flash access */
	if (!prepare_flash_access())
		return false;

	try {
		SPIFlash flash(this, false, _spif_verbose);
		ret = flash.dump(_spif_filename, base_addr, len, _spif_rd_burst);
	} catch (std::exception &e) {
		printError(e.what());
		ret = false;
	}

	/* reload bitstream */
	return post_flash_access() && ret;
}
