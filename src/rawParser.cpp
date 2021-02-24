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

#include <stdexcept>
#include <utility>

#include "configBitstreamParser.hpp"
#include "display.hpp"
#include "rawParser.hpp"

using namespace std;

RawParser::RawParser(const string &filename, bool reverseOrder):
		ConfigBitstreamParser(filename, ConfigBitstreamParser::BIN_MODE,
		false), _reverseOrder(reverseOrder)
{}

int RawParser::parse()
{
	_bit_data.resize(_file_size);
	std::move(_raw_data.begin(), _raw_data.end(), _bit_data.begin());
	_bit_length = _bit_data.size();

	if (_reverseOrder) {
		for (int i = 0; i < _bit_length; i++) {
			_bit_data[i] = reverseByte(_bit_data[i]);
		}
	}

	/* convert size to bit */
	_bit_length *= 8;

	return EXIT_SUCCESS;
}
