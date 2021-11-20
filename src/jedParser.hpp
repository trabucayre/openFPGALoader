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
		int len_for_section(int id) {return _data_list[id].len;}
		std::string get_fuselist() {return fuselist;}
		int get_fuse_count() {return _fuse_count;}
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
		void buildDataArray(const std::vector<std::string> &content,
				struct jed_data &jed);
		void parseEField(const std::vector<std::string> &content);
		void parseLField(const std::vector<std::string> &content);

		std::vector<struct jed_data> _data_list;
		int _fuse_count;
		int _pin_count;
		int _max_vect_test;
		uint64_t _featuresRow;
		uint16_t _feabits;
		bool _has_feabits;
		uint16_t _checksum;
		uint16_t _compute_checksum;
		uint32_t _userCode;
		uint8_t _security_settings;
		uint8_t _default_fuse_state;
		std::istringstream _ss;
		int _default_test_condition;
		int _arch_code;
		int _pinout_code;
		std::string fuselist;
};

#endif  // JEDPARSER_HPP_
