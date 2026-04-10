// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (C) 2019 Gwenhael Goavec-Merou <gwenhael.goavec-merou@trabucayre.com>
 */

#include <iostream>
#include <sstream>
#include <vector>
#include <cstdio>

#include "fsparser.hpp"
#include "display.hpp"


FsParser::FsParser(const std::string &filename, bool reverseByte, bool verbose):
			ConfigBitstreamParser(filename, ConfigBitstreamParser::ASCII_MODE,
			verbose), _reverseByte(reverseByte), _end_header(0), _checksum(0),
			_8Zero(0xff), _4Zero(0xff), _2Zero(0xff),
			_idcode(0), _compressed(false)
{
}

FsParser::~FsParser()
{
}

uint64_t FsParser::bitToVal(const char *bits, int len)
{
	uint64_t val = 0;
	for (int i = 0; i < len; i++)
		val = (val << 1) | (bits[i] == '1' ? 1 : 0);
	return val;
}

int FsParser::parseHeader()
{
	int ret = 0;
	std::string buffer;
	int line_index = 0;
	bool in_header = true;

	std::istringstream lineStream(_raw_data);

	while (std::getline(lineStream, buffer, '\n')) {
		/* store full/real file lenght */
		ret += buffer.size() + 1;

		/* FIXME: a line can't be empty -> error */
		if (buffer.empty())
			break;
		/* dos file */
		if (buffer.back() == '\r')
			buffer.pop_back();

		/* FIXME: a line can't be empty -> error */
		if (buffer.empty())
			break;
		/* drop all comment, base analyze on header */
		if (buffer[0] == '/')
			continue;

		const size_t line_length = buffer.size();

		/* store each line in dedicated buffer for future use */
		_lstRawData.push_back(buffer);

		/* only headers are parsed by next portion of code */
		if (!in_header)
			continue;

		/* a line must have at least 8 1/0 for the key */
		if (line_length < 8) {
			printError("FsParser: Potential corrupted file");
			return 0;
		}
		uint8_t c = bitToVal(buffer.substr(0, 8).c_str(), 8);
		uint8_t key = c & 0x7F;
		/* the line length depends on key/information */
		uint64_t val = bitToVal(buffer.c_str(), line_length);

		char __buf[10];
		int __buf_valid_bytes;
		switch (key) {
			case 0x06: /* idCode */
				if (line_length != 64) {
					printError("FsParser: length too short for key 0x06");
					return 0;
				}
				_idcode = (0xffffffff & val);
				__buf_valid_bytes = snprintf(__buf, 9, "%08x", _idcode);
				_hdr["idcode"] = std::string(__buf, __buf_valid_bytes);
				_hdr["idcode"].resize(8, ' ');
				break;
			case 0x0A: /* user code or checksum ? */
				__buf_valid_bytes = snprintf(__buf, 9, "%08x", (uint32_t)(0xffffffff & val));
				_hdr["CheckSum"] = std::string(__buf, __buf_valid_bytes);
				_hdr["CheckSum"].resize(8, ' ');
				break;
			case 0x0B: /* only present when bit_security is set */
				if (line_length != 32) {
					printError("FsParser: length too short for key 0x0B");
					return 0;
				}
				_hdr["SecurityBit"] = "ON";
				break;
			case 0x10: {
				if (line_length != 64) {
					printError("FsParser: length too short for key 0x10");
					return 0;
				}
				unsigned rate = (val >> 16) & 0xff;
				if (rate) {
					rate &= 0x7f;
					rate ^= 0x44;
					rate = (rate >> 2) + 1;
					rate = 125000000/rate;
				} else {
					rate = 2500000; // default
				}
				_hdr["LoadingRate"] = std::to_string(rate);
				_compressed = (val >> 13) & 1;
				_hdr["Compress"] = (_compressed) ? "ON" : "OFF";
				_hdr["ProgramDoneBypass"] = ((val >> 12) & 1) ? "ON" : "OFF";
				break;
			}
			case 0x12: /* unknown */
				if (line_length != 32) {
					printError("FsParser: length too short for key 0x12");
					return 0;
				}
				break;
			case 0x51:
				if (line_length != 64) {
					printError("FsParser: length too short for key 0x51");
					return 0;
				}
				/*
				[23:16] : a value used to replace 8x 0x00 in compress mode
				[15: 8] : a value used to replace 4x 0x00 in compress mode
				[ 7: 0] : a value used to replace 2x 0x00 in compress mode
				*/
				_8Zero = 0xff & (val >> 16);
				_4Zero = 0xff & (val >>  8);
				_2Zero = 0xff & (val >>  0);
				break;
			case 0x52: /* documentation issue */
				if (line_length != 64) {
					printError("FsParser: length too short for key 0x52");
					return 0;
				}
				uint32_t flash_addr;
				flash_addr = val & 0xffffffff;
				__buf_valid_bytes = snprintf(__buf, 9, "%08x", flash_addr);
				_hdr["SPIAddr"] = std::string(__buf, __buf_valid_bytes);
				_hdr["SPIAddr"].resize(8, ' ');

				break;
			case 0x3B: /* last header line with crc and cfg data length */
						/* documentation issue */
				if (line_length != 32) {
					printError("FsParser: length too short for key 0x3B");
					return 0;
				}
				in_header = false;
				uint8_t crc;
				crc = 0x01 & (val >> 23);

				_hdr["CRCCheck"] = (crc) ? "ON" : "OFF";
				_hdr["ConfDataLength"] = std::to_string(0xffff & val);
				_end_header = line_index;
				break;
		}

		line_index++;
	}

	return ret;
}

