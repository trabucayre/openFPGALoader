// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (C) 2021 Gwenhael Goavec-Merou <gwenhael.goavec-merou@trabucayre.com>
 * Copyright (C) 2021 Cologne Chip AG <support@colognechip.com>
 */

#ifndef SRC_COLOGNECHIPCFGPARSER_HPP_
#define SRC_COLOGNECHIPCFGPARSER_HPP_

#include <string>
#include <algorithm>

#include "configBitstreamParser.hpp"

class CologneChipCfgParser: public ConfigBitstreamParser {
	public:
		CologneChipCfgParser(const std::string &filename, bool reverseOrder);

		int parse() override;

	private:
		bool _reverseOrder;
};

#endif  // SRC_COLOGNECHIPCFGPARSER_HPP_
