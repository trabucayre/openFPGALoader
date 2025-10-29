// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (C) 2021 Gwenhael Goavec-Merou <gwenhael.goavec-merou@trabucayre.com>
 * Copyright (C) 2021 Cologne Chip AG <support@colognechip.com>
 */

#ifndef SRC_COLOGNECHIP_HPP_
#define SRC_COLOGNECHIP_HPP_

#include <unistd.h>
#include <regex>
#include <string>

#include "device.hpp"
#ifdef ENABLE_DIRTYJTAG
#include "dirtyJtag.hpp"
#endif
#include "jtag.hpp"
#include "ftdispi.hpp"
#include "ftdiJtagMPSSE.hpp"
#include "rawParser.hpp"
#include "colognechipCfgParser.hpp"
#include "spiFlash.hpp"
#include "progressBar.hpp"

class CologneChip: public Device, SPIInterface {
	public:
		CologneChip(FtdiSpi *spi, const std::string &filename,
			const std::string &file_type, Device::prog_type_t prg_type,
			uint16_t rstn_pin, uint16_t done_pin, uint16_t fail_pin, uint16_t oen_pin,
			bool verify, int8_t verbose);
		CologneChip(Jtag* jtag, const std::string &filename,
			const std::string &file_type, Device::prog_type_t prg_type,
			const std::string &board_name, const std::string &cable_name,
			bool verify, int8_t verbose);
		~CologneChip() {}

		bool cfgDone();
		void waitCfgDone();
		bool detect_flash() override;
		bool dumpFlash(uint32_t base_addr, uint32_t len) override;
		virtual bool protect_flash(uint32_t len) override {
			(void) len;
			printError("protect flash not supported"); return false;}
		virtual bool unprotect_flash() override {
			printError("unprotect flash not supported"); return false;}
		bool set_quad_bit(bool set_quad) override;
		bool bulk_erase_flash() override;
		void program(unsigned int offset, bool unprotect_flash) override;

		uint32_t idCode() override {return 0;}
		void reset() override;

	protected:
		bool prepare_flash_access() override;
		bool post_flash_access() override;

	private:
		void programSPI_sram(const uint8_t *data, int length);
		void programSPI_flash(unsigned int offset, const uint8_t *data, int length,
				bool unprotect_flash);
		void programJTAG_sram(const uint8_t *data, int length);
		void programJTAG_flash(unsigned int offset, const uint8_t *data, int length,
				bool unprotect_flash);

		/* spi interface via jtag */
		int spi_put(uint8_t cmd, const uint8_t *tx, uint8_t *rx,
					uint32_t len) override;
		int spi_put(const uint8_t *tx, uint8_t *rx, uint32_t len) override;
		int spi_wait(uint8_t cmd, uint8_t mask, uint8_t cond, uint32_t timeout,
					 bool verbose=false) override;

		FtdiSpi *_spi = NULL;
		FtdiJtagMPSSE *_ftdi_jtag = NULL;
#ifdef ENABLE_DIRTYJTAG
		DirtyJtag *_dirtyjtag = NULL;
#endif
		uint16_t _rstn_pin;
		uint16_t _done_pin;
		uint16_t _fail_pin;
		uint16_t _oen_pin;
};

#endif // SRC_COLOGNECHIP_HPP_
