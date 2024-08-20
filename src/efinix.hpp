// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (C) 2020-2023 Gwenhael Goavec-Merou <gwenhael.goavec-merou@trabucayre.com>
 */

#ifndef SRC_EFINIX_HPP_
#define SRC_EFINIX_HPP_

#include <string>

#include "device.hpp"
#include "ftdiJtagMPSSE.hpp"
#include "ftdispi.hpp"
#include "jtag.hpp"
#include "spiInterface.hpp"

class Efinix: public Device, SPIInterface {
	public:
		Efinix(FtdiSpi *spi, const std::string &filename,
			const std::string &file_type,
			uint16_t rst_pin, uint16_t done_pin, uint16_t oe_pin,
			bool verify, int8_t verbose);
		Efinix(Jtag* jtag, const std::string &filename,
			const std::string &file_type, Device::prog_type_t prg_type,
			const std::string &board_name, const std::string &device_package,
			const std::string &spiOverJtagPath,
			bool verify, int8_t verbose);
		~Efinix();

		void program(unsigned int offset, bool unprotect_flash) override;
		bool detect_flash() override;
		bool dumpFlash(uint32_t base_addr, uint32_t len) override;
		bool protect_flash(uint32_t len) override {
			(void) len;
			printError("protect flash not supported"); return false;}
		bool unprotect_flash() override {
			printError("unprotect flash not supported"); return false;}
		bool bulk_erase_flash() override {
			printError("bulk erase flash not supported"); return false;}
		/* not supported in SPI Active mode */
		uint32_t idCode() override {return 0;}
		void reset() override;

		/* spi interface */
		int spi_put(uint8_t cmd, const uint8_t *tx, uint8_t *rx,
			uint32_t len) override;
		int spi_put(const uint8_t *tx, uint8_t *rx, uint32_t len) override;
		int spi_wait(uint8_t cmd, uint8_t mask, uint8_t cond,
			uint32_t timeout, bool verbose = false) override;

	private:
		/* list of efinix family devices */
		enum efinix_family_t {
			TITANIUM_FAMILY = 0,
			TRION_FAMILY,
			UNKNOWN_FAMILY  = 999
		};
		void init_common(const Device::prog_type_t &prg_type);
		bool programSPI(unsigned int offset, const uint8_t *data,
				const int length, const bool unprotect_flash);
		bool programJTAG(const uint8_t *data, const int length);
		bool post_flash_access() override;
		bool prepare_flash_access() override;
		FtdiSpi *_spi;
		FtdiJtagMPSSE *_ftdi_jtag;
		uint16_t _rst_pin;
		uint16_t _done_pin;
		uint16_t _cs_pin;
		uint16_t _oe_pin;
		efinix_family_t _fpga_family;
		int _irlen;
		std::string _device_package;
		std::string _spiOverJtagPath;
};

#endif  // SRC_EFINIX_HPP_
