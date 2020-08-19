/*
 * Copyright (C) 2019 Gwenhael Goavec-Merou <gwenhael.goavec-merou@trabucayre.com>
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

#include "configBitstreamParser.hpp"
#include "mcsParser.hpp"

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
 *        04 -> extented linear addr record
 *        05 -> start linear addr record
 */

#define LEN_BASE  1
#define ADDR_BASE 3
#define TYPE_BASE 7
#define DATA_BASE 9

McsParser::McsParser(const string &filename, bool reverseOrder, bool verbose):
		ConfigBitstreamParser(filename, ConfigBitstreamParser::ASCII_MODE,
		verbose),
		_base_addr(0), _reverseOrder(reverseOrder)
{}

int McsParser::parse()
{
	string str;
	int ret;

	do {
		getline(_fd, str);
		ret = parseLine(str);
	} while (ret == 0);
	return (ret < 0) ? EXIT_FAILURE : EXIT_SUCCESS;
}

int McsParser::parseLine(string buffer)
{
	const char *buff = buffer.c_str();
	uint16_t tmp, byteLen, type, checksum;
	uint32_t addr, loc_addr;
	uint8_t sum = 0;

	if (buff[0] != ':') {
		cout << "Error: a line must start with ':'" << endl;
		return -1;
	}
	/* len */
	sscanf(buff + LEN_BASE, "%2hx", &byteLen);
	/* address */
	sscanf(buff + ADDR_BASE, "%4x", &addr);
	/* type */
	sscanf(buff + TYPE_BASE, "%2hx", &type);
	/* checksum */
	sscanf(buff + DATA_BASE + byteLen * 2, "%2hx", &checksum);

	sum = byteLen + type + (addr & 0xff) + ((addr >> 8) & 0xff);

	if (type == 0) {
		loc_addr = _base_addr + addr;
		char *ptr = (char *)(buff + DATA_BASE);
		for (int i = 0; i < byteLen; i++, ptr += 2) {
			sscanf(ptr, "%2hx", &tmp);
			_bit_data[loc_addr + i] = (_reverseOrder)? reverseByte(tmp):tmp;
			sum += tmp;
		}
		_bit_length += (byteLen * 8);
	} else if (type == 1) {
		return 1;
	} else if (type == 4) {
		sscanf(buff + DATA_BASE, "%4x", &loc_addr);
		_base_addr = (loc_addr << 16);
		sum += (loc_addr & 0xff) + ((loc_addr >> 8) & 0xff);
	} else {
		cerr << "Error: unknown type" << endl;
		return -1;
	}

	if (checksum != (0xff&((~sum)+1))) {
		cerr << "Error: wrong checksum" << endl;
		return -1;
	}

	return 0;
}
