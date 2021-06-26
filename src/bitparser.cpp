// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (C) 2019 Gwenhael Goavec-Merou <gwenhael.goavec-merou@trabucayre.com>
 */

#include "bitparser.hpp"
#include "display.hpp"
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <sstream>
#include <string>
#include <utility>
#ifndef _WIN32
#include <arpa/inet.h>
#else
//for ntohs
#include <winsock2.h>
#endif

using namespace std;

#define display(...) \
	do { if (_verbose) fprintf(stdout, __VA_ARGS__);} while(0)

BitParser::BitParser(const string &filename, bool reverseOrder, bool verbose):
	ConfigBitstreamParser(filename, ConfigBitstreamParser::BIN_MODE,
	verbose), _reverseOrder(reverseOrder)
{
}

BitParser::~BitParser()
{
}

int BitParser::parseHeader()
{
	int pos_data = 0;
	int ret = 1;
	short length;
	string tmp(64, ' ');
	int pos, prev_pos;

	/* Field 1 : misc header */
	length = *(uint16_t *)&_raw_data[0];
	length = ntohs(length);
	pos_data += length + 2;

	length = *(uint16_t *)&_raw_data[pos_data];
	length = ntohs(length);
	pos_data += 2;

	while (1) {
		/* type */
		uint8_t type;
		type = (uint8_t)_raw_data[pos_data++];

		if (type != 'e') {
			length = *(uint16_t *)&_raw_data[pos_data];
			length = ntohs(length);
			pos_data += 2;
		} else {
			length = 4;
		}
		tmp = _raw_data.substr(pos_data, length);
		pos_data += length;

		switch (type) {
			case 'a': /* design name:userid:synthesize tool version */
				prev_pos = 0;
				pos = tmp.find(";");
				_hdr["design_name"] = tmp.substr(prev_pos, pos);
				prev_pos = pos+1;

				pos = tmp.find(";", prev_pos);
				prev_pos = tmp.find("=", prev_pos) + 1;
				_hdr["userID"] = tmp.substr(prev_pos, pos-prev_pos);
				prev_pos = pos+1;

				prev_pos = tmp.find("=", prev_pos) + 1;
				_hdr["toolVersion"] = tmp.substr(prev_pos, length-prev_pos);
				break;
			case 'b': /* FPGA model */
				_hdr["part_name"] = tmp.substr(0, length);
				break;
			case 'c': /* buildDate */
				_hdr["date"] = tmp.substr(0, length);
				break;
			case 'd': /* buildHour */
				_hdr["hour"] = tmp.substr(0, length);
				break;
			case 'e': /* file size */
				_bit_length = 0;
				for (int i = 0; i < 4; i++) {
					_bit_length <<= 8;
					_bit_length |= 0xff & tmp[i];
				}
				return pos_data;

				break;
		}
	}

	return ret;
}

int BitParser::parse()
{
	/* process all field */
	int pos = parseHeader();

	/* rest of the file is data to send */
	_bit_data.resize(_raw_data.size() - pos);
	std::move(_raw_data.begin() + pos, _raw_data.end(), _bit_data.begin());
	_bit_length = _bit_data.size();

	if (_reverseOrder) {
		for (int i = 0; i < _bit_length; i++) {
			_bit_data[i] = reverseByte(_bit_data[i]);
		}
	}

	/* convert size to bit */
	_bit_length *= 8;

	return 0;
}
