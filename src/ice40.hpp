/*
 * Copyright (C) 2020 Gwenhael Goavec-Merou <gwenhael.goavec-merou@trabucayre.com>
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

#ifndef SRC_ICE40_HPP_
#define SRC_ICE40_HPP_

#include <string>

#include "device.hpp"
#include "ftdispi.hpp"

class Ice40: public Device {
	public:
		Ice40(FtdiSpi *spi, const std::string &filename,
			const std::string &file_type,
			uint16_t rst_pin, uint16_t done_pin,
			bool verify, int8_t verbose);
		~Ice40();

		void program(unsigned int offset = 0) override;
		/* not supported in SPI Active mode */
		int idCode() override {return 0;}
		void reset() override;

	private:
		FtdiSpi *_spi;
		uint16_t _rst_pin;
		uint16_t _done_pin;
};

#endif  // SRC_ICE40_HPP_
