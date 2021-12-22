// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (C) 2020 Gwenhael Goavec-Merou <gwenhael.goavec-merou@trabucayre.com>
 */

#ifndef SRC_EFINIX_HPP_
#define SRC_EFINIX_HPP_

#include <string>

#include "device.hpp"
#include "ftdiJtagMPSSE.hpp"
#include "ftdispi.hpp"
#include "jtag.hpp"

class Efinix: public Device {
	public:
		Efinix(FtdiSpi *spi, const std::string &filename,
			const std::string &file_type,
			uint16_t rst_pin, uint16_t done_pin, uint16_t oe_pin,
			bool verify, int8_t verbose);
		Efinix(Jtag* jtag, const std::string &filename,
			const std::string &file_type,
			const std::string &board_name,
			bool verify, int8_t verbose);
		~Efinix();

		void program(unsigned int offset, bool unprotect_flash) override;
		bool dumpFlash(const std::string &filename,
			uint32_t base_addr, uint32_t len);
		virtual bool protect_flash(uint32_t len) override {
			(void) len;
			printError("protect flash not supported"); return false;}
		virtual bool unprotect_flash() override {
			printError("unprotect flash not supported"); return false;}
		/* not supported in SPI Active mode */
		int idCode() override {return 0;}
		void reset() override;

	private:
		void programSPI(unsigned int offset, uint8_t *data, int length,
				bool unprotect_flash);
		void programJTAG(uint8_t *data, int length);
		FtdiSpi *_spi;
		FtdiJtagMPSSE *_ftdi_jtag;
		uint16_t _rst_pin;
		uint16_t _done_pin;
		uint16_t _cs_pin;
		uint16_t _oe_pin;
};

#endif  // SRC_EFINIX_HPP_
