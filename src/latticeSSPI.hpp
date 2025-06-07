// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (C) 2025 Gwenhael Goavec-Merou <gwenhael.goavec-merou@trabucayre.com>
 */

#ifndef SRC_LATTICESSPI_HPP_
#define SRC_LATTICESSPI_HPP_

#include <string>

#include "device.hpp"
#include "ftdispi.hpp"

class LatticeSSPI: public Device {
	public:
		LatticeSSPI(FtdiSpi *spi, const std::string &filename,
			const std::string &file_type, const int8_t verbose);

		uint32_t idCode() override;
		int userCode();
		void reset() override {}
		void program(unsigned int offset, bool unprotect_flash) override;

		bool protect_flash(uint32_t len) override { return false; }
		bool unprotect_flash() override { return false; }
		bool bulk_erase_flash() override { return false; }

	private:
		bool program_mem();
		bool pollBusyFlag(bool verbose=false);
		uint32_t readStatusReg();
		void displayReadReg(uint32_t dev);
		bool EnableISC(uint8_t flash_mode);
		bool DisableISC();
		bool flashErase(uint32_t mask);
		bool checkStatus(uint32_t val, uint32_t mask) {
			return ((readStatusReg() & mask) == val) ? true : false;
		}

		bool cmd_class_a(uint8_t cmd, uint8_t *rx, uint32_t len);
		bool cmd_class_b(uint8_t cmd, uint8_t *tx, uint32_t len);
		bool cmd_class_c(uint8_t cmd);
		uint32_t char_array_to_word(uint8_t *in);
		FtdiSpi *_spi;
};
#endif /* SRC_LATTICESSPI_HPP_ */
