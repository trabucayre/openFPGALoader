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

#ifndef FTDIJTAGMPSSE_H
#define FTDIJTAGMPSSE_H
#include <ftdi.h>
#include <iostream>
#include <string>
#include <vector>

#include "jtagInterface.hpp"
#include "ftdipp_mpsse.hpp"

/*!
 * \file FtdiJtagMPSSE.hpp
 * \class FtdiJtagMPSSE
 * \brief concrete class between jtag implementation and FTDI capable bitbang mode
 * \author Gwenhael Goavec-Merou
 */

class FtdiJtagMPSSE : public JtagInterface, private FTDIpp_MPSSE {
 public:
	FtdiJtagMPSSE(const FTDIpp_MPSSE::mpsse_bit_config &cable, std::string dev,
		const std::string &serial, uint32_t clkHZ, bool verbose = false);
	virtual ~FtdiJtagMPSSE();

	int setClkFreq(uint32_t clkHZ) override {
		return FTDIpp_MPSSE::setClkFreq(clkHZ);
	}

	/* TMS */
	int writeTMS(uint8_t *tms, int len, bool flush_buffer) override;
	/* clock */
	int toggleClk(uint8_t tms, uint8_t tdi, uint32_t clk_len) override;
	/* TDI */
	int writeTDI(uint8_t *tx, uint8_t *rx, uint32_t len, bool end) override;

	/*!
	 * \brief return internal buffer size (in byte).
	 * \return _buffer_size -3 for mpsse cmd + size, -1 for potential SEND_IMMEDIATE
	 */
	int get_buffer_size() override { return _buffer_size-3; }

	bool isFull() override { return false;}

	int flush() override;

 private:
	void init_internal(const FTDIpp_MPSSE::mpsse_bit_config &cable);
	bool _ch552WA; /* avoid errors with SiPeed tangNano */
};
#endif
