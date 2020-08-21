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
#ifndef SRC_ANLOGICBITPARSER_HPP_
#define SRC_ANLOGICBITPARSER_HPP_

#include <iostream>
#include <fstream>
#include <map>
#include <string>

#include "configBitstreamParser.hpp"

/*
 * Minimal implementation for anlogic bitstream
 * only parse header part and store data in a buffer
 */
class AnlogicBitParser: public ConfigBitstreamParser {
	public:
		AnlogicBitParser(const std::string &filename, bool verbose = false);
		~AnlogicBitParser();
		int parse() override;
		void displayHeader();

	private:
		int parseHeader();
};

#endif  // SRC_ANLOGICBITPARSER_HPP_
