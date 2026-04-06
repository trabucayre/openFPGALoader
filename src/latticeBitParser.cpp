// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (C) 2019-2022 Gwenhael Goavec-Merou <gwenhael.goavec-merou@trabucayre.com>
 */
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include <algorithm>
#include <charconv>
#include <cctype>
#include <iostream>
#include <locale>
#include <string_view>
#include <utility>

#include "display.hpp"
#include "part.hpp"

#include "latticeBitParser.hpp"


LatticeBitParser::LatticeBitParser(const std::string &filename, bool machxo2, bool ecp3,
	bool verbose):
	ConfigBitstreamParser(filename, ConfigBitstreamParser::BIN_MODE, verbose),
	_endHeader(0), _is_machXO2(machxo2), _is_ecp3(ecp3)
{}

LatticeBitParser::~LatticeBitParser()
{
}

std::string LatticeBitParser::fmtIdcode(uint32_t id)
{
	char tmp[8], buf[9] = "00000000";
	std::to_chars_result conv = std::to_chars(tmp, tmp + 8, id, 16);
	char *ptr = conv.ptr;
	memcpy(buf + (8 - (ptr - tmp)), tmp, ptr - tmp);
	return std::string(buf, 8);
}

int LatticeBitParser::parseHeader()
{
	int currPos = 0;

	if (_raw_data.empty()) {
		printError("LatticeBitParser: empty bitstream");
		return EXIT_FAILURE;
	}

	const uint32_t file_size = static_cast<uint32_t>(_raw_data.size());

	/* check header signature */

	/* radiant .bit start with LSCC */
	if (_raw_data[0] == 'L') {
		if (file_size < 4) {
			printError("LatticeBitParser: bitstream too small");
			return EXIT_FAILURE;
		}
		if (_raw_data.compare(0, 4, "LSCC") != 0) {
			printf("Wrong File %s\n", _raw_data.substr(0, 4).c_str());
			return EXIT_FAILURE;
		}
		currPos += 4;
	}

	/* Check if bitstream size may store at least 0xff00 + another 0xff */
	if (file_size <= currPos + 3) {
		printError("LatticeBitParser: bitstream too small");
		return EXIT_FAILURE;
	}

	/* bit file comment area start with 0xff00 */
	if ((uint8_t)_raw_data[currPos] != 0xff ||
			(uint8_t)_raw_data[currPos + 1] != 0x00) {
		printf("Wrong File %02x%02x\n", (uint8_t) _raw_data[currPos],
			(uint8_t)_raw_data[currPos + 1]);
		return EXIT_FAILURE;
	}
	currPos += 2;

	_endHeader = _raw_data.find(0xff, currPos);
	if (_endHeader == std::string::npos) {
		printError("Error: preamble not found\n");
		return EXIT_FAILURE;
	}

	/* .bit for MACHXO3D seems to have more 0xff before preamble key */
	size_t pos = _raw_data.find(0xb3, _endHeader);
	if (pos == std::string::npos) {
		printError("Preamble key not found");
		return EXIT_FAILURE;
	}

	/* preamble must have at least 3 x 0xff + enc_key byte before 0xb3 */
	if (pos < _endHeader + 4) {
		printError("LatticeBitParser: wrong preamble size");
		return EXIT_FAILURE;
	}

	//0xbe is the key for encrypted bitstreams in Nexus fpgas
	const uint8_t enc_key = static_cast<uint8_t>(_raw_data[pos - 1]);
	if (enc_key != 0xbd && enc_key != 0xbf && enc_key != 0xbe) {
		printError("Wrong preamble key");
		return EXIT_FAILURE;
	}
	_endHeader = pos - 4; // align to 3 Dummy Bytes + preamble (ie. Header start offset).

	if (currPos >= _endHeader) {
		printError("LatticeBitParser: no header");
		return EXIT_FAILURE;
	}

	/* parse header */
	std::string_view lineStream(_raw_data.data() + currPos, _endHeader - currPos);
	while (!lineStream.empty()) {
		const size_t null_pos = lineStream.find('\0');
		const std::string_view buff = lineStream.substr(0, null_pos);
		pos = buff.find(':');
		if (pos != std::string_view::npos) {
			const std::string_view key = buff.substr(0, pos);
			const std::string_view val = buff.substr(pos + 1);
			const size_t startPos = val.find_first_not_of(' ');
			if (startPos != std::string_view::npos) {
				const size_t endPos = val.find_last_not_of(' ');
				_hdr.insert_or_assign(std::string(key),
					std::string(val.substr(startPos, endPos - startPos + 1)));
			}
		}
		if (null_pos == std::string_view::npos)
			break;
		lineStream = lineStream.substr(null_pos + 1);
	}
	return EXIT_SUCCESS;
}

