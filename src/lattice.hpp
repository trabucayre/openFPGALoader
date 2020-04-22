/*
 * Copyright (C) 2019 Gwenhael Goavec-Merou <gwenhael.goavec-merou@trabucayre.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
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
		Lattice(Jtag *jtag, std::string filename, bool flash_wr, bool sram_wr,
			bool verbose);
		int idCode() override;
		int userCode();
		void reset() override {}
		void program(unsigned int offset) override;
		bool program_mem();
		bool program_flash(unsigned int offset);
		bool Verify(JedParser &_jed, bool unlock = false);

		/* spi interface */
		int spi_put(uint8_t cmd, uint8_t *tx, uint8_t *rx,
		uint16_t len) override;
		int spi_put(uint8_t *tx, uint8_t *rx, uint16_t len) override;
		int spi_wait(uint8_t cmd, uint8_t mask, uint8_t cond,
				uint32_t timeout, bool verbose=false) override;

	private:
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
		bool flashProg(uint32_t start_addr, std::vector<std::string> data);
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
