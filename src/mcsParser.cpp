// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (C) 2019 Gwenhael Goavec-Merou <gwenhael.goavec-merou@trabucayre.com>
 */

#include <algorithm>
#include <array>
#include <charconv>
#include <limits>
#include <string>
#include <string_view>

#include "configBitstreamParser.hpp"
#include "display.hpp"
#include "mcsParser.hpp"


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

McsParser::McsParser(const std::string &filename, bool reverseOrder, bool verbose):
		ConfigBitstreamParser(filename, ConfigBitstreamParser::ASCII_MODE,
		verbose),
		_base_addr(0), _reverseOrder(reverseOrder), _records()
{}

int McsParser::parse()
{
	std::string_view data(_raw_data);

	FlashDataSection *rec = nullptr;

	bool must_stop = false;
	size_t flash_size = 0;
	std::array<uint8_t, 255> tmp_buf{}; // max size for one data line

	while (!data.empty() && !must_stop) {
		const size_t nl_pos = data.find('\n');
		std::string_view str = data.substr(0,
			nl_pos != std::string_view::npos ? nl_pos : data.size());
		data = (nl_pos != std::string_view::npos)
			? data.substr(nl_pos + 1) : std::string_view{};

		uint8_t sum = 0, tmp = 0, byteLen = 0, type = 0, checksum = 0;
		uint16_t addr = 0;
		uint32_t loc_addr = 0;

		/* if '\r' is present -> drop */
		if (!str.empty() && str.back() == '\r')
			str.remove_suffix(1);

		const char *line = str.data();
		const size_t line_len = str.size();

		/* line can't be empty */
		if (line_len == 0) {
			printError("Error: file corrupted: empty line");
			return false;
		}

		if (str[0] != ':') {
			printError("Error: a line must start with ':'");
			return EXIT_FAILURE;
		}

		/* A line must have at least TYPE_BASE + 2 char (1 Byte) */
		if (line_len < (TYPE_BASE + 2)) {
			printf("Error: line too short");
			return EXIT_FAILURE;
		}

		/* len */
		auto ret = std::from_chars(line + LEN_BASE,
			line + LEN_BASE + 2, byteLen, 16);
		if (ret.ec != std::errc{} || ret.ptr != line + LEN_BASE + 2) {
			printError("Error: malformed length field");
			return EXIT_FAILURE;
		}
		/* address */
		ret = std::from_chars(line + ADDR_BASE,
			line + ADDR_BASE + 4, addr, 16);
		if (ret.ec != std::errc{} || ret.ptr != line + ADDR_BASE + 4) {
			printError("Error: malformed address field");
			return EXIT_FAILURE;
		}
		/* type */
		ret = std::from_chars(line + TYPE_BASE,
			line + TYPE_BASE + 2, type, 16);
		if (ret.ec != std::errc{} || ret.ptr != line + TYPE_BASE + 2) {
			printError("Error: malformed type field");
			return EXIT_FAILURE;
		}

		const size_t checksum_offset = DATA_BASE + (static_cast<size_t>(byteLen) * 2);
		/* check if line contains enought char for data + checksum */
		if (line_len < (checksum_offset + 2)) {
			printf("Error: line too short");
			return EXIT_FAILURE;
		}
		/* checksum */
		ret = std::from_chars(line + checksum_offset,
			line + checksum_offset + 2, checksum, 16);
		if (ret.ec != std::errc{} || ret.ptr != line + checksum_offset + 2) {
			printError("Error: malformed checksum field");
			return EXIT_FAILURE;
		}

		sum = byteLen + type + (addr & 0xff) + ((addr >> 8) & 0xff);

		switch (type) {
		case 0: { /* Data + addr */
			loc_addr = _base_addr + addr;
			const size_t end = static_cast<size_t>(loc_addr) + byteLen;

			if (end < static_cast<size_t>(loc_addr)) {
				printError("Error: record size overflow");
				return EXIT_FAILURE;
			}
			flash_size = std::max(flash_size, end);

			/* Check current record:
			 * Create if null
			 * Create when a jump in address range
			 */
			if (!rec || (rec->getLength() > 0 && rec->getCurrentAddr() != loc_addr)) {
				_records.emplace_back(loc_addr);
				rec = &_records.back();
			}

			const char *ptr = line + DATA_BASE;

			for (uint16_t i = 0; i < byteLen; i++, ptr += 2) {
				ret = std::from_chars(ptr, ptr + 2, tmp, 16);
				if (ret.ec != std::errc{} || ret.ptr != ptr + 2) {
					printError("Error: malformed data field");
					return EXIT_FAILURE;
				}
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
			if (byteLen != 2) {
				printError("Error for line with type 4: data field too short");
				return EXIT_FAILURE;
			}
			ret = std::from_chars(line + DATA_BASE,
				line + DATA_BASE + 4, loc_addr, 16);
			if (ret.ec != std::errc{} || ret.ptr != line + DATA_BASE + 4) {
				printError("Error: malformed extended linear address");
				return EXIT_FAILURE;
			}
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

	const size_t nbRecord = getRecordCount();
	if (nbRecord == 0) {
		printError("No record found: is empty file?");
		return EXIT_FAILURE;
	}

	_bit_data.assign(flash_size, 0xff);
	if (flash_size > static_cast<size_t>(std::numeric_limits<int>::max() / 8)) {
		printError("Error: bitstream too large");
		return EXIT_FAILURE;
	}
	_bit_length = flash_size * 8;
	for (const FlashDataSection& section : _records) {
		const size_t start = section.getStartAddr();
		const size_t length = section.getLength();

		if (start > _bit_data.size() || length > (_bit_data.size() - start)) {
			printError("Error: record out of range");
			return EXIT_FAILURE;
		}

		std::copy(section.getRecord().begin(),
			section.getRecord().end(),
			_bit_data.begin() + start);
	}

	return EXIT_SUCCESS;
}
