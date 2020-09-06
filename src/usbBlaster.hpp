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

#ifndef USBBLASTER_H
#define USBBLASTER_H
#include <ftdi.h>
#include <iostream>
#include <string>
#include <vector>

#include "board.hpp"
#include "jtagInterface.hpp"
#include "ftdipp_mpsse.hpp"

/*!
 * \file UsbBlaster.hpp
 * \class UsbBlaster
 * \brief altera/intel usb blaster implementation
 * \author Gwenhael Goavec-Merou
 */

class UsbBlaster : public JtagInterface {
 public:
	UsbBlaster(bool verbose = false);
	virtual ~UsbBlaster();

	int setClkFreq(uint32_t clkHZ) override;
	int setClkFreq(uint32_t clkHZ, char use_divide_by_5) override {
		(void)clkHZ; (void) use_divide_by_5;
		return 1;}

	/*!
	 * \brief drive TMS to move in JTAG state machine
	 * \param tms serie of TMS state
	 * \param len number of TMS state
	 * \param flush_buffer force flushing the buffer
	 * \return number of state written
	 * */
	int writeTMS(uint8_t *tms, int len, bool flush_buffer) override;

	/* TDI */
	int writeTDI(uint8_t *tx, uint8_t *rx, uint32_t len, bool end) override;
	/*!
	 * \brief toggle clock with static tms and tdi
	 * \param tms TMS state
	 * \param tdi TDI state
	 * \param clk_len number of clock cycle
	 * \return number of clock cycle
	 * */
	int toggleClk(uint8_t tms, uint8_t tdi, uint32_t clk_len) override;

	/*!
	 * \brief return internal buffer size (in byte).
	 * \return _buffer_size divided by 2 (two byte for clk) and divided by 8 (one
	 * state == one byte)
	 */
	int get_buffer_size() override { return 63/8/2; }

	bool isFull() override { return _nb_bit == 8*get_buffer_size();}

	int flush() override;

 private:
	struct ftdi_context *_ftdi;
	void init_internal();
	int writeByte(uint8_t *tdo, int nb_byte);
	int writeBit(uint8_t *tdo, int nb_bit);
	int write(bool read, int rd_len);
	int setBitmode(uint8_t mode);
	uint8_t *_in_buf;

	bool _verbose;
	uint8_t _tck_pin; /*!< tck pin: 1 << pin id */
	uint8_t _tms_pin; /*!< tms pin: 1 << pin id */
	uint8_t _tdi_pin; /*!< tdi pin: 1 << pin id */
	int _nb_bit;
	uint8_t _curr_tms;
	uint16_t _buffer_size;
};
#endif
