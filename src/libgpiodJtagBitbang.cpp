// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (C) 2020-2022 Gwenhael Goavec-Merou <gwenhael.goavec-merou@trabucayre.com>
 * Copyright (C) 2022 Niklas Ekström <mail@niklasekstrom.nu>
 *
 * libgpiod bitbang driver added by Niklas Ekström <mail@niklasekstrom.nu> in 2022
 */

#include "libgpiodJtagBitbang.hpp"

#include <gpiod.h>
#include <stdio.h>
#include <string.h>

#include <iostream>
#include <map>
#include <vector>
#include <string>
#include <stdexcept>


#include "display.hpp"

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
		const std::string &dev, __attribute__((unused)) uint32_t clkHZ,
		int8_t verbose):_verbose(verbose>1)
{
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
#ifdef GPIOD_APIV2
	const unsigned int pins[] = {_tck_pin, _tms_pin, _tdi_pin, _tdo_pin};
#else
	const int pins[] = {_tck_pin, _tms_pin, _tdi_pin, _tdo_pin};
#endif
	for (uint32_t i = 0; i < sizeof(pins) / sizeof(pins[0]); i++) {
		if (pins[i] < 0 || pins[i] >= 1000) {
			display("Pin %d is outside of valid range\n", pins[i]);
			throw std::runtime_error("A pin is outside of valid range\n");
		}

		for (uint32_t j = i + 1; j < sizeof(pins) / sizeof(pins[0]); j++) {
			if (pins[i] == pins[j]) {
				display("Two or more pins are assigned to the same pin number %d\n", pins[i]);
				throw std::runtime_error("Two or more pins are assigned to the same pin number\n");
			}
		}
	}

	_chip = gpiod_chip_open(chip_dev.c_str());
	if (!_chip) {
		display("Unable to open gpio chip %s\n", chip_dev.c_str());
		throw std::runtime_error("Unable to open gpio chip\n");
	}

#ifdef GPIOD_APIV2
	_tdo_req_cfg = gpiod_request_config_new();
	_tdi_req_cfg = gpiod_request_config_new();
	_tck_req_cfg = gpiod_request_config_new();
	_tms_req_cfg = gpiod_request_config_new();

	gpiod_request_config_set_consumer(_tdo_req_cfg, "_tdo");
	gpiod_request_config_set_consumer(_tdi_req_cfg, "_tdi");
	gpiod_request_config_set_consumer(_tck_req_cfg, "_tck");
	gpiod_request_config_set_consumer(_tms_req_cfg, "_tms");

	_tdo_settings = gpiod_line_settings_new();
	_tdi_settings = gpiod_line_settings_new();
	_tck_settings = gpiod_line_settings_new();
	_tms_settings = gpiod_line_settings_new();

	gpiod_line_settings_set_direction(
		_tdo_settings, GPIOD_LINE_DIRECTION_INPUT);
	gpiod_line_settings_set_direction(
		_tdi_settings, GPIOD_LINE_DIRECTION_OUTPUT);
	gpiod_line_settings_set_direction(
		_tck_settings, GPIOD_LINE_DIRECTION_OUTPUT);
	gpiod_line_settings_set_direction(
		_tms_settings, GPIOD_LINE_DIRECTION_OUTPUT);

	gpiod_line_settings_set_bias(
		_tdo_settings, GPIOD_LINE_BIAS_DISABLED);
	gpiod_line_settings_set_bias(
		_tdi_settings, GPIOD_LINE_BIAS_DISABLED);
	gpiod_line_settings_set_bias(
		_tck_settings, GPIOD_LINE_BIAS_DISABLED);
	gpiod_line_settings_set_bias(
		_tms_settings, GPIOD_LINE_BIAS_DISABLED);

	_tdo_line_cfg = gpiod_line_config_new();
	_tdi_line_cfg = gpiod_line_config_new();
	_tck_line_cfg = gpiod_line_config_new();
	_tms_line_cfg = gpiod_line_config_new();

	gpiod_line_config_add_line_settings(
		_tdo_line_cfg, &_tdo_pin, 1, _tdo_settings);
	gpiod_line_config_add_line_settings(
		_tdi_line_cfg, &_tdi_pin, 1, _tdi_settings);
	gpiod_line_config_add_line_settings(
		_tck_line_cfg, &_tck_pin, 1, _tck_settings);
	gpiod_line_config_add_line_settings(
		_tms_line_cfg, &_tms_pin, 1, _tms_settings);

	_tdo_request = gpiod_chip_request_lines(
		_chip, _tdo_req_cfg, _tdo_line_cfg);
	_tdi_request = gpiod_chip_request_lines(
		_chip, _tdi_req_cfg, _tdi_line_cfg);
	_tck_request = gpiod_chip_request_lines(
		_chip, _tck_req_cfg, _tck_line_cfg);
	_tms_request = gpiod_chip_request_lines(
		_chip, _tms_req_cfg, _tms_line_cfg);
#else
	_tdo_line = get_line(_tdo_pin, 0, GPIOD_LINE_REQUEST_DIRECTION_INPUT);
	_tdi_line = get_line(_tdi_pin, 0, GPIOD_LINE_REQUEST_DIRECTION_OUTPUT);
	_tck_line = get_line(_tck_pin, 0, GPIOD_LINE_REQUEST_DIRECTION_OUTPUT);
	_tms_line = get_line(_tms_pin, 1, GPIOD_LINE_REQUEST_DIRECTION_OUTPUT);
#endif

	_curr_tdi = 0;
	_curr_tck = 0;
	_curr_tms = 1;

	// FIXME: I'm unsure how this value should be set.
	// Maybe experiment, or think through what it should be.
	_clkHZ = 5000000;
}

