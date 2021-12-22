// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (C) 2019 Gwenhael Goavec-Merou <gwenhael.goavec-merou@trabucayre.com>
 */

#ifndef GOWIN_HPP_
#define GOWIN_HPP_

#include <stdint.h>
#include <iostream>
#include <string>
#include <vector>

#include "device.hpp"
#include "configBitstreamParser.hpp"
#include "jtag.hpp"
#include "spiInterface.hpp"

class Gowin: public Device, SPIInterface {
	public:
		Gowin(Jtag *jtag, std::string filename, const std::string &file_type,
				Device::prog_type_t prg_type, bool external_flash,
				bool verify, int8_t verbose);
		~Gowin();
		int idCode() override;
		void reset() override;
		void program(unsigned int offset, bool unprotect_flash) override;
		void programFlash();

		/* spi interface */
		virtual bool protect_flash(uint32_t len) override {
			(void) len;
			printError("protect flash not supported"); return false;}
		virtual bool unprotect_flash() override {
			printError("unprotect flash not supported"); return false;}
		int spi_put(uint8_t cmd, uint8_t *tx, uint8_t *rx,
			uint32_t len) override;
		int spi_put(uint8_t *tx, uint8_t *rx, uint32_t len) override;
		int spi_wait(uint8_t cmd, uint8_t mask, uint8_t cond,
			uint32_t timeout, bool verbose) override;

	private:
		bool wr_rd(uint8_t cmd, uint8_t *tx, int tx_len,
				uint8_t *rx, int rx_len, bool verbose = false);
		bool EnableCfg();
		bool DisableCfg();
		bool pollFlag(uint32_t mask, uint32_t value);
		bool eraseSRAM();
		bool eraseFLASH();
		bool flashSRAM(uint8_t *data, int length);
		bool flashFLASH(uint8_t *data, int length);
		void displayReadReg(uint32_t dev);
		uint32_t readStatusReg();
		uint32_t readUserCode();
		ConfigBitstreamParser *_fs;
		bool is_gw1n1;
		bool _external_flash; /**< select between int or ext flash */
		uint8_t _spi_sck;     /**< clk signal offset in bscan SPI */
		uint8_t _spi_cs;      /**< cs signal offset in bscan SPI */
		uint8_t _spi_di;      /**< di signal (mosi) offset in bscan SPI */
		uint8_t _spi_do;      /**< do signal (miso) offset in bscan SPI */
		uint8_t _spi_msk;     /** default spi msk with only do out */
};
#endif  // GOWIN_HPP_
