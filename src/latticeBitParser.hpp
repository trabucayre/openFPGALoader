// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (C) 2019 Gwenhael Goavec-Merou <gwenhael.goavec-merou@trabucayre.com>
 */
#ifndef LATTICEBITPARSER_HPP_
#define LATTICEBITPARSER_HPP_

#include <iostream>
#include <fstream>
#include <map>
#include <string>

#include "configBitstreamParser.hpp"

class LatticeBitParser: public ConfigBitstreamParser {
	public:
		LatticeBitParser(const std::string &filename, bool verbose = false);
		~LatticeBitParser();
		int parse() override;

	private:
		int parseHeader();
		size_t _endHeader;
};

#endif  // LATTICEBITPARSER_HPP_