int LatticeBitParser::parse()
{
	/* until 0xFFFFBDB3 0xFFFF */
	if (parseHeader() != EXIT_SUCCESS)
		return EXIT_FAILURE;

	/* check preamble */
	if (_endHeader + 4 >= _raw_data.size()) {
		printError("LatticeBitParser: truncated preamble");
		return EXIT_FAILURE;
	}
	uint32_t preamble = (*(uint32_t *)&_raw_data[_endHeader + 1]);
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
		std::map<std::string, std::string>::const_iterator part_it = _hdr.find("Part");
		if (part_it == _hdr.end()) {
			printError("LatticeBitParser: Missing Part in header section");
			return EXIT_FAILURE;
		}
		std::string_view subpart(part_it->second);
		const size_t pos = subpart.find_last_of('-');
		if (pos == std::string_view::npos) {
			printError("LatticeBitParser: invalid Part string");
			return EXIT_FAILURE;
		}
		subpart = subpart.substr(0, pos);

		for (const std::pair<const uint32_t, fpga_model> &fpga : fpga_list) {
			if (fpga.second.manufacturer != "lattice")
				continue;
			const std::string_view model = fpga.second.model;
			if (subpart.compare(0, model.size(), model) == 0) {
				_hdr["idcode"] = fmtIdcode(fpga.first);
				break;
			}
		}
	}

	/* read All data */
	if (!_is_machXO2) {
		/* According to FPGA-TN-02192-3.4
		 * the Lattice ECP3 must trasnmit at least 128 clock pulses before
		 * receiving the preamble.
		 * Here the header contains 3x8 Dummy bit + preamble so only
		 * 13bits 8x13= 112bits must be added as padding.
		 */
		const uint32_t offset = (_is_ecp3) ? 13 : 0;
		_bit_data.resize(_raw_data.size() - _endHeader + offset);
		if (_is_ecp3)
			std::fill_n(_bit_data.begin(), offset, uint8_t{0xff});
		std::move(_raw_data.begin() + _endHeader, _raw_data.end(), _bit_data.begin() + offset);
		_bit_length = _bit_data.size() * 8;
	} else {
		_endHeader++;
		const size_t len = _raw_data.size() - _endHeader;
		const size_t array_len = (len + 15) / 16;
		_bit_array.reserve(array_len);
		for (size_t i = 0; i < len; i += 16) {
			const size_t max_len = std::min<size_t>(16, len - i);
			_bit_array.emplace_back(16, '\xff');
			std::string &tmp = _bit_array.back();
			for (uint32_t pos = 0; pos < max_len; pos++)
				tmp[pos] = static_cast<char>(reverseByte(
					static_cast<uint8_t>(_raw_data[_endHeader + i + pos])));
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
	const uint8_t *raw = reinterpret_cast<const uint8_t *>(_raw_data.data());
	const size_t raw_size = _raw_data.size();
	const uint8_t *ptr;
	size_t pos = _endHeader + 5;  // drop 16 Dummy bits and preamble
	uint32_t idcode;

	while (pos < raw_size) {
		uint8_t cmd = raw[pos++];
		switch (cmd) {
		case BYPASS:
			break;
		case LSC_RESET_CRC:
			if (pos + 3 > raw_size) {
				printError("LatticeBitParser: truncated bitstream");
				return false;
			}
			pos += 3;
			break;
		case ECP3_VERIFY_ID:
			if (pos + 7 > raw_size) {
				printError("LatticeBitParser: truncated bitstream");
				return false;
			}
			ptr = raw + pos;
			idcode = (((uint32_t)reverseByte(ptr[6])) << 24) |
					 (((uint32_t)reverseByte(ptr[5])) << 16) |
					 (((uint32_t)reverseByte(ptr[4])) <<  8) |
					 (((uint32_t)reverseByte(ptr[3])) <<  0);
			_hdr["idcode"] = fmtIdcode(idcode);
			pos += 7;
			if (!_is_machXO2)
				return true;
			break;
		case VERIFY_ID:
			if (pos + 7 > raw_size) {
				printError("LatticeBitParser: truncated bitstream");
				return false;
			}
			ptr = raw + pos;
			idcode = (((uint32_t)ptr[3]) << 24) |
					 (((uint32_t)ptr[4]) << 16) |
					 (((uint32_t)ptr[5]) <<  8) |
					 (((uint32_t)ptr[6]) <<  0);
			_hdr["idcode"] = fmtIdcode(idcode);
			pos += 7;
			if (!_is_machXO2)
				return true;
			break;
		case LSC_WRITE_COMP_DIC:
			if (pos + 11 > raw_size) {
				printError("LatticeBitParser: truncated bitstream");
				return false;
			}
			pos += 11;
			break;
		case LSC_PROG_CNTRL0:
			if (pos + 7 > raw_size) {
				printError("LatticeBitParser: truncated bitstream");
				return false;
			}
			pos += 7;
			break;
		case LSC_INIT_ADDRESS:
			if (pos + 3 > raw_size) {
				printError("LatticeBitParser: truncated bitstream");
				return false;
			}
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
			if (pos + 3 > raw_size) {
				printError("LatticeBitParser: truncated bitstream");
				return false;
			}
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