LibgpiodJtagBitbang::~LibgpiodJtagBitbang()
{
#ifdef GPIOD_APIV2
	if (_tms_request)
		gpiod_line_request_release(_tms_request);
	if (_tms_line_cfg)
		gpiod_line_config_free(_tms_line_cfg);
	if (_tms_settings)
		gpiod_line_settings_free(_tms_settings);

	if (_tck_request)
		gpiod_line_request_release(_tck_request);
	if (_tck_line_cfg)
		gpiod_line_config_free(_tck_line_cfg);
	if (_tck_settings)
		gpiod_line_settings_free(_tck_settings);

	if (_tdi_request)
		gpiod_line_request_release(_tdi_request);
	if (_tdi_line_cfg)
		gpiod_line_config_free(_tdi_line_cfg);
	if (_tdi_settings)
		gpiod_line_settings_free(_tdi_settings);

	if (_tdo_request)
		gpiod_line_request_release(_tdo_request);
	if (_tdo_line_cfg)
		gpiod_line_config_free(_tdo_line_cfg);
	if (_tdo_settings)
		gpiod_line_settings_free(_tdo_settings);
#else
	if (_tms_line)
		gpiod_line_release(_tms_line);

	if (_tck_line)
		gpiod_line_release(_tck_line);

	if (_tdi_line)
		gpiod_line_release(_tdi_line);

	if (_tdo_line)
		gpiod_line_release(_tdo_line);
#endif

	if (_chip)
		gpiod_chip_close(_chip);
}

#ifndef GPIOD_APIV2
gpiod_line *LibgpiodJtagBitbang::get_line(unsigned int offset, int val, int dir)
{
	gpiod_line *line = gpiod_chip_get_line(_chip, offset);
	if (!line) {
		display("Unable to get gpio line %u\n", offset);
		throw std::runtime_error("Unable to get gpio line\n");
	}

	gpiod_line_request_config config = {
		.consumer = "openFPGALoader",
		.request_type = dir,
		.flags = 0,
	};

	int ret = gpiod_line_request(line, &config, val);
	if (ret < 0) {
		display("Error requesting gpio line %u\n", offset);
		throw std::runtime_error("Error requesting gpio line\n");
	}

	return line;
}
#endif

int LibgpiodJtagBitbang::update_pins(int tck, int tms, int tdi)
{
	if (tdi != _curr_tdi) {
#ifdef GPIOD_APIV2
		if (gpiod_line_request_set_value(_tdi_request, _tdi_pin,
			(tdi == 0) ? GPIOD_LINE_VALUE_INACTIVE :
				GPIOD_LINE_VALUE_ACTIVE) < 0)
#else
		if (gpiod_line_set_value(_tdi_line, tdi) < 0)
#endif
			display("Unable to set gpio pin tdi\n");
	}

	if (tms != _curr_tms) {
#ifdef GPIOD_APIV2
		if (gpiod_line_request_set_value(_tms_request, _tms_pin,
			(tms == 0) ? GPIOD_LINE_VALUE_INACTIVE :
				GPIOD_LINE_VALUE_ACTIVE) < 0)
#else
		if (gpiod_line_set_value(_tms_line, tms) < 0)
#endif
			display("Unable to set gpio pin tms\n");
	}

	if (tck != _curr_tck) {
#ifdef GPIOD_APIV2
		if (gpiod_line_request_set_value(_tck_request, _tck_pin,
			(tck == 0) ? GPIOD_LINE_VALUE_INACTIVE :
				GPIOD_LINE_VALUE_ACTIVE) < 0)
#else
		if (gpiod_line_set_value(_tck_line, tck) < 0)
#endif
			display("Unable to set gpio pin tck\n");
	}

	_curr_tdi = tdi;
	_curr_tms = tms;
	_curr_tck = tck;

	return 0;
}

int LibgpiodJtagBitbang::read_tdo()
{
#ifdef GPIOD_APIV2
	gpiod_line_value req = gpiod_line_request_get_value(
		_tdo_request, _tdo_pin);
        if (req == GPIOD_LINE_VALUE_ERROR)
        {
		display("Error reading TDO line\n");
		throw std::runtime_error("Error reading TDO line\n");
	}
	return (req == GPIOD_LINE_VALUE_ACTIVE) ? 1 : 0;
#else
	return gpiod_line_get_value(_tdo_line);
#endif
}

int LibgpiodJtagBitbang::setClkFreq(__attribute__((unused)) uint32_t clkHZ)
{
	// FIXME: The assumption is that calling the gpiod_line_set_value
	// routine will limit the clock frequency to lower than what is specified.
	// This needs to be verified, and possibly artificial delays should be added.
	return 0;
}

int LibgpiodJtagBitbang::writeTMS(uint8_t *tms_buf, uint32_t len,
		__attribute__((unused)) bool flush_buffer)
{
	int tms;

	if (len == 0) // nothing -> stop
		return len;

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

	for (uint32_t i = 0; i < len; i++) {
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
