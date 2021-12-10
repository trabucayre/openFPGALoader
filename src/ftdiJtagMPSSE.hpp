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
	FtdiJtagMPSSE(const FTDIpp_MPSSE::mpsse_bit_config &cable, std::string dev,
		const std::string &serial, uint32_t clkHZ, uint8_t verbose = 0);
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
	 * \brief return internal buffer size (in byte).
	 * \return _buffer_size -3 for mpsse cmd + size, -1 for potential SEND_IMMEDIATE
	 */
	int get_buffer_size() override { return _buffer_size-3; }

	bool isFull() override { return false;}

	int flush() override;

 private:
	void init_internal(const FTDIpp_MPSSE::mpsse_bit_config &cable);
	/*!
	 * \brief configure read and write edge (pos or neg), with freq < 15MHz
	 *        neg is used for write and pos to sample. with freq >= 15MHz
	 *        pos is used for write and neg to sample
	 */
	void config_edge();
	bool _ch552WA; /* avoid errors with SiPeed tangNano */
	uint8_t _write_mode; /**< write edge configuration */
	uint8_t _read_mode; /**< read edge configuration */
};
#endif
