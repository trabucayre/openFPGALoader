// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (C) 2022 Gwenhael Goavec-Merou <gwenhael.goavec-merou@trabucayre.com>
 */

#include "pofParser.hpp"

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>

#include <algorithm>
#include <cstring>
#include <string>
#include <map>
#include <regex>
#include <stdexcept>
#include <utility>
#include <vector>

#include "display.hpp"

POFParser::POFParser(const std::string &filename, bool verbose):
	ConfigBitstreamParser(filename, ConfigBitstreamParser::BIN_MODE,
	verbose)
{}

POFParser::~POFParser()
{}

uint8_t *POFParser::getData(const std::string &section_name)
{
	if (section_name == "")
		return (uint8_t*)_bit_data.data();
	auto section = mem_section.find(section_name);
	if (section == mem_section.end())
		return NULL;
	return (*section).second.data;
}

int POFParser::getLength(const std::string &section_name)
{
	if (section_name == "")
		return _bit_length;
	auto section = mem_section.find(section_name);
	if (section == mem_section.end())
		return -1;
	return (*section).second.len;
}

void POFParser::displayHeader()
{
	ConfigBitstreamParser::displayHeader();
	for (auto it = mem_section.begin(); it != mem_section.end(); it++) {
		memory_section_t v = (*it).second;
		char mess[1024];
		snprintf(mess, 1024, "%02x %4s: ", v.flag, v.section.c_str());
		printInfo(mess, false);
		snprintf(mess, 1024, "%08x %08x", v.offset, v.len);
		printSuccess(mess);
	}
}

int POFParser::parse()
{
	uint8_t *ptr = (uint8_t *)_raw_data.data();
	uint32_t pos = 0;

	if (_verbose)
		printf("[%08x:%08x] %s\n", 0, 3, ptr);
	/* [0:3]: POF\0 */
	ptr += 4;
	pos += 4;
	/* unknown */
	if (_verbose) {
		uint32_t first_section = ARRAY2INT32(ptr);
		printf("first section:     %08x %4u\n", first_section, first_section);
	}
	ptr += 4;
	pos += 4;
	/* number of packets */
	if (_verbose) {
		uint32_t num_packets = ARRAY2INT32(ptr);
		printf("number of packets: %08x %4u\n", num_packets, num_packets);
	}
	pos += 4;

	/* 16bit code + 32bits size + content */
	while (pos < static_cast<uint32_t>(_file_size)) {
		uint16_t flag = ARRAY2INT16((&_raw_data.data()[pos]));
		pos += 2;
		uint32_t size = ARRAY2INT32((&_raw_data.data()[pos]));
		pos += 4;
		pos += parseSection(flag, pos, size);
	}

	/* update pointers to memory area */
	ptr = (uint8_t *)_bit_data.data();
	mem_section["CFM0"].data = &ptr[mem_section["CFM0"].offset + 0x0C];
	mem_section["UFM"].data = &ptr[mem_section["UFM"].offset + 0x0C];
	mem_section["ICB"].data = &ptr[mem_section["ICB"].offset + 0x0C];

	return EXIT_SUCCESS;
}

