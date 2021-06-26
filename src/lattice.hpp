// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (C) 2019 Gwenhael Goavec-Merou <gwenhael.goavec-merou@trabucayre.com>
 */

#ifndef LATTICE_HPP_
#define LATTICE_HPP_

#include <stdint.h>
#include <iostream>
#include <string>
#include <vector>

#include "jtag.hpp"
#include "device.hpp"
#include "jedParser.hpp"
#include "latticeBitParser.hpp"
#include "spiInterface.hpp"

class Lattice: public Device, SPIInterface {
	public:
		Lattice(Jtag *jtag, std::string filename, const std::string &file_type,
			Device::prog_type_t prg_type, bool verify,
			int8_t verbose);
		int idCode() override;
		int userCode();
		void reset() override {}
		void program(unsigned int offset) override;
		bool program_mem();
		bool program_flash(unsigned int offset);
		bool Verify(std::vector<std::string> data, bool unlock = false);
		bool dumpFlash(const std::string &filename,
			uint32_t base_addr, uint32_t len) override;

		/* spi interface */
		int spi_put(uint8_t cmd, uint8_t *tx, uint8_t *rx,
		uint32_t len) override;
		int spi_put(uint8_t *tx, uint8_t *rx, uint32_t len) override;
		int spi_wait(uint8_t cmd, uint8_t mask, uint8_t cond,
				uint32_t timeout, bool verbose=false) override;

	private:
		enum lattice_family_t {
			MACHXO2_FAMILY = 0,
			MACHXO3_FAMILY = 1,
			MACHXO3D_FAMILY = 2,
			ECP5_FAMILY = 3,
			NEXUS_FAMILY = 4,
			UNKNOWN_FAMILY = 999
		};

		lattice_family_t _fpga_family;

		bool program_intFlash();
		bool program_extFlash(unsigned int offset);
		bool wr_rd(uint8_t cmd, uint8_t *tx, int tx_len,
				uint8_t *rx, int rx_len, bool verbose = false);
		void unlock();
		bool EnableISC(uint8_t flash_mode);
		bool DisableISC();
		bool EnableCfgIf();
		bool DisableCfg();
		bool pollBusyFlag(bool verbose = false);
		bool flashEraseAll();
		bool flashErase(uint8_t mask);
		bool flashProg(uint32_t start_addr, const std::string &name,
				std::vector<std::string> data);
		bool checkStatus(uint32_t val, uint32_t mask);
		void displayReadReg(uint32_t dev);
		uint32_t readStatusReg();
		uint64_t readFeaturesRow();
		bool writeFeaturesRow(uint64_t features, bool verify);
		uint16_t readFeabits();
		bool writeFeabits(uint16_t feabits, bool verify);
		bool writeProgramDone();
		bool loadConfiguration();

		/* test */
		bool checkID();
};
#endif  // LATTICE_HPP_
