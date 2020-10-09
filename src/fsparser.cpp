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

	/* we know each data size finished with 6 x 0xff
	 * and an optional checksum
	 * */
	int addr_length;
	/* GW1N-6 and GW1N(R)-9 are address length not multiple of byte */
	int padding = 0;

	/* Fs file format is MSB first
	 * so if reverseByte = false bit 0 -> 7, 1 -> 6,
	 * if true 0 -> 0, 1 -> 1
	 */

	uint32_t idcode = 0;

	for (auto &&line: vectTmp) {
		/* store header for futher informations */
		string data_line;
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
			data_line += (_reverseByte)? reverseByte(data): data;
		}
		if (line.size() < max) {
			if ((0x7F & data_line[0]) == 0x06) {
				string str = data_line.substr(data_line.size()-4, 4);
				for (auto it = str.begin(); it != str.end(); it++)
					idcode = idcode << 8 | (uint8_t)(*it);
			}
		}
	}
	_bit_length = (int)_bit_data.size() * 8;

	if (idcode == 0)
		printf("IDCODE not found\n");

	/* use idcode to determine Count of Address */
	unsigned nb_line = 0;
	switch (idcode) {
		case 0x0900281b: /* GW1N-1    */
		case 0x0900381b: /* GW1N-1S   */
		case 0x0100681b: /* GW1NZ-1   */
			nb_line = 274;
			addr_length = 1216;
			break;
		case 0x0100181b: /* GW1N-2    */
		case 0x1100181b: /* GW1N-2B   */
		case 0x0300081b: /* GW1NS-2   */
		case 0x0300181b: /* GW1NSx-2C */
		case 0x1100381b: /* GW1N-4B   */
			nb_line = 494;
			addr_length = 2296;
			break;
		case 0x0100481b: /* GW1N-6    */
		case 0x1100581b: /* GW1N-9    */
			nb_line = 712;
			addr_length = 2836;
			padding = 4;
			break;
		case 0x0000081b: /* GW2A-18   */
			nb_line = 1342;
			addr_length = 3376;
			break;
		case 0x0000281b: /* GW2A-55    */
			nb_line = 2038;
			addr_length = 5536;
			break;
		default:
			nb_line = 0;
	}

	for (auto &&line: vectTmp) {
		/* store bit for checksum */
		if ((int)line.size() == max)
			tmp += line.substr(padding, addr_length);
	}

	/* checksum */
	uint32_t sum = 0;
	uint32_t max_pos = (idcode == 0) ? tmp.size() : addr_length * nb_line;

	for (int pos = 0; pos < max_pos; pos+=16) {
		int16_t data16 = 0;
		for (int offset = 0; offset < 16; offset ++) {
			uint16_t val = (tmp[pos+offset] == '1'?1:0);
			data16 |= val << (15-offset);
		}
		sum += data16;
	}

	_checksum = sum;
	printf("checksum 0x%02x\n", _checksum);

	printSuccess("Done");

	return 0;
}
