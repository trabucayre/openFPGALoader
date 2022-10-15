// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (C) 2020 Gwenhael Goavec-Merou <gwenhael.goavec-merou@trabucayre.com>
 */

#ifndef FTDIJTAGBITBANG_H
#define FTDIJTAGBITBANG_H
#include <ftdi.h>
#include <iostream>
#include <string>
#include <vector>

#include "board.hpp"
#include "jtagInterface.hpp"
#include "ftdipp_mpsse.hpp"

/*!
 * \file FtdiJtagBitBang.hpp
 * \class FtdiJtagBitBang
 * \brief concrete class between jtag implementation and FTDI capable bitbang mode
 * \author Gwenhael Goavec-Merou
 */

class FtdiJtagBitBang : public JtagInterface, private FTDIpp_MPSSE {
 public:
	FtdiJtagBitBang(const cable_t &cable,
		const jtag_pins_conf_t *pin_conf, std::string dev, const std::string &serial,
		uint32_t clkHZ, uint8_t verbose = 0);
	virtual ~FtdiJtagBitBang();

	int setClkFreq(uint32_t clkHZ) override;

	/* TMS */
	int writeTMS(uint8_t *tms, uint32_t len, bool flush_buffer) override;

	/* TDI */
	int writeTDI(uint8_t *tx, uint8_t *rx, uint32_t len, bool end) override;
	int toggleClk(uint8_t tms, uint8_t tdo, uint32_t clk_len) override;

	/*!
	 * \brief return internal buffer size (in byte).
	 * \return _buffer_size divided by 2 (two byte for clk) and divided by 8 (one
	 * state == one byte)
	 */
	int get_buffer_size() override { return _buffer_size/8/2; }

	bool isFull() override { return _num == 8*get_buffer_size();}

	int flush() override;

 private:
	int write(uint8_t *tdo, int nb_bit);
	int setBitmode(uint8_t mode);

	uint8_t _bitmode;
	uint8_t _tck_pin; /*!< tck pin: 1 << pin id */
	uint8_t _tms_pin; /*!< tms pin: 1 << pin id */
	uint8_t _tdo_pin; /*!< tdo pin: 1 << pin id */
	uint8_t _tdi_pin; /*!< tdi pin: 1 << pin id */
	uint8_t _curr_tms;
	int _rx_size;
};
#endif
