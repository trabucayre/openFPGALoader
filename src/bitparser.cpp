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


#define display(...) \
	do { if (_verbose) fprintf(stdout, __VA_ARGS__);} while(0)

BitParser::BitParser(const std::string &filename, bool reverseOrder, bool verbose):
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
	uint16_t length;
	std::string tmp;

	/* Field 1 : misc header */
	if (_raw_data.size() < 2) {
		printError("BitParser: Bound check failure. Can't read Field 1 length");
		return -1;
	}
	length = *(uint16_t *)&_raw_data[0];
	length = ntohs(length);
	pos_data += length + 2;

	if (pos_data + 2 >= static_cast<int>(_raw_data.size())) {
		printError("BitParser: Bound check failure. Can't read Field 2 length");
		return -1;
	}

	length = ((static_cast<uint16_t>(_raw_data[pos_data]) & 0xff) << 8) |
		((static_cast<uint16_t>(_raw_data[pos_data + 1]) & 0xff) << 0);

	pos_data += 2;

	while (1) {
		/* type */
		if (pos_data >= static_cast<int>(_raw_data.size())) {
			printError("BitParser: Bound check failure. Field type");
			return -1;
		}
		const uint8_t type = (uint8_t)_raw_data[pos_data++];

		if (type != 'e') {
			if (pos_data + 2 >= static_cast<int>(_raw_data.size())) {
				printError("BitParser: Bound check failure, Field length");
				return -1;
			}
			length = ((static_cast<uint16_t>(_raw_data[pos_data]) & 0xff) << 8) |
				((static_cast<uint16_t>(_raw_data[pos_data + 1]) & 0xff) << 0);
			pos_data += 2;
		} else {
			length = 4;
		}
		if (static_cast<int>(_raw_data.size()) < pos_data + length) {
			printError("BitParser: Bound check failure. Field Data");
			return -1;
		}
		tmp = _raw_data.substr(pos_data, length);
		pos_data += length;

		switch (type) {
			case 'a': {  /* design name:userid:synthesize tool version */
				std::stringstream ss(tmp);
				std::string token;
				bool first = true;

				while (std::getline(ss, token, ';')) {
					if (first) {
						_hdr["design_name"] = token;
						first = false;
						continue;
					}

					auto pos = token.find('=');
					if (pos != std::string::npos) {
						auto key = token.substr(0, pos);
						if (length < pos + 1) {
							printError("Failed to find for key");
							return -1;
						}
						auto value = token.substr(pos + 1);
						if (key == "UserID") {
							_hdr["userId"] = value;
						} else if (key == "Version") {
							_hdr["version"] = value;
						} else if (key == "SW_CRC") {
							_hdr["crc"] = value;
						} else if (key == "COMPRESS") {
							_hdr["compress"] = value;
						} else {
							printWarn("Unknown key " + key);
						}
					}
				}
				} break;
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
	if (pos == -1) {
		printError("BitParser: parseHeader failed");
		return 1;
	}

	/* _bit_length is length of data to send */
	int rest_of_file_length = _file_size - pos;
	if (_bit_length < rest_of_file_length) {
		printWarn("File is longer than bitstream length declared in the header: " +
				std::to_string(rest_of_file_length) + " vs " + std::to_string(_bit_length)
		);
	} else if (_bit_length > rest_of_file_length) {
		printError("File is shorter than bitstream length declared in the header: " +
				std::to_string(rest_of_file_length) + " vs " + std::to_string(_bit_length)
		);
		return 1;
	}

	_bit_data.resize(_bit_length);
	std::move(_raw_data.begin() + pos, _raw_data.begin() + pos + _bit_length, _bit_data.begin());

	if (_reverseOrder) {
		for (int i = 0; i < _bit_length; i++) {
			_bit_data[i] = reverseByte(_bit_data[i]);
		}
	}

	/* convert size to bit */
	_bit_length *= 8;

	return 0;
}

/* perform a bit reversal of each byte [31:24], [23:16], [15:8] and [7:0] */
static uint32_t reverseBitIn32(const uint32_t in)
{
	return static_cast<uint32_t>(
		(static_cast<uint32_t>(
			BitParser::reverseByte(static_cast<uint8_t>(in >> 24))) << 24) |
		(static_cast<uint32_t>(
			BitParser::reverseByte(static_cast<uint8_t>(in >> 16))) << 16) |
		(static_cast<uint32_t>(
			BitParser::reverseByte(static_cast<uint8_t>(in >>  8))) <<  8) |
		(static_cast<uint32_t>(
			BitParser::reverseByte(static_cast<uint8_t>(in >>  0))) <<  0));
}

/* perform a bit reversal of each byte [15:8] and [7:0] */
static uint16_t reverseBitIn16(const uint16_t in)
{
	return static_cast<uint16_t>(
		(static_cast<uint16_t>(
			BitParser::reverseByte(static_cast<uint8_t>(in >> 8))) << 8) |
		(static_cast<uint16_t>(
			BitParser::reverseByte(static_cast<uint8_t>(in >> 0))) << 0));
}

/* Convert a char array to an uint32_t
 * char array is big-endian
 * an optional reverseByte is performed
 */
static uint32_t arrayCharToWord(const uint8_t *in, bool is_reversed)
{
	uint32_t out =
		(static_cast<uint32_t>(in[3]) <<  0) |
		(static_cast<uint32_t>(in[2]) <<  8) |
		(static_cast<uint32_t>(in[1]) << 16) |
		(static_cast<uint32_t>(in[0]) << 24);
	if (is_reversed)
		return reverseBitIn32(out);
	return out;
}

static uint16_t arrayCharTo16b(const uint8_t *in, bool is_reversed)
{
	uint16_t out = static_cast<uint16_t>(
		(static_cast<uint16_t>(in[1]) <<  0) |
		(static_cast<uint16_t>(in[0]) <<  8));
	if (is_reversed)
		return reverseBitIn16(out);
	return out;
}

/* See UG470 p.96-97: Bitstream Composition
 * the bitstream is composed by a serie of big-endian words
 * Search for a specific key followed by the idcode
 */
#define SYNC_WORD_D   0xAA995566
#define IDCODE_PKT_D 0x30018001
uint32_t BitParser::get_idcode(const uint8_t *data, uint32_t length,
	bool is_reversed)
{
	if (!data || length == 0) {
		printError("Empty bitstream");
		return 0;
	}

	/* configuration data section starts with a sync word */
	const uint32_t sync_w = is_reversed ? reverseBitIn32(SYNC_WORD_D) :
		SYNC_WORD_D;
	const uint32_t opcode_w = is_reversed ? reverseBitIn32(IDCODE_PKT_D) :
		IDCODE_PKT_D;
	/* According to UG470 the bitstream starts with
	 * 8 dummy words
	 * 2 bus width auto detect (2 x 32bits)
	 * 2 dummy words
	 * followed by the sync word
	 * So the sync word start at 12 x 4 Bytes
	 */
	const uint32_t sync_offset = 12 * 4;
	if ((sync_offset + 4) > length) {
		printError("Invalid bitstream: too short");
		return 0;
	}

	// 1. check sync word (just to be sure)
	const uint32_t sync_word = arrayCharToWord(&data[sync_offset], false);
	if (sync_word != sync_w) {
		printf("Invalid sync word %08x instead of %08x\n", sync_word,
			sync_w);
		return 0;
	}

	// 2. search magic key and IDCODE
	uint32_t idcode = 0;
	for (uint32_t i = sync_offset + 4; i < length; i += 4) {
		if ((length - i) < 4) {
			printError("Bitstream too small/corrupted\n");
			break;
		}
		const uint32_t dword = arrayCharToWord(&data[i], false);
		/* search for Packet Type 1: Write IDCODE register, WORD_COUNT=1 */
		if (dword == opcode_w) {
			if ((length - i) < 8) {
				printError("Invalid bitstream\n");
				break;
			}
			idcode = arrayCharToWord(&data[i + 4], is_reversed);
			break;
		}
	}
	if (idcode == 0)
		printError("IDCODE not found");

	return idcode;
}
