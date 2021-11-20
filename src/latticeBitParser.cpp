// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (C) 2019 Gwenhael Goavec-Merou <gwenhael.goavec-merou@trabucayre.com>
 */
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include <algorithm>
#include <functional>
#include <cctype>
#include <iostream>
#include <locale>
#include <sstream>
#include <utility>

#include "display.hpp"

#include "latticeBitParser.hpp"

using namespace std;

LatticeBitParser::LatticeBitParser(const string &filename, bool verbose):
	ConfigBitstreamParser(filename, ConfigBitstreamParser::BIN_MODE, verbose),
	_endHeader(0)
{}

LatticeBitParser::~LatticeBitParser()
{
}

int LatticeBitParser::parseHeader()
{
	int currPos = 0;

	/* check header signature */

	/* radiant .bit start with LSCC */
	if (_raw_data[0] == 'L') {
		if (_raw_data.substr(0, 4) != "LSCC") {
			printf("Wrong File %s\n", _raw_data.substr(0, 4).c_str());
			return EXIT_FAILURE;
		}
		currPos += 4;
	}

	/* bit file comment area start with 0xff00 */
	if ((uint8_t)_raw_data[currPos] != 0xff || (uint8_t)_raw_data[currPos + 1] != 0x00) {
		printf("Wrong File %02x%02x\n", (uint8_t) _raw_data[currPos],
			(uint8_t)_raw_data[currPos]);
		return EXIT_FAILURE;
	}
	currPos+=2;


	_endHeader = _raw_data.find(0xff, currPos);
	if (_endHeader == string::npos) {
		printError("Error: preamble not found\n");
		return EXIT_FAILURE;
	}

	/* parse header */
	istringstream lineStream(_raw_data.substr(currPos, _endHeader-currPos));
	string buff;
	while (std::getline(lineStream, buff, '\0')) {
		size_t pos = buff.find_first_of(':', 0);
		if (pos != string::npos) {
			string key(buff.substr(0, pos));
			string val(buff.substr(pos+1, buff.size()));
			int startPos = val.find_first_not_of(" ");
			int endPos = val.find_last_not_of(" ")+1;
			_hdr[key] = val.substr(startPos, endPos).c_str();
		}
	}
	return EXIT_SUCCESS;
}

int LatticeBitParser::parse()
{
	/* until 0xFFFFBDB3 0xFFFF */
	if (parseHeader() < 0)
		return EXIT_FAILURE;

	/* check preamble */
	if ((*(uint32_t *)&_raw_data[_endHeader+1]) != 0xb3bdffff) {
		printError("Error: missing preamble\n");
		return EXIT_FAILURE;
	}

	/* read All data */
	_bit_data.resize(_raw_data.size() - _endHeader);
	std::move(_raw_data.begin()+_endHeader, _raw_data.end(), _bit_data.begin());
	_bit_length = _bit_data.size() * 8;

	/* extract idcode from configuration data (area starting with 0xE2) */
	for (int i = 0; i < _bit_data.size();i++) {
		if ((uint8_t)_bit_data[i] != 0xe2)
			continue;
		/* E2: verif id */
		uint32_t idcode = (((uint32_t)_bit_data[i+4]) << 24) |
						(((uint32_t)_bit_data[i+5]) << 16) |
						(((uint32_t)_bit_data[i+6]) <<  8) |
						(((uint32_t)_bit_data[i+7]) <<  0);
		_hdr["idcode"] = string(8, ' ');
		snprintf(&_hdr["idcode"][0], 9, "%08x", idcode);
		break;
	}

	return 0;
}
