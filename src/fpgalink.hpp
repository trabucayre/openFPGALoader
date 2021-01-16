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

#ifndef SRC_FX2CABLE_HPP_
#define SRC_FX2CABLE_HPP_

#include <libusb.h>

#include "jtagInterface.hpp"
#include "board.hpp"


/*!
 * \file FX2Cable.hpp
 * \class FX2Cable
 * \brief concrete class between jtag implementation and FX2 cable
 */

class FpgaLink : public JtagInterface{

public:
	FpgaLink(bool verbose);
	int setClkFreq(uint32_t clkHZ) override{(void)clkHZ; return 0;}
	int setClkFreq(uint32_t clkHZ, char use_divide_by_5) override {(void)clkHZ; (void)use_divide_by_5;return 0;}

	/* TMS */
	int writeTMS(uint8_t *tms, int len, bool flush_buffer) override;
	/* TDI */
	int writeTDI(uint8_t *tx, uint8_t *rx, uint32_t len, bool end) override;
	/* clk */
	int toggleClk(uint8_t tms, uint8_t tdi, uint32_t clk_len) override;

	/*!
	 * \brief return internal buffer size (in byte).
	 * \return _buffer_size divided by 2 (two byte for clk) and divided by 8 (one
	 * state == one byte)
	 */
	int get_buffer_size() override { return 0;}

	bool isFull() override { return false;}

	int flush() override;

 protected:
	bool _verbose;
	struct FLContext *handle = NULL;
};


class FX2Cable : public FpgaLink {
 public:
	FX2Cable(bool verbose, const jtag_pins_conf_t *pin_conf, const char *ivp = NULL, const char *vp = NULL);
	virtual ~FX2Cable();

 private:
	void transcode_pin_config(const jtag_pins_conf_t *pin_conf, char* buffer);
};
#endif  // SRC_FX2CABLE_HPP_
