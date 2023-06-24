// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (C) 2019-2022 Gwenhael Goavec-Merou <gwenhael.goavec-merou@trabucayre.com>
 */
#ifndef SRC_LATTICEBITPARSER_HPP_
#define SRC_LATTICEBITPARSER_HPP_

#include <iostream>
#include <fstream>
#include <map>
#include <string>
#include <vector>

#include "configBitstreamParser.hpp"

class LatticeBitParser: public ConfigBitstreamParser {
	public:
		LatticeBitParser(const std::string &filename, bool machxo2,
			bool verbose = false);
		~LatticeBitParser();
		int parse() override;

		/*!
		 * \brief return configuration data with structure similar to jedec
		 * \return configuration data
		 */
		std::vector<std::string> getDataArray() {return _bit_array;}

	private:
		int parseHeader();
		bool parseCfgData();
		size_t _endHeader;
		bool _is_machXO2;
		/* data storage for machXO2 */
		std::vector<std::string> _bit_array;
};

#endif  // SRC_LATTICEBITPARSER_HPP_
