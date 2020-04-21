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

#ifndef SRC_SPIINTERFACE_HPP_
#define SRC_SPIINTERFACE_HPP_

#include <iostream>
#include <vector>

/*!
 * \file SPIInterface.hpp
 * \class SPIInterface
 * \brief abstract class between spi implementation and converters
 * \author Gwenhael Goavec-Merou
 */

class SPIInterface {
 public:
	virtual ~SPIInterface() {}

	/*!
	 * \brief send a command, followed by len byte.
	 * \param[in] cmd: command/opcode to send
	 * \param[in] tx: buffer to send
	 * \param[in] rx: buffer for read access
	 * \param[in] len: number of byte to send/receive (cmd not comprise)
	 *                 to send only a cmd set len to 0
	 * \return 0 when success
	 */
	virtual int spi_put(uint8_t cmd, uint8_t *tx, uint8_t *rx,
						uint16_t len) = 0;

	/*!
	 * \brief send a command, followed by len byte.
	 * \param[in] tx: buffer to send
	 * \param[in] rx: buffer for read access
	 * \param[in] len: number of byte to send/receive
	 * \return 0 when success
	 */
	virtual int spi_put(uint8_t *tx, uint8_t *rx, uint16_t len) = 0;

	/*!
	 * \brief wait until register content and mask match cond, or timeout
	 * \param[in] cmd: register to read
	 * \param[in] mask: mask used with read byte
	 * \param[in] cond: condition to wait
	 * \param[in] timeout: number of try before fail
	 * \return 0 when success, -ETIME when timeout occur
	 */
	virtual int spi_wait(uint8_t cmd, uint8_t mask, uint8_t cond,
			uint32_t timeout, bool verbose = false) = 0;
};
#endif  // SRC_SPIINTERFACE_HPP_
