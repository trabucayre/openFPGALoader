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
#include <locale>
#include <vector>

#include "anlogicBitParser.hpp"
#include "display.hpp"

using namespace std;

AnlogicBitParser::AnlogicBitParser(const string &filename, bool verbose):
	ConfigBitstreamParser(filename, ConfigBitstreamParser::BIN_MODE, verbose)
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
	int ret = EXIT_SUCCESS;
	printInfo("parseHeader");

	while(1) {
		string buffer;
		std::getline(_fd, buffer, '\n');

		if (_fd.eof()) {
			printError("End of file before start of data");
			return EXIT_FAILURE;
		}

		if (buffer.empty()) {
			printInfo("header end");
			break;
		}

		if (buffer[0] != '#') {
			printError("header must start with #\n");
			return EXIT_FAILURE;
		}

		string content = buffer.substr(2);
		size_t pos = content.find(':');
		if (pos == string::npos) {
			_hdr["tool"] = content;
		} else {
			string entry = content.substr(0, pos);
			string val = content.substr(pos+2);
			_hdr[entry] = val;
		}
	}

	return ret;
}

int AnlogicBitParser::parse()
{
	if (parseHeader() == EXIT_FAILURE)
		return EXIT_FAILURE;

	uint8_t dummy = _fd.get();
	if (dummy != 0x00) {
		printError("Header must end with 0x00 (binary) bit");
		return EXIT_FAILURE;
	}

	size_t start_data = _fd.tellg();
	start_data--;

	_fd.seekg(0, _fd.end);
	size_t size_data = (size_t)_fd.tellg() - start_data;
	_fd.seekg(start_data, _fd.beg);

	vector<uint8_t> data;
	data.resize(size_data);
	_fd.read(reinterpret_cast<char *>(&(data[0])), size_data);

	int pos = 0;
	std::vector<std::vector<uint8_t>> blocks;
    do {
        uint16_t len = (data[pos++] << 8);
        len += data[pos++];
        if ((len & 7) != 0) {
			printf("error\n");
			return EXIT_FAILURE;
		}
        len >>= 3;
        if ((pos + len) > data.size()) {
			printf("error\n");
			return EXIT_FAILURE;
		}

        std::vector<uint8_t> block = std::vector<uint8_t>(data.begin() + pos,
			data.begin() + pos + len);
        blocks.push_back(block);

        pos += len;
    } while (pos < data.size());


	_bit_data.clear();
	for (auto it = blocks.begin(); it != blocks.end(); it++) {
		for (size_t pos = 0; pos < it->size(); pos++) {
			_bit_data += reverseByte(((*it)[pos]));
		}
	}
	_bit_length = _bit_data.size() * 8;

	return 0;
}
