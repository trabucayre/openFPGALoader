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
#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>

#include <algorithm>
#include <functional>
#include <cctype>
#include <iostream>
#include <locale>

#include "latticeBitParser.hpp"

using namespace std;

LatticeBitParser::LatticeBitParser(const string &filename, bool verbose):
	ConfigBitstreamParser(filename, ConfigBitstreamParser::BIN_MODE, verbose),
	_attribs(), _endHeader(0)
{}

LatticeBitParser::~LatticeBitParser()
{
}

void LatticeBitParser::displayHeader()
{
	cout << "Lattice bitstream header infos" << endl;
	for (auto it = _attribs.begin(); it != _attribs.end(); it++) {
		cout << (*it).first << ": " << (*it).second << endl;
	}
}

int LatticeBitParser::parseHeader()
{
	/* end is wrong detected
	 * 3B is a command AFTER header
	 */

	int ret = 1;
	int currPos = _fd.tellg();
	uint32_t dummy32 = 0, dummyPrev = 0;
	do {
		dummyPrev = dummy32;
		_fd.read(reinterpret_cast<char *>(&dummy32), sizeof(uint32_t));
		if (_fd.eof()) {
			printf("End of file without header end\n");
			return EXIT_FAILURE;
		}
	} while (dummyPrev != 0xBDffffff && dummy32 != 0x3BFFFFB3);
	_endHeader = _fd.tellg();
	/* -8 => 2 * 4 byte for pattern
	 */
	int headerLength = _endHeader - currPos -8;
	_endHeader-=8;  // pattern is included to data to send
	_fd.seekg(currPos, _fd.beg);

	char buffer[headerLength];
	_fd.read(buffer, sizeof(char) * headerLength);

	int i = 0;
	while (i < headerLength) {
		string buff(buffer+i);
		i+= buff.size() + 1;
		int pos = buff.find_first_of(':', 0);
		if (pos == -1)  // useless
			continue;
		string key(buff.substr(0, pos));
		string val(buff.substr(pos+1, buff.size()));
		int startPos = val.find_first_not_of(" ");
		int endPos = val.find_last_not_of(" ")+1;
		_attribs[key] = val.substr(startPos, endPos).c_str();
	}

	return ret;
}

int LatticeBitParser::parse()
{
	uint8_t dummy[2];

	/* bit file start with 0xff00 */
	_fd.read(reinterpret_cast<char*>(&dummy), 2*sizeof(uint8_t));
	if (dummy[0] != 0xff || dummy[1] != 0x00) {
		printf("Wrong File %02x%02x\n", dummy[0], dummy[1]);
		return EXIT_FAILURE;
	}

	/* until 0xFFFFBDB3 0xFFFF */
	parseHeader();

	/* read All data */
	_fd.seekg(_endHeader, _fd.beg);
	char buffer[_file_size];
	int end = _file_size-_endHeader;
	_fd.read(buffer, end);

	for (int i = 0; i < end; i++)
		_bit_data+=(buffer[i]);

	_bit_length = _bit_data.size() * 8;
	return 0;
}