uint32_t POFParser::parseSection(uint16_t flag, uint32_t pos, uint32_t size)
{
	std::string content;
	std::string t;
	char mess[1024];

	if (_verbose) {
		snprintf(mess, 1024, "Flag: %02x (%d) Size: %u", flag, flag, size);
		printInfo(mess);
	}

	/* 0x01: software name/version */
	/* 0x02: full FPGAs model */
	/* 0x03: bitstream name ? */
	/* 0x3b: ? */
	/* 0x12: ? */
	/* 0x13: contains usercode / checksum */
	/* 0x24: ? */
	/* 0x11: ? */
	/* 0x18: ? */
	/* 0x15: ? */
	/* 0x34: ? */
	/* 0x35: ? */
	/* 0x38: ? */
	/* 0x08: ? CRC ? */
	switch (flag) {
		case 0x01:  // software name/version
			_hdr["tool"] = _raw_data.substr(pos, size);
			break;
		case 0x02:  // full FPGA part name
			_hdr["part_name"] = _raw_data.substr(pos, size-1); // -1 because '\0'
			break;
		case 0x03:  // bitstream/design/xxx name
			_hdr["design_name"] = _raw_data.substr(pos, size);
			break;
		case 0x08:  // last packet: CRC ?
			_hdr["maybeCRC"] = std::to_string(ARRAY2INT16((&_raw_data.data()[pos])));
			break;
		case 0x11:  // cfg data
					// 12 Bytes unknown
					// followed by UFM/CFM/DSM data
			if (_verbose) {
				content = _raw_data.substr(pos, size);
				uint32_t val0 = ARRAY2INT32((&content.c_str()[0]));
				uint32_t val1 = ARRAY2INT32((&content.c_str()[4]));
				uint32_t val2 = ARRAY2INT32((&content.c_str()[8]));
				printf("Flag 0x11: Unknown %08x %08x %08x\n", val0, val1, val2);
			}

			_bit_data.resize(size);
			std::copy(_raw_data.begin() + pos, _raw_data.begin() + pos + size,
					_bit_data.begin());
			_bit_length = size * 8;
			if (_verbose)
				printf("size %u %zu\n", size, _bit_data.size());
			break;
		case 0x13:  // contains usercode / checksum
			_hdr["usercode"] = std::to_string(ARRAY2INT32((&_raw_data.data()[pos+size-4])));
			if (_verbose) {
				t = _raw_data.substr(pos, size);
				for (size_t i = 0; i < t.size(); i++)
					printf("%02x ", static_cast<uint8_t>(t[i]));
				printf("\n");
				printf("%s\n", t.c_str());

				/* 4 x 32bits */
				uint32_t sec0, sec1, sec2, sec3;
				sec0 = (t[0] << 24) | (t[1] << 16) | (t[2] << 8)  | (t[3] << 0);
				sec1 = (t[4] << 24) | (t[5] << 16) | (t[6] << 8)  | (t[7] << 0);
				sec2 = (t[8] << 24) | (t[9] << 16) | (t[10] << 8)  | (t[11] << 0);
				sec3 = ((unsigned char)t[size-4] << 24) |
					((unsigned char)t[size-3] << 16) |
					((unsigned char)t[size-2] << 8) |
					((unsigned char)t[size-1] << 0);
				printf("sec0: %08x\n", sec0);
				printf("sec1: %08x\n", sec1);
				printf("sec2: %08x\n", sec2);
				printf("sec3: %08x\n", sec3);
			}
			break;
		case 0x1a:  // flash sections
					// 12Bytes ?
					// followed by flash sections separates by ';'
					// 1B + name + ' ' + cfg data offset (bits) + size (bits)
			content = _raw_data.substr(pos, size);
			parseFlag26(flag, pos, size, content);
			break;
		default:
			snprintf(mess, 1024, "unknown flag 0x%02x: offset %u length %u",
				flag, pos - 6, size);
			printWarn(mess);
			break;
	}

	return size;
}

/* section with flag 0x11A */
/* 3 x 32bits -> unknown
 * followed by flash sections separates by ';'
 * 1B + name + ' ' + cfg data offset (bits) + size (bits)
 */
void POFParser::parseFlag26(uint16_t flag, uint32_t pos,
		uint32_t size, const std::string &payload)
{
	if (_verbose)
		printf("%04x %08x %08x\n", flag, pos, size);

	if (size != payload.size())
		printf("mismatch size\n");

	if (_verbose) {
		uint32_t val0 = ARRAY2INT32((&payload.c_str()[0]));
		uint32_t val1 = ARRAY2INT32((&payload.c_str()[4]));
		uint32_t val2 = ARRAY2INT32((&payload.c_str()[8]));
		printf("%08x %08x %08x\n", val0, val1, val2);
	}

	std::regex regex{R"([;]+)"};  // split on space
	std::sregex_token_iterator it{payload.begin() + 12, payload.end(),
		regex, -1};
	std::vector<std::string> words{it, {}};

	std::regex sp_reg{R"([\s]+)"};  // split on space
	for (size_t i = 0; i < words.size(); i++) {
		std::sregex_token_iterator it{words[i].begin(), words[i].end(),
			sp_reg, -1};
		std::vector<std::string> sect{it, {}};
		uint32_t start = stoul(sect[1], nullptr, 16);
		uint32_t length = stoul(sect[2], nullptr, 16);
		uint8_t id = static_cast<uint8_t>(sect[0][0]);
		std::string name = sect[0].substr(1);
		mem_section.insert(std::pair<std::string, memory_section_t>(
					name, {id, name, start, NULL, length}));
	}
}
