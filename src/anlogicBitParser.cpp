// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (C) 2020 Gwenhael Goavec-Merou <gwenhael.goavec-merou@trabucayre.com>
 * Based on Miodrag Milanovic https://github.com/mmicko/prjtang/
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

/* read all ascii lines starting with '#'
 * stop when an empty line is found
 */
int AnlogicBitParser::parseHeader()
{
	int ret = 0;

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
			return -1;
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
