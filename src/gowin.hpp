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
#include "jtagInterface.hpp"
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
		bool detect_flash() override {
			if (is_gw5a)
				return SPIInterface::detect_flash();
			printError("protect flash not supported"); return false;}
		bool protect_flash(uint32_t len) override {
			(void) len;
			printError("protect flash not supported"); return false;}
		bool unprotect_flash() override {
			if (is_gw5a)
				return SPIInterface::unprotect_flash();
			printError("unprotect flash not supported"); return false;}
		bool bulk_erase_flash() override {
			if (is_gw5a)
				return SPIInterface::bulk_erase_flash();
			printError("bulk erase flash not supported"); return false;}
		bool dumpFlash(uint32_t base_addr, uint32_t len) override;
		int spi_put(uint8_t cmd, const uint8_t *tx, uint8_t *rx,
			uint32_t len) override;
		int spi_put(const uint8_t *tx, uint8_t *rx, uint32_t len) override;
		int spi_wait(uint8_t cmd, uint8_t mask, uint8_t cond,
			uint32_t timeout, bool verbose) override;

		/* ---------------- */
		/* Arora V specific */
		/* ---------------- */

		/*!
		 * \brief Send cmd, followed by (optional) tx sequence and fill
		 * rx if not null
		 * \param[in] cmd: SPI command
		 * \param[in] rx: Byte sequence to write (may be null)
		 * \param[out] tx: Byte buffer when read is requested
		 * \param[in] len: number of Byte to read/write (0 when no read/write)
		 * \return 0 on success, -1 otherwise
		 */
		int spi_put_gw5a(const uint8_t cmd, const uint8_t *tx, uint8_t *rx,
			uint32_t len);
		/*!
		 * \brief poll on cmd register until timeout or SPI reg content match
		 * cond with mask
		 * \param[in] cmd: SPI command
		 * \param[in] mask: mask to apply on SPI rx
		 * \param[in] cond: SPI rx value value
		 * \param[in] timeout: number of try before fail
		 * \param[in] verbose: display try
		 * \return 0 on success, -1 otherwise
		 */
		int spi_wait_gw5a(uint8_t cmd, uint8_t mask, uint8_t cond,
			uint32_t timeout, bool verbose);

	protected:
	/*!
	 * \brief prepare SPI flash access
	 */
	bool prepare_flash_access() override;
	/*!
	 * \brief end of SPI flash access
	 */
	bool post_flash_access() override;

	private:
		bool detectFamily();
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

		/* ---------------- */
		/* Arora V specific */
		/* ---------------- */
		/*!
		 * \brief Send the sequence to pass GW5A to SPI mode.
		 * \return true on success, false otherwise
		 */
		bool gw5a_disable_spi();
		/*!
		 * \brief Send the sequence to disable SPI mode for GW5A.
		 * \return true on success, false otherwise
		 */
		bool gw5a_enable_spi();

		ConfigBitstreamParser *_fs;
		uint32_t _idcode;
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
		JtagInterface::tck_edge_t _prev_rd_edge; /**< default probe rd edge cfg */
		JtagInterface::tck_edge_t _prev_wr_edge; /**< default probe wr edge cfg */
};
#endif  // SRC_GOWIN_HPP_
