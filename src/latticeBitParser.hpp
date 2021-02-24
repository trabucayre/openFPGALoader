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
#ifndef LATTICEBITPARSER_HPP_
#define LATTICEBITPARSER_HPP_

#include <iostream>
#include <fstream>
#include <map>
#include <string>

#include "configBitstreamParser.hpp"

class LatticeBitParser: public ConfigBitstreamParser {
	public:
		LatticeBitParser(const std::string &filename, bool verbose = false);
		~LatticeBitParser();
		int parse() override;

	private:
		int parseHeader();
		size_t _endHeader;
};

#endif  // LATTICEBITPARSER_HPP_
