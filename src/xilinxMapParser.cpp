// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (C) 2021 Gwenhael Goavec-Merou <gwenhael.goavec-merou@trabucayre.com>
 */
#include <algorithm>
#include <iostream>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <vector>

#include "jedParser.hpp"
#include "xilinxMapParser.hpp"

using namespace std;

XilinxMapParser::XilinxMapParser(const string &filename,
		uint16_t num_row, uint16_t num_col,
		JedParser *jed, const uint32_t usercode,
		bool verbose): ConfigBitstreamParser(filename,
			ConfigBitstreamParser::BIN_MODE, verbose),
		_num_row(num_row), _num_col(num_col), _usercode(usercode)
{
	_jed = jed;
	_map_table.resize(_num_row);
	for (int i = 0; i < _num_row; i++)
		_map_table[i].resize(_num_col);
	_bit_data.reserve(_num_row * _num_col);
}

/* extract info from map file
 * see XILINX PROGRAMMER QUALIFICATION SPECIFICATION 4.4.1
 */
int XilinxMapParser::parse()
{
	int col = 0;
	std::stringstream ss;
	ss.str(_raw_data);
	std::string line;

	while(std::getline(ss, line, '\n')) {
		bool empty = true;  // to said if line contains only '\t' (empty)
		                    // or one or more blank in a non empty line
		size_t prev_pos = 0, next_pos;
		int row = 0;
		/* suppress potential '\r' (thanks windows) */
		if (line.back() == '\r')
			line.pop_back();

		do {
			int map_val = 0;
			next_pos = line.find_first_of('\t', prev_pos);
			size_t end_pos = (next_pos == std::string::npos) ? line.size():next_pos;
			if (end_pos == prev_pos) { /* blank (transfer) bit ('\t') */
				if (empty)  // no data before
					map_val = BIT_ZERO;
				else  // blank into non empty line
					map_val = BIT_ONE;
			} else {
				empty = false;  // current line is not fully empty
				int len = end_pos - prev_pos;  // section len
				string cnt = line.substr(prev_pos, len);  // line substring
				if (cnt[0] <= '9' && cnt[0] >= '0') {  // numeric value (index)
					map_val = std::stoi(cnt, nullptr, 10);
				} else {  // information (done, spare, user, ...)
					if (!strncmp(cnt.c_str(), "spare", 5)) {
						map_val = BIT_ONE;
					} else if (!strncmp(cnt.c_str(), "sec_", 4)) {
						map_val = BIT_ONE;
					/* done is known don't wait for fuse array construct */
					} else if (!strncmp(cnt.c_str(), "done", 4)) {
						map_val = (cnt[5] == '0') ? BIT_ONE: BIT_ZERO;
					/* fill usercode now instead of waiting for fuse placement */
					} else if (!strncmp(cnt.c_str(), "user", 4)) {
						int idx = stoi(cnt.substr(5));
						map_val = ((_usercode >> idx) & 0x01) ? BIT_ONE : BIT_ZERO;
					} else {
						printf("unknown %s %s\n", cnt.c_str(), line.c_str());
						return false;
					}
				}
			}
			_map_table[row][col] = map_val;
			row++;
			prev_pos = next_pos + 1;
		} while (next_pos != std::string::npos);

		col++;  // update col after parsing each line
	}

	return jedApplyMap();
}

/* cfg_data build.
 * for fuse(x,y) set to 0, 1 or jed value (when map_data contains offset)
 * usercode and done bits are set to 0 or 1 at parse step
 */
bool XilinxMapParser::jedApplyMap()
{
	std::string listfuse = _jed->get_fuselist();
	std::string tmp;
	int row = 0;

	tmp.reserve(_map_table[0].size());
	_cfg_data.clear();
	_cfg_data.resize(_map_table.size());

	for (auto & fuse_row : _map_table) {
		tmp.clear();
		for (auto &map_bit : fuse_row) {
			int32_t map_val = map_bit;
			uint8_t bit_val;
			switch (map_val) {
				case BIT_ZERO:
					bit_val = 0;
					break;
				case BIT_ONE:
					bit_val = 1;
					break;
				default:  // map_val is an offset: get bit value from jed
					bit_val = listfuse[map_val] - '0';
			}
			tmp += bit_val;
			_bit_length++;
		}
		/* insert each row in back order (last bit 1st to send) */
		std::copy(tmp.rbegin(), tmp.rend(), std::back_inserter(_cfg_data[row]));
		row++;
	}

	return true;
}
