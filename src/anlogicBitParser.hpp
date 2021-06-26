// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (C) 2020 Gwenhael Goavec-Merou <gwenhael.goavec-merou@trabucayre.com>
 */
#ifndef SRC_ANLOGICBITPARSER_HPP_
#define SRC_ANLOGICBITPARSER_HPP_

#include <iostream>
#include <fstream>
#include <map>
#include <string>

#include "configBitstreamParser.hpp"

/*
 * Minimal implementation for anlogic bitstream
 * only parse header part and store data in a buffer
 */
class AnlogicBitParser: public ConfigBitstreamParser {
	public:
		AnlogicBitParser(const std::string &filename, bool reverseOrder,
			bool verbose = false);
		~AnlogicBitParser();
		int parse() override;

	private:
		int parseHeader();
		bool _reverseOrder;
};

#endif  // SRC_ANLOGICBITPARSER_HPP_
