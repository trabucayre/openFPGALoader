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

#ifndef SPIFLASH_HPP
#define SPIFLASH_HPP

#include "spiInterface.hpp"

class SPIFlash {
	public:
		SPIFlash(SPIInterface *spi, int8_t verbose);
		/* power */
		void power_up();
		void power_down();
		void reset();
		/* protection */
		int write_enable();
		int write_disable();
		int disable_protection();
		/* erase */
		int bulk_erase();
		int sector_erase(int addr);
		int sectors_erase(int base_addr, int len);
		/* write */
		int write_page(int addr, uint8_t *data, int len);
		/* read */
		int read(int base_addr, uint8_t *data, int len);
		/* combo flash + erase */
		int erase_and_prog(int base_addr, uint8_t *data, int len);
		/* display/info */
		uint8_t read_status_reg();
		void read_id();
		uint16_t readNonVolatileCfgReg();
		uint16_t readVolatileCfgReg();
	private:
		SPIInterface *_spi;
		int8_t _verbose;
};

#endif
