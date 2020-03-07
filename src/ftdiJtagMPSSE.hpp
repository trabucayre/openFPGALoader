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
		uint32_t clkHZ, bool verbose = false);
	FtdiJtagMPSSE(const FTDIpp_MPSSE::mpsse_bit_config &cable,
		uint32_t clkHZ, bool verbose);
	virtual ~FtdiJtagMPSSE();

	int setClkFreq(uint32_t clkHZ) override {
		return FTDIpp_MPSSE::setClkFreq(clkHZ);
	}
	int setClkFreq(uint32_t clkHZ, char use_divide_by_5) override {
		return FTDIpp_MPSSE::setClkFreq(clkHZ, use_divide_by_5);}

	/* TMS */
	int storeTMS(uint8_t *tms, int nb_bit, uint8_t tdi = 1,
				bool read = false) override;
	int writeTMS(uint8_t *tdo, int len = 0) override;
	/* TDI */
	int storeTDI(uint8_t tdi, int nb_bit, bool read) override;
	int storeTDI(uint8_t *tdi, int nb_byte, bool read) override;
	int writeTDI(uint8_t *tdo, int nb_bit) override;

	/*!
	 * \brief return internal buffer size (in byte).
	 * \return _buffer_size -3 for mpsse cmd + size, -1 for potential SEND_IMMEDIATE
	 */
	int get_buffer_size() override { return _buffer_size-3; }

	bool isFull() override { return false;}

 private:
	void init_internal(const FTDIpp_MPSSE::mpsse_bit_config &cable);
	uint8_t *_in_buf;
	bool _ch552WA; /* avoid errors with SiPeed tangNano */
};
#endif
