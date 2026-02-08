// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (C) 2020 Gwenhael Goavec-Merou <gwenhael.goavec-merou@trabucayre.com>
 */

#ifndef SRC_FTDIJTAGMPSSE_HPP_
#define SRC_FTDIJTAGMPSSE_HPP_

#include <ftdi.h>

#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

#include "cable.hpp"
#include "ftdipp_mpsse.hpp"
#include "jtagInterface.hpp"

/*!
 * \file FtdiJtagMPSSE.hpp
 * \class FtdiJtagMPSSE
 * \brief concrete class between jtag implementation and FTDI capable bitbang mode
 * \author Gwenhael Goavec-Merou
 */

class FtdiJtagMPSSE : public JtagInterface, public FTDIpp_MPSSE {
 public:
	FtdiJtagMPSSE(const cable_t &cable, const std::string &dev,
		const std::string &serial, uint32_t clkHZ, bool invert_read_edge,
		int8_t verbose = 0);
	virtual ~FtdiJtagMPSSE();

	int setClkFreq(uint32_t clkHZ) override;

	uint32_t getClkFreq() override {return FTDIpp_MPSSE::getClkFreq();}

	/*!
	 * Return constant to describe if read is on rising or falling TCK edge
	 */
	tck_edge_t getReadEdge() override {
		return _read_mode == MPSSE_READ_NEG ? FALLING_EDGE : RISING_EDGE;
	}
	/*!
	 * configure TCK edge used for read
	 */
	void setReadEdge(tck_edge_t rd_edge) override {
		_read_mode = rd_edge == FALLING_EDGE ? MPSSE_READ_NEG : 0;
	}
	/*!
	 * Return constant to describe if write is on rising or falling TCK edge
	 */
	tck_edge_t getWriteEdge() override {
		return _write_mode == MPSSE_WRITE_NEG ? FALLING_EDGE : RISING_EDGE;
	}
	/*!
	 * configure TCK edge used for write
	 */
	void setWriteEdge(tck_edge_t wr_edge) override {
		_write_mode = wr_edge == FALLING_EDGE ? MPSSE_WRITE_NEG : 0;
	}

	/* TMS */
	int writeTMS(const uint8_t *tms, uint32_t len, bool flush_buffer, const uint8_t tdi = 1) override;
	/* clock */
	int toggleClk(uint8_t tms, uint8_t tdi, uint32_t clk_len) override;
	/* TDI */
	int writeTDI(const uint8_t *tx, uint8_t *rx, uint32_t len, bool end) override;

	/*!
	 * \brief send TMD and TDI and receive tdo bits;
	 * \param tms: array of TMS values (used to write)
	 * \param tdi: array of TDI values (used to write)
	 * \param tdo: array of TDO values (used when read)
	 * \param len: number of bit to send/receive
	 * \return true with full buffers are sent, false otherwise
	 */
	bool writeTMSTDI(const uint8_t *tms, const uint8_t *tdi, uint8_t *tdo,
		uint32_t len) override;
	/*!
	 * \brief return internal buffer size (in byte).
	 * \return _buffer_size -3 for mpsse cmd + size, -1 for potential SEND_IMMEDIATE
	 */
	int get_buffer_size() override { return _buffer_size-3; }

	bool isFull() override { return false;}

	int flush() override;

 private:
	void init_internal(const mpsse_bit_config &cable);
	/* writeTMSTDI specifics */
	/*!
	 * \brief try to append tms buffer, flush content if > 6
	 * \param buffer: current tms buffer
	 * \param bit: bit to append
	 * \param offset: bits already stored
	 * \param tdo: buffer used when reading after flush
	 * \param len: length
	 * \return < 0 if transaction fails, offset + 1 when append and 0 when flush
	 */
	int32_t update_tms_buff(uint8_t *buffer, uint8_t bit,
		uint32_t offset, uint8_t tdi, uint8_t *tdo, bool end = false);
	uint32_t update_tdo_buff(uint8_t *buffer, uint8_t *tdo, uint32_t len);
	/*!
	 * \brief configure read and write edge (pos or neg), with freq < 15MHz
	 *        neg is used for write and pos to sample. with freq >= 15MHz
	 *        pos is used for write and neg to sample
	 */
	void config_edge();
	bool _ch552WA; /* avoid errors with SiPeed tangNano */
	bool _cmd8EWA; /* avoid errors with Sipeed FT2232H emulation */
	uint8_t _write_mode; /**< write edge configuration */
	uint8_t _read_mode; /**< read edge configuration */
	bool _invert_read_edge; /**< read edge selection (false: pos, true: neg) */
	bool _msb_first; /**< use MSB first, workaround for sipeed console */
	/* writeTMSTDI specifics */
	uint32_t _tdo_pos;
	uint8_t _curr_tdi;
	uint8_t _curr_tms;
};
#endif  // SRC_FTDIJTAGMPSSE_HPP_
