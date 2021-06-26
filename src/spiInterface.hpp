// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (C) 2020 Gwenhael Goavec-Merou <gwenhael.goavec-merou@trabucayre.com>
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
						uint32_t len) = 0;

	/*!
	 * \brief send a command, followed by len byte.
	 * \param[in] tx: buffer to send
	 * \param[in] rx: buffer for read access
	 * \param[in] len: number of byte to send/receive
	 * \return 0 when success
	 */
	virtual int spi_put(uint8_t *tx, uint8_t *rx, uint32_t len) = 0;

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
