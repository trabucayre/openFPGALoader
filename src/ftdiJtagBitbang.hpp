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

#ifndef FTDIJTAGBITBANG_H
#define FTDIJTAGBITBANG_H
#include <ftdi.h>
#include <iostream>
#include <string>
#include <vector>

#include "board.hpp"
#include "jtagInterface.hpp"
#include "ftdipp_mpsse.hpp"

/*!
 * \file FtdiJtagBitBang.hpp
 * \class FtdiJtagBitBang
 * \brief concrete class between jtag implementation and FTDI capable bitbang mode
 * \author Gwenhael Goavec-Merou
 */

class FtdiJtagBitBang : public JtagInterface, private FTDIpp_MPSSE {
 public:
	FtdiJtagBitBang(const FTDIpp_MPSSE::mpsse_bit_config &cable,
		const jtag_pins_conf_t *pin_conf, std::string dev, const std::string &serial,
		uint32_t clkHZ, bool verbose = false);
	virtual ~FtdiJtagBitBang();

	int setClkFreq(uint32_t clkHZ) override;
	int setClkFreq(uint32_t clkHZ, char use_divide_by_5) override {
		return FTDIpp_MPSSE::setClkFreq(clkHZ, use_divide_by_5);}

	/* TMS */
	int writeTMS(uint8_t *tms, int len, bool flush_buffer) override;

	/* TDI */
	int writeTDI(uint8_t *tx, uint8_t *rx, uint32_t len, bool end) override;
	int toggleClk(uint8_t tms, uint8_t tdo, uint32_t clk_len) override;

	/*!
	 * \brief return internal buffer size (in byte).
	 * \return _buffer_size divided by 2 (two byte for clk) and divided by 8 (one
	 * state == one byte)
	 */
	int get_buffer_size() override { return _buffer_size/8/2; }

	bool isFull() override { return _nb_bit == 8*get_buffer_size();}

	int flush() override;

 private:
	void init_internal(const FTDIpp_MPSSE::mpsse_bit_config &cable,
		const jtag_pins_conf_t *pin_conf);
	int write(uint8_t *tdo, int nb_bit);
	int setBitmode(uint8_t mode);
	uint8_t *_in_buf;

	uint8_t _bitmode;
	uint8_t _tck_pin; /*!< tck pin: 1 << pin id */
	uint8_t _tms_pin; /*!< tms pin: 1 << pin id */
	uint8_t _tdo_pin; /*!< tdo pin: 1 << pin id */
	uint8_t _tdi_pin; /*!< tdi pin: 1 << pin id */
	int _nb_bit;
	uint8_t _curr_tms;
};
#endif
