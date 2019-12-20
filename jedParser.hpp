/*
 * Copyright (C) 2019 Gwenhael Goavec-Merou <gwenhael.goavec-merou@trabucayre.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */
#ifndef JEDPARSER_HPP_
#define JEDPARSER_HPP_

#include <stdint.h>

#include <iostream>
#include <fstream>
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
		void display();

		size_t nb_section() { return _data_list.size();}
		size_t offset_for_section(int id) {return _data_list[id].offset;}
		std::vector<std::string> data_for_section(int id) {
			return _data_list[id].data;
		}
		std::string noteForSection(int id) {return _data_list[id].associatedPrevNote;}
		uint32_t feabits() {return _feabits;}
		uint64_t featuresRow() {return _featuresRow;}

	private:
		std::vector<std::string>readJEDLine();
		void buildDataArray(const std::string &content, struct jed_data &jed);
		void parseEField(const std::vector<std::string> content);
		void parseLField(const std::vector<std::string> content);

		std::vector<struct jed_data> _data_list;
		int _fuse_count;
		int _pin_count;
		uint64_t _featuresRow;
		uint16_t _feabits;
		uint16_t _checksum;
		uint32_t _userCode;
		uint8_t _security_settings;
		uint8_t _default_fuse_state;
};

#endif  // JEDPARSER_HPP_
