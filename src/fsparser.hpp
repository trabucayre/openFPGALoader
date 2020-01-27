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

#include "configBitstreamParser.hpp"

class FsParser: public ConfigBitstreamParser {
	public:
		FsParser(const std::string filename, bool reverseByte, bool verbose);
		~FsParser();
		int parse();

		uint16_t checksum() {return _checksum;}

	private:
		int parseHeader();
		bool _reverseByte;
		std::string _toolVersion;
		std::string _partNumber;
		std::string _devicePackage;
		bool _backgroundProgramming;
		uint16_t _checksum;
		bool _crcCheck;
		bool _compress;
		bool _encryption;
		bool _securityBit;
		bool _jtagAsRegularIO;
		std::string _date;
};

#endif  // FSPARSER_HPP_
