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

#ifndef FSPARSER_HPP_
#define FSPARSER_HPP_

#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "configBitstreamParser.hpp"

class FsParser: public ConfigBitstreamParser {
	public:
		FsParser(const std::string &filename, bool reverseByte, bool verbose);
		~FsParser();
		int parse() override;

		uint16_t checksum() {return _checksum;}

	private:
		int parseHeader();
		/**
		 * \brief convert an binary string representation to the corresponding
		 * value
		 *
		 * \param[in] bits: '1' or '0' buffer
		 * \param[in] len: array length (up to 64)
		 * \return converted value
		 */
		uint64_t bitToVal(const char *bits, int len);

		bool _reverseByte; /*!< direct or reverse bit */
		uint16_t _end_header; /*!< last header line */
		uint16_t _checksum; /*!< locally computed data checksum */
		uint8_t _8Zero; /*!< in compress mode, used to replace 8 * 0x00 */
		uint8_t _4Zero; /*!< in compress mode, used to replace 8 * 0x00 */
		uint8_t _2Zero; /*!< in compress mode, used to replace 8 * 0x00 */
		uint32_t _idcode; /*!< device idcode */
		bool _compressed; /*!< compress mode or not */
		std::vector<std::string> _lstRawData; /* cfg + EBR data buffer */
};

#endif  // FSPARSER_HPP_
