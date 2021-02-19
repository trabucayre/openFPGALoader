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

#ifndef SRC_ANLOGICCABLE_HPP_
#define SRC_ANLOGICCABLE_HPP_

#include <libusb.h>

#include "jtagInterface.hpp"

/*!
 * \file AnlogicCable.hpp
 * \class AnlogicCable
 * \brief concrete class between jtag implementation and anlogic cable
 * \author Gwenhael Goavec-Merou
 */

class AnlogicCable : public JtagInterface {
 public:
	AnlogicCable(uint32_t clkHZ, bool verbose);
	virtual ~AnlogicCable();

	int setClkFreq(uint32_t clkHZ) override;

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

 private:
	bool _verbose;

	int write(uint8_t *in_buf, uint8_t *out_buf, int len, int rd_len);

    libusb_device_handle *dev_handle;
	libusb_context *usb_ctx;
	uint8_t _tdi;
	uint8_t _tms;
};
#endif  // SRC_ANLOGICCABLE_HPP_
