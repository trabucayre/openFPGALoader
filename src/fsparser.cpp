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

#include <iostream>
#include <vector>
#include <cstdio>

#include "fsparser.hpp"
#include "display.hpp"

using namespace std;

FsParser::FsParser(string filename, bool reverseByte, bool verbose):
			ConfigBitstreamParser(filename, ConfigBitstreamParser::ASCII_MODE,
			verbose), _reverseByte(reverseByte),  _checksum(0)
{
}
FsParser::~FsParser()
{
}

int FsParser::parseHeader()
{
	int ret = 1;
	string buffer;

	while (1){
		std::getline(_fd, buffer, '\n');
		if (buffer[0] != '/')
			break;
		buffer = buffer.substr(2);
		size_t pos = buffer.find(':');
		if (pos == string::npos)
			continue;
		string v1, v2;
		v1 = buffer.substr(0, pos);
		if (pos+2 == buffer.size())
			v2 = "None";
		else
			v2 = buffer.substr(pos+2) + '\0';  // ':' + ' '

		_hdr[v1] = v2;
	}

	if (_hdr.find("CheckSum") != _hdr.end())
		sscanf(_hdr["CheckSum"].c_str(), "0x%04hx", &_checksum);

	if (_verbose) {
		for (auto &&t: _hdr)
			printInfo("x" + t.first + ": " + t.second);
	}

	return ret;
}

int FsParser::parse()
{
	uint8_t data;
	string buffer, tmp;

	printInfo("Parse " + _filename + ": ", true);

	parseHeader();
	_fd.seekg(0, _fd.beg);

	while (1) {
		std::getline(_fd, buffer, '\n');
		if (buffer.size() == 0)
			break;
		if (buffer[0] == '/')
			continue;
		tmp += buffer;
	}

	_bit_length = tmp.size();

	/* Fs file format is MSB first
	 * so if reverseByte = false bit 0 -> 7, 1 -> 6,
	 * if true 0 -> 0, 1 -> 1
	 */

	for (int i = 0; i < _bit_length; i+=8) {
		data = 0;
		for (int ii = 0; ii < 8; ii++) {
			uint8_t val = (tmp[i+ii] == '1'?1:0);
			if (_reverseByte)
				data |= val << ii;
			else
				data |= val << (7-ii);
		}
		_bit_data += data;
	}

	printSuccess("Done");

	return 0;
}
