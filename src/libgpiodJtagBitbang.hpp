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
		const std::string &dev, uint32_t clkHZ, uint8_t verbose);
	virtual ~LibgpiodJtagBitbang();

	int setClkFreq(uint32_t clkHZ) override;
	int writeTMS(uint8_t *tms_buf, uint32_t len, bool flush_buffer) override;
	int writeTDI(uint8_t *tx, uint8_t *rx, uint32_t len, bool end) override;
	int toggleClk(uint8_t tms, uint8_t tdo, uint32_t clk_len) override;

	int get_buffer_size() override { return 0; }
	bool isFull() override { return false; }
	int flush() override { return 0; }

 private:
	gpiod_line *get_line(unsigned int offset, int val, int dir);
	int update_pins(int tck, int tms, int tdi);
	int read_tdo();

	bool _verbose;

	int _tck_pin;
	int _tms_pin;
	int _tdo_pin;
	int _tdi_pin;

	gpiod_chip *_chip;

	gpiod_line *_tck_line;
	gpiod_line *_tms_line;
	gpiod_line *_tdo_line;
	gpiod_line *_tdi_line;

	int _curr_tms;
	int _curr_tdi;
	int _curr_tck;
};
#endif
