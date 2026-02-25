// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (C) 2019-2022 Gwenhael Goavec-Merou <gwenhael.goavec-merou@trabucayre.com>
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
#include "part.hpp"

#include "latticeBitParser.hpp"

using namespace std;

LatticeBitParser::LatticeBitParser(const string &filename, bool machxo2, bool ecp3,
	bool verbose):
	ConfigBitstreamParser(filename, ConfigBitstreamParser::BIN_MODE, verbose),
	_endHeader(0), _is_machXO2(machxo2), _is_ecp3(ecp3)
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
	if ((uint8_t)_raw_data[currPos] != 0xff ||
			(uint8_t)_raw_data[currPos + 1] != 0x00) {
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

	/* .bit for MACHXO3D seems to have more 0xff before preamble key */
	size_t pos = _raw_data.find(0xb3, _endHeader);
	if (pos == string::npos) {
		printError("Preamble key not found");
		return EXIT_FAILURE;
	}
	//0xbe is the key for encrypted bitstreams in Nexus fpgas
	if ((uint8_t)_raw_data[pos-1] != 0xbd && (uint8_t)_raw_data[pos-1] != 0xbf && (uint8_t)_raw_data[pos-1] != 0xbe) {
		printError("Wrong preamble key");
		return EXIT_FAILURE;
	}
	_endHeader = pos - 3; // align to 2 Dummy bits + preamble (ie. Header start offset).

	/* parse header */
	istringstream lineStream(_raw_data.substr(currPos, _endHeader - currPos - 1));
	string buff;
	while (std::getline(lineStream, buff, '\0')) {
		pos = buff.find_first_of(':', 0);
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
	uint32_t preamble = (*(uint32_t *)&_raw_data[_endHeader]);
	//0xb3beffff is the preamble for encrypted bitstreams in Nexus fpgas
	if ((preamble != 0xb3bdffff) && (preamble != 0xb3bfffff) && (preamble != 0xb3beffff)) {
		printError("Error: missing preamble\n");
		return EXIT_FAILURE;
	}

	printf("%08x\n", preamble);
	if (preamble == 0xb3bdffff) {
		/* extract idcode from configuration data (area starting with 0xE2)
		 * and check compression when machXO2
		 */
		if (parseCfgData() == false)
			return EXIT_FAILURE;
	} else {  // encrypted bitstream
		if (_is_machXO2) {
			printError("encrypted bitstream not supported for machXO2");
			return EXIT_FAILURE;
		}
		string part = getHeaderVal("Part");
		string subpart = part.substr(0, part.find_last_of("-"));
		for (auto && fpga : fpga_list) {
			if (fpga.second.manufacturer != "lattice")
				continue;
			string model = fpga.second.model;
			if (subpart.compare(0, model.size(), model) == 0) {
				char __buf[10];
				int __buf_valid_bytes;
				__buf_valid_bytes = snprintf(__buf, 9, "%08x", fpga.first);
				_hdr["idcode"] = string(__buf, __buf_valid_bytes);
				_hdr["idcode"].resize(8, ' ');
			}
		}
	}

	/* read All data */
	if (!_is_machXO2) {
		/* According to FPGA-TN-02192-3.4
		 * the Lattice ECP3 must trasnmit at least 128 clock pulses before
		 * receiving the preamble.
		 * Here the header contains 16 Dummy bit + preamble so only
		 * 14bits 8x14= 112bits must be added as padding.
		 */
		const uint32_t offset = (_is_ecp3) ? 14 : 0;
		_bit_data.resize(_raw_data.size() - _endHeader + offset);
		if (_is_ecp3) {
			std::string tmp(14, 0xff);
			std::move(tmp.begin(), tmp.end(), _bit_data.begin());
		}
		std::move(_raw_data.begin() + _endHeader, _raw_data.end(), _bit_data.begin() + offset);
		_bit_length = _bit_data.size() * 8;
	} else {
		const uint32_t len = _raw_data.size() - _endHeader;
		uint32_t max_len = 16;
		for (uint32_t i = 0; i < len; i+=max_len) {
			std::string tmp(16, 0xff);
			/* each line must have 16B */
			if (len < i + max_len)
				max_len = len - i;
			for (uint32_t pos = 0; pos < max_len; pos++)
				tmp[pos] = reverseByte(_raw_data[i+pos+_endHeader]);
			_bit_array.push_back(std::move(tmp));
		}
		_bit_length = _bit_array.size() * 16 * 8;
	}

	return 0;
}

#define LSC_WRITE_COMP_DIC 0x02
#define LSC_PROG_CNTRL0    0x22
#define LSC_RESET_CRC      0x3B
#define LSC_INIT_ADDRESS   0x46
#define LSC_SPI_MODE       0x79
#define LSC_PROG_INCR_CMP  0xB8
#define LSC_PROG_INCR_RTI  0x82
#define VERIFY_ID          0xE2
#define BYPASS             0xFF

/* ECP3 has a bit reversable (ie same values but 7-0 -> 0-7) */
#define ECP3_VERIFY_ID     0x47

bool LatticeBitParser::parseCfgData()
{
	uint8_t *ptr;
	size_t pos = _endHeader + 4;  // drop 16 Dummy bits and preamble
	uint32_t idcode;
	char __buf[10];
	int __buf_valid_bytes;
	while (pos < _raw_data.size()) {
		uint8_t cmd = (uint8_t) _raw_data[pos++];
		switch (cmd) {
		case BYPASS:
			break;
		case LSC_RESET_CRC:
			pos += 3;
			break;
		case ECP3_VERIFY_ID:
			ptr = (uint8_t*)&_raw_data[pos];
			idcode = (((uint32_t)reverseByte(ptr[6])) << 24) |
					 (((uint32_t)reverseByte(ptr[5])) << 16) |
					 (((uint32_t)reverseByte(ptr[4])) <<  8) |
					 (((uint32_t)reverseByte(ptr[3])) <<  0);
			__buf_valid_bytes = snprintf(__buf, 9, "%08x", idcode);
			_hdr["idcode"] = string(__buf, __buf_valid_bytes);
			_hdr["idcode"].resize(8, ' ');
			pos += 7;
			if (!_is_machXO2)
				return true;
			break;
		case VERIFY_ID:
			ptr = (uint8_t*)&_raw_data[pos];
			idcode = (((uint32_t)ptr[3]) << 24) |
					 (((uint32_t)ptr[4]) << 16) |
					 (((uint32_t)ptr[5]) <<  8) |
					 (((uint32_t)ptr[6]) <<  0);
			__buf_valid_bytes = snprintf(__buf, 9, "%08x", idcode);
			_hdr["idcode"] = string(__buf, __buf_valid_bytes);
			_hdr["idcode"].resize(8, ' ');
			pos += 7;
			if (!_is_machXO2)
				return true;
			break;
		case LSC_WRITE_COMP_DIC:
			pos += 11;
			break;
       case LSC_PROG_CNTRL0:
			pos += 7;
			break;
		case LSC_INIT_ADDRESS:
			pos += 3;
			break;
		case LSC_PROG_INCR_CMP:
			return true;
			break;
		case LSC_PROG_INCR_RTI:
			printError("Bitstream is not compressed- not writing.");
			return false;
		case LSC_SPI_MODE:  // optional: 0x79 + mode (fast-read:0x49,
							// dual-spi:0x51, qspi:0x59) + 2 x 0x00
			pos += 3;
			break;
		default:
			char mess[256];
			snprintf(mess, 256, "Unknown command type %02x.\n", cmd);
			printError(mess);
			return false;
		}
	}

	return false;
}
