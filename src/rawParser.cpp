// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (C) 2020 Gwenhael Goavec-Merou <gwenhael.goavec-merou@trabucayre.com>
 */

#include <stdexcept>
#include <utility>

#include "configBitstreamParser.hpp"
#include "display.hpp"
#include "rawParser.hpp"

using namespace std;

RawParser::RawParser(const string &filename, bool reverseOrder):
		ConfigBitstreamParser(filename, ConfigBitstreamParser::BIN_MODE,
		false), _reverseOrder(reverseOrder)
{}

int RawParser::parse()
{
	_bit_data.resize(_file_size);
	std::move(_raw_data.begin(), _raw_data.end(), _bit_data.begin());
	_bit_length = _bit_data.size();

	if (_reverseOrder) {
		for (int i = 0; i < _bit_length; i++) {
			_bit_data[i] = reverseByte(_bit_data[i]);
		}
	}

	/* convert size to bit */
	_bit_length *= 8;

	return EXIT_SUCCESS;
}
