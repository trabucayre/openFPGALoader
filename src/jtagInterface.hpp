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

#ifndef _JTAGINTERFACE_H_
#define _JTAGINTERFACE_H_

#include <iostream>
#include <vector>

/*!
 * \file JtagInterface.hpp
 * \class JtagInterface
 * \brief abstract class between jtag implementation and converters
 * \author Gwenhael Goavec-Merou
 */

class JtagInterface {
 public:
	virtual ~JtagInterface() {}

	virtual int setClkFreq(uint32_t clkHZ) = 0;
	virtual int setClkFreq(uint32_t clkHZ, char use_divide_by_5) = 0;

	/*!
	 * \brief store TMS states in internal buffer. Not limited to 8 states
	 * \param tms: array of TMS values
	 * \param nb_bit: number of TMS states to store
	 * \param tdi: state of TDI for all TMS to store
	 * \return number of states stored
	 */
	virtual int storeTMS(uint8_t *tms, int nb_bit, uint8_t tdi = 1,
						bool read = false) = 0;
	/*!
	 * \brief flush TMS internal buffer (ie. transmit to converter)
	 * \param tdo: pointer for read operation. May be NULL
	 * \param len: number of bit to send
	 * \return number of bit send/received
	 */
	virtual int writeTMS(uint8_t *tdo, int len = 0) = 0;
	/*!
	 * \brief store up to 8 TDI state(s) in internal buffer
	 * \param tdi: TDI value(s)
	 * \param tms: state of TMS for all TDI to store
	 * \param nb_bit: number of TMS states to store
	 * \return number of states stored
	 */
	virtual int storeTDI(uint8_t tdi, int nb_bit, bool read) = 0;
	/*!
	 * \brief store TDI multiple of 8 states in internal buffer.
	 * \param tdi: array of TDI values
	 * \param tms: state of TMS for all TDI to store
	 * \param nb_byte: number of TDI states to store
	 * \return number of states stored
	 */
	virtual int storeTDI(uint8_t *tdi, int nb_byte, bool read) = 0;
	/*!
	 * \brief flush TDI internal buffer (ie. transmit to converter)
	 * \param tdo: pointer for read operation. May be NULL
	 * \return number of bit send/received
	 */
	virtual int writeTDI(uint8_t *tdo, int nb_bit) = 0;

	/*!
	 * \brief return internal buffer size (in byte)
	 * \return internal buffer size
	 */
	virtual int get_buffer_size() = 0;

	/*!
	 * \brief return status of internal buffer
	 * \return true when internal buffer is full
	 */
	virtual bool isFull() = 0;
};
#endif  // _JTAGINTERFACE_H_
