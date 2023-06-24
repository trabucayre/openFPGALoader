// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (C) 2020 Gwenhael Goavec-Merou <gwenhael.goavec-merou@trabucayre.com>
 */

#include <sstream>
#include <stdexcept>

#include "configBitstreamParser.hpp"
#include "display.hpp"
#include "efinixHexParser.hpp"

using namespace std;

EfinixHexParser::EfinixHexParser(const string &filename):
		ConfigBitstreamParser(filename, ConfigBitstreamParser::ASCII_MODE,
		false)
{}

int EfinixHexParser::parse()
{
	string buffer;
	istringstream lineStream(_raw_data);

	while (std::getline(lineStream, buffer, '\n')) {
		_bit_data += std::stol(buffer, nullptr, 16);
	}
	_bit_length = _bit_data.size() * 8;

	return EXIT_SUCCESS;
}
