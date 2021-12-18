// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (C) 2019 Gwenhael Goavec-Merou <gwenhael.goavec-merou@trabucayre.com>
 */

/*
 * http://k1.spdns.de/Develop/Projects/GalAsm/info/galer/jedecfile.html
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <strings.h>

#include <algorithm>
#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>
#include <utility>
#include <vector>

#include "display.hpp"
#include "jedParser.hpp"

/* GGM: TODO
 * - use NOTE for Lxxx
 * - be less lattice compliant
 */

using namespace std;

JedParser::JedParser(string filename, bool verbose):
	ConfigBitstreamParser(filename, ConfigBitstreamParser::BIN_MODE, verbose),
	_fuse_count(0), _pin_count(0), _max_vect_test(0),
	_featuresRow(0), _feabits(0), _has_feabits(false), _checksum(0),
	_compute_checksum(0),
	_userCode(0), _security_settings(0), _default_fuse_state(0),
	_default_test_condition(0), _arch_code(0), _pinout_code(0)
{
}

/*!
 * \brief read a line with '\r''\n' or '\n' terminaison
 * check if last char is '\r'
 * \return the line without [\r]\n
 */
string JedParser::readline()
{
	string buffer;
	std::getline(_ss, buffer, '\n');
	if (!buffer.empty()) {
		/* if '\r' is present -> drop */
		if (buffer.back() == '\r')
			buffer.pop_back();
	}
	return buffer;
}

/* fill a vector with consecutive lines until '*'
 */
vector<string> JedParser::readJEDLine()
{
	string buffer;
	vector<string> lines;
	bool inLine = true;

	do {
		buffer = readline();
		if (buffer.empty())
			break;

		if (buffer.back() == '*') {
			inLine = false;
			buffer.pop_back();
		}
		lines.push_back(buffer);
	} while (inLine);
	return lines;
}

/* convert one serie ASCII 1/0 to a vector of
 * unsigned char
 */
void JedParser::buildDataArray(const string &content, struct jed_data &jed)
{
	size_t data_len = content.size();
	string tmp_buff;
	fuselist += content;
	for (size_t i = 0; i < content.size(); i+=8) {
		uint8_t data = 0;
		for (int ii = 0; ii < 8; ii++) {
			uint8_t val = (content[i+ii] == '1'?1:0);
			data |= val << ii;
		}
		tmp_buff += data;
	}
	jed.data.push_back(std::move(tmp_buff));
	jed.len += data_len;
}

/* convert one serie ASCII 1/0 to a vector of
 * unsigned char
 * string must be up to 8 bits
 */
void JedParser::buildDataArray(const vector<string> &content,
		struct jed_data &jed)
{
	size_t data_len = 0;
	string tmp_buff;
	for (size_t i = 0; i < content.size(); i++) {
		uint8_t data = 0;
		data_len += content[i].size();
		fuselist += content[i];
		for (size_t ii = 0; ii < content[i].size(); ii++) {
			uint8_t val = (content[i][ii] == '1'?1:0);
			data |= val << ii;
		}
		tmp_buff += data;
	}
	jed.data.push_back(std::move(tmp_buff));
	jed.len += data_len;
}

void JedParser::displayHeader()
{
	/* only lattice jed */
	if (_has_feabits) {
		printf("feabits :\n");
		printf("%04x <-> %d\n", _feabits, _feabits);
		/* 15-14: always 0 */
		printf("\tBoot Mode       : ");
		switch ((_feabits>>11)&0x07) {
		case 0:
			printf("Single Boot from Configuration Flash\n");
			break;
		case 1:
			printf("Dual Boot from Configuration Flash then External if there is a failure\n");
			break;
		case 3:
			printf("Single Boot from External Flash\n");
			break;
		default:
			printf("Error\n");
		}

		printf("\tMaster Mode SPI : %s\n",
			(((_feabits>>11)&0x01)?"enable":"disable"));
		printf("\tI2c port        : %s\n",
			(((_feabits>>10)&0x01)?"disable":"enable"));
		printf("\tSlave SPI port  : %s\n",
			(((_feabits>>9)&0x01)?"disable":"enable"));
		printf("\tJTAG port       : %s\n",
			(((_feabits>>8)&0x01)?"disable":"enable"));
		printf("\tDONE            : %s\n",
			(((_feabits>>7)&0x01)?"enable":"disable"));
		printf("\tINITN           : %s\n",
			(((_feabits>>6)&0x01)?"enable":"disable"));
		printf("\tPROGRAMN        : %s\n",
			(((_feabits>>5)&0x01)?"disable":"enable"));
		printf("\tMy_ASSP         : %s\n",
			(((_feabits>>4)&0x01)?"enable":"disable"));
		/* 3-0: always 0 */
	}

	printf("Pin Count  : %d\n", _pin_count);
	printf("Fuse Count : %d\n", _fuse_count);

	for (size_t i = 0; i < _data_list.size(); i++) {
		printf("area[%zd] %4d %4d ", i, _data_list[i].offset, _data_list[i].len);
		printf("%zu ", _data_list[i].data.size());
		for (size_t ii = 0; ii < _data_list[i].data.size(); ii++)
			for (size_t iii = 0; iii < _data_list[i].data[ii].size(); iii++)
				printf("%02x", (uint8_t)_data_list[i].data[ii][iii]);
		printf(" %s\n", _data_list[i].associatedPrevNote.c_str());
		if (_data_list[i].offset == 2656)
			break;
	}
}

/* E field, for latice contains two sub-field
 * 1: Exxxx\n : feature Row
 * 2: yyyy*\n : feabits
 */
