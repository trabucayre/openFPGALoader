// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (C) 2019 Gwenhael Goavec-Merou <gwenhael.goavec-merou@trabucayre.com>
 */

#ifndef BITPARSER_H
#define BITPARSER_H

#include <iostream>
#include <fstream>
#include <string>

#include "configBitstreamParser.hpp"

class BitParser: public ConfigBitstreamParser {
	public:
		BitParser(const std::string &filename, bool reverseOrder, bool verbose = false);
		~BitParser();
		int parse() override;

	private:
		int parseHeader();
		bool _reverseOrder;
};

#endif
