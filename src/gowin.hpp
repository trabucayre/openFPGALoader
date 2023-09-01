// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (C) 2019 Gwenhael Goavec-Merou <gwenhael.goavec-merou@trabucayre.com>
 */

#ifndef SRC_GOWIN_HPP_
#define SRC_GOWIN_HPP_

#include <stdint.h>
#include <iostream>
#include <string>
#include <vector>

#include "configBitstreamParser.hpp"
#include "device.hpp"
#include "jtag.hpp"
#include "spiInterface.hpp"

class Gowin: public Device, SPIInterface {
	public:
		Gowin(Jtag *jtag, std::string filename, const std::string &file_type,
				std::string mcufw, Device::prog_type_t prg_type,
				bool external_flash, bool verify, int8_t verbose);
		~Gowin();
		uint32_t idCode() override;
		void reset() override;
		void program(unsigned int offset, bool unprotect_flash) override;
		bool connectJtagToMCU() override;

		/* spi interface */
		bool protect_flash(uint32_t len) override {
			(void) len;
			printError("protect flash not supported"); return false;}
		bool unprotect_flash() override {
			printError("unprotect flash not supported"); return false;}
		bool bulk_erase_flash() override {
			printError("bulk erase flash not supported"); return false;}
		int spi_put(uint8_t cmd, const uint8_t *tx, uint8_t *rx,
			uint32_t len) override;
		int spi_put(const uint8_t *tx, uint8_t *rx, uint32_t len) override;
		int spi_wait(uint8_t cmd, uint8_t mask, uint8_t cond,
			uint32_t timeout, bool verbose) override;

	private:
		bool send_command(uint8_t cmd);
		void spi_gowin_write(const uint8_t *wr, uint8_t *rd, unsigned len);
		uint32_t readReg32(uint8_t cmd);
		void sendClkUs(unsigned us);
		bool enableCfg();
		bool disableCfg();
		bool pollFlag(uint32_t mask, uint32_t value);
		bool eraseSRAM();
		bool eraseFLASH();
		void programFlash();
		void programExtFlash(unsigned int offset, bool unprotect_flash);
		void programSRAM();
		bool writeSRAM(const uint8_t *data, int length);
		bool writeFLASH(uint32_t page, const uint8_t *data, int length);
		void displayReadReg(const char *, uint32_t dev);
		uint32_t readStatusReg();
		uint32_t readUserCode();
		/*!
		 * \brief compare usercode register with fs checksum and/or
		 *        .fs usercode field
		 */
		void checkCRC();
		ConfigBitstreamParser *_fs;
		bool is_gw1n1;
		bool is_gw2a;
		bool is_gw1n4;
		bool is_gw5a;
		bool skip_checksum;   /**< bypass checksum verification (GW2A) */
		bool _external_flash; /**< select between int or ext flash */
		uint8_t _spi_sck;     /**< clk signal offset in bscan SPI */
		uint8_t _spi_cs;      /**< cs signal offset in bscan SPI */
		uint8_t _spi_di;      /**< di signal (mosi) offset in bscan SPI */
		uint8_t _spi_do;      /**< do signal (miso) offset in bscan SPI */
		uint8_t _spi_msk;     /** default spi msk with only do out */
		ConfigBitstreamParser *_mcufw;
};
#endif  // SRC_GOWIN_HPP_
