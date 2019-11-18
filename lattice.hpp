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

#include "ftdijtag.hpp"
#include "device.hpp"
#include "jedParser.hpp"

class Lattice: public Device {
	public:
		Lattice(FtdiJtag *jtag, std::string filename);
		int idCode() override;
		void reset() override {}
		void program(unsigned int offset) override;
		bool Verify(JedParser &_jed, bool unlock = false);

	private:
		bool wr_rd(uint8_t cmd, uint8_t *tx, int tx_len,
				uint8_t *rx, int rx_len, bool verbose = false);
		void unlock();
		void displayReadReg(uint32_t dev);
		bool EnableISC(uint8_t flash_mode);
		bool DisableISC();
		bool EnableCfgIf();
		bool DisableCfg();
		bool pollBusyFlag(bool verbose = false);
		bool flashEraseAll();
		bool flashErase(uint8_t mask);
		bool flashProg(uint32_t start_addr, std::vector<std::string> data);
		uint32_t readStatusReg();
		void readFeaturesRow();
		bool writeFeaturesRow(uint64_t features);
		void readFeabits();
		bool writeFeabits(uint16_t feabits);
		bool writeProgramDone();
		bool loadConfiguration();

		/* test */
		bool checkID();
};
#endif  // LATTICE_HPP_
