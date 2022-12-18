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

using namespace std;

FsParser::FsParser(const string &filename, bool reverseByte, bool verbose):
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
	string buffer;
	int line_index = 0;
	bool in_header = true;

	istringstream lineStream(_raw_data);

	while (std::getline(lineStream, buffer, '\n')) {
		ret += buffer.size() + 1;
		if (buffer.empty())
			break;
		/* drop all comment, base analyze on header */
		if (buffer[0] == '/')
			continue;
		if (buffer[buffer.size()-1] == '\r')
			buffer.pop_back();

		/* store each line in dedicated buffer for future use
		 */
		_lstRawData.push_back(buffer);

		if (!in_header)
			continue;

		uint8_t c = bitToVal(buffer.substr(0, 8).c_str(), 8);
		uint8_t key = c & 0x7F;
		uint64_t val = bitToVal(buffer.c_str(), buffer.size());

		switch (key) {
			case 0x06: /* idCode */
				_idcode = (0xffffffff & val);
				_hdr["idcode"] = string(8, ' ');
				snprintf(&_hdr["idcode"][0], 9, "%08x", _idcode);
				break;
			case 0x0A: /* user code or checksum ? */
				_hdr["CheckSum"] = string(4, ' ');
				snprintf(&_hdr["CheckSum"][0], 5, "%04x", (uint16_t)(0xffff & val));
				break;
			case 0x0B: /* only present when bit_security is set */
				_hdr["SecurityBit"] = "ON";
				break;
			case 0x10:
				/* unknown conversion */
				_hdr["loading_rate"] = to_string(0xff & (val >> 16));
				_compressed = 0x01 & (val >> 13);
				_hdr["Compress"] = (_compressed) ? "ON" : "OFF";
				_hdr["ProgramDoneBypass"] = (0x01 & (val >> 12))?"ON":"OFF";
				break;
			case 0x12: /* unknown */
				break;
			case 0x51:
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
				uint32_t flash_addr;
				flash_addr = val & 0xffffffff;
				_hdr["SPIAddr"] = string(8, ' ');
				snprintf(&_hdr["SPIAddr"][0], 9, "%08x", flash_addr);

				break;
			case 0x3B: /* last header line with crc and cfg data length */
						/* documentation issue */
				in_header = false;
				uint8_t crc;
				crc = 0x01 & (val >> 23);

				_hdr["CRCCheck"] = (crc) ? "ON" : "OFF";
				_hdr["ConfDataLength"] = to_string(0xffff & val);
				_end_header = line_index;
				break;
		}

		line_index++;
	}

	return ret;
}

int FsParser::parse()
{
	string tmp;
	/* GW1N-6 and GW1N(R)-9 are address length not multiple of byte */
	int padding = 0;

	printInfo("Parse " + _filename + ": ");

	parseHeader();

	/* Fs file format is MSB first
	 * so if reverseByte = false bit 0 -> 7, 1 -> 6,
	 * if true 0 -> 0, 1 -> 1
	 */

	for (auto &&line : _lstRawData) {
		for (size_t i = 0; i < line.size(); i+=8) {
			uint8_t data = bitToVal(&line[i], 8);
			_bit_data += (_reverseByte) ? reverseByte(data) : data;
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
		case 0x1100181b: /* GW1N-2B   */
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
		string l = "";
		string line = *ll;
		if (_compressed) {
			for (size_t i = 0; i < line.size()-drop; i+=8) {
				uint8_t c = bitToVal((const char *)&line[i], 8);
				if (c == _8Zero)
					l += string(8*8, '0');
				else if (c == _4Zero)
					l += string(4*8, '0');
				else if (c == _2Zero)
					l += string(2*8, '0');
				else
					l += line.substr(i, 8);
			}
		} else {
			l = line.substr(0, line.size() - drop);
		}

		/* store bit for checksum */
		tmp += l.substr(padding, l.size() - padding);
	}

	/* checksum */
	_checksum = 0;
	for (uint32_t pos = 0; pos < tmp.size(); pos+=16)
		_checksum += (uint16_t)bitToVal(&tmp[pos], 16);

	if (_verbose)
		printf("checksum 0x%04x\n", _checksum);

	printSuccess("Done");

	return 0;
}
