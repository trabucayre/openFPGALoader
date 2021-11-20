// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (C) 2021 Martin Beynon <martin.beynon@abaco.com>
 */
#ifndef FEAPARSER_HPP_
#define FEAPARSER_HPP_

#include <stdint.h>

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "configBitstreamParser.hpp"

class FeaParser: public ConfigBitstreamParser {
	public:
		FeaParser(std::string filename, bool verbose = false);
		int parse() override;
		void displayHeader() override;

		uint32_t* featuresRow() {return _featuresRow;}
		uint32_t feabits() {return _feabits;}

	private:
		std::vector<std::string>readFeaFile();
		void parseFeatureRowAndFeabits(const std::vector<std::string> &content);

		uint32_t _featuresRow[3];
		uint32_t _feabits;
		bool _has_feabits;

		std::istringstream _ss;
};

#endif  // FEAPARSER_HPP_
