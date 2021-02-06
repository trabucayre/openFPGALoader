/*
 * Copyright (C) 2020 Gwenhael Goavec-Merou <gwenhael.goavec-merou@trabucayre.com>
 * Based on Miodrag Milanovic https://github.com/mmicko/prjtang/
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

#include <algorithm>
#include <functional>
#include <cctype>
#include <iostream>
#include <sstream>
#include <locale>
#include <vector>

#include "anlogicBitParser.hpp"
#include "display.hpp"

using namespace std;

AnlogicBitParser::AnlogicBitParser(const string &filename, bool reverseOrder,
	bool verbose):
	ConfigBitstreamParser(filename, ConfigBitstreamParser::BIN_MODE, verbose),
	_reverseOrder(reverseOrder)
{}

AnlogicBitParser::~AnlogicBitParser()
{
}

void AnlogicBitParser::displayHeader()
{
	cout << "Anlogic bitstream header infos" << endl;
	for (auto it = _hdr.begin(); it != _hdr.end(); it++) {
		cout << (*it).first << ": " << (*it).second << endl;
	}
}

/* read all ascii lines starting with '#'
 * stop when an empty line is found
 */
int AnlogicBitParser::parseHeader()
{
	int ret = 0;
	printInfo("parseHeader");

	string buffer;
	istringstream lineStream(_raw_data);

	while (std::getline(lineStream, buffer, '\n')) {
		ret += buffer.size() + 1;

		if (buffer.empty()) {
			printInfo("header end");
			break;
		}

		if (buffer[0] != '#') {
			printError("header must start with #\n");
			return EXIT_FAILURE;
		}

		string content = buffer.substr(2); // drop '# '
		size_t pos = content.find(':');
		if (pos == string::npos) {
			_hdr["tool"] = content;
		} else {
			string entry = content.substr(0, pos);
			string val = content.substr(pos+2);
			_hdr[entry] = val;
		}
	}

	if (_raw_data[ret] != 0x00) {
		printError("Header must end with 0x00 (binary) bit");
		ret = -1;
	}

	return ret;
}

int AnlogicBitParser::parse()
{
	int end_header = 0;
	/* fill raw buffer with file content */
	_fd.read((char *)&_raw_data[0], sizeof(char) * _file_size);
	if (_fd.gcount() != _file_size) {
		printError("Error: fails to read full file content");
		return EXIT_FAILURE;
	}

	/* parse header */
	if ((end_header = parseHeader()) == -1)
		return EXIT_FAILURE;

	unsigned int pos = end_header;
	std::vector<std::vector<uint8_t>> blocks;
	do {
		uint16_t len = (((uint16_t)_raw_data[pos]) << 8) |
			(0xff & (uint16_t)_raw_data[pos + 1]);

		pos += 2;
		if ((len & 7) != 0) {
			printf("error\n");
			return EXIT_FAILURE;
		}
		len >>= 3;
		if ((pos + len) > _raw_data.size()) {
			printf("error\n");
			return EXIT_FAILURE;
		}

		std::vector<uint8_t> block = std::vector<uint8_t>(_raw_data.begin() + pos,
			_raw_data.begin() + pos + len);
		blocks.push_back(block);

		pos += len;
	} while (pos < _raw_data.size());


	_bit_data.clear();
	for (auto it = blocks.begin(); it != blocks.end(); it++) {
		for (size_t pos = 0; pos < it->size(); pos++) {
			if (_reverseOrder == true)
				_bit_data += reverseByte(((*it)[pos]));
			else
				_bit_data += ((*it)[pos]);
		}
	}
	_bit_length = _bit_data.size() * 8;

	return 0;
}
