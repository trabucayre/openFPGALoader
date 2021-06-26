// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (C) 2019 Gwenhael Goavec-Merou <gwenhael.goavec-merou@trabucayre.com>
 */
#ifndef JEDPARSER_HPP_
#define JEDPARSER_HPP_

#include <stdint.h>

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "configBitstreamParser.hpp"

class JedParser: public ConfigBitstreamParser {
	private:
		struct jed_data {
			int offset;
			std::vector<std::string> data;
			int len;
			std::string associatedPrevNote;
		};

	public:
		JedParser(std::string filename, bool verbose = false);
		int parse() override;
		void displayHeader() override;

		size_t nb_section() { return _data_list.size();}
		size_t offset_for_section(int id) {return _data_list[id].offset;}
		std::vector<std::string> data_for_section(int id) {
			return _data_list[id].data;
		}
		std::string noteForSection(int id) {return _data_list[id].associatedPrevNote;}
		uint32_t feabits() {return _feabits;}
		uint64_t featuresRow() {return _featuresRow;}

	private:
		std::string readline();
		std::vector<std::string>readJEDLine();
		void buildDataArray(const std::string &content, struct jed_data &jed);
		void parseEField(const std::vector<std::string> &content);
		void parseLField(const std::vector<std::string> &content);

		std::vector<struct jed_data> _data_list;
		int _fuse_count;
		int _pin_count;
		uint64_t _featuresRow;
		uint16_t _feabits;
		uint16_t _checksum;
		uint32_t _userCode;
		uint8_t _security_settings;
		uint8_t _default_fuse_state;
		std::istringstream _ss;
};

#endif  // JEDPARSER_HPP_
