// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (C) 2019 Gwenhael Goavec-Merou <gwenhael.goavec-merou@trabucayre.com>
 */

#ifndef MCSPARSER_HPP
#define MCSPARSER_HPP

#include <string>

#include "configBitstreamParser.hpp"

class McsParser: public ConfigBitstreamParser {
	public:
		McsParser(const std::string &filename, bool reverseOrder, bool verbose);
		int parse() override;

	private:
		int _base_addr;
		bool _reverseOrder;
};

#endif

