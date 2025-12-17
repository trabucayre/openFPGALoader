// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (C) 2019 Gwenhael Goavec-Merou <gwenhael.goavec-merou@trabucayre.com>
 */

#include <array>
#include <sstream>
#include <string>

#include "configBitstreamParser.hpp"
#include "display.hpp"
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
 *        04 -> extended linear addr record
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
	istringstream lineStream(_raw_data);

	FlashDataSection *rec = nullptr;

	bool must_stop = false;
	std::array<uint8_t, 255> tmp_buf{}; // max size for one data line

	while (std::getline(lineStream, str, '\n') && !must_stop) {
		uint8_t sum = 0, tmp, byteLen, type, checksum;
		uint16_t addr;
		uint32_t loc_addr;

		/* if '\r' is present -> drop */
		if (str.back() == '\r')
			str.pop_back();

		if (str[0] != ':') {
			printError("Error: a line must start with ':'");
			return EXIT_FAILURE;
		}
		/* len */
		sscanf((char *)&str[LEN_BASE], "%2hhx", &byteLen);
		/* address */
		sscanf((char *)&str[ADDR_BASE], "%4hx", &addr);
		/* type */
		sscanf((char *)&str[TYPE_BASE], "%2hhx", &type);
		/* checksum */
		sscanf((char *)&str[DATA_BASE + byteLen * 2], "%2hhx", &checksum);

		sum = byteLen + type + (addr & 0xff) + ((addr >> 8) & 0xff);

		switch (type) {
		case 0: { /* Data + addr */
			loc_addr = _base_addr + addr;

			/* Check current record:
			 * Create if null
			 * Create when a jump in address range
			 */
			if (!rec || (rec->getLength() > 0 && rec->getCurrentAddr() != loc_addr)) {
				_records.emplace_back(loc_addr);
				rec = &_records.back();
			}

			const char *ptr = (char *)&str[DATA_BASE];

			for (uint16_t i = 0; i < byteLen; i++, ptr += 2) {
				sscanf(ptr, "%2hhx", &tmp);
				tmp_buf[i] = _reverseOrder ? reverseByte(tmp) : tmp;
				sum += tmp;
			}
			rec->append(tmp_buf.data(), byteLen);
			break;
		}
		case 1: /* End Of File */
			must_stop = true;
			break;
		case 4: /* Extended linear addr */
			sscanf((char*)&str[DATA_BASE], "%4x", &loc_addr);
			_base_addr = (loc_addr << 16);
			sum += (loc_addr & 0xff) + ((loc_addr >> 8) & 0xff);
			break;
		default:
			printError("Error: unknown type");
			return EXIT_FAILURE;
		}

		if (checksum != (0xff & ((~sum) + 1))) {
			printError("Error: wrong checksum");
			return EXIT_FAILURE;
		}
	}

	const uint32_t nbRecord = getRecordCount();
	const uint32_t record_base = getRecordBaseAddr(nbRecord - 1);
	const uint32_t record_length = getRecordLength(nbRecord - 1);
	const uint32_t flash_size = record_base + record_length;

	_bit_data.assign(flash_size, 0xff);
	_bit_length = flash_size * 8;
	for (uint32_t i = 0; i < nbRecord; i++) {
		const uint32_t record_base = getRecordBaseAddr(i);
		const std::vector<uint8_t> rec = getRecordData(i);
		std::copy(rec.begin(), rec.end(), _bit_data.begin() + record_base);
	}

	return EXIT_SUCCESS;
}
