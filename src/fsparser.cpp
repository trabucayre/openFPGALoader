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

	if (_verbose) {
		for (auto &&t: _hdr)
			printInfo("x" + t.first + ": " + t.second);
	}

	return ret;
}

int FsParser::parse()
{
	uint8_t data;
	vector<string> vectTmp;
	string buffer, tmp;

	printInfo("Parse " + _filename + ": ", true);

	parseHeader();
	_fd.seekg(0, _fd.beg);
	int max = 0;

	while (1) {
		std::getline(_fd, buffer, '\n');
		if (buffer.size() == 0)
			break;
		if (buffer[0] == '/')
			continue;
		vectTmp.push_back(buffer);
		/* needed to determine data used for checksum */
		if ((int)buffer.size() > max)
			max = (int) buffer.size();
	}

	/* we know each data size finished with checksum (16bits) + 6 x 0xff */
	int addr_length = max - 8 * 8;
	int padding = 0;
	/* GW1N-6 and GW1N(R)-9 are address length not multiple of byte */
	if (addr_length % 16 != 0) {
		padding = 4;
		addr_length -= 4;
	}

	/* Fs file format is MSB first
	 * so if reverseByte = false bit 0 -> 7, 1 -> 6,
	 * if true 0 -> 0, 1 -> 1
	 */

	for (auto &&line: vectTmp) {
		if ((int)line.size() == max)
			tmp += line.substr(padding, addr_length);
		for (int i = 0; i < (int)line.size(); i+=8) {
			data = 0;
			for (int ii = 0; ii < 8; ii++) {
				uint8_t val = (line[i+ii] == '1'?1:0);
				if (_reverseByte)
					data |= val << ii;
				else
					data |= val << (7-ii);
			}
			_bit_data += data;
		}
	}
	_bit_length = (int)_bit_data.size() * 8;
	printf("%02x\n", _checksum);
	uint32_t sum = 0;

	for (int pos = 0; pos < (int)tmp.size(); pos+=16) {
		int16_t data16 = 0;
		for (int offset = 0; offset < 16; offset ++) {
			uint16_t val = (tmp[pos+offset] == '1'?1:0);
			data16 |= val << (15-offset);
		}
		sum += data16;
	}

	_checksum = sum;

	printSuccess("Done");

	return 0;
}
