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
	//cout << "parsing " << _file_size << endl;
	char *c = new char[_file_size];
	_fd.read(c, sizeof(char) * _file_size);
	if (!_fd) {
		printError("Error: fail to read " + _filename);
		return EXIT_FAILURE;
	}
	//cout << _bit_data << endl;
	_bit_data.resize(_file_size);
	for (int i = 0; i < _file_size; i++)
		_bit_data[i] = (_reverseOrder) ? reverseByte(c[i]): c[i];
	_bit_length = _bit_data.size() * 8;
	//cout << "file length " << _bit_length << endl;
	delete(c);
	return EXIT_SUCCESS;
}