int FsParser::parse()
{
	std::string tmp;
	/* GW1N-6 and GW1N(R)-9 are address length not multiple of byte */
	int padding = 0;

	printInfo("Parse " + _filename + ": ");

	parseHeader();

	/* Fs file format is MSB first
	 * so if reverseByte = false bit 0 -> 7, 1 -> 6,
	 * if true 0 -> 0, 1 -> 1
	 */

	_bit_data.reserve(_raw_data.size() / 8);
	for (auto &&line : _lstRawData) {
		const size_t line_length = line.size();
		if ((line_length % 8) != 0) {
			printError("FsParser: truncated line in bitstream data");
			return EXIT_FAILURE;
		}
		for (size_t i = 0; i < line_length; i += 8) {
			uint8_t data = bitToVal(&line[i], 8);
			_bit_data.push_back((_reverseByte) ? reverseByte(data) : data);
		}
	}

	_bit_length = static_cast<int>(_bit_data.size() * 8);


	if (_idcode == 0)
		printWarn("Warning: IDCODE not found\n");

	/* use idcode to determine Count of Address */
	unsigned nb_line = 0;
	switch (_idcode) {
		case 0x0900281b: /* GW1N-1    */
		case 0x0900381b: /* GW1N-1S   */
		case 0x0100681b: /* GW1NZ-1   */
			nb_line = 274;
			break;
		case 0x0100181b: /* GW1N-2    */
		case 0x0120681b: /* GW1N(R/Z)-2/2B/2C, GW1N-1P5/1P5B/1P5C */
		case 0x0300081b: /* GW1NS-2   */
		case 0x0300181b: /* GW1NSx-2C */
		case 0x0100981b: /* GW1NSR-4C (warning! not documented) */
		case 0x0100381b: /* GW1N-4(ES)*/
		case 0x1100381b: /* GW1N-4B   */
			nb_line = 494;
			break;
		case 0x0100481b: /* GW1N-6(9C ES?) */
		case 0x1100481b: /* GW1N-9C   */
		case 0x0100581b: /* GW1N-9(ES)*/
		case 0x1100581b: /* GW1N-9    */
			nb_line = 712;
			padding = 4;
			if (_compressed)
				padding += 5 * 8;
			break;
		case 0x0000081b: /* GW2A-18   */
			nb_line = 1342;
			break;
		case 0x0000281b: /* GW2A-55    */
			nb_line = 2038;
			break;
		case 0x0001281b: /* GW5A-25 */
		case 0x0001681b: /* GW5AT-15 */
		case 0x0001481b: /* GW5AT-60 */
		case 0x0001081b: /* GW5AST-138 */
			/*
			 * FIXME: Lack of information,
			 * so just accept everything.
			 */
			nb_line = 65535;
			break;
		default:
			printWarn("Warning: Unknown IDCODE");
			nb_line = 0;
	}

	/* For configuration data checksum: number of lines can't be higher than
	 * the number indicates in TN653 but may be smaller (seen with the GW1NS-2C).
	 */
	if (stoul(_hdr["ConfDataLength"]) < nb_line)
		nb_line = stoi(_hdr["ConfDataLength"]);

	/* drop now useless header */
	_lstRawData.erase(_lstRawData.begin(), _lstRawData.begin() + _end_header + 1);
	_lstRawData.resize(nb_line);

	/* line full length depends on
	 * 1/ model
	 * 2/ (un)compress
	 * 3/ crc
	 * 4/ padding before data
	 * 5/ serie of 0xff at the end
	 */

	/* to compute checksum two situation
	 * 1/ uncompressed bitstream -> go
	 * 2/ compressed bitstream -> need to uncompress this before
	 */
	int drop = 6 * 8;
	if (_hdr["CRCCheck"] == "ON")
		drop += 2 * 8;
	for (auto &&ll = _lstRawData.begin();
			ll != _lstRawData.end(); ll++) {
		std::string l = "";
		const std::string &line = *ll;
		const size_t line_length = line.size();
		if (line_length < static_cast<size_t>(drop)) {
			printError("FsParser: truncated configuration line");
			return EXIT_FAILURE;
		}
		if (_compressed) {
			const size_t payload_len = line_length - drop;
			if ((payload_len % 8) != 0) {
				printError("FsParser: compressed line is not byte aligned");
				return EXIT_FAILURE;
			}
			l.reserve(payload_len * 8);
			for (size_t i = 0; i < payload_len; i += 8) {
				uint8_t c = bitToVal((const char *)&line[i], 8);
				if (c == _8Zero)
					l += std::string(8*8, '0');
				else if (c == _4Zero)
					l += std::string(4*8, '0');
				else if (c == _2Zero)
					l += std::string(2*8, '0');
				else
					l += line.substr(i, 8);
			}
		} else {
			l = line.substr(0, line_length - drop);
		}
		const size_t l_length = l.size();

		/* store bit for checksum */
		if (static_cast<size_t>(padding) > l_length) {
			printError("FsParser: invalid padding for configuration line");
			return EXIT_FAILURE;
		}
		tmp += l.substr(padding, l_length - padding);
	}

	/* checksum */
	_checksum = 0;
	if ((tmp.size() % 16) != 0) {
		printError("FsParser: checksum data is truncated");
		return EXIT_FAILURE;
	}
	for (uint32_t pos = 0; pos < tmp.size(); pos+=16)
		_checksum += (uint16_t)bitToVal(&tmp[pos], 16);

	if (_verbose)
		printf("checksum 0x%04x\n", _checksum);

	printSuccess("Done");

	return 0;
}