void JedParser::parseEField(const vector<string> &content)
{
	_featuresRow = 0;
	string featuresRow = content[0].substr(1);
	for (size_t i = 0; i < featuresRow.size(); i++)
		_featuresRow |= ((featuresRow[i] - '0') << i);
	string feabits = content[1];
	_feabits = 0;
	for (size_t i = 0; i < feabits.size(); i++) {
		_feabits |= ((feabits[i] - '0') << i);
	}
}

void JedParser::parseLField(const vector<string> &content)
{
	int start_offset;
	sscanf(content[0].substr(1).c_str(), "%d", &start_offset);
	/* two possibilities
	 * current line finish with '*' : Lxxxx YYYYY*<EOF>
	 * or current line is only offset and next(s) line(s) are data :
	 * Lxxxx<EOF>
	 */
	struct jed_data d;
	d.offset = start_offset;
	d.len = 0;
	if (content.size() > 1) {
		for (size_t i = 1; i < content.size(); i++) {
			if (content[i].size() != 0)
				buildDataArray((content[i]), d);
		}
	} else {
		// search space
		std::istringstream iss(content[0]);
		vector<string> myList((std::istream_iterator<string>(iss)),
		std::istream_iterator<string>());

		myList.erase(myList.begin());

		buildDataArray(myList, d);
	}
	_data_list.push_back(std::move(d));
}

int JedParser::parse()
{
	string previousNote;

	_ss.str(_raw_data);

	string content;

	/* JED file may have some ASCII line before STX (0x02)
	 * read until STX or EOF
	 */
	char c;
	do {
		_ss.read(&c, 1);
	} while (_ss && c != 0x02);

	/* if file descriptor == EOF
	 * return an ERROR
	 */
	if (!_ss) {
		printError("Error: STX not found: wrong file");
		return EXIT_FAILURE;
	}

	/* the line starting with STX may contains
	 * others informations */
	_ss.read(&c, 1);
	if (c != '*')  // something to process
		_ss.seekg(-1, _ss.cur);
	else  // STX in a dedicated line
		content = readline();

	/* read full content
	 * JED file end fix ETX (0x03) + file checksum + \n
	 */
	std::vector<string>lines;
	int first_pos;
	char instr;
	do {
		lines = readJEDLine();
		/* empty or end of file */
		if (lines.size() == 0) {
			if (_ss.eof()) {
				break;
			} else {
				instr = 0;
				continue;
			}
		}
		instr = lines[0][0];

		switch (instr) {
		case 'N':  // note
			/* note may start with "N " or "NOTE " */
			first_pos = lines[0].find_first_of(' ', 0) + 1;
			previousNote = lines[0].substr(first_pos);
			break;
		case 'Q':
			int count;
			sscanf(lines[0].c_str()+2, "%d", &count);
			switch (lines[0][1]) {
				case 'F':  // fuse count
					_fuse_count = count;
					break;
				case 'P':  // pin count
					_pin_count = count;
					break;
				case 'V':  // pin count
					_max_vect_test = count;
					break;
				default:
					cerr << "Error for 'Q' unknown qualifier " << lines[0] << endl;
					return EXIT_FAILURE;
			}
			break;
		case 'G':
			_security_settings = static_cast<uint8_t>(lines[0][1]) - '0';
			break;
		case 'F':
			_default_fuse_state = lines[0][1] - '0';
			break;
		case 'J':
			sscanf(lines[0].c_str() + 1, "%d", &_arch_code);
			sscanf(lines[0].c_str() + 3, "%d", &_pinout_code);
			break;
		case 'C':
			sscanf(lines[0].c_str() + 1, "%hx", &_checksum);
			break;
		case 0x03:
			if (_verbose)
				cout << "end" << endl;
			break;
		case 'E':
			parseEField(lines);
			_has_feabits = true;
			break;
		case 'L':  // fuse offset
			parseLField(lines);
			_data_list[_data_list.size()-1].associatedPrevNote = previousNote;
			break;
		case 'U':  // userCode
			switch (lines[0][1]) {
				case 'H': /* hex */
					sscanf(lines[0].c_str() + 2, "%x", &_userCode);
					break;
				case 'A': /* ASCII */
					sscanf(lines[0].c_str() + 2, "%d", &_userCode);
					break;
				default: /* binary */
					for (size_t ii = 1; ii < lines[0].size(); ii++)
						_userCode = ((_userCode << 1) | (lines[0][ii] - '0'));
			}
			break;
		case 'X':  // default test condition
			sscanf(lines[0].c_str() + 1, "%d", &_default_test_condition);
			break;
		default:
			printf("inconnu\n");
			cout << lines[0]<< endl;
			return EXIT_FAILURE;
		}
	} while (instr != 0x03);

	int size = 0;
	for (size_t area = 0; area < _data_list.size(); area++) {
		size += _data_list[area].len;
	}

	std::string fuses;
	std::copy(fuselist.begin(), fuselist.end(), std::back_inserter(fuses));

	if (fuses.size() % 8) {
		int diff = (((fuses.size() / 8) + 1) * 8) - fuses.size();
		for (int i = 0; i < diff; i++)
			fuses += '0';
	}

	for (size_t i = 0; i < fuses.size(); i+=8)
		_compute_checksum += reverseByte(std::stoi(fuses.substr(i, 8),
				nullptr, 2));

	if (_verbose)
		printf("theorical checksum %x -> %x\n", _checksum, _compute_checksum);
	if (_checksum != _compute_checksum) {
		printError("Error: wrong checksum");
		return EXIT_FAILURE;
	}

	if (_verbose)
		printf("array size %zd\n", _data_list[0].data.size());

	if (_fuse_count != size) {
		printError("Not all fuses are programmed");
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
