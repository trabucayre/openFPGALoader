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
#include <string.h>
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
	int currPos = _fd.tellg();
	char tmp[_file_size-currPos];
	char field[256];
	bool foundEndHeader = false;
	uint32_t *d;

	_fd.read(tmp, (_file_size-currPos)*sizeof(char));

	for (int i = 0; i < _file_size-currPos;) {
		if (tmp[i] == 0xff) {
			d = (uint32_t*)(tmp+i);
			if (d[0] != 0xBDffffff && (0xffffff00 & d[1]) != 0x3BFFFF00){
				foundEndHeader = true;
				_endHeader = i + currPos -1;
				break;
			}
			i++;
		} else {
			strcpy(field, tmp+i);
			string buff(field);
			int pos = buff.find_first_of(':', 0);
			if (pos != -1) {
				string key(buff.substr(0, pos));
				string val(buff.substr(pos+1, buff.size()));
				int startPos = val.find_first_not_of(" ");
				int endPos = val.find_last_not_of(" ")+1;
				_attribs[key] = val.substr(startPos, endPos).c_str();
			}
			i+=strlen(field)+1;
		}
	}

	return (foundEndHeader) ? EXIT_SUCCESS : EXIT_FAILURE;
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
	if (parseHeader() == EXIT_FAILURE)
		return EXIT_FAILURE;

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
