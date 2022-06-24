// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (C) 2020 Gwenhael Goavec-Merou <gwenhael.goavec-merou@trabucayre.com>
 *
 * libgpiod bitbang driver added by Niklas Ekström <mail@niklasekstrom.nu> in 2022
 */

#include <stdio.h>
#include <string.h>

#include <iostream>
#include <map>
#include <vector>
#include <string>
#include <stdexcept>

#include <gpiod.h>

#include "display.hpp"
#include "libgpiodJtagBitbang.hpp"

#define DEBUG 1

#ifdef DEBUG
#define display(...) \
	do { \
		if (_verbose) fprintf(stdout, __VA_ARGS__); \
	}while(0)
#else
#define display(...) do {}while(0)
#endif

LibgpiodJtagBitbang::LibgpiodJtagBitbang(
		const jtag_pins_conf_t *pin_conf,
		const std::string &dev, uint32_t clkHZ, uint8_t verbose)
{
	_verbose = verbose;

	_tck_pin = pin_conf->tck_pin;
	_tms_pin = pin_conf->tms_pin;
	_tdi_pin = pin_conf->tdi_pin;
	_tdo_pin = pin_conf->tdo_pin;

	std::string chip_dev = dev;
	if (chip_dev.empty())
		chip_dev = "/dev/gpiochip0";

	display("libgpiod jtag bitbang driver, dev=%s, tck_pin=%d, tms_pin=%d, tdi_pin=%d, tdo_pin=%d\n",
		chip_dev.c_str(), _tck_pin, _tms_pin, _tdi_pin, _tdo_pin);

	if (chip_dev.length() < 14 || chip_dev.substr(0, 13) != "/dev/gpiochip") {
		display("Invalid gpio chip %s, should be /dev/gpiochipX\n", chip_dev.c_str());
		throw std::runtime_error("Invalid gpio chip\n");
	}

	/* Validate pins */
	int pins[] = {_tck_pin, _tms_pin, _tdi_pin, _tdo_pin};
	for (int i = 0; i < sizeof(pins) / sizeof(pins[0]); i++) {
		if (pins[i] < 0 || pins[i] >= 1000) {
			display("Pin %d is outside of valid range\n", pins[i]);
			throw std::runtime_error("A pin is outside of valid range\n");
		}

		for (int j = i + 1; j < sizeof(pins) / sizeof(pins[0]); j++) {
			if (pins[i] == pins[j]) {
				display("Two or more pins are assigned to the same pin number %d\n", pins[i]);
				throw std::runtime_error("Two or more pins are assigned to the same pin number\n");
			}
		}
	}

	_chip = gpiod_chip_open_by_name(chip_dev.substr(5).c_str());
	if (!_chip) {
		display("Unable to open gpio chip %s\n", chip_dev.c_str());
		throw std::runtime_error("Unable to open gpio chip\n");
	}

	_tdo_line = get_line(_tdo_pin, 0, GPIOD_LINE_REQUEST_DIRECTION_INPUT);
	_tdi_line = get_line(_tdi_pin, 0, GPIOD_LINE_REQUEST_DIRECTION_OUTPUT);
	_tck_line = get_line(_tck_pin, 0, GPIOD_LINE_REQUEST_DIRECTION_OUTPUT);
	_tms_line = get_line(_tms_pin, 1, GPIOD_LINE_REQUEST_DIRECTION_OUTPUT);

	_curr_tdi = 0;
	_curr_tck = 0;
	_curr_tms = 1;

	// FIXME: I'm unsure how this value should be set.
	// Maybe experiment, or think through what it should be.
	_clkHZ = 5000000;
}

LibgpiodJtagBitbang::~LibgpiodJtagBitbang()
{
	if (_tms_line)
		gpiod_line_release(_tms_line);

	if (_tck_line)
		gpiod_line_release(_tck_line);

	if (_tdi_line)
		gpiod_line_release(_tdi_line);

	if (_tdo_line)
		gpiod_line_release(_tdo_line);

	if (_chip)
		gpiod_chip_close(_chip);
}

gpiod_line *LibgpiodJtagBitbang::get_line(unsigned int offset, int val, int dir)
{
	gpiod_line *line = gpiod_chip_get_line(_chip, offset);
	if (!line) {
		display("Unable to get gpio line %d\n", offset);
		throw std::runtime_error("Unable to get gpio line\n");
	}

	gpiod_line_request_config config = {
		.consumer = "openFPGALoader",
		.request_type = dir,
		.flags = 0,
	};

	int ret = gpiod_line_request(line, &config, val);
	if (ret < 0) {
		display("Error requesting gpio line %d\n", offset);
		throw std::runtime_error("Error requesting gpio line\n");
	}

	return line;
}

int LibgpiodJtagBitbang::update_pins(int tck, int tms, int tdi)
{
	if (tdi != _curr_tdi) {
		if (gpiod_line_set_value(_tdi_line, tdi) < 0)
			display("Unable to set gpio pin tdi\n");
	}

	if (tms != _curr_tms) {
		if (gpiod_line_set_value(_tms_line, tms) < 0)
			display("Unable to set gpio pin tms\n");
	}

	if (tck != _curr_tck) {
		if (gpiod_line_set_value(_tck_line, tck) < 0)
			display("Unable to set gpio pin tck\n");
	}

	_curr_tdi = tdi;
	_curr_tms = tms;
	_curr_tck = tck;

	return 0;
}

int LibgpiodJtagBitbang::read_tdo()
{
	return gpiod_line_get_value(_tdo_line);
}

int LibgpiodJtagBitbang::setClkFreq(uint32_t clkHZ)
{
	// FIXME: The assumption is that calling the gpiod_line_set_value
	// routine will limit the clock frequency to lower than what is specified.
	// This needs to be verified, and possibly artificial delays should be added.
	return 0;
}

int LibgpiodJtagBitbang::writeTMS(uint8_t *tms_buf, uint32_t len, bool flush_buffer)
{
	int tms;

	for (uint32_t i = 0; i < len; i++) {
		tms = ((tms_buf[i >> 3] & (1 << (i & 7))) ? 1 : 0);

		update_pins(0, tms, 0);
		update_pins(1, tms, 0);
	}

	update_pins(0, tms, 0);

	return len;
}

int LibgpiodJtagBitbang::writeTDI(uint8_t *tx, uint8_t *rx, uint32_t len, bool end)
{
	int tms = _curr_tms;
	int tdi = _curr_tdi;

	if (rx)
		memset(rx, 0, len / 8);

	for (uint32_t i = 0, pos = 0; i < len; i++) {
		if (end && (i == len - 1))
			tms = 1;

		if (tx)
			tdi = (tx[i >> 3] & (1 << (i & 7))) ? 1 : 0;

		update_pins(0, tms, tdi);
		update_pins(1, tms, tdi);

		if (rx) {
			if (read_tdo() > 0)
				rx[i >> 3] |= 1 << (i & 7);
		}
	}

	update_pins(0, tms, tdi);

	return len;
}

int LibgpiodJtagBitbang::toggleClk(uint8_t tms, uint8_t tdi, uint32_t clk_len)
{
	for (uint32_t i = 0; i < clk_len; i++) {
		update_pins(0, tms, tdi);
		update_pins(1, tms, tdi);
	}

	update_pins(0, tms, tdi);

	return clk_len;
}
