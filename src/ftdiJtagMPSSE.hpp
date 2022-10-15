// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (C) 2020 Gwenhael Goavec-Merou <gwenhael.goavec-merou@trabucayre.com>
 */

#ifndef FTDIJTAGMPSSE_H
#define FTDIJTAGMPSSE_H
#include <ftdi.h>
#include <iostream>
#include <string>
#include <vector>

#include "cable.hpp"
#include "jtagInterface.hpp"
#include "ftdipp_mpsse.hpp"

/*!
 * \file FtdiJtagMPSSE.hpp
 * \class FtdiJtagMPSSE
 * \brief concrete class between jtag implementation and FTDI capable bitbang mode
 * \author Gwenhael Goavec-Merou
 */

class FtdiJtagMPSSE : public JtagInterface, public FTDIpp_MPSSE {
 public:
	FtdiJtagMPSSE(const cable_t &cable, std::string dev,
		const std::string &serial, uint32_t clkHZ, bool invert_read_edge,
		int8_t verbose = 0);
	virtual ~FtdiJtagMPSSE();

	int setClkFreq(uint32_t clkHZ) override;

	uint32_t getClkFreq() override {return FTDIpp_MPSSE::getClkFreq();}

	/* TMS */
	int writeTMS(uint8_t *tms, uint32_t len, bool flush_buffer) override;
	/* clock */
	int toggleClk(uint8_t tms, uint8_t tdi, uint32_t clk_len) override;
	/* TDI */
	int writeTDI(uint8_t *tx, uint8_t *rx, uint32_t len, bool end) override;

	/*!
	 * \brief send TMD and TDI and receive tdo bits;
	 * \param tms: array of TMS values (used to write)
	 * \param tdi: array of TDI values (used to write)
	 * \param tdo: array of TDO values (used when read)
	 * \param len: number of bit to send/receive
	 * \return true with full buffers are sent, false otherwise
	 */
	virtual bool writeTMSTDI(const uint8_t *tms, const uint8_t *tdi, uint8_t *tdo,
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
		uint32_t offset, uint8_t tdi, uint8_t *tdo, bool end=false);
	uint32_t update_tdo_buff(uint8_t *buffer, uint8_t *tdo, uint32_t len);
	/*!
	 * \brief configure read and write edge (pos or neg), with freq < 15MHz
	 *        neg is used for write and pos to sample. with freq >= 15MHz
	 *        pos is used for write and neg to sample
	 */
	void config_edge();
	bool _ch552WA; /* avoid errors with SiPeed tangNano */
	uint8_t _write_mode; /**< write edge configuration */
	uint8_t _read_mode; /**< read edge configuration */
	bool _invert_read_edge; /**< read edge selection (false: pos, true: neg) */
	/* writeTMSTDI specifics */
	uint32_t _tdo_pos;
	uint8_t _curr_tdi;
	uint8_t _curr_tms;
};
#endif
