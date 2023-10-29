// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (C) 2020 Gwenhael Goavec-Merou <gwenhael.goavec-merou@trabucayre.com>
 */

#ifndef SRC_JTAGINTERFACE_HPP_
#define SRC_JTAGINTERFACE_HPP_

#include <cstdint>
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

	enum tck_edge_t {
		FALLING_EDGE = 0,
		RISING_EDGE  = 1,
		NONE_EDGE    = 2,
	};

	/*!
	 * Return constant to describe if read is on rising or falling TCK edge
	 */
	virtual tck_edge_t getReadEdge() { return NONE_EDGE; }
	/*!
	 * configure TCK edge used for read
	 */
	virtual void setReadEdge(tck_edge_t rd_edge) { (void) rd_edge; }
	/*!
	 * Return constant to describe if write is on rising or falling TCK edge
	 */
	virtual tck_edge_t getWriteEdge() { return NONE_EDGE; }
	/*!
	 * configure TCK edge used for write
	 */
	virtual void setWriteEdge(tck_edge_t wr_edge) { (void)wr_edge; }

	/*!
	 * \brief flush TMS internal buffer (ie. transmit to converter)
	 * \param tdo: pointer for read operation. May be NULL
	 * \param len: number of bit to send
	 * \return number of bit send/received
	 */
	virtual int writeTMS(const uint8_t *tms, uint32_t len, bool flush_buffer, const uint8_t tdi = 1) = 0;

	/*!
	 * \brief send TDI bits (mainly in shift DR/IR state)
	 * \param tdi: array of TDI values (used to write)
	 * \param tdo: array of TDO values (used when read)
	 * \param len: number of bit to send/receive
	 * \param end: in JTAG state machine last bit and tms are set in same time
	 *             but only in shift[i|d]r, if end is false tms remain the same.
	 * \return number of bit written and/or read
	 */
	virtual int writeTDI(const uint8_t *tx, uint8_t *rx, uint32_t len, bool end) = 0;
	/*!
	 * \brief send TMD and TDI and receive tdo bits;
	 * \param tms: array of TMS values (used to write)
	 * \param tdi: array of TDI values (used to write)
	 * \param tdo: array of TDO values (used when read)
	 * \param len: number of bit to send/receive
	 * \return true with full buffers are sent, false otherwise
	 */
	virtual bool writeTMSTDI(const uint8_t *tms, const uint8_t *tdi,
			uint8_t *tdo, uint32_t len)
	{ (void)tms; (void)tdi; (void)tdo; (void)len; return false;}
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
#endif  // SRC_JTAGINTERFACE_HPP_
