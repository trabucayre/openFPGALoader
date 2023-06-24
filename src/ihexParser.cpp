// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (c) 2021 Gwenhael Goavec-Merou <gwenhael.goavec-merou@trabucayre.com>
 */


#include <sstream>
#include <string>

#include "configBitstreamParser.hpp"
#include "display.hpp"
#include "ihexParser.hpp"

using namespace std;

/* line format
 * :LLAAAATTHH...HHCC
 * LL   : nb octets de data dans la ligne (hexa)
 * AAAA : addresse du debut de la ligne ou mettre les data 
 * TT   : type de la ligne (cf. plus bas)
 * HH   : le champ de data
 * CC   : Checksum (cf. plus bas)
 */
/* type : 00 -> data + addr 16b
 *        01 -> end of file
 *        02 -> extended addr
 *        03 -> start segment addr record
 *        04 -> extended linear addr record
 *        05 -> start linear addr record
 */

#define LEN_BASE  1
#define ADDR_BASE 3
#define TYPE_BASE 7
#define DATA_BASE 9

IhexParser::IhexParser(const string &filename, bool reverseOrder, bool verbose):
		ConfigBitstreamParser(filename, ConfigBitstreamParser::ASCII_MODE,
		verbose),
		_base_addr(0), _reverseOrder(reverseOrder)
{}

int IhexParser::parse()
{
	string str;
	istringstream lineStream(_raw_data);

	uint16_t next_addr = 0;
	bool is_first = true;
	data_line_t cnt;
	cnt.length = 0;

	while (std::getline(lineStream, str, '\n')) {
		char *ptr;
		uint8_t sum = 0;
		uint16_t tmp, byteLen, type, checksum;
		uint32_t addr, loc_addr;

		/* if '\r' is present -> drop */
		if (str.back() == '\r')
			str.pop_back();

		if (str[0] == '#')  // comment
			continue;
		if (str[0] != ':') {
			printError("Error: a line must start with ':'");
			return EXIT_FAILURE;
		}
		/* len */
		sscanf((char *)&str[LEN_BASE], "%2hx", &byteLen);
		/* address */
		sscanf((char *)&str[ADDR_BASE], "%4x", &addr);
		/* type */
		sscanf((char *)&str[TYPE_BASE], "%2hx", &type);
		/* checksum */
		sscanf((char *)&str[DATA_BASE + byteLen * 2], "%2hx", &checksum);

		sum = byteLen + type + (addr & 0xff) + ((addr >> 8) & 0xff);

		switch (type) {
		case 0:
			loc_addr = _base_addr + addr;
			/* if this is the first line
			 * prepare structure with base address
			 * if previous address + line length didn't match new addr
			 * -> break -> store and start a new section
			 */
			if (next_addr != addr || is_first) {
				if (!is_first)
					_array_content.push_back(cnt);
				cnt.addr = loc_addr;
				cnt.length = 0;
				cnt.line_data.clear();
				is_first = false;
			}

			ptr = (char *)&str[DATA_BASE];
			if (_bit_data.size() < loc_addr + byteLen)
				_bit_data.resize(2*(loc_addr + byteLen));
			for (int i = 0; i < byteLen; i++, ptr += 2) {
				sscanf(ptr, "%2hx", &tmp);
				_bit_data[loc_addr + i] = (_reverseOrder)? reverseByte(tmp):tmp;
				sum += tmp;
				cnt.line_data.push_back(_bit_data[loc_addr+i]);
			}
			cnt.length += byteLen;
			next_addr = addr + byteLen;
			_bit_length += (byteLen * 8);
			break;
		case 1:
			if (cnt.length != 0)
				_array_content.push_back(cnt);
			return EXIT_SUCCESS;
			break;
		default:
			printError("Error: unknown type");
			return EXIT_FAILURE;
		}

		if (checksum != (0xff&((~sum)+1))) {
			printError("Error: wrong checksum");
			return EXIT_FAILURE;
		}
	}

	return EXIT_SUCCESS;
}
