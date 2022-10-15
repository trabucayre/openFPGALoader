// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (C) 2021 Gwenhael Goavec-Merou <gwenhael.goavec-merou@trabucayre.com>
 */

#ifndef SRC_CH552_JTAG_HPP_
#define SRC_CH552_JTAG_HPP_
#include <ftdi.h>
#include <iostream>
#include <string>
#include <vector>

#include "cable.hpp"
#include "jtagInterface.hpp"
#include "ftdipp_mpsse.hpp"

/*!
 * \file ch552_jtag.hpp
 * \class ch552_jtag
 * \brief ch552_jtag firmware (ft2232C clone) implementation
 * \author Gwenhael Goavec-Merou
 */

class CH552_jtag : public JtagInterface, private FTDIpp_MPSSE {
 public:
	CH552_jtag(const cable_t &cable, std::string dev,
		const std::string &serial, uint32_t clkHZ, uint8_t verbose = false);
	virtual ~CH552_jtag();

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
	void init_internal(const mpsse_bit_config &cable);
	uint32_t _to_read; /*!< amount of byte to read */
};
#endif  // SRC_CH552_JTAG_HPP_
