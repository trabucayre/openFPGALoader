// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (C) 2020-2022 Gwenhael Goavec-Merou <gwenhael.goavec-merou@trabucayre.com>
 * Copyright (C) 2022 Niklas Ekström <mail@niklasekstrom.nu>
 *
 * libgpiod bitbang driver added by Niklas Ekström <mail@niklasekstrom.nu> in 2022
 */

#ifndef LIBGPIODBITBANG_H
#define LIBGPIODBITBANG_H

#include <string>

#include "gpiod.h"

#include "board.hpp"
#include "jtagInterface.hpp"

/*!
 * \file LibgpiodJtagBitbang.hpp
 * \class LibgpiodJtagBitbang
 * \brief concrete class between jtag implementation and gpio bitbang
 * \author Niklas Ekström
 */

struct gpiod_chip;
struct gpiod_line;

class LibgpiodJtagBitbang : public JtagInterface {
 public:
	LibgpiodJtagBitbang(const jtag_pins_conf_t *pin_conf,
		const std::string &dev, uint32_t clkHZ, int8_t verbose);
	virtual ~LibgpiodJtagBitbang();

	int setClkFreq(uint32_t clkHZ) override;
	int writeTMS(const uint8_t *tms_buf, uint32_t len, bool flush_buffer, const uint8_t tdi = 1) override;
	int writeTDI(const uint8_t *tx, uint8_t *rx, uint32_t len, bool end) override;
	int toggleClk(uint8_t tms, uint8_t tdo, uint32_t clk_len) override;
	bool writeTMSTDI(const uint8_t *tms, const uint8_t *tdi, uint8_t *tdo,
			 uint32_t len) override;

	int get_buffer_size() override { return 0; }
	bool isFull() override { return false; }
	int flush() override { return 0; }

 private:
#ifndef GPIOD_APIV2
	gpiod_line *get_line(unsigned int offset, int val, int dir);
	int update_pins(int tck, int tms, int tdi);
#else
	int update_pins(gpiod_line_value tms, gpiod_line_value tdi);
#endif
	int read_tdo();

	bool _verbose;

#ifdef GPIOD_APIV2
	unsigned int _tck_pin;
	unsigned int _tms_pin;
	unsigned int _tdo_pin;
	unsigned int _tdi_pin;
	unsigned int _out_pins[4];
	unsigned int _tms_group[2];
	unsigned int _tdi_group[2];
#else
	int _tck_pin;
	int _tms_pin;
	int _tdo_pin;
	int _tdi_pin;
#endif

	gpiod_chip *_chip;

#ifdef GPIOD_APIV2
	gpiod_request_config *_out_req_cfg;

	gpiod_line_config *_out_line_cfg;

	gpiod_line_settings *_in_settings;
	gpiod_line_settings *_out_settings;

	gpiod_line_request *_out_request;

	gpiod_line_value _curr_tms;
	gpiod_line_value _curr_tdi;
#else
	gpiod_line *_tck_line;
	gpiod_line *_tms_line;
	gpiod_line *_tdo_line;
	gpiod_line *_tdi_line;
	int _curr_tms;
	int _curr_tdi;
#endif

	int _curr_tck;
};
#endif
