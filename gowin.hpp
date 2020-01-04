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

#ifndef GOWIN_HPP_
#define GOWIN_HPP_

#include <stdint.h>
#include <iostream>
#include <string>
#include <vector>

#include "device.hpp"
#include "fsparser.hpp"
#include "ftdijtag.hpp"
#include "jedParser.hpp"

class Gowin: public Device {
	public:
		Gowin(FtdiJtag *jtag, std::string filename, bool flash_wr, bool sram_wr,
				bool verbose);
		~Gowin();
		int idCode() override;
		void reset() override {}
		void program(unsigned int offset) override;
		void programFlash();

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
		FsParser *_fs;
};
#endif  // GOWIN_HPP_
