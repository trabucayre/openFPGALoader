// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (C) 2021 Gwenhael Goavec-Merou <gwenhael.goavec-merou@trabucayre.com>
 * Copyright (C) 2021 Cologne Chip AG <support@colognechip.com>
 */

#include <sstream>

#include "colognechipCfgParser.hpp"

CologneChipCfgParser::CologneChipCfgParser(const std::string &filename):
		ConfigBitstreamParser(filename, ConfigBitstreamParser::ASCII_MODE,
		false)
{}

int CologneChipCfgParser::parse()
{
	std::string buffer;
	std::istringstream lineStream(_raw_data);

	while (std::getline(lineStream, buffer, '\n')) {
		std::string val = buffer.substr(0, buffer.find("//"));
		val.erase(std::remove_if(val.begin(), val.end(), ::isspace), val.end());
		if (val != "") {
			_bit_data += std::stol(val, nullptr, 16);
		}
	}
	_bit_length = _bit_data.size() * 8;

	return EXIT_SUCCESS;
}
