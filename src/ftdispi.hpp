// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (C) 2019 Gwenhael Goavec-Merou <gwenhael.goavec-merou@trabucayre.com>
 */

#ifndef SRC_FTDISPI_HPP_
#define SRC_FTDISPI_HPP_

#include <ftdi.h>
#include <iostream>
#include <vector>

#include "board.hpp"
#include "ftdipp_mpsse.hpp"
#include "spiInterface.hpp"

class FtdiSpi : public FTDIpp_MPSSE, SPIInterface {
 public:
	enum SPI_endianness {
		SPI_MSB_FIRST = 0,
		SPI_LSB_FIRST = 1
	};

	enum SPI_CS_mode {
		SPI_CS_AUTO   = 0,
		SPI_CS_MANUAL = 1
	};

	FtdiSpi(int vid, int pid, unsigned char interface, uint32_t clkHZ,
		bool verbose);
	FtdiSpi(const FTDIpp_MPSSE::mpsse_bit_config &conf,
		spi_pins_conf_t spi_config, uint32_t clkHZ,
		bool verbose);
	~FtdiSpi();

	void setMode(uint8_t mode);
	void setEndianness(unsigned char endian) {
		_endian =(endian == SPI_MSB_FIRST) ? 0 : MPSSE_LSB;
	}

	/* CS handling */
	void setCSmode(uint8_t cs_mode) {_cs_mode = cs_mode;}
	bool confCs(char stat);
	bool setCs();
	bool clearCs();

	int ft2232_spi_wr_then_rd(const uint8_t *tx_data, uint32_t tx_len,
							uint8_t *rx_data, uint32_t rx_len);
	int ft2232_spi_wr_and_rd(uint32_t writecnt,
							const uint8_t *writearr, uint8_t *readarr);

	/* spi interface */
	int spi_put(uint8_t cmd, uint8_t *tx, uint8_t *rx,
			uint32_t len) override;
	int spi_put(uint8_t *tx, uint8_t *rx, uint32_t len) override;
	int spi_wait(uint8_t cmd, uint8_t mask, uint8_t cond,
			uint32_t timeout, bool verbose=false) override;

 protected:
	/*!
	 * \brief move device to SPI access
	 */
	virtual bool prepare_flash_access() override {return true;}
	/*!
	 * \brief end of device to SPI access
	 */
	virtual bool post_flash_access() override {return true;}

 private:
	uint8_t _cs;
	uint16_t _cs_bits;
	uint8_t _clk;
	uint8_t _clk_idle;
	uint8_t _wr_mode;
	uint8_t _rd_mode;
	unsigned char _endian;
	uint8_t _cs_mode;
	uint16_t _holdn;
	uint16_t _wpn;
};

#endif  // SRC_FTDISPI_HPP_
