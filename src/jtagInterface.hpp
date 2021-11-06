// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (C) 2020 Gwenhael Goavec-Merou <gwenhael.goavec-merou@trabucayre.com>
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
	virtual uint32_t getClkFreq() {return _clkHZ;}

	/*!
	 * \brief flush TMS internal buffer (ie. transmit to converter)
	 * \param tdo: pointer for read operation. May be NULL
	 * \param len: number of bit to send
	 * \return number of bit send/received
	 */
	virtual int writeTMS(uint8_t *tms, uint32_t len, bool flush_buffer) = 0;

	/*!
	 * \brief send TDI bits (mainly in shift DR/IR state)
	 * \param tdi: array of TDI values (used to write)
	 * \param tdo: array of TDO values (used when read)
	 * \param len: number of bit to send/receive
	 * \param end: in JTAG state machine last bit and tms are set in same time
	 *             but only in shift[i|d]r, if end is false tms remain the same.
	 * \return number of bit written and/or read
	 */
	virtual int writeTDI(uint8_t *tx, uint8_t *rx, uint32_t len, bool end) = 0;

	/*!
	 * \brief toggle clk without touch of TDI/TMS
	 * \param tms: state of tms signal
	 * \param tdi: state of tdi signal
	 * \param clk_len: number of clock cycle
	 * \return number of clock cycle send
	 */
	virtual int toggleClk(uint8_t tms, uint8_t tdi, uint32_t clk_len) = 0;

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

	/*!
	 * \brief force internal flush buffer
	 * \return 1 if success, 0 if nothing to write, -1 is something wrong
	 */
	virtual int flush() = 0;
 protected:
	uint32_t _clkHZ; /*!< current clk frequency */
};
#endif  // _JTAGINTERFACE_H_
